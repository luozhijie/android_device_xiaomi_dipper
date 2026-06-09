/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.dipper.antiflicker;

import android.app.Service;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.database.ContentObserver;
import android.hardware.display.BrightnessInfo;
import android.net.Uri;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.IBinder;
import android.os.Parcel;
import android.os.PowerManager;
import android.os.RemoteException;
import android.os.ServiceManager;
import android.os.SystemClock;
import android.os.UserHandle;
import android.provider.Settings;
import android.util.Log;
import android.util.MathUtils;
import android.view.Display;

import lineageos.providers.LineageSettings;

import java.util.Arrays;

public final class AntiFlickerColorService extends Service {

    private static final String TAG = "AntiFlickerColorService";

    private static final int SCREEN_BRIGHTNESS_DC_THRESHOLD = 40;
    private static final float MIN_COLOR_SCALE = 0.05f;
    private static final long FAST_POLL_INTERVAL_MS = 10;
    private static final long IDLE_POLL_INTERVAL_MS = 500;
    private static final long IDLE_TIMEOUT_MS = 2000;
    private static final int SURFACE_FLINGER_TRANSACTION_COLOR_MATRIX = 1015;

    private static final Uri DISPLAY_ANTI_FLICKER =
            LineageSettings.System.getUriFor(LineageSettings.System.DISPLAY_ANTI_FLICKER);
    private static final Uri DISPLAY_COLOR_ADJUSTMENT =
            LineageSettings.System.getUriFor(LineageSettings.System.DISPLAY_COLOR_ADJUSTMENT);
    private static final Uri SCREEN_BRIGHTNESS =
            Settings.System.getUriFor(Settings.System.SCREEN_BRIGHTNESS);

    private final int[] mLastRgb = new int[] { -1, -1, -1 };

    private HandlerThread mHandlerThread;
    private Handler mHandler;
    private IBinder mFlinger;
    private ContentObserver mSettingsObserver;
    private boolean mPolling;
    private boolean mScreenOn;
    private int mLastBrightness = -1;
    private long mLastBrightnessChangeTime;

    private final Runnable mPollRunnable = new Runnable() {
        @Override
        public void run() {
            if (!mPolling) {
                return;
            }
            final long delay = updateColorScale();
            mHandler.postDelayed(this, delay);
        }
    };

