package com.aether.fpsmeter;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.provider.Settings;
import android.util.Log;

public class FpsReceiver extends BroadcastReceiver {
    private static final String TAG = "AetherFpsReceiver";

    @Override
    public void onReceive(Context context, Intent intent) {
        String action = intent != null ? intent.getAction() : null;
        if (action == null) return;

        Log.d(TAG, "Received broadcast action: " + action);

        if ("com.aether.fpsmeter.ACTION_TOGGLE".equals(action)) {
            if (FpsOverlayService.isRunning) {
                stopService(context);
            } else {
                startService(context);
            }
        } else if ("com.aether.fpsmeter.ACTION_START".equals(action)) {
            startService(context);
        } else if ("com.aether.fpsmeter.ACTION_STOP".equals(action)) {
            stopService(context);
        }
    }

    private void startService(Context context) {
        try {
            Intent serviceIntent = new Intent(context, FpsOverlayService.class);
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                context.startForegroundService(serviceIntent);
            } else {
                context.startService(serviceIntent);
            }
        } catch (Exception e) {
            Log.e(TAG, "Failed to start FpsOverlayService", e);
        }
    }

    private void stopService(Context context) {
        try {
            Intent serviceIntent = new Intent(context, FpsOverlayService.class);
            serviceIntent.setAction("ACTION_STOP");
            context.stopService(serviceIntent);
        } catch (Exception e) {
            Log.e(TAG, "Failed to stop FpsOverlayService", e);
        }
    }
}
