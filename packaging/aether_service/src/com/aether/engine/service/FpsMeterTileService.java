package com.aether.engine.service;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.graphics.drawable.Icon;
import android.os.Build;
import android.service.quicksettings.Tile;
import android.service.quicksettings.TileService;

/**
 * AETHER Engine - Floating FPS Meter Quick Settings Tile
 * 1-tap toggles the real-time Game-Turbo AMOLED FPS HUD overlay.
 * Synchronized with com.aether.fpsmeter via system broadcast.
 * Zero CPU / Zero RAM overhead during idle.
 */
public class FpsMeterTileService extends TileService {

    private static boolean sLocallyActive = false;
    private BroadcastReceiver stateReceiver = null;

    @Override
    public void onStartListening() {
        super.onStartListening();
        registerReceiverIfNeeded();
        sLocallyActive = AetherIpcHelper.isFpsMeterActive();
        updateTileUI(sLocallyActive, false);
    }

    @Override
    public void onStopListening() {
        unregisterReceiverIfNeeded();
        super.onStopListening();
    }

    @Override
    public void onClick() {
        super.onClick();
        boolean target = !sLocallyActive;
        // Visual pending feedback
        updateTileUI(target, true);
        AetherIpcHelper.toggleFpsMeter(this);
    }

    private void registerReceiverIfNeeded() {
        if (stateReceiver == null) {
            stateReceiver = new BroadcastReceiver() {
                @Override
                public void onReceive(Context context, Intent intent) {
                    if (intent != null && "com.aether.fpsmeter.STATE_CHANGED".equals(intent.getAction())) {
                        sLocallyActive = intent.getBooleanExtra("isRunning", sLocallyActive);
                        AetherIpcHelper.setFpsMeterActiveCached(sLocallyActive);
                        updateTileUI(sLocallyActive, false);
                    }
                }
            };
            IntentFilter filter = new IntentFilter("com.aether.fpsmeter.STATE_CHANGED");
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                registerReceiver(stateReceiver, filter, Context.RECEIVER_EXPORTED);
            } else {
                registerReceiver(stateReceiver, filter);
            }
        }
    }

    private void unregisterReceiverIfNeeded() {
        if (stateReceiver != null) {
            try {
                unregisterReceiver(stateReceiver);
            } catch (Exception ignored) {}
            stateReceiver = null;
        }
    }

    private void updateTileUI(boolean active, boolean isPending) {
        Tile tile = getQsTile();
        if (tile == null) return;

        tile.setIcon(Icon.createWithResource(this, R.drawable.ic_qs_fps));
        tile.setLabel(getString(R.string.tile_fps_name));
        tile.setState(active ? Tile.STATE_ACTIVE : Tile.STATE_INACTIVE);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            if (isPending) {
                tile.setSubtitle(getString(R.string.tile_pending));
            } else {
                tile.setSubtitle(getString(active ? R.string.tile_fps_running : R.string.tile_fps_stopped));
            }
        }

        tile.updateTile();
    }
}
