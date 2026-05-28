/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "android.hardware.gnss@2.0-service.dipper-virtual"

#include "Gnss.h"

#include <hidl/HidlTransportSupport.h>
#include <log/log.h>

using ::android::OK;
using ::android::sp;
using ::android::hardware::configureRpcThreadpool;
using ::android::hardware::joinRpcThreadpool;
using ::android::hardware::gnss::V2_0::IGnss;
using ::android::hardware::gnss::V2_0::implementation::Gnss;

int main() {
    sp<IGnss> gnss = new Gnss();
    configureRpcThreadpool(1, true);

    if (gnss->registerAsService() != OK) {
        ALOGE("Could not register virtual GNSS service");
        return 1;
    }

    joinRpcThreadpool();
    return 1;
}
