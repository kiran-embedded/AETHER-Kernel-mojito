package com.aether.engine.service;

import android.graphics.drawable.Icon;
import android.os.Build;
import android.service.quicksettings.Tile;
import android.service.quicksettings.TileService;

/**
 * AETHER Engine - Bypass Charging Quick Settings Tile
 * Directly powers the motherboard from USB, completely bypassing the battery.
 * Zero CPU / Zero RAM overhead during idle. Non-blocking with monotonic request sequencing.
 */
public class BypassChargingTileService extends TileService {

    private long mCurrentRequestId = 0;
    private boolean mConfirmedState = false;

    @Override
    public void onStartListening() {
        super.onStartListening();
        mConfirmedState = AetherIpcHelper.isBypassActive();
        updateTileUI(mConfirmedState, false);

        final long reqId = ++mCurrentRequestId;
        AetherIpcHelper.refreshBypassStateAsync(reqId, (id, success, confirmed) -> {
            if (id != mCurrentRequestId) return;
            if (mConfirmedState != confirmed) {
                mConfirmedState = confirmed;
                updateTileUI(confirmed, false);
            }
        });
    }

    @Override
    public void onClick() {
        super.onClick();
        final boolean target = !mConfirmedState;
        final long reqId = ++mCurrentRequestId;

        // Visual pending feedback
        updateTileUI(target, true);

        AetherIpcHelper.setBypassActiveAsync(target, reqId, (id, success, confirmed) -> {
            if (id != mCurrentRequestId) return;
            mConfirmedState = confirmed;
            updateTileUI(confirmed, false);
        });
    }

    private void updateTileUI(boolean active, boolean isPending) {
        Tile tile = getQsTile();
        if (tile == null) return;

        tile.setIcon(Icon.createWithResource(this, R.drawable.ic_qs_bypass));
        tile.setLabel(getString(R.string.tile_bypass_name));
        tile.setState(active ? Tile.STATE_ACTIVE : Tile.STATE_INACTIVE);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            if (isPending) {
                tile.setSubtitle(getString(R.string.tile_pending));
            } else {
                tile.setSubtitle(getString(active ? R.string.tile_bypass_active : R.string.tile_bypass_inactive));
            }
        }

        tile.updateTile();
    }
}
