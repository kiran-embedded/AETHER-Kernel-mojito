package com.aether.engine.service;

import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.service.quicksettings.TileService;

/**
 * AETHER QS Boot Receiver — Native System Service Pattern
 *
 * Fires on BOOT_COMPLETED / LOCKED_BOOT_COMPLETED / MY_PACKAGE_REPLACED.
 * Requests SystemUI to refresh tile listening records via the official
 * TileService.requestListeningState() API without starting background services.
 *
 * BOOTLOOP SAFETY:
 *   - Zero filesystem I/O, zero network, zero process spawning.
 *   - Uses official Android API 24+ TileService.requestListeningState().
 *   - directBootAware="true" fires before CE-unlock without crashing.
 */
public class AetherQsBootReceiver extends BroadcastReceiver {

    @Override
    public void onReceive(Context context, Intent intent) {
        final String action = intent != null ? intent.getAction() : null;
        if (action == null) return;

        switch (action) {
            case Intent.ACTION_BOOT_COMPLETED:
            case "android.intent.action.LOCKED_BOOT_COMPLETED":
            case Intent.ACTION_MY_PACKAGE_REPLACED:
                refreshTileRecords(context);
                break;
            default:
                break;
        }
    }

    private void refreshTileRecords(Context context) {
        Class<?>[] tileClasses = new Class<?>[]{
            BypassChargingTileService.class,
            AetherProfileTileService.class,
            FpsMeterTileService.class,
            DcDimmingTileService.class,
            HbmTileService.class,
            TouchBoostTileService.class,
            RamCleanerTileService.class,
            HiFiAudioTileService.class
        };

        for (Class<?> cls : tileClasses) {
            try {
                TileService.requestListeningState(context, new ComponentName(context, cls));
            } catch (Exception ignored) {
                // Silently swallow any framework exception during early boot
            }
        }
    }
}
