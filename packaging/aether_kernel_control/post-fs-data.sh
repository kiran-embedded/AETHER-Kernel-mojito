#!/system/bin/sh
MODDIR=${0%/*}

# 1. Allow system & webui to manipulate DC Dim, HBM, DRM disp_param and FPS Counter nodes
chown system:graphics /sys/class/drm/card*-DSI-*/disp_param /sys/devices/platform/soc/*.qcom,mdss_mdp/drm/card*/card*-DSI-*/disp_param 2>/dev/null || true
chmod 0666 /sys/class/drm/card*-DSI-*/disp_param /sys/devices/platform/soc/*.qcom,mdss_mdp/drm/card*/card*-DSI-*/disp_param 2>/dev/null || true
chown system:system /sys/devices/platform/soc/soc:qcom,dsi-display/anti_flicker /sys/devices/platform/soc/c440000.qcom,spmi/spmi-0/spmi0-05/c440000.qcom,spmi:qcom,pm6150l@5:qcom,lcdb@ec00/regulator/regulator.63/soc:qcom,dsi-display-lab/anti_flicker 2>/dev/null || true
chmod 0666 /sys/devices/platform/soc/soc:qcom,dsi-display/anti_flicker /sys/devices/platform/soc/c440000.qcom,spmi/spmi-0/spmi0-05/c440000.qcom,spmi:qcom,pm6150l@5:qcom,lcdb@ec00/regulator/regulator.63/soc:qcom,dsi-display-lab/anti_flicker 2>/dev/null || true

chown system:system /sys/devices/platform/soc/soc:qcom,dsi-display/hbm /sys/devices/platform/soc/c440000.qcom,spmi/spmi-0/spmi0-05/c440000.qcom,spmi:qcom,pm6150l@5:qcom,lcdb@ec00/regulator/regulator.63/soc:qcom,dsi-display-lab/hbm 2>/dev/null || true
chmod 0666 /sys/devices/platform/soc/soc:qcom,dsi-display/hbm /sys/devices/platform/soc/c440000.qcom,spmi/spmi-0/spmi0-05/c440000.qcom,spmi:qcom,pm6150l@5:qcom,lcdb@ec00/regulator/regulator.63/soc:qcom,dsi-display-lab/hbm 2>/dev/null || true

chown system:graphics /sys/devices/platform/soc/ae00000.qcom,mdss_mdp/drm/card0/sde-crtc-0/measured_fps 2>/dev/null || true
chmod 0666 /sys/devices/platform/soc/ae00000.qcom,mdss_mdp/drm/card0/sde-crtc-0/measured_fps 2>/dev/null || true

# 2. Allow system & webui to manipulate KProfiles modes and parameters
chown system:system /sys/module/kprofiles/parameters/auto_kp 2>/dev/null || true
chmod 0666 /sys/module/kprofiles/parameters/auto_kp 2>/dev/null || true

chown system:system /sys/kernel/kprofiles/kp_mode 2>/dev/null || true
chmod 0666 /sys/kernel/kprofiles/kp_mode 2>/dev/null || true

# 3. Allow system & webui to manipulate Aether in-kernel control nodes
chown system:system /sys/kernel/aether/* 2>/dev/null || true
chmod 0666 /sys/kernel/aether/* 2>/dev/null || true

# 4. Safe live Magisk / KernelSU policy injection
if command -v magiskpolicy >/dev/null 2>&1; then
    magiskpolicy --live "allow init sysfs file setattr" 2>/dev/null || true
    magiskpolicy --live "allow system_app vendor_sysfs_graphics dir search" 2>/dev/null || true
    magiskpolicy --live "allow system_app vendor_sysfs_graphics file { ioctl read write getattr lock append map open watch watch_reads }" 2>/dev/null || true
    magiskpolicy --live "allow system_app sysfs dir search" 2>/dev/null || true
    magiskpolicy --live "allow system_app sysfs file { ioctl read write getattr lock append map open watch watch_reads }" 2>/dev/null || true
    magiskpolicy --live "allow untrusted_app sysfs file { ioctl read write getattr lock append map open watch watch_reads }" 2>/dev/null || true
fi
