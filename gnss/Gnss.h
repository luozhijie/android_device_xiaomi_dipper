/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <android/hardware/gnss/2.0/IGnss.h>
#include <hidl/Status.h>

#include <atomic>
#include <mutex>
#include <thread>

namespace android {
namespace hardware {
namespace gnss {
namespace V2_0 {
namespace implementation {

using ::android::sp;
using ::android::hardware::hidl_vec;
using ::android::hardware::Return;
using ::android::hardware::Void;

struct Gnss : public IGnss {
    Gnss();
    ~Gnss();

    Return<bool> setCallback(const sp<V1_0::IGnssCallback>& callback) override;
    Return<bool> start() override;
    Return<bool> stop() override;
    Return<void> cleanup() override;
    Return<bool> injectTime(int64_t timeMs, int64_t timeReferenceMs,
                            int32_t uncertaintyMs) override;
    Return<bool> injectLocation(double latitudeDegrees, double longitudeDegrees,
                                float accuracyMeters) override;
    Return<void> deleteAidingData(V1_0::IGnss::GnssAidingData aidingDataFlags) override;
    Return<bool> setPositionMode(V1_0::IGnss::GnssPositionMode mode,
                                 V1_0::IGnss::GnssPositionRecurrence recurrence,
                                 uint32_t minIntervalMs, uint32_t preferredAccuracyMeters,
                                 uint32_t preferredTimeMs) override;
    Return<sp<V1_0::IAGnssRil>> getExtensionAGnssRil() override;
    Return<sp<V1_0::IGnssGeofencing>> getExtensionGnssGeofencing() override;
    Return<sp<V1_0::IAGnss>> getExtensionAGnss() override;
    Return<sp<V1_0::IGnssNi>> getExtensionGnssNi() override;
    Return<sp<V1_0::IGnssMeasurement>> getExtensionGnssMeasurement() override;
    Return<sp<V1_0::IGnssNavigationMessage>> getExtensionGnssNavigationMessage() override;
    Return<sp<V1_0::IGnssXtra>> getExtensionXtra() override;
    Return<sp<V1_0::IGnssConfiguration>> getExtensionGnssConfiguration() override;
    Return<sp<V1_0::IGnssDebug>> getExtensionGnssDebug() override;
    Return<sp<V1_0::IGnssBatching>> getExtensionGnssBatching() override;

    Return<bool> setCallback_1_1(const sp<V1_1::IGnssCallback>& callback) override;
    Return<bool> setPositionMode_1_1(V1_0::IGnss::GnssPositionMode mode,
                                     V1_0::IGnss::GnssPositionRecurrence recurrence,
                                     uint32_t minIntervalMs, uint32_t preferredAccuracyMeters,
                                     uint32_t preferredTimeMs, bool lowPowerMode) override;
    Return<sp<V1_1::IGnssConfiguration>> getExtensionGnssConfiguration_1_1() override;
    Return<sp<V1_1::IGnssMeasurement>> getExtensionGnssMeasurement_1_1() override;
    Return<bool> injectBestLocation(const V1_0::GnssLocation& location) override;

    Return<sp<V2_0::IGnssConfiguration>> getExtensionGnssConfiguration_2_0() override;
    Return<sp<V2_0::IGnssDebug>> getExtensionGnssDebug_2_0() override;
    Return<sp<V2_0::IAGnss>> getExtensionAGnss_2_0() override;
    Return<sp<V2_0::IAGnssRil>> getExtensionAGnssRil_2_0() override;
    Return<sp<V2_0::IGnssMeasurement>> getExtensionGnssMeasurement_2_0() override;
    Return<sp<measurement_corrections::V1_0::IMeasurementCorrections>>
    getExtensionMeasurementCorrections() override;
    Return<sp<visibility_control::V1_0::IGnssVisibilityControl>>
    getExtensionVisibilityControl() override;
    Return<sp<V2_0::IGnssBatching>> getExtensionGnssBatching_2_0() override;
    Return<bool> setCallback_2_0(const sp<V2_0::IGnssCallback>& callback) override;
    Return<bool> injectBestLocation_2_0(const V2_0::GnssLocation& location) override;

  private:
    struct Config {
        bool enabled;
        double latitude;
        double longitude;
        double altitude;
        float accuracy;
        float speed;
        float bearing;
        uint32_t intervalMs;
    };

    static Config readConfig();
    static bool isValidConfig(const Config& config);
    static V2_0::GnssLocation buildLocation(const Config& config);
    static hidl_vec<V2_0::IGnssCallback::GnssSvInfo> buildSvStatus();

    void reportStatus(V1_0::IGnssCallback::GnssStatusValue status);
    void reportSvStatus();
    void reportLocation(const V2_0::GnssLocation& location);
    void notifyCallbackLocked();

    std::atomic<bool> mIsActive;
    std::atomic<uint32_t> mMinIntervalMs;
    std::thread mThread;
    std::mutex mMutex;
    sp<V1_0::IGnssCallback> mCallback_1_0;
    sp<V1_1::IGnssCallback> mCallback_1_1;
    sp<V2_0::IGnssCallback> mCallback_2_0;
};

}  // namespace implementation
}  // namespace V2_0
}  // namespace gnss
}  // namespace hardware
}  // namespace android
