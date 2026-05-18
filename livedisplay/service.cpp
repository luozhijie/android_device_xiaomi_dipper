/*
 * SPDX-FileCopyrightText: 2019-2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "vendor.lineage.livedisplay-service.dipper"

#include <cstdlib>
#include <memory>
#include <string>

#include <android-base/logging.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <binder/ProcessState.h>

#include "AntiFlicker.h"
#include "SunlightEnhancement.h"

using ::aidl::vendor::lineage::livedisplay::AntiFlicker;
using ::aidl::vendor::lineage::livedisplay::SunlightEnhancement;

int main() {
    std::shared_ptr<AntiFlicker> af = ndk::SharedRefBase::make<AntiFlicker>();
    std::shared_ptr<SunlightEnhancement> se = ndk::SharedRefBase::make<SunlightEnhancement>();
    const std::string antiFlickerInstance = std::string() + AntiFlicker::descriptor + "/default";
    const std::string sunlightEnhancementInstance =
            std::string() + SunlightEnhancement::descriptor + "/default";

    android::ProcessState::self()->setThreadPoolMaxThreadCount(1);
    ABinderProcess_setThreadPoolMaxThreadCount(1);

    LOG(INFO) << "Dipper LiveDisplay HAL custom service is starting.";

    if (af == nullptr || se == nullptr) {
        LOG(ERROR) << "Can not create an instance of LiveDisplay HAL Iface, exiting.";
        return EXIT_FAILURE;
    }

    binder_status_t status =
            AServiceManager_addService(af->asBinder().get(), antiFlickerInstance.c_str());
    if (status != STATUS_OK) {
        LOG(ERROR) << "Could not register service for LiveDisplay HAL AntiFlicker Iface ("
                   << status << ")";
        return EXIT_FAILURE;
    }

    status = AServiceManager_addService(se->asBinder().get(), sunlightEnhancementInstance.c_str());
    if (status != STATUS_OK) {
        LOG(ERROR) << "Could not register service for LiveDisplay HAL SunlightEnhancement Iface ("
                   << status << ")";
        return EXIT_FAILURE;
    }

    ABinderProcess_startThreadPool();
    LOG(INFO) << "LiveDisplay HAL custom service is ready.";
    ABinderProcess_joinThreadPool();

    LOG(ERROR) << "LiveDisplay HAL custom service failed to join thread pool.";
    return EXIT_FAILURE;
}
