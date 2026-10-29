package com.aether.engine.service;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.FileWriter;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.ThreadFactory;
import java.util.concurrent.TimeUnit;

/**
 * AETHER Engine - Thread-Safe Asynchronous SysFS & Kernel Control Bridge
 * Target: Xiaomi Redmi Note 10 (mojito / sunny) | Qualcomm SM6150 (Snapdragon 678)
 *
 * Design Architecture:
 *  - 100% Non-blocking: all disk I/O and process execution executed off the main thread.
 *  - Serialized worker queue: prevents concurrency races and overlapping commands.
 *  - Monotonic request sequencing: stale responses from rapid taps are cleanly discarded.
 *  - Separate confirmed, target, and pending states with read-back verification.
 *  - Zero background daemon dependency.
 */
public final class AetherIpcHelper {
    private static final String TAG = "AetherTileIpc";

    public static final String NODE_INSTANT_BYPASS = "/sys/kernel/aether/instant_bypass";
    public static final String NODE_ANTI_FLICKER = "/sys/kernel/aether/anti_flicker";
    public static final String NODE_ANTI_FLICKER_ALT = "/sys/devices/platform/soc/soc:qcom,dsi-display/anti_flicker";
    public static final String NODE_KP_MODE = "/sys/kernel/kprofiles/kp_mode";
    public static final String NODE_HBM = "/sys/kernel/aether/hbm";
    public static final String NODE_HBM_ALT = "/sys/devices/platform/soc/soc:qcom,dsi-display/hbm";
    public static final String NODE_TOUCH_BOOST = "/sys/kernel/aether/aacc_screen_boost";
    public static final String NODE_HIFI = "/sys/kernel/aether/hifi_mode";

    private static final Handler sMainHandler = new Handler(Looper.getMainLooper());

    private static final ExecutorService sAsyncIo = Executors.newSingleThreadExecutor(new ThreadFactory() {
        @Override
        public Thread newThread(Runnable r) {
            Thread t = new Thread(r, "AetherIpcWorker");
            t.setDaemon(true);
            t.setPriority(Thread.NORM_PRIORITY - 1);
            return t;
        }
    });

    // In-memory cache of confirmed kernel states
    private static volatile boolean sBypassActive = false;
    private static volatile boolean sDcDimmingActive = false;
    private static volatile boolean sHbmActive = false;
    private static volatile boolean sTouchBoostActive = false;
    private static volatile boolean sHiFiAudioActive = false;
    private static volatile int sProfileMode = 2; // 2 = Balanced (default)
    private static volatile boolean sFpsMeterActive = false;

    static {
        // Asynchronously populate cache on class load
        sAsyncIo.execute(AetherIpcHelper::refreshAllStatesSync);
    }

    private AetherIpcHelper() {}

    public interface BooleanCallback {
        void onResult(long requestId, boolean success, boolean confirmedValue);
    }

    public interface IntegerCallback {
        void onResult(long requestId, boolean success, int confirmedValue);
    }

    // ── Low-Level SysFS Operations ───────────────────────────────────

    public static String readNodeSync(String path, String defaultValue) {
        File file = new File(path);
        if (!file.exists()) {
            return defaultValue;
        }
        try (BufferedReader reader = new BufferedReader(new FileReader(file), 64)) {
            String line = reader.readLine();
            return (line != null) ? line.trim() : defaultValue;
        } catch (Exception e) {
            return defaultValue;
        }
    }

    private static boolean writeNodeDirect(String path, String value) {
        File file = new File(path);
        if (!file.exists()) {
            return false;
        }
        try (FileWriter writer = new FileWriter(file)) {
            writer.write(value + "\n");
            writer.flush();
            return true;
        } catch (Exception e) {
            return false;
        }
    }

    private static boolean writeNodeWithRootFallback(String path, String value) {
        // 1. Direct sysfs write (instant, 0ms, works when system_app domain or 0666)
        if (writeNodeDirect(path, value)) {
            return true;
        }

        // 2. Bounded root execution fallback (1000ms max timeout)
        Process p = null;
        try {
            p = Runtime.getRuntime().exec(new String[]{"su", "-c", "echo '" + value + "' > '" + path + "'"});
            if (!p.waitFor(1000, TimeUnit.MILLISECONDS)) {
                p.destroyForcibly();
                return false;
            }
            return p.exitValue() == 0;
        } catch (Exception e) {
            if (p != null) p.destroyForcibly();
            return false;
        }
    }

