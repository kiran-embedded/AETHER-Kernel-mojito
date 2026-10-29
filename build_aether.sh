#!/usr/bin/env bash
# ==============================================================================
# PROJECT AETHER NEXT — Production Kernel Build Script
# Target: Xiaomi Redmi Note 10 (mojito / sunny) | Qualcomm SM6150 (Snapdragon 678)
# ==============================================================================
set -eo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# ------------------------------------------------------------------------------
# 1. Path Space Handling (Kbuild constraint workaround)
# ------------------------------------------------------------------------------
# Linux Kbuild explicitly disallows paths containing spaces or colons.
# If the current directory path contains spaces, re-exec seamlessly inside an
# isolated user & mount namespace with a clean bind-mount to /tmp/aether_build_tree.
if [[ -z "${AETHER_IN_NS:-}" ]]; then
    if [[ "$SCRIPT_DIR" =~ [[:space:]:] ]]; then
        echo "[AETHER] Path contains spaces: '$SCRIPT_DIR'"
        echo "[AETHER] Initializing isolated namespace bind-mount for Kbuild compatibility..."
        NS_MOUNT="/tmp/aether_build_tree"
        mkdir -p "$NS_MOUNT"
        exec unshare -m -r bash -c "
            mount --bind '$SCRIPT_DIR' '$NS_MOUNT'
            cd '$NS_MOUNT'
            export AETHER_IN_NS=1
            export AETHER_HOST_DIR='$SCRIPT_DIR'
            exec ./$(basename "${BASH_SOURCE[0]}") \"\$@\"
        " -- "$@"
    fi
fi

# ------------------------------------------------------------------------------
# 2. Build Environment & Architecture Variables
# ------------------------------------------------------------------------------
export ARCH="arm64"
export SUBARCH="arm64"
export DEFCONFIG="mojito_defconfig"
export OUT_DIR="${SCRIPT_DIR}/out"
export LOG_FILE="${OUT_DIR}/build.log"
export ARTIFACTS_DIR="${SCRIPT_DIR}/artifacts_aether"

# Custom Kernel Identification
export KBUILD_BUILD_USER="kiran"
export KBUILD_BUILD_HOST="ProjectAether"

# Toolchain paths: prioritize user's LLVM bin directory
export PATH="/home/kirancybergrid/bin:/usr/local/bin:/usr/bin:/bin:${PATH}"

export CLANG_TRIPLE="aarch64-linux-gnu-"
export CROSS_COMPILE="aarch64-linux-gnu-"
export CROSS_COMPILE_COMPAT="arm-linux-gnueabi-"

export CC="clang"
export HOSTCC="clang"
export HOSTCXX="clang++"
export LD="ld.lld"
export HOSTLD="ld.lld"
export AR="llvm-ar"
export NM="llvm-nm"
export OBJCOPY="llvm-objcopy"
export OBJDUMP="llvm-objdump"
export READELF="llvm-readelf"
export STRIP="llvm-strip"
export LLVM_IAS=1

# Memory-aware parallelism:
# Ryzen 3 3200G (4 cores, 4 threads, 16 GB RAM).
# ThinLTO link phase uses significant RAM; limit to 4 parallel jobs to prevent OOM.
CPU_CORES=$(nproc 2>/dev/null || echo 4)
BUILD_JOBS="${AETHER_JOBS:-$CPU_CORES}"

MAKE_ARGS=(
    ARCH="${ARCH}"
    SUBARCH="${SUBARCH}"
    O="${OUT_DIR}"
    CC="${CC}"
    HOSTCC="${HOSTCC}"
    HOSTCXX="${HOSTCXX}"
    LD="${LD}"
    HOSTLD="${HOSTLD}"
    AR="${AR}"
    NM="${NM}"
    OBJCOPY="${OBJCOPY}"
    OBJDUMP="${OBJDUMP}"
    READELF="${READELF}"
    STRIP="${STRIP}"
    CLANG_TRIPLE="${CLANG_TRIPLE}"
    CROSS_COMPILE="${CROSS_COMPILE}"
    CROSS_COMPILE_COMPAT="${CROSS_COMPILE_COMPAT}"
    LLVM_IAS="${LLVM_IAS}"
    KCFLAGS="-Wno-error -Wno-visibility -Wno-unused-but-set-variable -Wno-int-conversion -Wno-fortify-source"
)

