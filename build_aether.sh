#!/usr/bin/env bash
# ==============================================================================
# AETHER Kernel Build & Packaging Script
# Target: Xiaomi Redmi Note 10 4G (mojito / sunny) — SM6150
# Editions: standard | micfix | all
# ==============================================================================
set -euo pipefail

KERNEL_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${KERNEL_DIR}"

TARGET_EDITION="${1:-standard}"

KERNEL_NAME="AETHER"
DEVICE="mojito"
DEFCONFIG="vendor/mojito_defconfig"
OUT_DIR="${KERNEL_DIR}/out_artifacts"
mkdir -p "${OUT_DIR}" patches

# Toolchain environment configuration
export ARCH=arm64
export SUBARCH=arm64
export CC=clang
export CLANG_TRIPLE=aarch64-linux-gnu-
export CROSS_COMPILE=aarch64-linux-gnu-
export CROSS_COMPILE_COMPAT=arm-linux-gnueabi-
export LD_COMPAT=arm-linux-gnueabi-ld
export AR=aarch64-linux-gnu-ar
export NM=aarch64-linux-gnu-nm
export OBJCOPY=aarch64-linux-gnu-objcopy
export OBJDUMP=aarch64-linux-gnu-objdump
export STRIP=aarch64-linux-gnu-strip

MAKE_ARGS=(
    ARCH=arm64
    CC=clang
    CLANG_TRIPLE=aarch64-linux-gnu-
    CROSS_COMPILE=aarch64-linux-gnu-
    CROSS_COMPILE_COMPAT=arm-linux-gnueabi-
    LD_COMPAT=arm-linux-gnueabi-ld
    AR=aarch64-linux-gnu-ar
    NM=aarch64-linux-gnu-nm
    OBJCOPY=aarch64-linux-gnu-objcopy
    OBJDUMP=aarch64-linux-gnu-objdump
    STRIP=aarch64-linux-gnu-strip
)

# 1. Defconfig check / generation
if [ ! -f .config ]; then
    echo "[-] Generating .config from ${DEFCONFIG}..."
    make "${MAKE_ARGS[@]}" ${DEFCONFIG}
else
    echo "[+] Using existing .config"
fi