    private static void refreshAllStatesSync() {
        sBypassActive = "1".equals(readNodeSync(NODE_INSTANT_BYPASS, "0"));

        String dcVal = readNodeSync(NODE_ANTI_FLICKER, "");
        if (dcVal.isEmpty()) dcVal = readNodeSync(NODE_ANTI_FLICKER_ALT, "0");
        sDcDimmingActive = "1".equals(dcVal);

        String hbmVal = readNodeSync(NODE_HBM, "");
        if (hbmVal.isEmpty()) hbmVal = readNodeSync(NODE_HBM_ALT, "0");
        sHbmActive = "1".equals(hbmVal);

        sTouchBoostActive = "1".equals(readNodeSync(NODE_TOUCH_BOOST, "0"));
        sHiFiAudioActive = "1".equals(readNodeSync(NODE_HIFI, "0"));

        String kpVal = readNodeSync(NODE_KP_MODE, "2");
        try {
            int mode = Integer.parseInt(kpVal);
            sProfileMode = (mode == 0) ? 2 : mode;
        } catch (Exception e) {
            sProfileMode = 2;
        }
    }

    // ── Generic Async Boolean Node Handler ───────────────────────────

    private static void setBooleanNodeInternal(
            final String primaryNode,
            final String altNode,
            final boolean targetValue,
            final long requestId,
            final BooleanCallback callback) {

        sAsyncIo.execute(() -> {
            String valStr = targetValue ? "1" : "0";
            boolean writeOk = writeNodeWithRootFallback(primaryNode, valStr);
            if (altNode != null && new File(altNode).exists()) {
                writeNodeWithRootFallback(altNode, valStr);
            }

            // Read-back verification
            String readBack = readNodeSync(primaryNode, "");
            if (readBack.isEmpty() && altNode != null) {
                readBack = readNodeSync(altNode, "0");
            }
            boolean confirmed = "1".equals(readBack);
            boolean success = writeOk && (confirmed == targetValue);

            if (callback != null) {
                sMainHandler.post(() -> callback.onResult(requestId, success, confirmed));
            }
        });
    }

    private static void refreshBooleanNodeInternal(
            final String primaryNode,
            final String altNode,
            final long requestId,
            final BooleanCallback callback) {

        sAsyncIo.execute(() -> {
            String readBack = readNodeSync(primaryNode, "");
            if (readBack.isEmpty() && altNode != null) {
                readBack = readNodeSync(altNode, "0");
            }
            boolean confirmed = "1".equals(readBack);

            if (callback != null) {
                sMainHandler.post(() -> callback.onResult(requestId, true, confirmed));
            }
        });
    }

    // ── Bypass Charging ──────────────────────────────────────────────

    public static boolean isBypassActive() {
        return sBypassActive;
    }

    public static void setBypassActiveAsync(boolean enable, long reqId, BooleanCallback callback) {
        setBooleanNodeInternal(NODE_INSTANT_BYPASS, null, enable, reqId, (id, success, confirmed) -> {
            sBypassActive = confirmed;
            if (callback != null) callback.onResult(id, success, confirmed);
        });
    }

    public static void refreshBypassStateAsync(long reqId, BooleanCallback callback) {
        refreshBooleanNodeInternal(NODE_INSTANT_BYPASS, null, reqId, (id, success, confirmed) -> {
            sBypassActive = confirmed;
            if (callback != null) callback.onResult(id, success, confirmed);
        });
    }

    // ── DC Dimming (Anti-Flicker) ─────────────────────────────────────

    public static boolean isDcDimmingActive() {
        return sDcDimmingActive;
    }

    public static void setDcDimmingActiveAsync(boolean enable, long reqId, BooleanCallback callback) {
        setBooleanNodeInternal(NODE_ANTI_FLICKER, NODE_ANTI_FLICKER_ALT, enable, reqId, (id, success, confirmed) -> {
            sDcDimmingActive = confirmed;
            if (callback != null) callback.onResult(id, success, confirmed);
        });
    }

    public static void refreshDcDimmingStateAsync(long reqId, BooleanCallback callback) {
        refreshBooleanNodeInternal(NODE_ANTI_FLICKER, NODE_ANTI_FLICKER_ALT, reqId, (id, success, confirmed) -> {
            sDcDimmingActive = confirmed;
            if (callback != null) callback.onResult(id, success, confirmed);
        });
    }

    // ── High Brightness Mode (HBM 1100 Nits) ─────────────────────────

    public static boolean isHbmActive() {
        return sHbmActive;
    }

    public static void setHbmActiveAsync(boolean enable, long reqId, BooleanCallback callback) {
        setBooleanNodeInternal(NODE_HBM, NODE_HBM_ALT, enable, reqId, (id, success, confirmed) -> {
            sHbmActive = confirmed;
            if (callback != null) callback.onResult(id, success, confirmed);
        });
    }

    public static void refreshHbmStateAsync(long reqId, BooleanCallback callback) {
        refreshBooleanNodeInternal(NODE_HBM, NODE_HBM_ALT, reqId, (id, success, confirmed) -> {
            sHbmActive = confirmed;
            if (callback != null) callback.onResult(id, success, confirmed);
        });
    }

    // ── 240Hz Touch Response Boost ───────────────────────────────────

    public static boolean isTouchBoostActive() {
        return sTouchBoostActive;
    }

