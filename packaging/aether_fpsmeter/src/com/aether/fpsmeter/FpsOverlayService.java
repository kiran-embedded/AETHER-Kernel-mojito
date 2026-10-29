package com.aether.fpsmeter;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.Service;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.graphics.Color;
import android.graphics.PixelFormat;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.os.Build;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.view.Choreographer;
import android.view.Display;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowManager;
import android.widget.TextView;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.util.Locale;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.TimeUnit;

/**
 * AETHER Engine - Real-Time AMOLED Hardware FPS Overlay Service
 * Target: Xiaomi Redmi Note 10 (mojito / sunny) | Qualcomm SM6150 (Snapdragon 678)
 *
 * Architecture:
 *  - Background sampling engine (0ms UI thread I/O).
 *  - Validated monotonic rolling window calculation: FPS = n / ((t_n - t_0) / 10^9).
 *  - Primary source: Verified kernel hardware display commit interface (/sys/kernel/aether/fps, sde-crtc).
 *  - Secondary fallback: Monotonic frame callback rolling window without clamping.
 *  - Screen off/on power-aware suspension (0% CPU when screen is off).
 *  - Preserves exact Game-Turbo floating bubble design, drag physics, and colors.
 */
public class FpsOverlayService extends Service implements Choreographer.FrameCallback {

    private static final String CHANNEL_ID = "aether_fps_channel";
    private static final int NOTIF_ID = 67801;
    public static boolean isRunning = false;

    private WindowManager windowManager;
    private TextView bubbleView;
    private WindowManager.LayoutParams params;

    private final FpsCalculator mFpsCalculator = new FpsCalculator();
    private final Handler mMainHandler = new Handler(Looper.getMainLooper());
    private ScheduledExecutorService mSamplerExecutor;

    private float mDisplayRefreshRate = 60.0f;
    private boolean isDetailed = true;
    private String lastRenderedText = "";
    private boolean mIsScreenOn = true;
    private BroadcastReceiver mScreenStateReceiver;

    private int initialX;
    private int initialY;
    private float initialTouchX;
    private float initialTouchY;
    private long touchStartTime = 0;

    private String mWorkingFpsNode = null;
    private boolean mCheckedFpsNode = false;

    @Override
    public void onCreate() {
        super.onCreate();
        isRunning = true;
        broadcastState(true);
        persistState(true);
        createNotificationChannel();

        if (Build.VERSION.SDK_INT >= 35) {
            startForeground(NOTIF_ID, buildNotification(), 1024);
        } else {
            startForeground(NOTIF_ID, buildNotification());
        }

        windowManager = (WindowManager) getSystemService(Context.WINDOW_SERVICE);
        if (windowManager != null) {
            Display display = windowManager.getDefaultDisplay();
            if (display != null) {
                mDisplayRefreshRate = display.getMode().getRefreshRate();
                if (mDisplayRefreshRate <= 0.0f) {
                    mDisplayRefreshRate = 60.0f;
                }
            }
        }

        initBubbleView();
        registerScreenStateReceiver();

        // Start background sampling engine (every 250ms)
        mSamplerExecutor = Executors.newSingleThreadScheduledExecutor(r -> {
            Thread t = new Thread(r, "AetherFpsSampler");
            t.setDaemon(true);
            t.setPriority(Thread.NORM_PRIORITY - 1);
            return t;
        });

        mSamplerExecutor.scheduleWithFixedDelay(this::sampleAndRenderFps, 250, 250, TimeUnit.MILLISECONDS);

        // Register Choreographer frame callback for monotonic timestamp collection
        Choreographer.getInstance().postFrameCallback(this);
    }

    private void initBubbleView() {
        bubbleView = new TextView(this);
        bubbleView.setText("0 FPS");
        bubbleView.setTextColor(Color.parseColor("#94A3B8")); // Standby 0 FPS
        bubbleView.setTextSize(12f);
        bubbleView.setTypeface(Typeface.MONOSPACE, Typeface.BOLD);
        bubbleView.setGravity(Gravity.CENTER);

        int padH = (int) (12 * getResources().getDisplayMetrics().density);
        int padV = (int) (5 * getResources().getDisplayMetrics().density);
        bubbleView.setPadding(padH, padV, padH, padV);

        GradientDrawable bg = new GradientDrawable();
        bg.setColor(Color.parseColor("#000000")); // Pure AMOLED pitch black
        bg.setCornerRadius(30f);
        bg.setStroke(2, Color.parseColor("#00D4FF")); // Neon Cyan border
        bubbleView.setBackground(bg);
        bubbleView.setElevation(8f);

        int layoutType;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            layoutType = WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY;
        } else {
            layoutType = WindowManager.LayoutParams.TYPE_PHONE;
        }

