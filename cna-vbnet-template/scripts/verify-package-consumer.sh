#!/usr/bin/env bash
set -euo pipefail

repository_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
dotnet_command="${DOTNET_COMMAND:-dotnet}"
cna_cs_root="${CNA_CS_ROOT:-$(cd "$repository_root/../cna-cs" 2>/dev/null && pwd || true)}"
package_version="${CNA_LOCAL_PACKAGE_VERSION:-0.1.0-local.1}"
native_library="${CNA_NATIVE_LIBRARY:-}"

if [[ "$(uname -s)" != "Linux" || "$(uname -m)" != "x86_64" ]]; then
    printf 'Package acceptance is evidence-scoped to Linux x64; this host is %s/%s.\n' \
        "$(uname -s)" "$(uname -m)" >&2
    exit 2
fi

if [[ -z "$cna_cs_root" || ! -f "$cna_cs_root/CNA.sln" ]]; then
    printf '%s\n' 'Set CNA_CS_ROOT to a CNA-CS checkout containing CNA.sln.' >&2
    exit 2
fi

if [[ -z "$native_library" || ! -f "$native_library" ]]; then
    printf '%s\n' 'Set CNA_NATIVE_LIBRARY to a compatible Linux x64 libcna_c_api.so.' >&2
    exit 2
fi

acceptance_property="$cna_cs_root/Directory.Build.props"
if ! grep -q 'CnaPackageAcceptance' "$acceptance_property"; then
    printf '%s\n' 'This CNA-CS checkout does not expose the local package-acceptance mode.' >&2
    exit 2
fi

acceptance_root="$(mktemp -d "${TMPDIR:-/tmp}/cna-vb-package-acceptance.XXXXXXXX")"
feed="$acceptance_root/feed"
packages="$acceptance_root/packages"
cli_home="$acceptance_root/cli-home"
mkdir -p "$feed" "$packages" "$cli_home"
trap 'rm -rf "$acceptance_root"' EXIT

export DOTNET_CLI_HOME="$cli_home"
export NUGET_PACKAGES="$packages"
export DOTNET_NOLOGO=1
export DOTNET_SKIP_FIRST_TIME_EXPERIENCE=1
export DOTNET_CLI_TELEMETRY_OPTOUT=1

common_properties=(
    -p:CnaPackageAcceptance=true
    -p:CnaPackageVersion="$package_version"
)

"$dotnet_command" restore "$cna_cs_root/CNA.sln" "${common_properties[@]}"
"$dotnet_command" build "$cna_cs_root/CNA.sln" -c Release --no-restore -m:1 "${common_properties[@]}"

"$dotnet_command" pack "$cna_cs_root/src/CNA.Interop/CNA.Interop.csproj" -c Release --no-build --no-restore \
    -o "$feed" "${common_properties[@]}" \
    -p:CnaNativeLibrary="$native_library" -p:CnaNativeRid=linux-x64
"$dotnet_command" pack "$cna_cs_root/src/CNA.Framework/CNA.Framework.csproj" -c Release --no-build --no-restore \
    -o "$feed" "${common_properties[@]}"
"$dotnet_command" pack "$cna_cs_root/src/CNA.XnaCompat/CNA.XnaCompat.csproj" -c Release --no-build --no-restore \
    -o "$feed" "${common_properties[@]}"

for package_id in CNA.Interop CNA.Framework CNA.XnaCompat; do
    package_path="$feed/$package_id.$package_version.nupkg"
    if [[ ! -f "$package_path" ]]; then
        printf 'Missing local acceptance package: %s\n' "$package_path" >&2
        exit 1
    fi
done

DOTNET_COMMAND="$dotnet_command" \
CNA_PACKAGE_SOURCE_ROOT="$cna_cs_root" \
CNA_TEMPLATE_USE_XVFB="${CNA_TEMPLATE_USE_XVFB:-0}" \
CNA_TEMPLATE_RUN_STABILITY="${CNA_TEMPLATE_RUN_STABILITY:-1}" \
CNA_TEMPLATE_REQUIRE_3D="${CNA_TEMPLATE_REQUIRE_3D:-0}" \
    "$repository_root/scripts/verify-template.sh" \
        --mode package --package-feed "$feed" --package-version "$package_version"

printf 'PACKAGE_ACCEPTANCE=PASS\nPACKAGE_VERSION=%s\n' "$package_version"
