/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.dipper.virtuallocation;

import android.app.ActionBar;
import android.os.Bundle;
import android.text.InputType;
import android.preference.EditTextPreference;
import android.preference.PreferenceActivity;
import android.preference.SwitchPreference;
import android.view.MenuItem;
import android.widget.Toast;

public class VirtualLocationActivity extends PreferenceActivity {

    private interface Validator {
        boolean isValid(String value);
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setTitle(R.string.app_name);
        addPreferencesFromResource(R.xml.virtual_location_settings);

        ActionBar actionBar = getActionBar();
        if (actionBar != null) {
            actionBar.setDisplayHomeAsUpEnabled(true);
        }

        bindSwitch(VirtualLocationUtils.KEY_ENABLED, VirtualLocationUtils.PROP_ENABLED);
        bindText(VirtualLocationUtils.KEY_LATITUDE, VirtualLocationUtils.PROP_LATITUDE,
                VirtualLocationUtils.DEFAULT_LATITUDE, true,
                value -> isDoubleInRange(value, -90.0, 90.0));
        bindText(VirtualLocationUtils.KEY_LONGITUDE, VirtualLocationUtils.PROP_LONGITUDE,
                VirtualLocationUtils.DEFAULT_LONGITUDE, true,
                value -> isDoubleInRange(value, -180.0, 180.0));
        bindText(VirtualLocationUtils.KEY_ACCURACY, VirtualLocationUtils.PROP_ACCURACY,
                VirtualLocationUtils.DEFAULT_ACCURACY, false,
                value -> isDoubleInRange(value, 0.1, 10000.0));
        bindText(VirtualLocationUtils.KEY_ALTITUDE, VirtualLocationUtils.PROP_ALTITUDE,
                VirtualLocationUtils.DEFAULT_ALTITUDE, true,
                value -> isDoubleInRange(value, -1000.0, 10000.0));
        bindText(VirtualLocationUtils.KEY_SPEED, VirtualLocationUtils.PROP_SPEED,
                VirtualLocationUtils.DEFAULT_SPEED, false,
                value -> isDoubleInRange(value, 0.0, 1000.0));
        bindText(VirtualLocationUtils.KEY_BEARING, VirtualLocationUtils.PROP_BEARING,
                VirtualLocationUtils.DEFAULT_BEARING, false,
                value -> isDoubleInRange(value, 0.0, 360.0));
        bindText(VirtualLocationUtils.KEY_INTERVAL,
                VirtualLocationUtils.PROP_INTERVAL_MS, VirtualLocationUtils.DEFAULT_INTERVAL_MS,
                false, value -> isIntegerInRange(value, 200, 60000));
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() == android.R.id.home) {
            finish();
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    private void bindSwitch(String key, String property) {
        SwitchPreference preference = (SwitchPreference) findPreference(key);
        preference.setPersistent(false);
        preference.setChecked(VirtualLocationUtils.getBoolean(property, false));
        preference.setOnPreferenceChangeListener((pref, newValue) -> {
            VirtualLocationUtils.set(property, Boolean.TRUE.equals(newValue) ? "true" : "false");
            return true;
        });
    }

    private void bindText(String key, String property, String defaultValue, boolean signed,
            Validator validator) {
        EditTextPreference preference = (EditTextPreference) findPreference(key);
        preference.setPersistent(false);
        preference.getEditText().setInputType(InputType.TYPE_CLASS_NUMBER
                | InputType.TYPE_NUMBER_FLAG_DECIMAL
                | (signed ? InputType.TYPE_NUMBER_FLAG_SIGNED : 0));
        preference.setText(VirtualLocationUtils.get(property, defaultValue));
        updateSummary(preference);
        preference.setOnPreferenceChangeListener((pref, newValue) -> {
            String value = String.valueOf(newValue).trim();
            if (!validator.isValid(value)) {
                Toast.makeText(this, R.string.virtual_location_invalid_value,
                        Toast.LENGTH_SHORT).show();
                return false;
            }
            VirtualLocationUtils.set(property, value);
            preference.setText(value);
            updateSummary(preference);
            return false;
        });
    }

    private static void updateSummary(EditTextPreference preference) {
        preference.setSummary(preference.getText());
    }

    private static boolean isDoubleInRange(String value, double min, double max) {
        try {
            double parsed = Double.parseDouble(value);
            return parsed >= min && parsed <= max;
        } catch (NumberFormatException e) {
            return false;
        }
    }

    private static boolean isIntegerInRange(String value, int min, int max) {
        try {
            int parsed = Integer.parseInt(value);
            return parsed >= min && parsed <= max;
        } catch (NumberFormatException e) {
            return false;
        }
    }
}
