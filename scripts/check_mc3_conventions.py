#!/usr/bin/env python3
"""plan_09 IG-09-006/007: Iron Gang's own MC3 authoring conventions, enforced.

Mesh Craft's XSD deliberately leaves several things open that a *project* has to close. The one
that matters most here is `collision`: the schema types it `xs:string` with a default of "none", so
any spelling validates. Iron Gang has been writing "static" and "trigger" across six files -- values
that are **not** in MC3's own documented vocabulary (none/box/sphere/capsule/mesh/convex) and that
nothing has ever checked.

That is a deliberate divergence, not a mistake, and this script is where it is written down:

  MC3's vocabulary describes the collider's SHAPE. Iron Gang's describes its ROLE. Every collider in
  this game is an axis-aligned box (`PrototypeWorld`'s `Aabb`), so shape carries no information --
  while "is this a wall, a trigger volume, or decoration?" carries all of it. The values reach the
  glTF verbatim as `node.extras.collision`, so a future importer reads the role directly.

  The cost, recorded so nobody rediscovers it: Mesh Craft's own Walk Mode reads the shape vocabulary
  and will not understand "static". A file authored for both would need revisiting.

Also enforced: document-level units and axes, and that every geometry object states its collision
explicitly rather than inheriting the schema default -- "unspecified" and "deliberately none" look
identical otherwise, and only one of them is an authoring decision.
"""

import argparse
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

# Role, not shape. See the module docstring.
ALLOWED_COLLISION = ("none", "static", "trigger")
GEOMETRY_TAGS = (
    "box", "cube", "sphere", "cylinder", "cone", "plane", "torus",
    "capsule", "disk", "grid", "icosphere", "mesh", "extrude",
)
CONTAINER_TAGS = ("group", "union", "difference", "intersection")
REQUIRED_UNIT = "meter"
REQUIRED_COORDINATE_SYSTEM = "right_handed_y_up"


class ConventionError(ValueError):
    pass


def _walk(element, path: Path, where: str, violations: list[str]) -> int:
    """Returns how many geometry objects were examined, so a run that visits nothing can say so."""
    examined = 0
    for child in element:
        label = f"{child.tag} '{child.get('name', '?')}'"
        if child.tag in GEOMETRY_TAGS:
            examined += 1
            collision = child.get("collision")
            if collision is None:
                violations.append(
                    f"{path.name}: {where} {label} does not state a collision role; MC3 defaults it "
                    f"to \"none\", which makes \"unspecified\" and \"deliberately none\" identical")
            elif collision not in ALLOWED_COLLISION:
                violations.append(
                    f"{path.name}: {where} {label} has collision=\"{collision}\"; Iron Gang allows "
                    f"{', '.join(ALLOWED_COLLISION)}")
        elif child.tag in CONTAINER_TAGS:
            examined += _walk(child, path, where, violations)
    return examined


def check_file(path: Path) -> tuple[list[str], int]:
    violations: list[str] = []
    examined = 0
    try:
        root = ET.parse(path).getroot()
    except (OSError, ET.ParseError) as error:
        raise ConventionError(f"could not parse {path}: {error}") from error
    if root.tag != "mc3":
        raise ConventionError(f"{path}: expected an mc3 root element")

    if not root.get("model"):
        violations.append(f"{path.name}: the document declares no model name")
    unit = root.get("unit", REQUIRED_UNIT)
    if unit != REQUIRED_UNIT:
        violations.append(f"{path.name}: unit=\"{unit}\"; Iron Gang authors in {REQUIRED_UNIT}")
    axes = root.get("coordinate_system", REQUIRED_COORDINATE_SYSTEM)
    if axes != REQUIRED_COORDINATE_SYSTEM:
        violations.append(
            f"{path.name}: coordinate_system=\"{axes}\"; Iron Gang authors in "
            f"{REQUIRED_COORDINATE_SYSTEM}")

    definitions = root.find("definitions")
    if definitions is not None:
        for definition in definitions.findall("definition"):
            identifier = definition.get("id")
            if not identifier:
                violations.append(f"{path.name}: a <definition> has no id")
            examined += _walk(definition, path, f"definition '{identifier or '?'}':", violations)

    objects = root.find("objects")
    if objects is None:
        violations.append(f"{path.name}: the document has no <objects> section")
    else:
        examined += _walk(objects, path, "object", violations)
    return violations, examined


def check(sources: list[Path]) -> list[str]:
    if not sources:
        raise ConventionError("no MC3 sources given; the check would pass vacuously")
    violations: list[str] = []
    examined = 0
    for source in sorted(sources):
        found, count = check_file(source)
        violations.extend(found)
        examined += count
    if examined == 0:
        # A checker that visits nothing reports success forever. That is the failure mode where a
        # tag list goes stale and the whole thing quietly stops policing anything.
        raise ConventionError("no geometry objects were examined; the check would pass vacuously")
    return violations


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("sources", nargs="*", help="MC3 files; defaults to assets/source/mc3/*.mc3.xml")
    parser.add_argument("--project-root", default=".")
    options = parser.parse_args(argv)
    root = Path(options.project_root).resolve()
    sources = ([Path(s) for s in options.sources] if options.sources
               else sorted((root / "assets" / "source" / "mc3").glob("*.mc3.xml")))
    try:
        violations = check(sources)
    except ConventionError as error:
        print(f"check-mc3-conventions: {error}", file=sys.stderr)
        return 2
    if violations:
        for violation in violations:
            print(f"check-mc3-conventions: {violation}", file=sys.stderr)
        return 1
    print(f"check-mc3-conventions: {len(sources)} MC3 source(s) follow the conventions")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
