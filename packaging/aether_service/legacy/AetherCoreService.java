package com.aether.engine.service;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.Service;
import android.app.usage.UsageEvents;
import android.app.usage.UsageStatsManager;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.net.LocalSocket;
import android.net.LocalSocketAddress;
import android.os.BatteryManager;
import android.os.Build;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.util.Log;

import java.io.InputStream;
import java.io.OutputStream;
import java.io.File;
import java.io.BufferedReader;
import java.io.FileReader;
import java.util.HashSet;
import java.util.Set;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class AetherCoreService extends Service {
    private static final String TAG = "AetherService";
    private static final String AETHER_SOCKET_NAME = "aether_ipc";
    private static final String NOTIFICATION_CHANNEL_ID = "aether_core_channel";

    private final ExecutorService executor = Executors.newCachedThreadPool();
    private final Handler mainHandler = new Handler(Looper.getMainLooper());
    private volatile boolean isRunning = false;
    private LocalSocket socket;
    private OutputStream socketOutput;
    private InputStream socketInput;

    private String lastForegroundPkg = "";
    private int currentProfile = -1; // 0=Default, 1=Battery, 2=Balanced, 3=Gaming, 4=Media, 5=Performance (Heavy App)
    private int batteryPct = 100;
    private boolean isFastCharging = false;

    // Dynamic Launcher Package Tracker (Auto-detects ANY launcher without hardcoded names)
    private final Set<String> detectedLaunchers = new HashSet<>();
    private String defaultHomePackage = "";

    // Universal Classification Caches
    private final Map<String, Integer> appProfileCache = new ConcurrentHashMap<>();
    private final Set<String> learnedHeavyApps = new HashSet<>();
    private long lastLearnedFileCheck = 0;

    private boolean isScreenOn = true;

    private final BroadcastReceiver batteryReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (intent == null) return;
            int level = intent.getIntExtra(BatteryManager.EXTRA_LEVEL, -1);
            int scale = intent.getIntExtra(BatteryManager.EXTRA_SCALE, -1);
            if (level >= 0 && scale > 0) {
                batteryPct = (level * 100) / scale;
            }

            int plugged = intent.getIntExtra(BatteryManager.EXTRA_PLUGGED, -1);
            isFastCharging = (plugged == BatteryManager.BATTERY_PLUGGED_AC);
            reevaluateProfile();
        }
    };

    private final BroadcastReceiver screenReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (intent == null) return;
            String action = intent.getAction();
            if (Intent.ACTION_SCREEN_OFF.equals(action)) {
                isScreenOn = false;
                sendProfileCommand("SCREEN_OFF");
                Log.d(TAG, "Screen off: engaging deep sleep mode, stopping monitor timer.");
            } else if (Intent.ACTION_SCREEN_ON.equals(action)) {
                isScreenOn = true;
                sendProfileCommand("SCREEN_ON");
                Log.d(TAG, "Screen on: resuming real-time app monitor.");
                startAppMonitorTimer();
            }
        }
    };

    @Override
    public void onCreate() {
        super.onCreate();
        Log.i(TAG, "AETHER Engine Fully Packed Platform Service initialized.");
        startSilentForeground();
        try {
            registerReceiver(batteryReceiver, new IntentFilter(Intent.ACTION_BATTERY_CHANGED));
        } catch (Exception e) {
            Log.w(TAG, "Battery receiver registration notice: " + e.getMessage());
        }

        try {
            IntentFilter screenFilter = new IntentFilter();
            screenFilter.addAction(Intent.ACTION_SCREEN_OFF);
            screenFilter.addAction(Intent.ACTION_SCREEN_ON);
            if (Build.VERSION.SDK_INT >= 33) {
                registerReceiver(screenReceiver, screenFilter, Context.RECEIVER_NOT_EXPORTED);
            } else {
                registerReceiver(screenReceiver, screenFilter);
            }
        } catch (Exception e) {
            Log.w(TAG, "Screen receiver registration notice: " + e.getMessage());
        }

        isRunning = true;
        executor.execute(this::socketWorkerLoop);
        startAppMonitorTimer();
    }

    private void startSilentForeground() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            try {
                NotificationChannel channel = new NotificationChannel(
                        NOTIFICATION_CHANNEL_ID,
                        "AETHER Platform Management",
                        NotificationManager.IMPORTANCE_MIN
                );
                channel.setDescription("Hardware deadline orchestration active");
                channel.setShowBadge(false);
                channel.enableVibration(false);
                channel.enableLights(false);

                NotificationManager nm = getSystemService(NotificationManager.class);
                if (nm != null) {
                    nm.createNotificationChannel(channel);
                }

                Notification notification = new Notification.Builder(this, NOTIFICATION_CHANNEL_ID)
                        .setContentTitle("AETHER Performance Engine")
                        .setContentText("Hardware deadline orchestration active")
                        .setSmallIcon(android.R.drawable.stat_notify_sync)
                        .setOngoing(true)
                        .build();

                if (Build.VERSION.SDK_INT >= 34) {
                    try {
                        startForeground(1001, notification, android.content.pm.ServiceInfo.FOREGROUND_SERVICE_TYPE_SPECIAL_USE);
                    } catch (Throwable t) {
                        startForeground(1001, notification);
                    }
                } else {
                    startForeground(1001, notification);
                }
            } catch (Exception e) {
                Log.w(TAG, "startSilentForeground caught non-fatal exception: " + e.getMessage());
            }
        }
    }

    /**
     * Dynamically queries PackageManager for ANY installed or active Home/Launcher package.
     * Works on Infinity X ROM, Trebuchet, Lawnchair, Nova, Pixel Launcher, MIUI, etc.
     */
    private void updateAutoDetectedLaunchers() {
        try {
            PackageManager pm = getPackageManager();
            Intent homeIntent = new Intent(Intent.ACTION_MAIN);
            homeIntent.addCategory(Intent.CATEGORY_HOME);

            // 1. Discover current default launcher
            ResolveInfo defaultResolve = pm.resolveActivity(homeIntent, PackageManager.MATCH_DEFAULT_ONLY);
            if (defaultResolve != null && defaultResolve.activityInfo != null) {
                defaultHomePackage = defaultResolve.activityInfo.packageName;
                detectedLaunchers.add(defaultHomePackage);
                Log.i(TAG, "Auto-detected Active Default Launcher: " + defaultHomePackage);
            }

            // 2. Discover all available launchers on the device
            for (ResolveInfo ri : pm.queryIntentActivities(homeIntent, 0)) {
                if (ri.activityInfo != null && ri.activityInfo.packageName != null) {
                    detectedLaunchers.add(ri.activityInfo.packageName);
                }
            }
        } catch (Exception e) {
            Log.w(TAG, "Failed to resolve launcher packages: " + e.getMessage());
        }
    }

    private boolean isLauncher(String pkg) {
        if (pkg == null || pkg.isEmpty()) return false;
        if (pkg.equals(defaultHomePackage) || detectedLaunchers.contains(pkg)) {
            return true;
        }
        String lower = pkg.toLowerCase();
        return lower.contains("launcher") || lower.contains("quickstep") ||
               lower.contains("nexuslauncher") || lower.contains("trebuchet") ||
               lower.contains("infinity") || lower.contains("lawnchair") ||
               lower.contains("novalauncher") || lower.contains("miui.home");
    }

    /**
     * Periodically reloads the persistent learned heavy apps database.
     * Synchronized with in-kernel stutter learning from aetherd.
     */
    private void reloadLearnedAppsFile() {
        File file = new File("/data/adb/aether/learned_apps.txt");
        if (!file.exists() || !file.canRead()) return;
        try (BufferedReader reader = new BufferedReader(new FileReader(file))) {
            String line;
            synchronized (learnedHeavyApps) {
                while ((line = reader.readLine()) != null) {
                    line = line.trim().toLowerCase();
                    if (!line.isEmpty()) {
                        learnedHeavyApps.add(line);
                    }
                }
            }
        } catch (Exception ignored) {}
    }

    private boolean isLearnedHeavyApp(String pkg) {
        if (pkg == null || pkg.isEmpty()) return false;
        long now = System.currentTimeMillis();
        if (now - lastLearnedFileCheck > 4000) {
            lastLearnedFileCheck = now;
            reloadLearnedAppsFile();
        }
        String lower = pkg.toLowerCase();
        synchronized (learnedHeavyApps) {
            for (String app : learnedHeavyApps) {
                if (lower.contains(app)) return true;
            }
        }
        return false;
    }

    /**
     * 100% Universal Game Detection:
     * 1. Official Android framework category metadata (ApplicationInfo.CATEGORY_GAME).
     * 2. Android manifest flag (ApplicationInfo.FLAG_IS_GAME).
     * 3. Native Game Engine & Runtime inspection in nativeLibraryDir
     *    (Unity, Unreal Engine 4/5, Godot, Cocos2d, Il2Cpp, Emulators, GGPerks targets).
     * 4. Gaming platform and game enhancer keywords fallback.
     */
    private boolean isGameUniversally(String pkg) {
        if (pkg == null || pkg.isEmpty()) return false;
        try {
            PackageManager pm = getPackageManager();
            ApplicationInfo appInfo = pm.getApplicationInfo(pkg, 0);

            // 1. Android Framework Category (Play Store official metadata)
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                if (appInfo.category == ApplicationInfo.CATEGORY_GAME) {
                    return true;
                }
            }
            if ((appInfo.flags & ApplicationInfo.FLAG_IS_GAME) != 0) {
                return true;
            }

            // 2. Native Game Engine Inspection (GGPerks, Game Turbo, sideloaded, indie games)
            if (appInfo.nativeLibraryDir != null) {
                File libDir = new File(appInfo.nativeLibraryDir);
                if (libDir.exists() && libDir.isDirectory()) {
                    File[] libs = libDir.listFiles();
                    if (libs != null) {
                        for (File lib : libs) {
                            String name = lib.getName().toLowerCase();
                            if (name.contains("unity") || name.contains("ue4") || name.contains("unreal") ||
                                name.contains("godot") || name.contains("cocos") || name.contains("il2cpp") ||
                                name.contains("main.so") || name.contains("cri_ware") ||
                                name.contains("retroarch") || name.contains("ppsspp") || name.contains("dolphin") ||
                                name.contains("citra") || name.contains("yuzu") || name.contains("duckstation") ||
                                name.contains("winlator") || name.contains("box64")) {
                                return true;
                            }
                        }
                    }
                }
            }
        } catch (Exception ignored) {}

        // 3. Fallback name patterns for games, game boosters, and emulators
        String lower = pkg.toLowerCase();
        return lower.contains("game") || lower.contains("gaming") ||
               lower.contains("ggperks") || lower.contains("gameturbo") ||
               lower.contains("emulator") || lower.contains("genshin") ||
               lower.contains("pubg") || lower.contains("bgmi") ||
               lower.contains("freefire") || lower.contains("cod") ||
               lower.contains("roblox") || lower.contains("minecraft");
    }

    private boolean isMediaUniversally(String pkg) {
        if (pkg == null || pkg.isEmpty()) return false;
        try {
            PackageManager pm = getPackageManager();
            ApplicationInfo appInfo = pm.getApplicationInfo(pkg, 0);
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                if (appInfo.category == ApplicationInfo.CATEGORY_AUDIO ||
                    appInfo.category == ApplicationInfo.CATEGORY_VIDEO) {
                    return true;
                }
            }
        } catch (Exception ignored) {}

        String lower = pkg.toLowerCase();
        return lower.contains("youtube") || lower.contains("netflix") || lower.contains("primevideo") ||
               lower.contains("disney") || lower.contains("hotstar") || lower.contains("hulu") ||
               lower.contains("twitch") || lower.contains("spotify") || lower.contains("videolan") ||
               lower.contains("vlc") || lower.contains("mxplayer") || lower.contains("mxtech") ||
               lower.contains("apple.android.music") || lower.contains("deezer") || lower.contains("soundcloud") ||
               lower.contains("jiosaavn") || lower.contains("gaana") || lower.contains("wynk") ||
               lower.contains("bilibili") || lower.contains("crunchyroll") || lower.contains("audible");
    }

    private boolean isHeavyDynamicApp(String pkg) {
        if (pkg == null || pkg.isEmpty()) return false;
        if (isLearnedHeavyApp(pkg)) return true;

        String lower = pkg.toLowerCase();
        return lower.contains("amazon") || lower.contains("flipkart") || lower.contains("myntra") ||
               lower.contains("aliexpress") || lower.contains("ebay") || lower.contains("walmart") ||
               lower.contains("target") || lower.contains("shopee") || lower.contains("lazada") ||
               lower.contains("shein") || lower.contains("temu") || lower.contains("meesho") ||
               lower.contains("ajio") || lower.contains("tata_neu") || lower.contains("zepto") ||
               lower.contains("blinkit") || lower.contains("instamart") || lower.contains("swiggy") ||
               lower.contains("zomato") || lower.contains("ubereats") || lower.contains("shopping");
    }

    private boolean isProductivityApp(String pkg) {
        if (pkg == null || pkg.isEmpty()) return false;
        try {
            PackageManager pm = getPackageManager();
            ApplicationInfo appInfo = pm.getApplicationInfo(pkg, 0);
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                if (appInfo.category == ApplicationInfo.CATEGORY_PRODUCTIVITY) {
                    return true;
                }
            }
        } catch (Exception ignored) {}

        String lower = pkg.toLowerCase();
        return lower.contains("kindle") || lower.contains("reader") || lower.contains("pdf") ||
               lower.contains("books") || lower.contains("notes") || lower.contains("keep") ||
               lower.contains("notion") || lower.contains("evernote") || lower.contains("obsidian") ||
               lower.contains("docs") || lower.contains("sheets") || lower.contains("office") ||
               lower.contains("adobe") || lower.contains("wps");
    }

    /**
     * Universal App Classifier:
     * Classifies ANY app from Google Play Store or third-party stores into the optimal profile.
     * Profile 1: BATTERY (Reading, productivity)
     * Profile 2: BALANCED (Launchers, social media, general browsing)
     * Profile 3: GAMING (100% universal 3D engine detection)
     * Profile 4: MEDIA (Slack-time energy harvesting)
     * Profile 5: PERFORMANCE (Intelligent Heavy App mode capped at 1.70 GHz sweet spot, zero frequency blasting!)
     */
    private int classifyAppUniversally(String pkg) {
        if (pkg == null || pkg.isEmpty()) return 2;

        if (isLearnedHeavyApp(pkg)) {
            return 5;
        }

        Integer cached = appProfileCache.get(pkg);
        if (cached != null) return cached;

        int profile;
        if (isLauncher(pkg)) {
            profile = 2;
        } else if (isHeavyDynamicApp(pkg)) {
            profile = 5;
        } else if (isGameUniversally(pkg)) {
            profile = 3;
        } else if (isMediaUniversally(pkg)) {
            profile = 4;
        } else if (isProductivityApp(pkg)) {
            profile = 1;
        } else {
            profile = 2;
        }

        appProfileCache.put(pkg, profile);
        return profile;
    }

    private void socketWorkerLoop() {
        int backoffMs = 1000;
        while (isRunning) {
            try {
                if (socket == null || !socket.isConnected()) {
                    Log.i(TAG, "Connecting to aetherd daemon (@" + AETHER_SOCKET_NAME + ")");
                    socket = new LocalSocket();
                    LocalSocketAddress address = new LocalSocketAddress(
                            AETHER_SOCKET_NAME,
                            LocalSocketAddress.Namespace.ABSTRACT
                    );
                    socket.connect(address);
                    socketOutput = socket.getOutputStream();
                    socketInput = socket.getInputStream();
                    Log.i(TAG, "Connected to aetherd daemon successfully.");
                    backoffMs = 1000; // Reset backoff upon successful connection

                    sendProfileCommand("SET_PROFILE:BALANCED");
                }

                byte[] buffer = new byte[128];
                int readBytes = socketInput.read(buffer);
                if (readBytes < 0) {
                    throw new Exception("Daemon closed socket connection.");
                }
            } catch (Exception e) {
                closeSocket();
                try {
                    Thread.sleep(backoffMs);
                } catch (InterruptedException ignored) {}
                backoffMs = Math.min(backoffMs * 2, 8000); // Bounded backoff up to 8s
            }
        }
    }

    private void sendProfileCommand(final String command) {
        executor.execute(() -> {
            synchronized (this) {
                if (socketOutput != null && socket != null && socket.isConnected()) {
                    try {
                        socketOutput.write((command + "\n").getBytes());
                        socketOutput.flush();
                        Log.d(TAG, "Dispatched profile: " + command);
                    } catch (Exception e) {
                        Log.w(TAG, "Socket write error: " + e.getMessage());
                        closeSocket();
                    }
                }
            }
        });
    }

    private void closeSocket() {
        try {
            if (socketOutput != null) socketOutput.close();
            if (socketInput != null) socketInput.close();
            if (socket != null) socket.close();
        } catch (Exception ignored) {}
        socketOutput = null;
        socketInput = null;
        socket = null;
    }

    private void startAppMonitorTimer() {
        mainHandler.postDelayed(new Runnable() {
            @Override
            public void run() {
                if (isRunning && isScreenOn) {
                    checkForegroundPackage();
                    mainHandler.postDelayed(this, 750);
                }
            }
        }, 750);
    }

    private void checkForegroundPackage() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP_MR1) {
            UsageStatsManager usm = (UsageStatsManager) getSystemService(Context.USAGE_STATS_SERVICE);
            if (usm == null) return;

            long time = System.currentTimeMillis();
            UsageEvents events = usm.queryEvents(time - 2500, time);
            UsageEvents.Event event = new UsageEvents.Event();
            String currentPkg = "";

            while (events.hasNextEvent()) {
                events.getNextEvent(event);
                if (event.getEventType() == UsageEvents.Event.MOVE_TO_FOREGROUND) {
                    currentPkg = event.getPackageName();
                }
            }

            if (!currentPkg.isEmpty() && !currentPkg.equals(lastForegroundPkg)) {
                lastForegroundPkg = currentPkg;
                if (isLauncher(currentPkg)) {
                    sendProfileCommand("PULSE_GESTURE");
                } else {
                    sendProfileCommand("PULSE_LAUNCH");
                }
                reevaluateProfile();
            }
        }
    }

    private void reevaluateProfile() {
        // Emergency battery protection: low battery drops aggressive boosting
        if (batteryPct <= 15 && !isFastCharging) {
            if (currentProfile != 1) {
                currentProfile = 1;
                sendProfileCommand("SET_PROFILE:BATTERY");
            }
            return;
        }

        // Dynamic Launcher & Gesture Auto-Detection
        if (isLauncher(lastForegroundPkg)) {
            sendProfileCommand("PULSE_GESTURE");
            if (currentProfile != 2) {
                currentProfile = 2;
                sendProfileCommand("SET_PROFILE:BALANCED");
            }
            return;
        }

        // Universal Classification across all Play Store & third-party apps
        int targetProfile = classifyAppUniversally(lastForegroundPkg);
        if (targetProfile != currentProfile) {
            currentProfile = targetProfile;
            switch (targetProfile) {
                case 1:
                    sendProfileCommand("SET_PROFILE:BATTERY");
                    break;
                case 3:
                    sendProfileCommand("SET_PROFILE:GAMING");
                    break;
                case 4:
                    sendProfileCommand("SET_PROFILE:MEDIA");
                    break;
                case 5:
                    sendProfileCommand("SET_PROFILE:PERFORMANCE");
                    break;
                case 2:
                default:
                    sendProfileCommand("SET_PROFILE:BALANCED");
                    break;
            }
        }
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        return START_STICKY;
    }

    @Override
    public void onDestroy() {
        isRunning = false;
        try { unregisterReceiver(batteryReceiver); } catch (Exception ignored) {}
        try { unregisterReceiver(screenReceiver); } catch (Exception ignored) {}
        executor.shutdownNow();
        closeSocket();
        super.onDestroy();
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }
}
