#!/usr/bin/env bash
set -eo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SDK_DIR="/home/kirancybergrid/Android/Sdk"
BUILD_TOOLS="${SDK_DIR}/build-tools/34.0.0"
PLATFORM_JAR="${SDK_DIR}/platforms/android-34/android.jar"
KEYSTORE="/home/kirancybergrid/.android/debug.keystore"

AAPT2="${BUILD_TOOLS}/aapt2"
D8="${BUILD_TOOLS}/d8"
ZIPALIGN="${BUILD_TOOLS}/zipalign"
APKSIGNER="${BUILD_TOOLS}/apksigner"

BUILD_DIR="${SCRIPT_DIR}/build"
GEN_DIR="${BUILD_DIR}/gen"
CLASSES_DIR="${BUILD_DIR}/classes"
RES_ZIP="${BUILD_DIR}/compiled_res.zip"
OUTPUT_APK="${SCRIPT_DIR}/AetherEngineCore.apk"

echo "[aether-apk] Compiling Headless AETHER Engine Android Service with QS Tiles..."
rm -rf "${BUILD_DIR}"
mkdir -p "${CLASSES_DIR}" "${GEN_DIR}"

# 1. Compile Vector Drawables and String Resources
"${AAPT2}" compile --dir "${SCRIPT_DIR}/res" -o "${RES_ZIP}"

# 2. Link Resources & Generate R.java
"${AAPT2}" link -o "${BUILD_DIR}/unaligned.apk" \
    -I "${PLATFORM_JAR}" \
    --manifest "${SCRIPT_DIR}/AndroidManifest.xml" \
    "${RES_ZIP}" \
    --java "${GEN_DIR}"

# 3. Compile Java Source Files (including generated R.java)
javac -cp "${PLATFORM_JAR}" \
    -d "${CLASSES_DIR}" \
    "${SCRIPT_DIR}/src/com/aether/engine/service/"*.java \
    "${GEN_DIR}/com/aether/engine/service/"*.java

# 4. DEX Conversion
"${D8}" --lib "${PLATFORM_JAR}" \
    --output "${BUILD_DIR}/" \
    "${CLASSES_DIR}/com/aether/engine/service/"*.class

# 5. Inject classes.dex into package
(cd "${BUILD_DIR}" && zip -q -u unaligned.apk classes.dex)

# 5. Zipalign (4-byte boundary)
"${ZIPALIGN}" -p -f 4 "${BUILD_DIR}/unaligned.apk" "${OUTPUT_APK}"

# 6. Cryptographic Signing (v1 + v2 + v3 scheme)
"${APKSIGNER}" sign --ks "${KEYSTORE}" \
    --ks-pass pass:android \
    "${OUTPUT_APK}"

"${APKSIGNER}" verify "${OUTPUT_APK}"

ROOT_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"
if [ -d "${ROOT_DIR}/packaging/aether_kernel_control/tools" ]; then
    cp -f "${OUTPUT_APK}" "${ROOT_DIR}/packaging/aether_kernel_control/tools/AetherEngineCore.apk"
    chmod 0644 "${ROOT_DIR}/packaging/aether_kernel_control/tools/AetherEngineCore.apk"
    echo "[aether-apk] Synchronized to packaging/aether_kernel_control/tools/AetherEngineCore.apk"
fi

if [ -d "${ROOT_DIR}/packaging/anykernel3/tools" ]; then
    cp -f "${OUTPUT_APK}" "${ROOT_DIR}/packaging/anykernel3/tools/AetherEngineCore.apk"
    chmod 0644 "${ROOT_DIR}/packaging/anykernel3/tools/AetherEngineCore.apk"
    echo "[aether-apk] Synchronized to packaging/anykernel3/tools/AetherEngineCore.apk"
fi

if [ -d "${ROOT_DIR}/packaging/aether_kernel_control/system/app/AetherEngineCore" ]; then
    cp -f "${OUTPUT_APK}" "${ROOT_DIR}/packaging/aether_kernel_control/system/app/AetherEngineCore/AetherEngineCore.apk"
    chmod 0644 "${ROOT_DIR}/packaging/aether_kernel_control/system/app/AetherEngineCore/AetherEngineCore.apk"
    echo "[aether-apk] Synchronized to packaging/aether_kernel_control/system/app/AetherEngineCore/AetherEngineCore.apk"
fi

echo "[aether-apk] Successfully generated headless APK: $(ls -lh "${OUTPUT_APK}" | awk '{print $9, "(" $5 ")"}')"
