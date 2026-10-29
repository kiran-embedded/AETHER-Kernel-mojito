package com.aether.fpsmeter;

/**
 * AETHER Engine - Validated Rolling-Window Frame Rate Calculation Engine
 * Upstream References & Principles:
 *  - AOSP SurfaceFlinger & FrameTimeline monotonic frame delta model
 *  - Formula: FPS = n / ((t_n - t_0) / 10^9) = (n * 10^9) / (t_n - t_0)
 *  - Strict monotonic ordering: rejects t_n <= t_{n-1}
 *  - Zero object allocation in sampling loop (fixed circular primitive buffer)
 *  - Separation of raw measured instantaneous FPS and smoothed presentation FPS
 *  - Stutter-preserving: large frame drops immediately snap without smoothing lag
 *  - No artificial clamping to display refresh rate
 */
public final class FpsCalculator {

    private static final int BUFFER_CAPACITY = 64;
    private static final long MAX_FRAME_GAP_NS = 1_000_000_000L; // 1.0 second gap = screen idle/paused
    private static final long IDLE_THRESHOLD_NS = 1_000_000_000L; // 1.0s without frame = 0.0 FPS

    private final long[] mTimestamps = new long[BUFFER_CAPACITY];
    private int mHead = 0;
    private int mCount = 0;
    private long mLastTimestampNs = 0;

    private double mInstantFps = 0.0;
    private double mSmoothedFps = 0.0;

    public synchronized void reset() {
        mHead = 0;
        mCount = 0;
        mLastTimestampNs = 0;
        mInstantFps = 0.0;
        mSmoothedFps = 0.0;
    }

    /**
     * Add a validated monotonic frame timestamp in nanoseconds.
     * Rejects duplicate or out-of-order timestamps.
     */
    public synchronized boolean addSample(long timestampNs) {
        if (timestampNs <= 0) {
            return false;
        }

        // 1. Monotonic ordering verification: reject non-monotonic or duplicate samples
        if (mLastTimestampNs > 0 && timestampNs <= mLastTimestampNs) {
            return false;
        }

        // 2. Frame gap handling: if delta > 1.0s, user paused or screen was idle.
        // Reset window to prevent long gaps from skewing rolling average.
        if (mLastTimestampNs > 0 && (timestampNs - mLastTimestampNs) > MAX_FRAME_GAP_NS) {
            mHead = 0;
            mCount = 0;
        }

        mTimestamps[mHead] = timestampNs;
        mHead = (mHead + 1) % BUFFER_CAPACITY;
        if (mCount < BUFFER_CAPACITY) {
            mCount++;
        }
        mLastTimestampNs = timestampNs;
        return true;
    }

    /**
     * Compute rolling FPS over the bounded time window:
     * FPS = n / ((t_n - t_0) / 10^9)
     *
     * @param currentNs Current monotonic time from System.nanoTime()
     * @param windowDurationNs Target rolling window length (e.g., 500_000_000L = 500ms)
     * @return Calculated FPS (0.0 if idle or insufficient samples)
     */
    public synchronized double calculateRollingFps(long currentNs, long windowDurationNs) {
        // Check for idle display: if last sample was > 1.0s ago, screen is static
        if (mCount < 2 || mLastTimestampNs == 0 || (currentNs - mLastTimestampNs) > IDLE_THRESHOLD_NS) {
            mInstantFps = 0.0;
            mSmoothedFps = 0.0;
            return 0.0;
        }

        long newestTime = mLastTimestampNs;
        long oldestTime = newestTime;
        int validIntervals = 0;

        // Traverse backwards from newest to find samples within windowDurationNs
        for (int i = 1; i < mCount; i++) {
            int idx = (mHead - 1 - i + BUFFER_CAPACITY) % BUFFER_CAPACITY;
            long t = mTimestamps[idx];
            if (t <= 0) break;

            long elapsed = newestTime - t;
            if (elapsed > windowDurationNs) {
                // Include this boundary sample to span the full window
                oldestTime = t;
                validIntervals++;
                break;
            }
            oldestTime = t;
            validIntervals++;
        }

        if (validIntervals < 1) {
            return mSmoothedFps;
        }

        long totalElapsedNs = newestTime - oldestTime;
        if (totalElapsedNs <= 0) {
            return mSmoothedFps;
        }

        // Core Mathematical Model: FPS = n / ((t_n - t_0) / 10^9)
        double fps = ((double) validIntervals * 1_000_000_000.0) / (double) totalElapsedNs;
        mInstantFps = fps;

        // Adaptive smoothing:
        // If there is a sharp frame drop (|delta| > 4.0 FPS) or frame rate is near zero,
        // snap immediately to raw measured value so real stutters are NEVER masked!
        double delta = Math.abs(fps - mSmoothedFps);
        if (delta > 4.0 || fps < 1.0 || mSmoothedFps < 1.0) {
            mSmoothedFps = fps;
        } else {
            // Light low-pass filter (alpha = 0.55) during steady frame rates
            mSmoothedFps = (0.55 * fps) + (0.45 * mSmoothedFps);
        }

        return mSmoothedFps;
    }

    /**
     * Feed an external hardware-measured FPS sample (from display driver commit interface).
     */
    public synchronized double processHardwareFps(double rawHwFps) {
        if (rawHwFps <= 0.2) {
            mInstantFps = 0.0;
            mSmoothedFps = 0.0;
            return 0.0;
        }

        mInstantFps = rawHwFps;
        double delta = Math.abs(rawHwFps - mSmoothedFps);
        if (delta > 3.5 || rawHwFps < 2.0 || mSmoothedFps < 2.0) {
            mSmoothedFps = rawHwFps;
        } else {
            mSmoothedFps = (0.60 * rawHwFps) + (0.40 * mSmoothedFps);
        }
        return mSmoothedFps;
    }

    public synchronized double getInstantFps() {
        return mInstantFps;
    }

    public synchronized double getSmoothedFps() {
        return mSmoothedFps;
    }
}
