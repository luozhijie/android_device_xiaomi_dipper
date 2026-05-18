/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "AntiFlickerService"

#include <cerrno>
#include <cstring>
#include <string>
#include <unistd.h>

#include <android-base/file.h>
#include <android-base/logging.h>

#include "AntiFlicker.h"

namespace {

static constexpr const char* kDispParamPath =
        "/sys/devices/platform/soc/ae00000.qcom,mdss_mdp/drm/card0/card0-DSI-1/disp_param";
static constexpr const char* kDispParamFallbackPath = "/sys/class/drm/card0-DSI-1/disp_param";

static constexpr const char* kDispParamDcOff = "0x50000";
static constexpr const char* kDispParamDcOn = "0x40000";

bool writeDispParam(const char* value) {
    const std::string payload = std::string(value) + "\n";
    if (android::base::WriteStringToFile(payload, kDispParamPath) ||
        android::base::WriteStringToFile(payload, kDispParamFallbackPath)) {
        return true;
    }

    LOG(ERROR) << "Failed to write " << value << " to disp_param";
    return false;
}

}  // anonymous namespace

namespace aidl {
namespace vendor {
namespace lineage {
namespace livedisplay {

AntiFlicker::~AntiFlicker() {
    enabled_ = false;
    writeDispParam(kDispParamDcOff);
}

bool AntiFlicker::isSupported() {
    if (access(kDispParamPath, W_OK) != 0 && access(kDispParamFallbackPath, W_OK) != 0) {
        LOG(ERROR) << "Failed to access disp_param nodes, error=" << errno << " ("
                   << strerror(errno) << ")";
        return false;
    }

    return true;
}

ndk::ScopedAStatus AntiFlicker::getEnabled(bool* _aidl_return) {
    *_aidl_return = enabled_.load();
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus AntiFlicker::setEnabled(bool enabled) {
    if (enabled == enabled_.load()) {
        return ndk::ScopedAStatus::ok();
    }

    if (enabled) {
        if (!isSupported() || !writeDispParam(kDispParamDcOn)) {
            return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
        }
        enabled_ = true;
        LOG(INFO) << "Kernel DC clamp enabled";
    } else {
        enabled_ = false;
        if (!writeDispParam(kDispParamDcOff)) {
            return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
        }
        LOG(INFO) << "Kernel DC clamp disabled";
    }

    return ndk::ScopedAStatus::ok();
}

}  // namespace livedisplay
}  // namespace lineage
}  // namespace vendor
}  // namespace aidl
