#!/usr/bin/env python3
"""plan_03 IG-03-006: the layering checker must actually catch each rule it claims to enforce.

The repository currently passes every rule, so running the checker against the real tree proves
only that it does not crash. Each rule is therefore also exercised against a fixture that breaks
exactly that rule -- otherwise a checker with a typo'd prefix would report "boundaries hold"
forever and nobody would know.
"""

import shutil
import sys
import unittest
from pathlib import Path
from tempfile import TemporaryDirectory

CHECKER = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else None
PROJECT_ROOT = Path(sys.argv[2]).resolve() if len(sys.argv) > 2 else None

sys.path.insert(0, str(CHECKER.parent) if CHECKER else ".")
import check_layering  # noqa: E402


CMAKE = """
add_library(iron_gang_core STATIC
    src/Core/Thing.cpp
)
add_executable(iron_gang
    src/main.cpp
)
"""


class LayeringCheckerTests(unittest.TestCase):
    def setUp(self):
        self._directory = TemporaryDirectory()
        self.root = Path(self._directory.name)
        (self.root / "src" / "Core").mkdir(parents=True)
        (self.root / "include" / "IronGang" / "Core").mkdir(parents=True)
        (self.root / "include" / "IronGang" / "Application").mkdir(parents=True)
        (self.root / "CMakeLists.txt").write_text(CMAKE)
        (self.root / "src" / "Core" / "Thing.cpp").write_text('#include "IronGang/Core/Thing.hpp"\n')
        (self.root / "src" / "main.cpp").write_text("#include <cstdio>\n")
        self.public = self.root / "include" / "IronGang" / "Core" / "Thing.hpp"
        self.public.write_text("#include <string>\n")

    def tearDown(self):
        self._directory.cleanup()

    def test_a_clean_tree_passes(self):
        self.assertEqual(check_layering.check(self.root), [])

    def test_public_header_naming_sharp_runtime_is_caught(self):
        self.public.write_text('#include "System/Text/Json/JsonElement.hpp"\n')
        violations = check_layering.check(self.root)
        self.assertEqual(len(violations), 1, violations)
        self.assertIn("private dependency", violations[0])

    def test_public_header_naming_a_cna_internal_is_caught(self):
        self.public.write_text('#include "CNA/Internal/Graphics/ImageLoader.hpp"\n')
        self.assertEqual(len(check_layering.check(self.root)), 1)

    def test_public_header_reaching_into_src_is_caught(self):
        self.public.write_text('#include "../../../src/Core/Secret.hpp"\n')
        violations = check_layering.check(self.root)
        self.assertEqual(len(violations), 1, violations)
        self.assertIn("reach into src/", violations[0])

    def test_public_header_using_cna_runtime_is_caught(self):
        self.public.write_text('#include "Microsoft/Xna/Framework/Game.hpp"\n')
        violations = check_layering.check(self.root)
        self.assertEqual(len(violations), 1, violations)
        self.assertIn("only the executable", violations[0])

    def test_the_executables_own_header_may_use_cna_runtime(self):
        # The exception, and it is narrow: only include/IronGang/Application.
        application = self.root / "include" / "IronGang" / "Application" / "Game.hpp"
        application.write_text('#include "Microsoft/Xna/Framework/Game.hpp"\n')
        self.assertEqual(check_layering.check(self.root), [])

    def test_a_core_source_using_cna_runtime_is_caught(self):
        (self.root / "src" / "Core" / "Thing.cpp").write_text(
            '#include "Microsoft/Xna/Framework/GraphicsDeviceManager.hpp"\n')
        violations = check_layering.check(self.root)
        self.assertEqual(len(violations), 1, violations)
        self.assertIn("CNA::GraphicsCore only", violations[0])

    def test_a_core_source_may_use_private_dependencies(self):
        # The whole point of the split: the library uses sharp-runtime, its headers just must not
        # say so.
        (self.root / "src" / "Core" / "Thing.cpp").write_text(
            '#include "System/IO/File.hpp"\n#include "CNA/Internal/Graphics/ImageLoader.hpp"\n')
        self.assertEqual(check_layering.check(self.root), [])

    def test_a_vacuous_pass_is_refused(self):
        # A checker that finds nothing to check must fail loudly rather than report success -- the
        # failure mode where a moved directory silently disables the whole thing.
        shutil.rmtree(self.root / "include")
        (self.root / "include").mkdir()
        with self.assertRaises(check_layering.LayeringError):
            check_layering.check(self.root)

        (self.root / "CMakeLists.txt").write_text("add_library(iron_gang_core STATIC\n)\n")
        with self.assertRaises(check_layering.LayeringError):
            check_layering.check(self.root)

    def test_a_source_listed_in_cmake_but_missing_is_an_error(self):
        (self.root / "src" / "Core" / "Thing.cpp").unlink()
        with self.assertRaises(check_layering.LayeringError):
            check_layering.check(self.root)

    def test_the_real_repository_passes(self):
        self.assertEqual(check_layering.check(PROJECT_ROOT), [])


if __name__ == "__main__":
    if CHECKER is None or PROJECT_ROOT is None:
        print("usage: test_layering.py <check_layering.py> <project-root>", file=sys.stderr)
        raise SystemExit(2)
    unittest.main(argv=[sys.argv[0]], verbosity=2)
