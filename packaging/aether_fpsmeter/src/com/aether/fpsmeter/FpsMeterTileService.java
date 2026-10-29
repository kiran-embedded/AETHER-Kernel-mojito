package com.aether.fpsmeter;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.graphics.drawable.Icon;
import android.os.Build;
import android.service.quicksettings.Tile;
import android.service.quicksettings.TileService;

/**
 * AETHER FPS Meter - Quick Settings Tile Provider
 * Native in-process TileService: 100% synchronous, zero SELinux overhead,
 * zero animation lag, and zero idle resource usage.
 */
public class FpsMeterTileService extends TileService {

    private BroadcastReceiver stateReceiver = null;

    @Override
    public void onStartListening() {
        super.onStartListening();
        registerReceiverIfNeeded();
        updateTileState(FpsOverlayService.isRunning);
    }

    @Override
    public void onStopListening() {
        unregisterReceiverIfNeeded();
        super.onStopListening();
    }

    @Override
    public void onClick() {
        super.onClick();
        boolean willRun = !FpsOverlayService.isRunning;
        if (willRun) {
            startOverlayService();
        } else {
            stopOverlayService();
        }
        updateTileState(willRun);
    }

    private void registerReceiverIfNeeded() {
        if (stateReceiver == null) {
            stateReceiver = new BroadcastReceiver() {
                @Override
                public void onReceive(Context context, Intent intent) {
                    if (intent != null && "com.aether.fpsmeter.STATE_CHANGED".equals(intent.getAction())) {
                        boolean running = intent.getBooleanExtra("isRunning", FpsOverlayService.isRunning);
                        updateTileState(running);
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

    private void startOverlayService() {
        try {
            Intent intent = new Intent(this, FpsOverlayService.class);
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                startForegroundService(intent);
            } else {
                startService(intent);
            }
        } catch (Exception ignored) {}
    }

    private void stopOverlayService() {
        try {
            Intent intent = new Intent(this, FpsOverlayService.class);
            intent.setAction("ACTION_STOP");
            stopService(intent);
        } catch (Exception ignored) {}
    }

    private void updateTileState(boolean active) {
        Tile tile = getQsTile();
        if (tile == null) return;

        tile.setIcon(Icon.createWithResource(this, R.drawable.ic_qs_fps));
        tile.setLabel(getString(R.string.tile_fps_name));
        tile.setState(active ? Tile.STATE_ACTIVE : Tile.STATE_INACTIVE);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            tile.setSubtitle(getString(active ? R.string.tile_fps_running : R.string.tile_fps_stopped));
        }

        tile.updateTile();
    }
}
