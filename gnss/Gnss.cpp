/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "DipperVirtualGnss"

#include "Gnss.h"

#include <android-base/parsedouble.h>
#include <android-base/properties.h>
#include <log/log.h>
#include <utils/SystemClock.h>

#include <algorithm>
#include <chrono>

namespace android {
namespace hardware {
namespace gnss {
namespace V2_0 {
namespace implementation {

namespace {

constexpr char kPropEnabled[] = "persist.sys.virtual_gnss.enabled";
constexpr char kPropLatitude[] = "persist.sys.virtual_gnss.latitude";
constexpr char kPropLongitude[] = "persist.sys.virtual_gnss.longitude";
constexpr char kPropAltitude[] = "persist.sys.virtual_gnss.altitude";
constexpr char kPropAccuracy[] = "persist.sys.virtual_gnss.accuracy";
constexpr char kPropSpeed[] = "persist.sys.virtual_gnss.speed";
constexpr char kPropBearing[] = "persist.sys.virtual_gnss.bearing";
constexpr char kPropIntervalMs[] = "persist.sys.virtual_gnss.interval_ms";

constexpr double kDefaultLatitude = 39.908722;
constexpr double kDefaultLongitude = 116.397499;
constexpr double kDefaultAltitude = 50.0;
constexpr float kDefaultAccuracy = 5.0f;
constexpr uint32_t kDefaultIntervalMs = 1000;

int64_t utcTimeMillis() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
            .count();
}

uint32_t getUintProperty(const char* key, uint32_t defaultValue) {
    return android::base::GetUintProperty(key, defaultValue);
}

double getDoubleProperty(const char* key, double defaultValue) {
    double value;
    return android::base::ParseDouble(android::base::GetProperty(key, ""), &value) ? value
                                                                                   : defaultValue;
}

float getFloatProperty(const char* key, float defaultValue) {
    float value;
    return android::base::ParseFloat(android::base::GetProperty(key, ""), &value) ? value
                                                                                  : defaultValue;
}

}  // namespace

Gnss::Gnss() : mIsActive(false), mMinIntervalMs(kDefaultIntervalMs) {}

Gnss::~Gnss() {
    stop();
}

Return<bool> Gnss::setCallback(const sp<V1_0::IGnssCallback>& callback) {
    if (callback == nullptr) {
        return false;
    }

    std::lock_guard<std::mutex> lock(mMutex);
    mCallback_1_0 = callback;
    return true;
}

Return<bool> Gnss::setCallback_1_1(const sp<V1_1::IGnssCallback>& callback) {
    if (callback == nullptr) {
        return false;
    }

    std::lock_guard<std::mutex> lock(mMutex);
    mCallback_1_1 = callback;
    mCallback_1_0 = callback;
    notifyCallbackLocked();
    return true;
}

Return<bool> Gnss::setCallback_2_0(const sp<V2_0::IGnssCallback>& callback) {
    if (callback == nullptr) {
        return false;
    }

    std::lock_guard<std::mutex> lock(mMutex);
    mCallback_2_0 = callback;
    mCallback_1_1 = callback;
    mCallback_1_0 = callback;
    notifyCallbackLocked();
    return true;
}

void Gnss::notifyCallbackLocked() {
    constexpr uint32_t capabilities =
            static_cast<uint32_t>(V1_0::IGnssCallback::Capabilities::SCHEDULING) |
            static_cast<uint32_t>(V1_0::IGnssCallback::Capabilities::SINGLE_SHOT);

    if (mCallback_2_0 != nullptr) {
        auto ret = mCallback_2_0->gnssSetCapabilitiesCb_2_0(capabilities);
        if (!ret.isOk()) {
            ALOGW("Failed to report GNSS 2.0 capabilities");
        }
    } else if (mCallback_1_0 != nullptr) {
        auto ret = mCallback_1_0->gnssSetCapabilitesCb(capabilities);
        if (!ret.isOk()) {
            ALOGW("Failed to report GNSS 1.0 capabilities");
        }
    }

    if (mCallback_1_0 != nullptr) {
        V1_0::IGnssCallback::GnssSystemInfo info = {.yearOfHw = 2026};
        auto ret = mCallback_1_0->gnssSetSystemInfoCb(info);
        if (!ret.isOk()) {
            ALOGW("Failed to report GNSS system info");
        }
    }

    if (mCallback_1_1 != nullptr) {
        auto ret = mCallback_1_1->gnssNameCb("Dipper Virtual GNSS");
        if (!ret.isOk()) {
            ALOGW("Failed to report GNSS name");
        }
    }
}

Return<bool> Gnss::start() {
    if (mIsActive.exchange(true)) {
        stop();
        mIsActive = true;
    }

    reportStatus(V1_0::IGnssCallback::GnssStatusValue::SESSION_BEGIN);
    mThread = std::thread([this]() {
        while (mIsActive) {
            const Config config = readConfig();
            if (isValidConfig(config)) {
                reportSvStatus();
                reportLocation(buildLocation(config));
            }

            uint32_t intervalMs = config.intervalMs > 0 ? config.intervalMs : mMinIntervalMs.load();
            intervalMs = std::max<uint32_t>(intervalMs, 200);
            std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));
        }
    });