    public static void setTouchBoostActiveAsync(boolean enable, long reqId, BooleanCallback callback) {
        setBooleanNodeInternal(NODE_TOUCH_BOOST, null, enable, reqId, (id, success, confirmed) -> {
            sTouchBoostActive = confirmed;
            if (callback != null) callback.onResult(id, success, confirmed);
        });
    }

    public static void refreshTouchBoostStateAsync(long reqId, BooleanCallback callback) {
        refreshBooleanNodeInternal(NODE_TOUCH_BOOST, null, reqId, (id, success, confirmed) -> {
            sTouchBoostActive = confirmed;
            if (callback != null) callback.onResult(id, success, confirmed);
        });
    }

    // ── Studio Hi-Fi AudioDAC Boost ──────────────────────────────────

    public static boolean isHiFiAudioActive() {
        return sHiFiAudioActive;
    }

    public static void setHiFiAudioActiveAsync(boolean enable, long reqId, BooleanCallback callback) {
        setBooleanNodeInternal(NODE_HIFI, null, enable, reqId, (id, success, confirmed) -> {
            sHiFiAudioActive = confirmed;
            if (callback != null) callback.onResult(id, success, confirmed);
        });
    }

    public static void refreshHiFiAudioStateAsync(long reqId, BooleanCallback callback) {
        refreshBooleanNodeInternal(NODE_HIFI, null, reqId, (id, success, confirmed) -> {
            sHiFiAudioActive = confirmed;
            if (callback != null) callback.onResult(id, success, confirmed);
        });
    }

    // ── Performance Profile (KProfiles) ──────────────────────────────
    // 1 = Battery Saver, 2 = Balanced (or 0), 3 = Gaming Turbo

    public static int getProfileMode() {
        return sProfileMode;
    }

    public static void setProfileMode(int mode) {
        sProfileMode = mode;
        setProfileModeAsync(mode, 0, null);
    }

    public static void setProfileModeAsync(final int mode, final long reqId, final IntegerCallback callback) {
        sAsyncIo.execute(() -> {
            String valStr = String.valueOf(mode);
            boolean writeOk = writeNodeWithRootFallback(NODE_KP_MODE, valStr);
            String readBack = readNodeSync(NODE_KP_MODE, "2");
            int confirmedMode = 2;
            try {
                int parsed = Integer.parseInt(readBack);
                confirmedMode = (parsed == 0) ? 2 : parsed;
            } catch (Exception ignored) {}

            sProfileMode = confirmedMode;
            boolean success = writeOk && (confirmedMode == mode);

            final int finalConfirmed = confirmedMode;
            if (callback != null) {
                sMainHandler.post(() -> callback.onResult(reqId, success, finalConfirmed));
            }
        });
    }

    public static void refreshProfileModeAsync(final long reqId, final IntegerCallback callback) {
        sAsyncIo.execute(() -> {
            String readBack = readNodeSync(NODE_KP_MODE, "2");
            int confirmedMode = 2;
            try {
                int parsed = Integer.parseInt(readBack);
                confirmedMode = (parsed == 0) ? 2 : parsed;
            } catch (Exception ignored) {}

            sProfileMode = confirmedMode;
            final int finalConfirmed = confirmedMode;
            if (callback != null) {
                sMainHandler.post(() -> callback.onResult(reqId, true, finalConfirmed));
            }
        });
    }

    // ── 1-Tap RAM Compaction Cleaner ─────────────────────────────────

    public static void triggerRamCleanAsync(final Runnable onComplete) {
        sAsyncIo.execute(() -> {
            Process p = null;
            try {
                // Drop caches and compact memory with bounded 2000ms timeout
                p = Runtime.getRuntime().exec(new String[]{
                        "su", "-c", "echo 3 > /proc/sys/vm/drop_caches; echo 1 > /proc/sys/vm/compact_memory"
                });
                if (!p.waitFor(2000, TimeUnit.MILLISECONDS)) {
                    p.destroyForcibly();
                }
            } catch (Exception ignored) {
                if (p != null) p.destroyForcibly();
            } finally {
                if (onComplete != null) {
                    sMainHandler.post(onComplete);
                }
            }
        });
    }

    // ── FPS Meter Service Integration ────────────────────────────────

    public static boolean isFpsMeterActive() {
        return sFpsMeterActive;
    }

    public static void setFpsMeterActiveCached(boolean active) {
        sFpsMeterActive = active;
    }

    public static void toggleFpsMeter(Context context) {
        try {
            Intent intent = new Intent("com.aether.fpsmeter.ACTION_TOGGLE");
            intent.setComponent(new ComponentName("com.aether.fpsmeter", "com.aether.fpsmeter.FpsReceiver"));
            intent.addFlags(Intent.FLAG_INCLUDE_STOPPED_PACKAGES);
            context.sendBroadcast(intent);
        } catch (Exception e) {
            Log.e(TAG, "Failed to toggle FPS meter", e);
        }
    }
}
