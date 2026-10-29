package com.aether.engine.service;

import android.graphics.drawable.Icon;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.service.quicksettings.Tile;
import android.service.quicksettings.TileService;

/**
 * AETHER Engine - 1-Tap RAM Compaction & Cache Purge Quick Settings Tile
 * Compacts memory, drops clean pagecache, and frees RAM without blocking storage I/O.
 * Zero CPU / Zero RAM overhead during idle. Debounced against rapid clicks.
 */
public class RamCleanerTileService extends TileService {

    private final Handler handler = new Handler(Looper.getMainLooper());
    private boolean mIsCleaning = false;

    @Override
    public void onStartListening() {
        super.onStartListening();
        if (!mIsCleaning) {
            setIdleState();
        }
    }

    @Override
    public void onClick() {
        super.onClick();
        if (mIsCleaning) {
            return; // Debounce rapid clicks while operation is in flight
        }
        mIsCleaning = true;

        Tile tile = getQsTile();
        if (tile != null) {
            tile.setIcon(Icon.createWithResource(this, R.drawable.ic_qs_ram));
            tile.setLabel(getString(R.string.tile_ram_name));
            tile.setState(Tile.STATE_ACTIVE);

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                tile.setSubtitle(getString(R.string.tile_ram_cleaned));
            }
            tile.updateTile();
        }

        // Asynchronously trigger native drop_caches + compact_memory (zero fstrim hang)
        AetherIpcHelper.triggerRamCleanAsync(() -> {
            handler.postDelayed(() -> {
                mIsCleaning = false;
                setIdleState();
            }, 1500);
        });
    }

    private void setIdleState() {
        Tile tile = getQsTile();
        if (tile == null) return;

        tile.setIcon(Icon.createWithResource(this, R.drawable.ic_qs_ram));
        tile.setLabel(getString(R.string.tile_ram_name));
        tile.setState(Tile.STATE_INACTIVE);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            tile.setSubtitle(getString(R.string.tile_ram_idle));
        }

        tile.updateTile();
    }

    @Override
    public void onStopListening() {
        handler.removeCallbacksAndMessages(null);
        mIsCleaning = false;
        super.onStopListening();
    }
}
