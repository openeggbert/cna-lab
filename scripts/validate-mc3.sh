#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
input="${1:-$project_root/assets/source/mc3/prototype_city_block.mc3.xml}"
# The schema lives in the Mesh Craft checkout. Search rather than assume one layout: this
# repository has already been moved once (see NEXT.md), and a hard-coded "../mesh-craft" silently
# skipped validation for everyone whose workspace was one level deeper.
find_schema() {
  local candidate
  for candidate in \
    "${MC3_SCHEMA:-}" \
    "${MESH_CRAFT_SOURCE_DIR:+$MESH_CRAFT_SOURCE_DIR/mc3/mc3.xsd}" \
    "${MESH_CRAFT_BUILD_DIR:+$MESH_CRAFT_BUILD_DIR/../mc3/mc3.xsd}" \
    "${IRON_GANG_CNA_DIR:+$IRON_GANG_CNA_DIR/../mesh-craft/mc3/mc3.xsd}" \
    "$project_root/../mesh-craft/mc3/mc3.xsd" \
    "$project_root/../../mesh-craft/mc3/mc3.xsd"
  do
    if [[ -n "$candidate" && -f "$candidate" ]]; then
      printf '%s' "$candidate"
      return 0
    fi
  done
  return 1
}

schema="$(find_schema || true)"

if [[ ! -f "$input" ]]; then
  echo "MC3 input not found: $input" >&2
  exit 1
fi
if [[ -z "$schema" ]]; then
  echo "MC3 schema (mc3.xsd) not found in any known Mesh Craft location." >&2
  echo "Set MC3_SCHEMA to the file, or MESH_CRAFT_SOURCE_DIR to the checkout." >&2
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
