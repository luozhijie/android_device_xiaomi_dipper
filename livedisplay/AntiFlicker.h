/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <aidl/vendor/lineage/livedisplay/BnAntiFlicker.h>

#include <atomic>

namespace aidl {
namespace vendor {
namespace lineage {
namespace livedisplay {

class AntiFlicker : public BnAntiFlicker {
  public:
    ~AntiFlicker();

    bool isSupported();

    // Methods from ::aidl::vendor::lineage::livedisplay::BnAntiFlicker follow.
    ndk::ScopedAStatus getEnabled(bool* _aidl_return) override;
    ndk::ScopedAStatus setEnabled(bool enabled) override;

  private:
    std::atomic<bool> enabled_ = false;
};

}  // namespace livedisplay
}  // namespace lineage
}  // namespace vendor
}  // namespace aidl
