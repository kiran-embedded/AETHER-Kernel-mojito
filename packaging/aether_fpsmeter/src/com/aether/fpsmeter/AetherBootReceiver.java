package com.aether.fpsmeter;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Build;

/**
 * AETHER Boot Receiver — Native System Service Pattern
 *
 * Fires on BOOT_COMPLETED and LOCKED_BOOT_COMPLETED (directBootAware=true),
 * exactly like com.google.android.gms.GmsBootReceiver.
 *
 * Auto-restores FPS overlay if it was active before the last power-off,
 * without any user interaction (zero-friction native service restoration).
 */
public class AetherBootReceiver extends BroadcastReceiver {

    @Override
    public void onReceive(Context context, Intent intent) {
        final String action = intent != null ? intent.getAction() : null;
        if (action == null) return;

        switch (action) {
            case Intent.ACTION_BOOT_COMPLETED:
            case "android.intent.action.LOCKED_BOOT_COMPLETED":
            case Intent.ACTION_MY_PACKAGE_REPLACED:
                restoreIfNeeded(context);
                break;
            default:
                break;
        }
    }

    private void restoreIfNeeded(Context context) {
        try {
            // Read the persisted ON/OFF state from device-encrypted (DE) storage
            // DE storage is available immediately at LOCKED_BOOT_COMPLETED, before user unlock
            SharedPreferences prefs = context.createDeviceProtectedStorageContext()
                    .getSharedPreferences("aether_fps_prefs", Context.MODE_PRIVATE);

            boolean wasActive = prefs.getBoolean("fps_overlay_active", false);

            if (wasActive) {
                Intent startIntent = new Intent(context, FpsOverlayService.class);
                startIntent.setAction("ACTION_START");
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                    // startForegroundService: Android 8+ (same as Play Services uses)
                    context.startForegroundService(startIntent);
                } else {
                    context.startService(startIntent);
                }
            }
        } catch (Exception ignored) {
            // Silently fail — never crash the boot sequence
        }
    }
}