# ------------------------------------------------------------------------------
# 3. Helper Functions
# ------------------------------------------------------------------------------
info()  { echo -e "\033[1;34m[AETHER INFO]\033[0m $*"; }
ok()    { echo -e "\033[1;32m[AETHER OK]\033[0m $*"; }
warn()  { echo -e "\033[1;33m[AETHER WARN]\033[0m $*"; }
error() { echo -e "\033[1;31m[AETHER ERROR]\033[0m $*" >&2; exit 1; }

check_tool() {
    local cmd="$1"
    local desc="$2"
    if ! command -v "$cmd" &>/dev/null; then
        error "Required tool '$cmd' ($desc) not found in PATH."
    fi
}

# ------------------------------------------------------------------------------
# 4. Command Handling (clean, menuconfig, build)
# ------------------------------------------------------------------------------

# ── KernelSU-Next Root Configuration ─────────────────────────────────────────
KSU_VARIANT="next"
export AETHER_KSU_TAG="ksunext"
info "Configuring dedicated KernelSU-Next (Native UAPI v5 + SuSFS v2.3.0)..."

case "${1:-build}" in
    clean)
        info "Cleaning build output directory..."
        rm -rf "${OUT_DIR}" "${ARTIFACTS_DIR}"
        ok "Clean complete."
        exit 0
        ;;
    menuconfig)
        info "Opening interactive kernel configuration..."
        mkdir -p "${OUT_DIR}"
        make "${MAKE_ARGS[@]}" "${DEFCONFIG}"
        make "${MAKE_ARGS[@]}" menuconfig
        make "${MAKE_ARGS[@]}" savedefconfig
        cp -v "${OUT_DIR}/defconfig" "arch/arm64/configs/${DEFCONFIG}"
        ok "Updated arch/arm64/configs/${DEFCONFIG}."
        exit 0
        ;;
    build)
        ;;
    *)
        echo "Usage: $0 [build|clean|menuconfig] [--ksu=next|stock]"
        exit 1
        ;;
esac

# ------------------------------------------------------------------------------
# 5. Pre-Flight Toolchain Validation
# ------------------------------------------------------------------------------
info "Running pre-flight toolchain validation..."
check_tool "clang" "LLVM C Compiler"
check_tool "ld.lld" "LLD Linker"
check_tool "llvm-ar" "LLVM Archiver"
check_tool "llvm-nm" "LLVM Symbol Extractor"
check_tool "llvm-objcopy" "LLVM Objcopy"
check_tool "aarch64-linux-gnu-as" "GNU/LLVM AArch64 Assembler"

CLANG_VER=$(clang --version | head -n1)
LLD_VER=$(ld.lld --version | head -n1)
COMMIT_SHA=$(git rev-parse HEAD 2>/dev/null || echo "UNKNOWN")
BRANCH_NAME=$(git rev-parse --abbrev-ref HEAD 2>/dev/null || echo "UNKNOWN")

info "Git Branch:     ${BRANCH_NAME}"
info "Git Commit:     ${COMMIT_SHA}"
info "Clang Version:  ${CLANG_VER}"
info "LLD Version:    ${LLD_VER}"
info "Target Device:  Xiaomi Redmi Note 10 (mojito / sunny)"
info "Parallel Jobs:  ${BUILD_JOBS} (16 GB RAM Profile)"

mkdir -p "${OUT_DIR}"
mkdir -p "${ARTIFACTS_DIR}"

# ------------------------------------------------------------------------------
# 6. Kernel Configuration
# ------------------------------------------------------------------------------
info "Configuring kernel with '${DEFCONFIG}'..."
make "${MAKE_ARGS[@]}" "${DEFCONFIG}" 2>&1 | tee "${LOG_FILE}"

# Verify critical configuration symbols
check_config() {
    local sym="$1"
    local expected="$2"
    local val
    val=$(grep -E "^${sym}=" "${OUT_DIR}/.config" || true)
    if [[ "$expected" == "y" && -z "$val" ]]; then
        error "Assertion failed: $sym is not enabled in .config!"
    fi
}

