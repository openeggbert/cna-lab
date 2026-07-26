#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
input="${1:-$project_root/assets/source/mc3/prototype_city_block.mc3.xml}"
mesh_craft_dir="${MESH_CRAFT_SOURCE_DIR:-$project_root/../mesh-craft}"
schema="${MC3_SCHEMA:-$mesh_craft_dir/mc3/mc3.xsd}"

if [[ ! -f "$input" ]]; then
  echo "MC3 input not found: $input" >&2
  exit 1
fi
if [[ ! -f "$schema" ]]; then
  echo "MC3 schema not found: $schema" >&2
  echo "Set MESH_CRAFT_SOURCE_DIR or MC3_SCHEMA." >&2
  exit 1
fi

if command -v xmllint >/dev/null 2>&1; then
  xmllint --noout --schema "$schema" "$input"
  exit 0
fi

python3 - "$schema" "$input" <<'PY'
import sys
try:
    from lxml import etree
except ImportError as exc:
    raise SystemExit("Neither xmllint nor Python lxml is available for XSD validation") from exc
schema_path, input_path = sys.argv[1], sys.argv[2]
schema = etree.XMLSchema(etree.parse(schema_path))
document = etree.parse(input_path)
if not schema.validate(document):
    for error in schema.error_log:
        print(error, file=sys.stderr)
    raise SystemExit(1)
print(f"MC3 valid: {input_path}")
PY
