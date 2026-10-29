#!/usr/bin/env bash
set -eo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SDK_DIR="/home/kirancybergrid/Android/Sdk"
BUILD_TOOLS="${SDK_DIR}/build-tools/34.0.0"
PLATFORM_JAR="${SDK_DIR}/platforms/android-34/android.jar"
KEYSTORE="${SCRIPT_DIR}/debug.keystore"

AAPT2="${BUILD_TOOLS}/aapt2"
D8="${BUILD_TOOLS}/d8"
ZIPALIGN="${BUILD_TOOLS}/zipalign"
APKSIGNER="${BUILD_TOOLS}/apksigner"

BUILD_DIR="${SCRIPT_DIR}/build"
CLASSES_DIR="${BUILD_DIR}/classes"
OUTPUT_APK="${SCRIPT_DIR}/AetherFpsMeter.apk"

echo "[fps-apk] Compiling Real-Time AETHER FPS Monitor APK..."
rm -rf "${BUILD_DIR}"
mkdir -p "${CLASSES_DIR}" "${BUILD_DIR}/dex"

# 1. Compile resources with AAPT2
"${AAPT2}" compile --dir "${SCRIPT_DIR}/res" -o "${BUILD_DIR}/compiled_res.zip"
"${AAPT2}" link -o "${BUILD_DIR}/unaligned.apk" \
    -I "${PLATFORM_JAR}" \
    --manifest "${SCRIPT_DIR}/AndroidManifest.xml" \
    --java "${BUILD_DIR}/gen" \
    "${BUILD_DIR}/compiled_res.zip"

# 2. Compile Java Source Files
javac -cp "${PLATFORM_JAR}" \
    -d "${CLASSES_DIR}" \
    "${SCRIPT_DIR}/src/com/aether/fpsmeter/"*.java \
    "${BUILD_DIR}/gen/com/aether/fpsmeter/"*.java

# 3. DEX Conversion with D8
"${D8}" --lib "${PLATFORM_JAR}" \
    --output "${BUILD_DIR}/dex/" \
    "${CLASSES_DIR}/com/aether/fpsmeter/"*.class

# 4. Inject classes.dex into APK
(cd "${BUILD_DIR}/dex" && zip -q -u "${BUILD_DIR}/unaligned.apk" classes.dex)

# 5. Zipalign (4-byte boundary)
"${ZIPALIGN}" -p -f 4 "${BUILD_DIR}/unaligned.apk" "${OUTPUT_APK}"

# 6. Cryptographic Signing (v1 + v2 + v3 scheme)
"${APKSIGNER}" sign --ks "${KEYSTORE}" \
    --ks-pass pass:android \
    "${OUTPUT_APK}"

"${APKSIGNER}" verify "${OUTPUT_APK}"

ROOT_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"
if [ -d "${ROOT_DIR}/packaging/aether_kernel_control/tools" ]; then
    cp -f "${OUTPUT_APK}" "${ROOT_DIR}/packaging/aether_kernel_control/tools/AetherFpsMeter.apk"
    chmod 0644 "${ROOT_DIR}/packaging/aether_kernel_control/tools/AetherFpsMeter.apk"
fi
if [ -d "${ROOT_DIR}/packaging/anykernel3/tools" ]; then
    cp -f "${OUTPUT_APK}" "${ROOT_DIR}/packaging/anykernel3/tools/AetherFpsMeter.apk"
    chmod 0644 "${ROOT_DIR}/packaging/anykernel3/tools/AetherFpsMeter.apk"
fi
if [ -d "${ROOT_DIR}/packaging/aether_kernel_control/system/app/AetherFpsMeter" ]; then
    cp -f "${OUTPUT_APK}" "${ROOT_DIR}/packaging/aether_kernel_control/system/app/AetherFpsMeter/AetherFpsMeter.apk"
    chmod 0644 "${ROOT_DIR}/packaging/aether_kernel_control/system/app/AetherFpsMeter/AetherFpsMeter.apk"
fi

echo "[fps-apk] Successfully built Real-Time AETHER FPS Monitor: $(ls -lh "${OUTPUT_APK}" | awk '{print $9, "(" $5 ")"}')"