build_single_edition() {
    local ed="$1"
    local build_start=$(date +%s)
    local date_tag=$(date +"%Y%m%d_%H%M%S")
    local pkg_dir="${KERNEL_DIR}/packaging/anykernel3"
    
    echo "=================================================="
    echo " Building ${KERNEL_NAME} Kernel [${ed}] for ${DEVICE}"
    echo " Timestamp: $(date)"
    echo "=================================================="

    if [ "$ed" = "standard" ]; then
        # Ensure stock audio drivers
        if grep -q "TX_MACRO_TX_UNMUTE_DELAY_MS[[:space:]]*5" techpack/audio/asoc/codecs/bolero/tx-macro.c 2>/dev/null; then
            echo "[-] Reverting audio calibration to stock drivers..."
            git checkout HEAD -- techpack/audio/asoc/codecs/bolero/tx-macro.c techpack/audio/asoc/codecs/wcd937x/wcd937x-regmap.c techpack/audio/asoc/codecs/wcd937x/wcd937x.c
        fi
        sed -i 's/Edition  : .*/Edition  : Standard (Stock Baseline)        ║";/' "${pkg_dir}/anykernel.sh"
        sed -i 's/kernel.string=.*/kernel.string=AETHER Kernel v1.4 [Standard] by Kiran_Embedded/' "${pkg_dir}/anykernel.sh"
        local zip_dest_name="AETHER-Kernel-mojito-v1.4-Standard.zip"
    else
        # Apply enhanced audio drivers
        if ! grep -q "TX_MACRO_TX_UNMUTE_DELAY_MS[[:space:]]*5" techpack/audio/asoc/codecs/bolero/tx-macro.c 2>/dev/null; then
            echo "[+] Applying enhanced audio calibration patch..."
            git apply patches/audio_micfix.patch
        fi
        sed -i 's/Edition  : .*/Edition  : Enhanced (Calibrated)            ║";/' "${pkg_dir}/anykernel.sh"
        sed -i 's/kernel.string=.*/kernel.string=AETHER Kernel v1.4 [Enhanced] by Kiran_Embedded/' "${pkg_dir}/anykernel.sh"
        local zip_dest_name="AETHER-Kernel-mojito-v1.4-Enhanced.zip"
    fi

    # 2. Compile Kernel Image
    echo "[+] Compiling ARM64 Kernel Image (-j$(nproc))..."
    make -j$(nproc) "${MAKE_ARGS[@]}" Image

    # 3. Compile Device Tree Blobs (dtbs)
    echo "[+] Compiling Device Tree Blobs (dtbs)..."
    make -j$(nproc) "${MAKE_ARGS[@]}" dtbs

    # 4. Assemble DTB & DTBO
    local dtb_out="${OUT_DIR}/dtb"
    local dtbo_out="${OUT_DIR}/dtbo.img"
    if [ -f arch/arm64/boot/dts/qcom/sm6150.dtb ]; then
        cp arch/arm64/boot/dts/qcom/sm6150.dtb "${dtb_out}"
    else
        echo "[-] ERROR: sm6150.dtb not found!"
        exit 1
    fi

    python3 scripts/dtc/libfdt/mkdtboimg.py create "${dtbo_out}" --page_size=4096 \
        arch/arm64/boot/dts/qcom/sm6150-idp-overlay.dtbo \
        arch/arm64/boot/dts/qcom/sm6150-usbc-idp-overlay.dtbo \
        arch/arm64/boot/dts/qcom/sm6150-external-codec-idp-overlay.dtbo \
        arch/arm64/boot/dts/qcom/sm6150-qrd-overlay.dtbo \
        arch/arm64/boot/dts/qcom/sm6150-cmd-mode-display-idp-overlay.dtbo \
        arch/arm64/boot/dts/qcom/sm6150-rumi-overlay.dtbo \
        arch/arm64/boot/dts/qcom/sm6150-usbc-minidp-idp-overlay.dtbo

    # 5. Copy Kernel Image
    local image_out="${OUT_DIR}/Image"
    cp arch/arm64/boot/Image "${image_out}"

    # 6. AnyKernel3 Packaging
    local zip_out="${OUT_DIR}/${KERNEL_NAME}-Kernel-${DEVICE}-${ed}-${date_tag}.zip"
    if [ -d "${pkg_dir}" ]; then
        cp "${image_out}" "${pkg_dir}/Image"
        cp "${dtb_out}" "${pkg_dir}/dtb"
        
        cd "${pkg_dir}"
        zip -r9 -q "${zip_out}" * -x ".*" -x "__MACOSX"
        cd "${KERNEL_DIR}"
        
        rm -f "${pkg_dir}/Image" "${pkg_dir}/dtb"
        sha256sum "${zip_out}" > "${zip_out}.sha256"

        # Sync to user paths
        mkdir -p "/home/kirancybergrid/Downloads" "/home/kirancybergrid/Documents/redmi note 10 sunny"
        cp "${zip_out}" "/home/kirancybergrid/Downloads/${zip_dest_name}"
        cp "${zip_out}" "/home/kirancybergrid/Documents/redmi note 10 sunny/${zip_dest_name}"
        echo "[+] Packaged: ${zip_out}"
        echo "[+] Synced to: /home/kirancybergrid/Documents/redmi note 10 sunny/${zip_dest_name}"
        echo "[+] Synced to: /home/kirancybergrid/Downloads/${zip_dest_name}"
    fi

    # 7. Package AETHER Kernel Control (WebUI) Module
    local module_dir="${KERNEL_DIR}/packaging/aether_kernel_control"
    local module_zip="${OUT_DIR}/AETHER-Kernel-Control-WebUI-v1.4.0.zip"
    if [ -d "${module_dir}" ]; then
        echo "[+] Packaging AETHER Kernel Control WebUI Module (v1.4.0)..."
        cd "${module_dir}"
        zip -r9 -q "${module_zip}" * -x ".*" -x "__MACOSX"
        cd "${KERNEL_DIR}"
        sha256sum "${module_zip}" > "${module_zip}.sha256"
        cp -f "${module_zip}" "/home/kirancybergrid/Downloads/AETHER-Kernel-Control-WebUI-v1.4.0.zip"
        cp -f "${module_zip}" "/home/kirancybergrid/Downloads/AETHER-Kernel-Control-WebUI.zip"
        cp -f "${module_zip}" "/home/kirancybergrid/Documents/redmi note 10 sunny/AETHER-Kernel-Control-WebUI-v1.4.0.zip"
        cp -f "${module_zip}" "/home/kirancybergrid/Documents/redmi note 10 sunny/AETHER-Kernel-Control-WebUI.zip"
        echo "[+] Packaged WebUI Module: ${module_zip}"
    fi

    # 8. Package AETHER SuSFS Stock Spoofer Module
    local spoofer_dir="${KERNEL_DIR}/packaging/aether_susfs_spoofer"
    local spoofer_zip="${OUT_DIR}/AETHER-SuSFS-Stock-Spoofer-v2.3.0.zip"
    if [ -d "${spoofer_dir}" ]; then
        echo "[+] Packaging AETHER SuSFS Stock Spoofer Module (v2.3.0)..."
        cd "${spoofer_dir}"
        zip -r9 -q "${spoofer_zip}" * -x ".*" -x "__MACOSX"
        cd "${KERNEL_DIR}"
        sha256sum "${spoofer_zip}" > "${spoofer_zip}.sha256"
        cp -f "${spoofer_zip}" "/home/kirancybergrid/Downloads/AETHER-SuSFS-Stock-Spoofer-v2.3.0.zip"
        cp -f "${spoofer_zip}" "/home/kirancybergrid/Downloads/AETHER-SuSFS-Stock-Spoofer.zip"
        cp -f "${spoofer_zip}" "/home/kirancybergrid/Documents/redmi note 10 sunny/AETHER-SuSFS-Stock-Spoofer-v2.3.0.zip"
        cp -f "${spoofer_zip}" "/home/kirancybergrid/Documents/redmi note 10 sunny/AETHER-SuSFS-Stock-Spoofer.zip"
        echo "[+] Packaged SuSFS Spoofer Module: ${spoofer_zip}"
    fi

    local build_end=$(date +%s)
    local duration=$((build_end - build_start))
    echo "=================================================="
    echo " [${ed}] BUILD COMPLETE in $((duration / 60))m $((duration % 60))s"
    echo " Image SHA256:  $(sha256sum "${image_out}" | awk '{print $1}')"
    echo " Kernel ZIP:    $(sha256sum "${zip_out}" | awk '{print $1}')"
    [ -f "${module_zip}" ] && echo " Module ZIP:    $(sha256sum "${module_zip}" | awk '{print $1}')"
    echo "=================================================="
}

if [ "${TARGET_EDITION}" = "all" ]; then
    build_single_edition standard
    build_single_edition enhanced
    git checkout HEAD -- techpack/audio/asoc/codecs/bolero/tx-macro.c techpack/audio/asoc/codecs/wcd937x/wcd937x-regmap.c techpack/audio/asoc/codecs/wcd937x/wcd937x.c 2>/dev/null || true
elif [ "${TARGET_EDITION}" = "standard" ]; then
    build_single_edition standard
else
    build_single_edition enhanced
    git checkout HEAD -- techpack/audio/asoc/codecs/bolero/tx-macro.c techpack/audio/asoc/codecs/wcd937x/wcd937x-regmap.c techpack/audio/asoc/codecs/wcd937x/wcd937x.c 2>/dev/null || true
fi

# Update master sha256sum.txt
sha256sum "/home/kirancybergrid/Documents/redmi note 10 sunny"/AETHER-*.zip > "/home/kirancybergrid/Documents/redmi note 10 sunny/sha256sum.txt" 2>/dev/null || true
cp -f "/home/kirancybergrid/Documents/redmi note 10 sunny/sha256sum.txt" /home/kirancybergrid/Downloads/ 2>/dev/null || true