info "Verifying core configuration assertions..."
check_config "CONFIG_ARCH_SM6150" "y"
check_config "CONFIG_PSI" "y"
check_config "CONFIG_SCHED_TUNE" "y"
check_config "CONFIG_DEFAULT_USE_ENERGY_AWARE" "y"
check_config "CONFIG_CPU_FREQ_GOV_SCHEDUTIL" "y"
check_config "CONFIG_ZRAM" "y"
ok "Defconfig verified."

# ------------------------------------------------------------------------------
# 7. Compilation Phase
# ------------------------------------------------------------------------------
START_TIME=$(date +%s)
info "Starting kernel compilation (Image + DTBs) at $(date)..."

# Build Image and DTBs
make -j"${BUILD_JOBS}" "${MAKE_ARGS[@]}" Image dtbs 2>&1 | tee -a "${LOG_FILE}"

BUILD_TIME=$(( $(date +%s) - START_TIME ))
BUILD_MINUTES=$(( BUILD_TIME / 60 ))
BUILD_SECONDS=$(( BUILD_TIME % 60 ))
ok "Compilation completed in ${BUILD_MINUTES}m ${BUILD_SECONDS}s."

# ------------------------------------------------------------------------------
# 8. Post-Build Validation & Packaging
# ------------------------------------------------------------------------------
info "Validating compiled artifacts..."

IMAGE_OUT="${OUT_DIR}/arch/arm64/boot/Image"
VMLINUX_OUT="${OUT_DIR}/vmlinux"

if [[ ! -f "$IMAGE_OUT" ]]; then
    error "Kernel Image was not generated! Expected at: $IMAGE_OUT"
fi

if [[ ! -f "$VMLINUX_OUT" ]]; then
    error "vmlinux was not generated! Expected at: $VMLINUX_OUT"
fi

IMAGE_SIZE=$(stat -c%s "$IMAGE_OUT")
VMLINUX_SIZE=$(stat -c%s "$VMLINUX_OUT")

info "Kernel Image Size: $(( IMAGE_SIZE / 1024 / 1024 )) MB ($IMAGE_SIZE bytes)"
info "vmlinux Size:      $(( VMLINUX_SIZE / 1024 / 1024 )) MB ($VMLINUX_SIZE bytes)"

# Copy artifacts to output directory
cp -v "$IMAGE_OUT" "${ARTIFACTS_DIR}/Image"
cp -v "${OUT_DIR}/.config" "${ARTIFACTS_DIR}/kernel.config"

# Collect SM6150 DTBs
mkdir -p "${ARTIFACTS_DIR}/dtbs"
find "${OUT_DIR}/arch/arm64/boot/dts/qcom" -name "*sm6150*.dtb" -exec cp -v {} "${ARTIFACTS_DIR}/dtbs/" \; 2>/dev/null || true

