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
 * AETHER Engine - Dynamic Performance Profile Quick Settings Tile
 * 1-tap cycles between: Balanced ➔ Gaming Turbo ➔ Battery Saver ➔ Balanced
 * Long-press launches the interactive 3-card chooser dialog.
 * Non-blocking with monotonic request sequencing and verified readback.
 */
public class AetherProfileTileService extends TileService {

    private BroadcastReceiver profileReceiver = null;
    private long mCurrentRequestId = 0;
    private int mConfirmedMode = 2; // Default Balanced

    @Override
    public void onStartListening() {
        super.onStartListening();
        registerReceiverIfNeeded();
        mConfirmedMode = AetherIpcHelper.getProfileMode();
        updateTileUI(mConfirmedMode, false);

        final long reqId = ++mCurrentRequestId;
        AetherIpcHelper.refreshProfileModeAsync(reqId, (id, success, confirmed) -> {
            if (id != mCurrentRequestId) return;
            if (mConfirmedMode != confirmed) {
                mConfirmedMode = confirmed;
                updateTileUI(confirmed, false);
            }
        });
    }

    @Override
    public void onStopListening() {
        unregisterReceiverIfNeeded();
        super.onStopListening();
    }

    @Override
    public void onClick() {
        super.onClick();
        int next;
        // Cycle: Balanced (2) -> Gaming Turbo (3) -> Battery Saver (1) -> Balanced (2)
        if (mConfirmedMode == 2) {
            next = 3; // Gaming Turbo
        } else if (mConfirmedMode == 3) {
            next = 1; // Battery Saver
        } else {
            next = 2; // Balanced
        }

        final long reqId = ++mCurrentRequestId;
        // Visual pending feedback
        updateTileUI(next, true);

        AetherIpcHelper.setProfileModeAsync(next, reqId, (id, success, confirmed) -> {
            if (id != mCurrentRequestId) return;
            mConfirmedMode = confirmed;
            updateTileUI(confirmed, false);
            try {
                sendBroadcast(new Intent("com.aether.engine.PROFILE_CHANGED"));
            } catch (Exception ignored) {}
        });
    }

    private void registerReceiverIfNeeded() {
        if (profileReceiver == null) {
            profileReceiver = new BroadcastReceiver() {
                @Override
                public void onReceive(Context context, Intent intent) {
                    mConfirmedMode = AetherIpcHelper.getProfileMode();
                    updateTileUI(mConfirmedMode, false);
                }
            };
            IntentFilter filter = new IntentFilter("com.aether.engine.PROFILE_CHANGED");
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                registerReceiver(profileReceiver, filter, Context.RECEIVER_EXPORTED);
            } else {
                registerReceiver(profileReceiver, filter);
            }
        }
    }

    private void unregisterReceiverIfNeeded() {
        if (profileReceiver != null) {
            try {
                unregisterReceiver(profileReceiver);
            } catch (Exception ignored) {}
            profileReceiver = null;
        }
    }

    private void updateTileUI(int mode, boolean isPending) {
        Tile tile = getQsTile();
        if (tile == null) return;

        tile.setIcon(Icon.createWithResource(this, R.drawable.ic_qs_profile));
        tile.setLabel(getString(R.string.tile_profile_name));

        String subtitle;
        int state;

        if (mode == 3) {
            subtitle = getString(R.string.tile_profile_gaming);
            state = Tile.STATE_ACTIVE;
        } else if (mode == 1) {
            subtitle = getString(R.string.tile_profile_battery);
            state = Tile.STATE_ACTIVE;
        } else {
            subtitle = getString(R.string.tile_profile_balanced);
            state = Tile.STATE_INACTIVE;
        }

        tile.setState(state);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            if (isPending) {
                tile.setSubtitle(getString(R.string.tile_pending));
            } else {
                tile.setSubtitle(subtitle);
            }
        }

        tile.updateTile();
    }
}