        params = new WindowManager.LayoutParams(
                WindowManager.LayoutParams.WRAP_CONTENT,
                WindowManager.LayoutParams.WRAP_CONTENT,
                layoutType,
                WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE
                        | WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS
                        | WindowManager.LayoutParams.FLAG_HARDWARE_ACCELERATED,
                PixelFormat.TRANSLUCENT
        );

        params.gravity = Gravity.TOP | Gravity.START;
        params.x = 40;
        params.y = 140;

        bubbleView.setOnTouchListener(new View.OnTouchListener() {
            @Override
            public boolean onTouch(View v, MotionEvent event) {
                switch (event.getAction()) {
                    case MotionEvent.ACTION_DOWN:
                        initialX = params.x;
                        initialY = params.y;
                        initialTouchX = event.getRawX();
                        initialTouchY = event.getRawY();
                        touchStartTime = System.currentTimeMillis();
                        return true;

                    case MotionEvent.ACTION_MOVE:
                        params.x = initialX + (int) (event.getRawX() - initialTouchX);
                        params.y = initialY + (int) (event.getRawY() - initialTouchY);
                        try {
                            windowManager.updateViewLayout(bubbleView, params);
                        } catch (Exception ignored) {}
                        return true;

                    case MotionEvent.ACTION_UP:
                        long duration = System.currentTimeMillis() - touchStartTime;
                        float diffX = Math.abs(event.getRawX() - initialTouchX);
                        float diffY = Math.abs(event.getRawY() - initialTouchY);
                        if (duration < 250 && diffX < 15 && diffY < 15) {
                            // Click detected: toggle format (detailed X.X vs rounded integer)
                            isDetailed = !isDetailed;
                        }
                        return true;
                }
                return false;
            }
        });

