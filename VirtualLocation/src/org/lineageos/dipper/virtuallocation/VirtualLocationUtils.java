/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.dipper.virtuallocation;

import android.os.SystemProperties;

public final class VirtualLocationUtils {

    static final String KEY_ENABLED = "virtual_location_enabled";
    static final String KEY_LATITUDE = "virtual_location_latitude";
    static final String KEY_LONGITUDE = "virtual_location_longitude";
    static final String KEY_ACCURACY = "virtual_location_accuracy";
    static final String KEY_ALTITUDE = "virtual_location_altitude";
    static final String KEY_SPEED = "virtual_location_speed";
    static final String KEY_BEARING = "virtual_location_bearing";
    static final String KEY_INTERVAL = "virtual_location_interval";

    static final String PROP_ENABLED = "persist.sys.virtual_gnss.enabled";
    static final String PROP_LATITUDE = "persist.sys.virtual_gnss.latitude";
    static final String PROP_LONGITUDE = "persist.sys.virtual_gnss.longitude";
    static final String PROP_ACCURACY = "persist.sys.virtual_gnss.accuracy";
    static final String PROP_ALTITUDE = "persist.sys.virtual_gnss.altitude";
    static final String PROP_SPEED = "persist.sys.virtual_gnss.speed";
    static final String PROP_BEARING = "persist.sys.virtual_gnss.bearing";
    static final String PROP_INTERVAL_MS = "persist.sys.virtual_gnss.interval_ms";

    static final String DEFAULT_LATITUDE = "39.908722";
    static final String DEFAULT_LONGITUDE = "116.397499";
    static final String DEFAULT_ACCURACY = "5.0";
    static final String DEFAULT_ALTITUDE = "50.0";
    static final String DEFAULT_SPEED = "0.0";
    static final String DEFAULT_BEARING = "0.0";
    static final String DEFAULT_INTERVAL_MS = "1000";

    private VirtualLocationUtils() {
    }

    static String get(String property, String defaultValue) {
        return SystemProperties.get(property, defaultValue);
    }

    static boolean getBoolean(String property, boolean defaultValue) {
        return SystemProperties.getBoolean(property, defaultValue);
    }

    static void set(String property, String value) {
        SystemProperties.set(property, value);
    }
}
