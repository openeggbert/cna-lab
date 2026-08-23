#!/usr/bin/env bash
set -euo pipefail

repository_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
dotnet_command="${DOTNET_COMMAND:-dotnet}"
template_mode="development"
package_feed=""
package_version="0.1.0-local.1"

usage() {
    printf '%s\n' \
        "Usage: $0 [--mode development|package] [--package-feed DIR] [--package-version VERSION]" \
        "" \
        "Environment:" \
        "  CNA_CS_ROOT                 CNA-CS checkout for Development mode" \
        "  CNA_NATIVE_LIBRARY          Exact native CNA C ABI library" \
        "  CNA_NATIVE_DIR              Directory containing the native CNA C ABI library" \
        "  CNA_TEMPLATE_USE_XVFB=1     Run native tests under xvfb-run" \
        "  CNA_TEMPLATE_RUN_STABILITY=1  Also run 600-frame tests"
}

while (($# > 0)); do
    case "$1" in
        --mode)
            (($# >= 2)) || { usage >&2; exit 2; }
            template_mode="$2"
            shift 2
            ;;
        --package-feed)
            (($# >= 2)) || { usage >&2; exit 2; }
            package_feed="$2"
            shift 2
            ;;
        --package-version)
            (($# >= 2)) || { usage >&2; exit 2; }
            package_version="$2"
            shift 2
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            printf 'Unknown argument: %s\n' "$1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

case "$template_mode" in
    development|package) ;;
    *) printf 'Unsupported mode: %s\n' "$template_mode" >&2; exit 2 ;;
esac

if [[ "$template_mode" == "package" && -z "$package_feed" ]]; then
    printf '%s\n' 'Package mode requires --package-feed.' >&2
    exit 2
fi

if [[ "$template_mode" == "development" ]]; then
    cna_cs_root="${CNA_CS_ROOT:-$(cd "$repository_root/../cna-cs" 2>/dev/null && pwd || true)}"
    if [[ -z "$cna_cs_root" || ! -f "$cna_cs_root/src/CNA.XnaCompat/CNA.XnaCompat.csproj" ]]; then
        printf '%s\n' 'Development mode requires CNA_CS_ROOT containing src/CNA.XnaCompat/CNA.XnaCompat.csproj.' >&2
        exit 2
    fi
else
    cna_cs_root=""
    package_feed="$(cd "$package_feed" && pwd)"
fi

verification_root="$(mktemp -d "${TMPDIR:-/tmp}/cna-vb-template-verify.XXXXXXXX")"
template_hive="$verification_root/template-hive"
generated_root="$verification_root/generated"
cli_home="$verification_root/cli-home"
packages_root="$verification_root/packages"
mkdir -p "$template_hive" "$generated_root" "$cli_home" "$packages_root"
trap 'rm -rf "$verification_root"' EXIT

export DOTNET_CLI_HOME="$cli_home"
export NUGET_PACKAGES="$packages_root"
export DOTNET_NOLOGO=1
export DOTNET_SKIP_FIRST_TIME_EXPERIENCE=1
export DOTNET_CLI_TELEMETRY_OPTOUT=1

run_dotnet() {
    "$dotnet_command" "$@"
}

run_game() {
    local project_path="$1"
    local frame_option="$2"
    shift 2

    local command=("$dotnet_command" run --project "$project_path" --no-build --no-restore "$@" -- "$frame_option")
    if [[ "${CNA_TEMPLATE_USE_XVFB:-0}" == "1" ]]; then
        xvfb-run -a "${command[@]}"
    else
        "${command[@]}"
    fi
}

audit_generated_paths() {
    local project_directory="$1"
    local configured_source="${2:-}"
    local obsolete_binding_pattern='cna-'"vbnet"
    local checkout_prefix='/'"rv/"

    if grep -R -I -i -E "$obsolete_binding_pattern|\.\./cna-cs|$checkout_prefix" "$project_directory"; then
        printf '%s\n' 'Generated project contains a forbidden source/binding path.' >&2
        return 1
    fi

    if [[ -n "$configured_source" ]] && grep -R -I -F "$configured_source" "$project_directory"; then
        printf '%s\n' 'Generated project embeds the configured CNA-CS checkout.' >&2
        return 1
    fi
}

audit_configured_source_path() {
    local project_directory="$1"
    local configured_source="${2:-}"

    if [[ -n "$configured_source" ]] && grep -R -I -F "$configured_source" "$project_directory"; then
        printf '%s\n' 'Generated project embeds the configured CNA-CS checkout.' >&2
        return 1
    fi
}

run_native_tests_if_configured() {
    local project_path="$1"
    shift

    if [[ -z "${CNA_NATIVE_LIBRARY:-}" && -z "${CNA_NATIVE_DIR:-}" ]]; then
        printf '%s\n' 'Native runtime not configured; source runtime tests skipped.'
        return
    fi

    run_game "$project_path" --smoke-test "$@"
    if [[ "${CNA_TEMPLATE_RUN_STABILITY:-0}" == "1" ]]; then
        run_game "$project_path" --stability-test "$@"
    fi
}

if [[ "$template_mode" == "development" ]]; then
    printf 'TEMPLATE_MODE=Development\n'
    run_dotnet restore "$repository_root/CnaVbTemplate.vbproj" -p:CnaCsRoot="$cna_cs_root"
    run_dotnet build "$repository_root/CnaVbTemplate.vbproj" --no-restore -m:1 -p:CnaCsRoot="$cna_cs_root"
    run_dotnet restore "$repository_root/tests/CNA.VB.CompileProbe/CNA.VB.CompileProbe.vbproj" -p:CnaCsRoot="$cna_cs_root"
    run_dotnet build "$repository_root/tests/CNA.VB.CompileProbe/CNA.VB.CompileProbe.vbproj" --no-restore -m:1 -p:CnaCsRoot="$cna_cs_root"
    run_dotnet run --project "$repository_root/tests/CNA.VB.CompileProbe/CNA.VB.CompileProbe.vbproj" --no-build --no-restore -p:CnaCsRoot="$cna_cs_root"
    run_native_tests_if_configured "$repository_root/CnaVbTemplate.vbproj" -p:CnaCsRoot="$cna_cs_root"
fi

run_dotnet new install "$repository_root" --force --debug:custom-hive "$template_hive"

generated_name="FreshVbGame"
generated_project="$generated_root/$generated_name/$generated_name.vbproj"
if [[ "$template_mode" == "development" ]]; then
    run_dotnet new cna-game-vb -n "$generated_name" -o "$generated_root/$generated_name" \
        --consumerMode Development --debug:custom-hive "$template_hive"
    audit_generated_paths "$generated_root/$generated_name" "$cna_cs_root"

    run_dotnet restore "$generated_project" -p:CnaCsRoot="$cna_cs_root"
    run_dotnet build "$generated_project" --no-restore -m:1 -p:CnaCsRoot="$cna_cs_root"
    run_native_tests_if_configured "$generated_project" -p:CnaCsRoot="$cna_cs_root"
else
    printf 'TEMPLATE_MODE=Package\n'
    run_dotnet new cna-game-vb -n "$generated_name" -o "$generated_root/$generated_name" \
        --consumerMode Package --cnaPackageVersion "$package_version" \
        --debug:custom-hive "$template_hive"
    audit_generated_paths "$generated_root/$generated_name" "${CNA_PACKAGE_SOURCE_ROOT:-}"

    if grep -R -I -E 'CnaCsRoot|CNA_CS_ROOT|ProjectReference' "$generated_root/$generated_name"; then
        printf '%s\n' 'Package consumer retained a source-checkout reference hook.' >&2
        exit 1
    fi
    grep -Fq '<PackageReference Include="CNA.XnaCompat"' "$generated_project"

    run_dotnet new nugetconfig --output "$verification_root"
    nuget_config="$verification_root/nuget.config"
    run_dotnet nuget remove source nuget --configfile "$nuget_config"
    run_dotnet nuget add source "$package_feed" --name cna-local --configfile "$nuget_config"

    run_dotnet restore "$generated_project" --configfile "$nuget_config"
    run_dotnet build "$generated_project" --no-restore -m:1
    audit_configured_source_path "$generated_root/$generated_name" "${CNA_PACKAGE_SOURCE_ROOT:-}"

    output_native="$generated_root/$generated_name/bin/Debug/net8.0/runtimes/linux-x64/native/libcna_c_api.so"
    if [[ ! -f "$output_native" ]]; then
        printf 'Package build did not produce the expected Linux x64 native asset: %s\n' "$output_native" >&2
        exit 1
    fi

    unset CNA_NATIVE_LIBRARY CNA_NATIVE_DIR
    run_game "$generated_project" --smoke-test
    if [[ "${CNA_TEMPLATE_RUN_STABILITY:-0}" == "1" ]]; then
        run_game "$generated_project" --stability-test
    fi
fi

printf 'BUILD=PASS\nPATH_AUDIT=PASS\nTemplate verification completed.\n'