    return true;
}

Return<bool> Gnss::stop() {
    mIsActive = false;
    if (mThread.joinable()) {
        mThread.join();
    }
    reportStatus(V1_0::IGnssCallback::GnssStatusValue::SESSION_END);
    return true;
}

Return<void> Gnss::cleanup() {
    stop();
    std::lock_guard<std::mutex> lock(mMutex);
    mCallback_2_0 = nullptr;
    mCallback_1_1 = nullptr;
    mCallback_1_0 = nullptr;
    return Void();
}

Return<bool> Gnss::injectTime(int64_t, int64_t, int32_t) {
    return true;
}

Return<bool> Gnss::injectLocation(double, double, float) {
    return true;
}

Return<void> Gnss::deleteAidingData(V1_0::IGnss::GnssAidingData) {
    return Void();
}

Return<bool> Gnss::setPositionMode(V1_0::IGnss::GnssPositionMode,
                                   V1_0::IGnss::GnssPositionRecurrence,
                                   uint32_t minIntervalMs, uint32_t, uint32_t) {
    if (minIntervalMs > 0) {
        mMinIntervalMs = minIntervalMs;
    }
    return true;
}

Return<bool> Gnss::setPositionMode_1_1(V1_0::IGnss::GnssPositionMode mode,
                                       V1_0::IGnss::GnssPositionRecurrence recurrence,
                                       uint32_t minIntervalMs, uint32_t preferredAccuracyMeters,
                                       uint32_t preferredTimeMs, bool) {
    return setPositionMode(mode, recurrence, minIntervalMs, preferredAccuracyMeters,
                           preferredTimeMs);
}

Return<sp<V1_0::IAGnssRil>> Gnss::getExtensionAGnssRil() {
    return nullptr;
}

Return<sp<V1_0::IGnssGeofencing>> Gnss::getExtensionGnssGeofencing() {
    return nullptr;
}

Return<sp<V1_0::IAGnss>> Gnss::getExtensionAGnss() {
    return nullptr;
}

Return<sp<V1_0::IGnssNi>> Gnss::getExtensionGnssNi() {
    return nullptr;
}

Return<sp<V1_0::IGnssMeasurement>> Gnss::getExtensionGnssMeasurement() {
    return nullptr;
}

Return<sp<V1_0::IGnssNavigationMessage>> Gnss::getExtensionGnssNavigationMessage() {
    return nullptr;
}

Return<sp<V1_0::IGnssXtra>> Gnss::getExtensionXtra() {
    return nullptr;
}

Return<sp<V1_0::IGnssConfiguration>> Gnss::getExtensionGnssConfiguration() {
    return nullptr;
}

Return<sp<V1_0::IGnssDebug>> Gnss::getExtensionGnssDebug() {
    return nullptr;
}

Return<sp<V1_0::IGnssBatching>> Gnss::getExtensionGnssBatching() {
    return nullptr;
}

Return<sp<V1_1::IGnssConfiguration>> Gnss::getExtensionGnssConfiguration_1_1() {
    return nullptr;
}

Return<sp<V1_1::IGnssMeasurement>> Gnss::getExtensionGnssMeasurement_1_1() {
    return nullptr;
}

Return<bool> Gnss::injectBestLocation(const V1_0::GnssLocation&) {
    return true;
}

Return<sp<V2_0::IGnssConfiguration>> Gnss::getExtensionGnssConfiguration_2_0() {
    return nullptr;
}

Return<sp<V2_0::IGnssDebug>> Gnss::getExtensionGnssDebug_2_0() {
    return nullptr;
}

Return<sp<V2_0::IAGnss>> Gnss::getExtensionAGnss_2_0() {
    return nullptr;
}

Return<sp<V2_0::IAGnssRil>> Gnss::getExtensionAGnssRil_2_0() {
    return nullptr;
}

Return<sp<V2_0::IGnssMeasurement>> Gnss::getExtensionGnssMeasurement_2_0() {
    return nullptr;
}

Return<sp<measurement_corrections::V1_0::IMeasurementCorrections>>
Gnss::getExtensionMeasurementCorrections() {
    return nullptr;
}

Return<sp<visibility_control::V1_0::IGnssVisibilityControl>> Gnss::getExtensionVisibilityControl() {
    return nullptr;
}

Return<sp<V2_0::IGnssBatching>> Gnss::getExtensionGnssBatching_2_0() {
    return nullptr;
}

Return<bool> Gnss::injectBestLocation_2_0(const V2_0::GnssLocation&) {
    return true;
}

Gnss::Config Gnss::readConfig() {
    Config config = {
            .enabled = android::base::GetBoolProperty(kPropEnabled, false),
            .latitude = getDoubleProperty(kPropLatitude, kDefaultLatitude),
            .longitude = getDoubleProperty(kPropLongitude, kDefaultLongitude),
            .altitude = getDoubleProperty(kPropAltitude, kDefaultAltitude),
            .accuracy = getFloatProperty(kPropAccuracy, kDefaultAccuracy),
            .speed = getFloatProperty(kPropSpeed, 0.0f),
            .bearing = getFloatProperty(kPropBearing, 0.0f),
            .intervalMs = getUintProperty(kPropIntervalMs, kDefaultIntervalMs),
    };
    return config;
}

bool Gnss::isValidConfig(const Config& config) {
    return config.enabled && config.latitude >= -90.0 && config.latitude <= 90.0 &&
           config.longitude >= -180.0 && config.longitude <= 180.0 && config.accuracy > 0.0f;
}

V2_0::GnssLocation Gnss::buildLocation(const Config& config) {
    V1_0::GnssLocation v1Location = {
            .gnssLocationFlags = static_cast<uint16_t>(
                    V1_0::GnssLocationFlags::HAS_LAT_LONG |
                    V1_0::GnssLocationFlags::HAS_ALTITUDE |
                    V1_0::GnssLocationFlags::HAS_SPEED |
                    V1_0::GnssLocationFlags::HAS_BEARING |
                    V1_0::GnssLocationFlags::HAS_HORIZONTAL_ACCURACY |
                    V1_0::GnssLocationFlags::HAS_VERTICAL_ACCURACY |
                    V1_0::GnssLocationFlags::HAS_SPEED_ACCURACY |
                    V1_0::GnssLocationFlags::HAS_BEARING_ACCURACY),
            .latitudeDegrees = config.latitude,
            .longitudeDegrees = config.longitude,
            .altitudeMeters = config.altitude,
            .speedMetersPerSec = config.speed,
            .bearingDegrees = config.bearing,
            .horizontalAccuracyMeters = config.accuracy,
            .verticalAccuracyMeters = config.accuracy,
            .speedAccuracyMetersPerSecond = 0.1f,
            .bearingAccuracyDegrees = 1.0f,
            .timestamp = utcTimeMillis(),
    };

    V2_0::ElapsedRealtime elapsedRealtime = {
            .flags = V2_0::ElapsedRealtimeFlags::HAS_TIMESTAMP_NS |
                     V2_0::ElapsedRealtimeFlags::HAS_TIME_UNCERTAINTY_NS,
            .timestampNs = static_cast<uint64_t>(android::elapsedRealtimeNano()),
            .timeUncertaintyNs = 1000000,
    };

    return {
            .v1_0 = v1Location,
            .elapsedRealtime = elapsedRealtime,
    };
}

hidl_vec<V2_0::IGnssCallback::GnssSvInfo> Gnss::buildSvStatus() {
    hidl_vec<V2_0::IGnssCallback::GnssSvInfo> svInfo(4);
    for (size_t i = 0; i < svInfo.size(); ++i) {
        V1_0::IGnssCallback::GnssSvInfo v1Info = {
                .svid = static_cast<int16_t>(3 + i),
                .constellation = V1_0::GnssConstellationType::GPS,
                .cN0Dbhz = 35.0f + static_cast<float>(i),
                .elevationDegrees = 35.0f + static_cast<float>(i * 5),
                .azimuthDegrees = 45.0f + static_cast<float>(i * 60),
                .carrierFrequencyHz = 1575420000.0f,
                .svFlag = static_cast<uint8_t>(
                        V1_0::IGnssCallback::GnssSvFlags::HAS_EPHEMERIS_DATA |
                        V1_0::IGnssCallback::GnssSvFlags::HAS_ALMANAC_DATA |
                        V1_0::IGnssCallback::GnssSvFlags::USED_IN_FIX |
                        V1_0::IGnssCallback::GnssSvFlags::HAS_CARRIER_FREQUENCY),
        };
        svInfo[i] = {
                .v1_0 = v1Info,
                .constellation = V2_0::GnssConstellationType::GPS,
        };
    }
    return svInfo;
}

void Gnss::reportStatus(V1_0::IGnssCallback::GnssStatusValue status) {
    std::lock_guard<std::mutex> lock(mMutex);
    if (mCallback_1_0 != nullptr) {
        mCallback_1_0->gnssStatusCb(status);
    }
}

void Gnss::reportSvStatus() {
    std::lock_guard<std::mutex> lock(mMutex);
    if (mCallback_2_0 != nullptr) {
        mCallback_2_0->gnssSvStatusCb_2_0(buildSvStatus());
    }
}

void Gnss::reportLocation(const V2_0::GnssLocation& location) {
    std::lock_guard<std::mutex> lock(mMutex);
    if (mCallback_2_0 != nullptr) {
        mCallback_2_0->gnssLocationCb_2_0(location);
    } else if (mCallback_1_0 != nullptr) {
        mCallback_1_0->gnssLocationCb(location.v1_0);
    }
}

}  // namespace implementation
}  // namespace V2_0
}  // namespace gnss
}  // namespace hardware
}  // namespace android