# Generate SHA256 checksums
info "Generating SHA-256 checksums..."
(
    cd "${ARTIFACTS_DIR}"
    sha256sum Image kernel.config > SHA256SUMS
    if [ -d dtbs ] && [ "$(ls -A dtbs 2>/dev/null)" ]; then
        sha256sum dtbs/*.dtb >> SHA256SUMS
    fi
)

# Generate Build Manifest
KSU_UAPI_VER="$( [[ "${KSU_VARIANT}" == "stock" ]] && echo 6 || echo 5 )"
cat <<EOF > "${ARTIFACTS_DIR}/build_manifest.json"
{
  "project": "Aether Next",
  "device": "Xiaomi Redmi Note 10 (mojito / sunny)",
  "soc": "Qualcomm Snapdragon 678 (SM6150)",
  "branch": "${BRANCH_NAME}",
  "commit": "${COMMIT_SHA}",
  "compiler": "${CLANG_VER}",
  "linker": "${LLD_VER}",
  "build_date": "$(date -u +"%Y-%m-%dT%H:%M:%SZ")",
  "build_duration_seconds": ${BUILD_TIME},
  "ksu": {
    "variant": "${KSU_VARIANT}",
    "tag": "${AETHER_KSU_TAG}",
    "uapi_version": ${KSU_UAPI_VER}
  },
  "artifacts": {
    "Image": {
      "size_bytes": ${IMAGE_SIZE},
      "sha256": "$(sha256sum "${ARTIFACTS_DIR}/Image" | awk '{print $1}')"
    }
  }
}
EOF

ok "Build successful!"
ok "Artifacts ready at: ${ARTIFACTS_DIR}/"
ok "Manifest saved:     ${ARTIFACTS_DIR}/build_manifest.json"

# ------------------------------------------------------------------------------
# 9. Packaging Phase (AnyKernel3, WebUI, SuSFS Spoofer)
# ------------------------------------------------------------------------------
# Note: AETHER Engine native daemon (aetherd) is purged to eliminate background polling CPU overhead.
# AetherEngineCore.apk is retained as the zero-resource Quick Settings Tile Provider (0% daemon overhead).
rm -f "${SCRIPT_DIR}/packaging/anykernel3/tools/aetherd" "${SCRIPT_DIR}/packaging/aether_kernel_control/tools/aetherd" 2>/dev/null || true

if [ -f "${SCRIPT_DIR}/packaging/aether_fpsmeter/build_apk.sh" ]; then
    info "Compiling AETHER FPS Monitor APK..."
    "${SCRIPT_DIR}/packaging/aether_fpsmeter/build_apk.sh"
    cp -f "${SCRIPT_DIR}/packaging/aether_fpsmeter/AetherFpsMeter.apk" "${SCRIPT_DIR}/packaging/anykernel3/tools/AetherFpsMeter.apk"
    mkdir -p "${SCRIPT_DIR}/packaging/aether_kernel_control/tools"
    mkdir -p "${SCRIPT_DIR}/packaging/aether_kernel_control/system/app/AetherFpsMeter"
    cp -f "${SCRIPT_DIR}/packaging/aether_fpsmeter/AetherFpsMeter.apk" "${SCRIPT_DIR}/packaging/aether_kernel_control/tools/AetherFpsMeter.apk"
    cp -f "${SCRIPT_DIR}/packaging/aether_fpsmeter/AetherFpsMeter.apk" "${SCRIPT_DIR}/packaging/aether_kernel_control/system/app/AetherFpsMeter/AetherFpsMeter.apk"
fi

if [ -f "${SCRIPT_DIR}/packaging/aether_service/build_apk.sh" ]; then
    info "Compiling AETHER Quick Settings Tiles APK (Zero-Resource Tile Provider)..."
    "${SCRIPT_DIR}/packaging/aether_service/build_apk.sh"
    cp -f "${SCRIPT_DIR}/packaging/aether_service/AetherEngineCore.apk" "${SCRIPT_DIR}/packaging/anykernel3/tools/AetherEngineCore.apk"
    mkdir -p "${SCRIPT_DIR}/packaging/aether_kernel_control/tools"
    mkdir -p "${SCRIPT_DIR}/packaging/aether_kernel_control/system/app/AetherEngineCore"
    cp -f "${SCRIPT_DIR}/packaging/aether_service/AetherEngineCore.apk" "${SCRIPT_DIR}/packaging/aether_kernel_control/tools/AetherEngineCore.apk"
    cp -f "${SCRIPT_DIR}/packaging/aether_service/AetherEngineCore.apk" "${SCRIPT_DIR}/packaging/aether_kernel_control/system/app/AetherEngineCore/AetherEngineCore.apk"
fi

info "Packaging flashable AnyKernel3 ZIP..."
AK3_STAGING="${ARTIFACTS_DIR}/package_staging"
rm -rf "${AK3_STAGING}"
if [ -d "${SCRIPT_DIR}/packaging/anykernel3" ]; then
    mkdir -p "${AK3_STAGING}"
    cp -r "${SCRIPT_DIR}/packaging/anykernel3"/* "${AK3_STAGING}/"
fi

if [ -d "${AK3_STAGING}" ]; then
    cp -f "${IMAGE_OUT}" "${AK3_STAGING}/Image"
    find "${OUT_DIR}/arch/arm64/boot/dts/qcom" -name "*sm6150*.dtb" -exec cp -f {} "${AK3_STAGING}/dtb" \; 2>/dev/null || true
    
    KERNEL_ZIP="${SCRIPT_DIR}/AETHER-Kernel-mojito-v1.8-stable-mglru-${AETHER_KSU_TAG:-ksunext}-susfs.zip"
    rm -f "${KERNEL_ZIP}" "${KERNEL_ZIP}.sha256"
    (
        cd "${AK3_STAGING}"
        zip -r9 -q "${KERNEL_ZIP}" * -x ".*" -x "__MACOSX"
    )
    sha256sum "${KERNEL_ZIP}" > "${KERNEL_ZIP}.sha256"
    ok "Packaged Kernel ZIP: ${KERNEL_ZIP}"

    if [ -d "/media/kirancybergrid/SANDISK" ]; then
        cp -f "${KERNEL_ZIP}" "/media/kirancybergrid/SANDISK/" 2>/dev/null || true
        [ -d "/media/kirancybergrid/SANDISK/aether kernal android 17" ] && cp -f "${KERNEL_ZIP}"* "/media/kirancybergrid/SANDISK/aether kernal android 17/" 2>/dev/null || true
        [ -d "/media/kirancybergrid/SANDISK/AETHER_v1.8_Latest" ] && cp -f "${KERNEL_ZIP}" "/media/kirancybergrid/SANDISK/AETHER_v1.8_Latest/" 2>/dev/null || true
        ok "Synced Kernel ZIP to SD Card (/media/kirancybergrid/SANDISK/)"
    fi
fi

# Package WebUI Module
if [ -d "${SCRIPT_DIR}/packaging/aether_kernel_control" ]; then
    info "Packaging AETHER Kernel Control (WebUI) Module..."
    WEBUI_ZIP="${SCRIPT_DIR}/AETHER-Kernel-Control-WebUI-v1.8.zip"
    rm -f "${WEBUI_ZIP}" "${WEBUI_ZIP}.sha256"
    (
        cd "${SCRIPT_DIR}/packaging/aether_kernel_control"
        zip -r9 -q "${WEBUI_ZIP}" * -x ".*" -x "__MACOSX"
    )
    sha256sum "${WEBUI_ZIP}" > "${WEBUI_ZIP}.sha256"
    ok "Packaged WebUI ZIP: ${WEBUI_ZIP}"

    if [ -d "/media/kirancybergrid/SANDISK" ]; then
        cp -f "${WEBUI_ZIP}" "/media/kirancybergrid/SANDISK/" 2>/dev/null || true
        [ -d "/media/kirancybergrid/SANDISK/aether kernal android 17" ] && cp -f "${WEBUI_ZIP}"* "/media/kirancybergrid/SANDISK/aether kernal android 17/" 2>/dev/null || true
        [ -d "/media/kirancybergrid/SANDISK/AETHER_v1.6_Latest" ] && cp -f "${WEBUI_ZIP}" "/media/kirancybergrid/SANDISK/AETHER_v1.6_Latest/" 2>/dev/null || true
        ok "Synced WebUI ZIP to SD Card (/media/kirancybergrid/SANDISK/)"
    fi
fi

# Package SuSFS Spoofer Module
if [ -d "${SCRIPT_DIR}/packaging/aether_susfs_spoofer" ]; then
    info "Packaging AETHER SuSFS Stock Spoofer Module..."
    SPOOFER_ZIP="${SCRIPT_DIR}/AETHER-SuSFS-Stock-Spoofer-v2.3.0.zip"
    rm -f "${SPOOFER_ZIP}" "${SPOOFER_ZIP}.sha256"
    (
        cd "${SCRIPT_DIR}/packaging/aether_susfs_spoofer"
        zip -r9 -q "${SPOOFER_ZIP}" * -x ".*" -x "__MACOSX"
    )
    sha256sum "${SPOOFER_ZIP}" > "${SPOOFER_ZIP}.sha256"
    ok "Packaged SuSFS Spoofer ZIP: ${SPOOFER_ZIP}"

    if [ -d "/media/kirancybergrid/SANDISK" ]; then
        cp -f "${SPOOFER_ZIP}" "/media/kirancybergrid/SANDISK/" 2>/dev/null || true
        [ -d "/media/kirancybergrid/SANDISK/aether kernal android 17" ] && cp -f "${SPOOFER_ZIP}"* "/media/kirancybergrid/SANDISK/aether kernal android 17/" 2>/dev/null || true
        [ -d "/media/kirancybergrid/SANDISK/AETHER_v1.6_Latest" ] && cp -f "${SPOOFER_ZIP}" "/media/kirancybergrid/SANDISK/AETHER_v1.6_Latest/" 2>/dev/null || true
        ok "Synced SuSFS Spoofer ZIP to SD Card (/media/kirancybergrid/SANDISK/)"
    fi
fi

exit 0