    private final BroadcastReceiver mScreenReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            final String action = intent.getAction();
            if (!Intent.ACTION_SCREEN_ON.equals(action) &&
                    !Intent.ACTION_SCREEN_OFF.equals(action)) {
                return;
            }
            mHandler.post(() -> {
                mScreenOn = Intent.ACTION_SCREEN_ON.equals(action);
                updateState();
            });
        }
    };

    public static void startService(Context context) {
        context.startServiceAsUser(new Intent(context, AntiFlickerColorService.class),
                UserHandle.CURRENT);
    }

    @Override
    public void onCreate() {
        super.onCreate();

        mHandlerThread = new HandlerThread(TAG);
        mHandlerThread.start();
        mHandler = new Handler(mHandlerThread.getLooper());

        mFlinger = ServiceManager.getService("SurfaceFlinger");

        final PowerManager powerManager = getSystemService(PowerManager.class);
        mScreenOn = powerManager != null && powerManager.isInteractive();

        mSettingsObserver = new ContentObserver(mHandler) {
            @Override
            public void onChange(boolean selfChange, Uri uri) {
                updateState();
            }
        };
        getContentResolver().registerContentObserver(DISPLAY_ANTI_FLICKER, false,
                mSettingsObserver, UserHandle.USER_ALL);
        getContentResolver().registerContentObserver(DISPLAY_COLOR_ADJUSTMENT, false,
                mSettingsObserver, UserHandle.USER_ALL);
        getContentResolver().registerContentObserver(SCREEN_BRIGHTNESS, false,
                mSettingsObserver, UserHandle.USER_ALL);

        final IntentFilter filter = new IntentFilter();
        filter.addAction(Intent.ACTION_SCREEN_ON);
        filter.addAction(Intent.ACTION_SCREEN_OFF);
        registerReceiver(mScreenReceiver, filter);

        mHandler.post(this::updateState);
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        return START_STICKY;
    }

    @Override
    public void onDestroy() {
        stopPolling();
        applyColorScale(-1, 1.0f);
        unregisterReceiver(mScreenReceiver);
        getContentResolver().unregisterContentObserver(mSettingsObserver);
        mHandlerThread.quitSafely();
        super.onDestroy();
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    private void updateState() {
        if (mFlinger == null) {
            mFlinger = ServiceManager.getService("SurfaceFlinger");
        }
        if (mFlinger == null) {
            stopPolling();
            return;
        }

        if (isAntiFlickerEnabled() && mScreenOn) {
            startPolling();
        } else {
            stopPolling();
            applyColorScale(-1, 1.0f);
        }
    }

    private boolean isAntiFlickerEnabled() {
        return LineageSettings.System.getIntForUser(getContentResolver(),
                LineageSettings.System.DISPLAY_ANTI_FLICKER, 0, UserHandle.USER_CURRENT) == 1;
    }

    private void startPolling() {
        if (mPolling) {
            updateColorScale();
            return;
        }

        mPolling = true;
        mLastBrightness = -1;
        mLastBrightnessChangeTime = SystemClock.uptimeMillis();
        mLastRgb[0] = mLastRgb[1] = mLastRgb[2] = -1;
        mHandler.post(mPollRunnable);
    }

    private void stopPolling() {
        if (!mPolling) {
            return;
        }

        mPolling = false;
        mHandler.removeCallbacks(mPollRunnable);
    }

    private long updateColorScale() {
        final int brightness = readCurrentBrightness();
        final long now = SystemClock.uptimeMillis();
        if (brightness != mLastBrightness) {
            mLastBrightness = brightness;
            mLastBrightnessChangeTime = now;
        }

        float scale = 1.0f;
        if (brightness >= 0 && brightness < SCREEN_BRIGHTNESS_DC_THRESHOLD) {
            final float brightnessRatio = (float) brightness / SCREEN_BRIGHTNESS_DC_THRESHOLD;
            scale = MIN_COLOR_SCALE + (1.0f - MIN_COLOR_SCALE) * brightnessRatio;
        }

        applyColorScale(brightness, scale);
        return now - mLastBrightnessChangeTime < IDLE_TIMEOUT_MS
                ? FAST_POLL_INTERVAL_MS : IDLE_POLL_INTERVAL_MS;
    }

    private void applyColorScale(int brightness, float scale) {
        if (mFlinger == null) {
            return;
        }

        final float[] baseRgb = getBaseColorAdjustment();
        final float[] colorMatrix = new float[] {
                baseRgb[0] * scale, 0.0f, 0.0f, 0.0f,
                0.0f, baseRgb[1] * scale, 0.0f, 0.0f,
                0.0f, 0.0f, baseRgb[2] * scale, 0.0f,
                0.0f, 0.0f, 0.0f, 1.0f,
        };
        final int[] rgb = new int[] {
                toCalibrationInt(colorMatrix[0]),
                toCalibrationInt(colorMatrix[5]),
                toCalibrationInt(colorMatrix[10]),
        };

        if (Arrays.equals(rgb, mLastRgb)) {
            return;
        }

        if (setColorMatrix(colorMatrix)) {
            mLastRgb[0] = rgb[0];
            mLastRgb[1] = rgb[1];
            mLastRgb[2] = rgb[2];
            Log.i(TAG, "Applied color dimming, brightness=" + brightness + ", scale=" + scale
                    + ", rgb=" + rgb[0] + " " + rgb[1] + " " + rgb[2]);
        } else {
            Log.e(TAG, "Failed to apply color dimming, scale=" + scale);
        }
    }

    private boolean setColorMatrix(float[] matrix) {
        final Parcel data = Parcel.obtain();
        try {
            data.writeInterfaceToken("android.ui.ISurfaceComposer");
            data.writeInt(1);
            for (float value : matrix) {
                data.writeFloat(value);
            }
            mFlinger.transact(SURFACE_FLINGER_TRANSACTION_COLOR_MATRIX, data, null, 0);
            return true;
        } catch (RemoteException e) {
            Log.e(TAG, "Failed to set SurfaceFlinger color matrix", e);
            return false;
        } finally {
            data.recycle();
        }
    }

    private int toCalibrationInt(float value) {
        return MathUtils.constrain(Math.round(value * 255.0f), 0, 255);
    }

    private float[] getBaseColorAdjustment() {
        final String value = LineageSettings.System.getStringForUser(getContentResolver(),
                LineageSettings.System.DISPLAY_COLOR_ADJUSTMENT, UserHandle.USER_CURRENT);
        final float[] rgb = new float[] { 1.0f, 1.0f, 1.0f };
        final String[] parts = value == null ? null : value.split(" ");
        if (parts == null || parts.length != 3) {
            return rgb;
        }

        try {
            rgb[0] = MathUtils.constrain(Float.parseFloat(parts[0]), 0.0f, 1.0f);
            rgb[1] = MathUtils.constrain(Float.parseFloat(parts[1]), 0.0f, 1.0f);
            rgb[2] = MathUtils.constrain(Float.parseFloat(parts[2]), 0.0f, 1.0f);
        } catch (NumberFormatException e) {
            Log.e(TAG, "Invalid color adjustment: " + value, e);
        }
        return rgb;
    }

    private int readCurrentBrightness() {
        final Display display = getDisplay();
        final BrightnessInfo brightnessInfo = display != null ? display.getBrightnessInfo() : null;
        if (brightnessInfo != null) {
            return MathUtils.constrain(Math.round(brightnessInfo.adjustedBrightness * 255.0f),
                    0, 255);
        }

        return Settings.System.getIntForUser(getContentResolver(), Settings.System.SCREEN_BRIGHTNESS,
                -1, UserHandle.USER_CURRENT);
    }
}
