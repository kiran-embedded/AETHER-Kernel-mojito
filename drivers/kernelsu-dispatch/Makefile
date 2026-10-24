# SPDX-License-Identifier: GPL-2.0-or-later
#
# drivers/kernelsu-dispatch/Makefile (Kbuild)
#
# Aether Dual-KSU Dynamic Build Dispatch
# =======================================
# This file is the single routing point that decides which KSU
# implementation to compile based on the Kconfig variant choice.
#
# CONFIG_KSU_VARIANT_NEXT  → compile KernelSU-Next (drivers/kernelsu/)
# CONFIG_KSU_VARIANT_STOCK → compile KernelSU stock (KernelSU/kernel/)
#
# "drivers/kernelsu/" is already a symlink to KernelSU-Next/kernel/
# and remains untouched.  For stock, we point obj-y at the stock tree
# using an absolute path so Kbuild can find it regardless of working dir.
# ───────────────────────────────────────────────────────────────────

# ── KernelSU-Next path (via existing symlink) ───────────────────────
# The symlink drivers/kernelsu → ../KernelSU-Next/kernel is kept as-is.
# KernelSU-Next Kbuild is at drivers/kernelsu/Kbuild (via symlink).
obj-$(CONFIG_KSU_VARIANT_NEXT) += ../kernelsu/

# ── KernelSU stock path ─────────────────────────────────────────────
# KernelSU stock Kbuild is at KernelSU/kernel/Kbuild.
# Resolve absolute path so Kbuild doesn't trip on the space in SCRIPT_DIR.
KSU_STOCK_SRC := $(abspath $(srctree)/KernelSU/kernel)
obj-$(CONFIG_KSU_VARIANT_STOCK) += $(KSU_STOCK_SRC)/
