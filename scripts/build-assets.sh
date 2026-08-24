#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
input="${1:-$project_root/assets/source/mc3/prototype_city_block.mc3.xml}"
output_root="${2:-$project_root/assets/generated/prototype_city_block}"

: "${MESH_CRAFT_BUILD_DIR:?Set MESH_CRAFT_BUILD_DIR to a built Mesh Craft directory}"
: "${CNA_BUILD_DIR:?Set CNA_BUILD_DIR to a built CNA directory containing cna_tool_gltf_to_cnj}"

mc3togltf="$MESH_CRAFT_BUILD_DIR/mc3togltf/mc3togltf"
gltf_to_cnj="$CNA_BUILD_DIR/cna_tool_gltf_to_cnj"

if [[ ! -x "$mc3togltf" ]]; then
  echo "mc3togltf not found or not executable: $mc3togltf" >&2
  exit 1
fi
if [[ ! -x "$gltf_to_cnj" ]]; then
  echo "cna_tool_gltf_to_cnj not found or not executable: $gltf_to_cnj" >&2
  exit 1
fi

"$project_root/scripts/validate-mc3.sh" "$input"
"$project_root/scripts/content_budget.py" \
  --policy "$project_root/assets/content-budgets.json" \
  --source "$input"

mkdir -p "$output_root/glb" "$output_root/cnj"
base_name="$(basename "$input")"
base_name="${base_name%.xml}"
base_name="${base_name%.mc3}"

glb="$output_root/glb/$base_name.glb"
"$mc3togltf" --stats --quantize-mesh-attributes "$input" "$glb"
"$gltf_to_cnj" "$glb" "$output_root/cnj" "$base_name" 1.0

echo "Generated runtime assets in: $output_root"
