#!/usr/bin/env bash
set -euo pipefail

adapter_root=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
java_root=${CNA_JAVA_ROOT:-"$adapter_root/../cna-java"}
template_root=${CNA_KOTLIN_TEMPLATE_ROOT:-"$adapter_root/../cna-kotlin-template"}

for project in "$java_root" "$adapter_root" "$template_root"; do
    if [[ ! -x "$project/gradlew" ]]; then
        echo "Gradle Wrapper not found: $project/gradlew" >&2
        exit 2
    fi
done

verification_root=$(mktemp -d "${TMPDIR:-/tmp}/cna-kotlin-verify.XXXXXX")
repository="$verification_root/maven"
generated="$verification_root/generated"

cleanup() {
    if [[ -n "${verification_root:-}" && -d "$verification_root" ]]; then
        rm -rf -- "$verification_root"
    fi
}
trap cleanup EXIT

echo "[1/5] Build current CNA-Java and publish its exact artifact to $repository"
(
    cd "$java_root"
    ./gradlew --no-daemon clean check publishToMavenLocal \
        "-Dmaven.repo.local=$repository"
)

echo "[2/5] Build, audit, and publish CNA-Kotlin against that CNA-Java artifact"
(
    cd "$adapter_root"
    ./gradlew --no-daemon clean check sourcesJar publishToMavenLocal \
        "-PcnaRepository=$repository" \
        "-Dmaven.repo.local=$repository"
)

echo "[3/5] Build the maintained Kotlin/JVM template from Maven artifacts"
(
    cd "$template_root"
    ./gradlew --no-daemon clean check installDist \
        "-PcnaRepository=$repository"
)

echo "[4/5] Generate and build a standalone consumer with distinct game/main packages"
python3 "$template_root/scripts/generate_project.py" \
    --output "$generated" \
    --project-name "Verification Kotlin Game" \
    --package org.openeggbert.verification.game \
    --main-package org.openeggbert.verification.desktop \
    --game-class VerificationGame \
    --group org.openeggbert.verification \
    --artifact-id cna-kotlin-generated-verification

if rg -n \
    -e "$adapter_root" \
    -e "$java_root" \
    -e "$template_root" \
    -e '\.\./cna-java' \
    -e '\.\./cna-kotlin' \
    -e 'cna-kotlin-(common|desktop|android|js)' \
    "$generated" -g '!gradle-wrapper.jar'; then
    echo "Generated project contains a forbidden repository/dependency reference" >&2
    exit 1
fi
(
    cd "$generated"
    ./gradlew --no-daemon clean check installDist \
        "-PcnaRepository=$repository"
)

echo "[5/5] Native execution through CNA-Java JNI"
if [[ -n "${CNA_NATIVE_LIBRARY:-}" ]]; then
    case "$(uname -s)" in
        Linux*) jni_library="$java_root/build/native/libcna_java_jni.so" ;;
        Darwin*) jni_library="$java_root/build/native/libcna_java_jni.dylib" ;;
        MINGW*|MSYS*|CYGWIN*) jni_library="$java_root/build/native/cna_java_jni.dll" ;;
        *) echo "Unsupported host for JNI verification: $(uname -s)" >&2; exit 2 ;;
    esac
    if [[ ! -f "$jni_library" ]]; then
        echo "CNA-Java JNI adapter not found after build: $jni_library" >&2
        exit 2
    fi
    (
        cd "$template_root"
        CNA_JNI_LIBRARY="$jni_library" CNA_RENDERER="${CNA_RENDERER:-HEADLESS}" \
            ./gradlew --no-daemon run "-PcnaRepository=$repository" --args=--smoke-test
    )
    (
        cd "$generated"
        CNA_JNI_LIBRARY="$jni_library" CNA_RENDERER="${CNA_RENDERER:-HEADLESS}" \
            ./gradlew --no-daemon run "-PcnaRepository=$repository" --args=--smoke-test
    )
    if [[ "${CNA_RUN_STABILITY_TEST:-0}" == "1" ]]; then
        (
            cd "$template_root"
            CNA_JNI_LIBRARY="$jni_library" CNA_RENDERER="${CNA_RENDERER:-HEADLESS}" \
                ./gradlew --no-daemon run "-PcnaRepository=$repository" --args=--stability-test
        )
    else
        echo "600-frame stability run skipped (set CNA_RUN_STABILITY_TEST=1 to enable it)."
    fi
else
    echo "Native runs skipped because CNA_NATIVE_LIBRARY is not set."
fi

echo "CNA-Kotlin integration verification passed without using the global Maven repository."