        try {
            windowManager.addView(bubbleView, params);
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    private void registerScreenStateReceiver() {
        mScreenStateReceiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                if (intent == null || intent.getAction() == null) return;
                if (Intent.ACTION_SCREEN_OFF.equals(intent.getAction())) {
                    mIsScreenOn = false;
                    mFpsCalculator.reset();
                    try {
                        Choreographer.getInstance().removeFrameCallback(FpsOverlayService.this);
                    } catch (Exception ignored) {}
                } else if (Intent.ACTION_SCREEN_ON.equals(intent.getAction())) {
                    mIsScreenOn = true;
                    mFpsCalculator.reset();
                    Choreographer.getInstance().postFrameCallback(FpsOverlayService.this);
                }
            }
        };
        IntentFilter filter = new IntentFilter();
        filter.addAction(Intent.ACTION_SCREEN_OFF);
        filter.addAction(Intent.ACTION_SCREEN_ON);
        registerReceiver(mScreenStateReceiver, filter);
    }

    @Override
    public void doFrame(long frameTimeNanos) {
        if (!isRunning || !mIsScreenOn) return;

        // Collect monotonic frame timestamps into calculation window
        mFpsCalculator.addSample(frameTimeNanos);

        Choreographer.getInstance().postFrameCallback(this);
    }

    /**
     * Background execution: Samples kernel hardware node or computes rolling window FPS.
     */
    private void sampleAndRenderFps() {
        if (!isRunning || !mIsScreenOn) return;

        double fps = queryHardwareFps();
        if (fps >= 0.0) {
            // Primary source: verified hardware display driver commit interface
            fps = mFpsCalculator.processHardwareFps(fps);
        } else {
            // Secondary fallback: monotonic frame callback rolling window
            fps = mFpsCalculator.calculateRollingFps(System.nanoTime(), 500_000_000L);
        }

        renderFpsOnMainThread(fps);
    }

    private String getFpsNodePath() {
        if (mCheckedFpsNode && mWorkingFpsNode != null) {
            return mWorkingFpsNode;
        }

        String[] candidates = new String[]{
                "/sys/kernel/aether/fps",
                "/sys/kernel/aether/measured_fps",
                "/sys/class/drm/card0/sde-crtc-0/measured_fps",
                "/sys/class/drm/card0/sde-crtc-1/measured_fps",
                "/sys/devices/platform/soc/ae00000.qcom,mdss_mdp/drm/card0/sde-crtc-0/measured_fps",
                "/sys/devices/platform/soc/ae00000.qcom,mdss_mdp/drm/card0/sde-crtc-1/measured_fps"
        };

        for (String path : candidates) {
            File f = new File(path);
            if (f.exists() && f.canRead()) {
                mWorkingFpsNode = path;
                mCheckedFpsNode = true;
                return path;
            }
        }

        mCheckedFpsNode = true;
        return null;
    }

    private double queryHardwareFps() {
        String path = getFpsNodePath();
        if (path == null) {
            return -1.0;
        }

        try (BufferedReader br = new BufferedReader(new FileReader(path), 64)) {
            String line = br.readLine();
            if (line == null) return -1.0;
            line = line.trim();

            if (line.contains("fps:")) {
                int idx = line.indexOf("fps:");
                String sub = line.substring(idx + 4).trim();
                String[] parts = sub.split("\\s+");
                if (parts.length > 0) {
                    return Double.parseDouble(parts[0]);
                }
            } else if (line.matches("^[0-9]+(\\.[0-9]+)?$")) {
                return Double.parseDouble(line);
            }
        } catch (Exception e) {
            mWorkingFpsNode = null; // Re-evaluate candidate paths if read fails
            mCheckedFpsNode = false;
        }
        return -1.0;
    }

    private void renderFpsOnMainThread(final double fps) {
        final int color;
        if (fps <= 0.5) {
            color = Color.parseColor("#94A3B8"); // Standby 0 FPS (Gray)
        } else if (fps >= (mDisplayRefreshRate - 2.0f)) {
            color = Color.parseColor("#10B981"); // Smooth Full Refresh Rate (Green)
        } else if (fps >= (mDisplayRefreshRate * 0.75f)) {
            color = Color.parseColor("#FBBF24"); // Moderate Load (Amber)
        } else {
            color = Color.parseColor("#00D4FF"); // Active / Dropped Stutter (Cyan)
        }

        final String text;
        if (fps <= 0.5) {
            text = "0 FPS";
        } else if (isDetailed) {
            text = String.format(Locale.US, "%.1f FPS", fps);
        } else {
            text = String.format(Locale.US, "%d FPS", Math.round(fps));
        }

        if (text.equals(lastRenderedText)) {
            return; // Zero redundant layout / WindowManager invalidations
        }
        lastRenderedText = text;

        mMainHandler.post(() -> {
            if (bubbleView != null) {
                bubbleView.setText(text);
                bubbleView.setTextColor(color);
            }
        });
    }

    private void createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            NotificationChannel chan = new NotificationChannel(
                    CHANNEL_ID,
                    "AETHER System Service",
                    NotificationManager.IMPORTANCE_MIN
            );
            chan.setDescription("Hardware frame rate monitor");
            chan.setShowBadge(false);
            chan.setSound(null, null);
            chan.enableVibration(false);
            chan.enableLights(false);
            chan.setLockscreenVisibility(Notification.VISIBILITY_SECRET);
            NotificationManager nm = (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);
            if (nm != null) {
                nm.createNotificationChannel(chan);
            }
        }
    }

    private Notification buildNotification() {
        Notification.Builder builder;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            builder = new Notification.Builder(this, CHANNEL_ID);
        } else {
            builder = new Notification.Builder(this);
        }
        return builder.setContentTitle("AETHER System Service")
                .setContentText("Hardware Frame Pacer Active")
                .setSmallIcon(android.R.drawable.ic_menu_compass)
                .setOngoing(true)
                .setPriority(Notification.PRIORITY_MIN)
                .setVisibility(Notification.VISIBILITY_SECRET)
                .build();
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        if (intent != null && "ACTION_STOP".equals(intent.getAction())) {
            persistState(false);
            stopSelf();
            return START_NOT_STICKY;
        }
        return START_NOT_STICKY;
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        isRunning = false;
        broadcastState(false);
        persistState(false);

        if (mScreenStateReceiver != null) {
            try {
                unregisterReceiver(mScreenStateReceiver);
            } catch (Exception ignored) {}
            mScreenStateReceiver = null;
        }

        try {
            Choreographer.getInstance().removeFrameCallback(this);
        } catch (Exception ignored) {}

        if (mSamplerExecutor != null) {
            mSamplerExecutor.shutdownNow();
            mSamplerExecutor = null;
        }

        if (bubbleView != null && windowManager != null) {
            try {
                windowManager.removeView(bubbleView);
            } catch (Exception ignored) {}
            bubbleView = null;
        }

        try {
            stopForeground(true);
        } catch (Exception ignored) {}
    }

    private void persistState(boolean active) {
        try {
            Context deCtx = createDeviceProtectedStorageContext();
            SharedPreferences prefs = deCtx.getSharedPreferences(
                    "aether_fps_prefs", Context.MODE_PRIVATE);
            prefs.edit().putBoolean("fps_overlay_active", active).apply();
        } catch (Exception ignored) {}
    }

    private void broadcastState(boolean active) {
        try {
            Intent intent = new Intent("com.aether.fpsmeter.STATE_CHANGED");
            intent.putExtra("isRunning", active);
            intent.addFlags(Intent.FLAG_INCLUDE_STOPPED_PACKAGES);
            sendBroadcast(intent);
        } catch (Exception ignored) {}
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }
}
