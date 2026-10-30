/*
 * Copyright (c) 2020-2021 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "input_event_hub.h"
#include "gfx_utils/graphic_log.h"
#include "input-event-codes.h"

namespace OHOS {
namespace {
const uint32_t TOUCH_DEV_ID = 1;
const uint32_t MOUSE_DEV_ID = 2;
const uint32_t UNKNOW_DEV_ID = 32;
}
IInputInterface* InputEventHub::inputInterface_ = nullptr;
InputEventCb InputEventHub::callback_ = {};
InputHostCb InputEventHub::hotPlugCallback_ = {};
InputEventHub::ReadCallback InputEventHub::readCallback_ = nullptr;
InputEventHub::DeviceStatusCallback InputEventHub::deviceStatusCallback_ = nullptr;

InputEventHub* InputEventHub::GetInstance()
{
    static InputEventHub instance;
    return &instance;
}

InputEventHub::InputEventHub()
{
    for (uint8_t i = 0; i < MAX_INPUT_DEVICE_NUM; i++) {
        mountDevIndex_[i] = UNKNOW_DEV_ID;
    }
    openDev_ = 0;

    data_.deviceId = UNKNOW_DEV_ID;
    data_.state = 0;
    data_.timestamp = 0;
    data_.type = InputDevType::INDEV_TYPE_UNKNOWN;
    data_.x = 0;
    data_.y = 0;
}

void InputEventHub::SetUp()
{
    int32_t ret = GetInputInterface(&inputInterface_);
    if (ret != INPUT_SUCCESS) {
        GRAPHIC_LOGE("get input driver interface failed!");
        return;
    }
    if (inputInterface_ == nullptr || inputInterface_->iInputManager == nullptr ||
        inputInterface_->iInputReporter == nullptr) {
        GRAPHIC_LOGE("input interface or input manager is nullptr!");
        return;
    }

    // Scan and open devices before registering the hot-plug callback, so that
    // mountDevIndex_ is not modified concurrently by the driver thread while SetUp
    // is still filling it in.
    uint8_t num = ScanInputDevice();
    if (num == 0) {
        GRAPHIC_LOGE("There is no device!");
        return;
    }
    for (uint8_t i = 0; i < num; i++) {
        if (!OpenDevice(i)) {
            GRAPHIC_LOGE("open input device failed, dev=%u", mountDevIndex_[i]);
        }
    }

    hotPlugCallback_ = {};
    hotPlugCallback_.HotPlugCallback = HotPlugCallback;
    ret = inputInterface_->iInputReporter->RegisterHotPlugCallback(&hotPlugCallback_);
    if (ret != INPUT_SUCCESS) {
        GRAPHIC_LOGE("register input hotplug callback failed!");
    }
}

bool InputEventHub::OpenDevice(uint8_t index)
{
    if (index >= MAX_INPUT_DEVICE_NUM || inputInterface_ == nullptr || inputInterface_->iInputManager == nullptr ||
        inputInterface_->iInputReporter == nullptr) {
        return false;
    }

    const uint32_t devIndex = mountDevIndex_[index];
    if (inputInterface_->iInputManager->OpenInputDevice(devIndex) != INPUT_SUCCESS) {
        return false;
    }

    callback_.EventPkgCallback = EventCallback;
    if (inputInterface_->iInputReporter->RegisterReportCallback(devIndex, &callback_) != INPUT_SUCCESS) {
        (void)inputInterface_->iInputManager->CloseInputDevice(devIndex);
        return false;
    }

    openDev_ |= (1U << index);
    return true;
}

void InputEventHub::CloseDevice(uint8_t index, bool disconnected)
{
    // Test-and-clear the device bit atomically: only the path that succeeds in
    // clearing the bit performs the cleanup, so TearDown and HotPlugCallback will
    // never close the same device twice.
    uint32_t expected = openDev_.load();
    while ((expected & (1U << index)) != 0) {
        if (openDev_.compare_exchange_weak(expected, expected & ~(1U << index))) {
            break;
        }
    }

    if (inputInterface_ == nullptr || inputInterface_->iInputManager == nullptr ||
        inputInterface_->iInputReporter == nullptr) {
        GRAPHIC_LOGE("input interface or input manager is nullptr, skip closing device!");
        return;
    }

    const uint32_t devIndex = mountDevIndex_[index];
#ifdef DRIVERS_PERIPHERAL_INPUT_FEATURE_HOTPLUG_SAFE_REMOVE
    // With the safe-remove feature enabled, the driver has already destroyed the
    // device vnode before reporting offline. CloseInputDevice would recycle the HDF
    // service and race with the vnode reclamation; RemoveDisconnectedInputDevice
    // only unregisters the listener and releases the node, which is safe here.
    if (disconnected && inputInterface_->iInputManager->RemoveDisconnectedInputDevice != nullptr) {
        if (inputInterface_->iInputManager->RemoveDisconnectedInputDevice(devIndex) != INPUT_SUCCESS) {
            GRAPHIC_LOGE("remove disconnected input device failed, dev=%u", devIndex);
        }
        return;
    }
#endif
    if (inputInterface_->iInputReporter->UnregisterReportCallback(devIndex) != INPUT_SUCCESS) {
        GRAPHIC_LOGE("Unregister callback failed!");
    }
    if (inputInterface_->iInputManager->CloseInputDevice(devIndex) != INPUT_SUCCESS) {
        GRAPHIC_LOGE("Unmount device failed!");
    }
}

void InputEventHub::TearDown()
{
    for (uint8_t i = 0; i < MAX_INPUT_DEVICE_NUM; i++) {
        CloseDevice(i, false);
    }

    if (inputInterface_ != nullptr) {
        if (inputInterface_->iInputReporter != nullptr &&
            inputInterface_->iInputReporter->UnregisterHotPlugCallback != nullptr) {
            (void)inputInterface_->iInputReporter->UnregisterHotPlugCallback();
        }
        hotPlugCallback_ = {};
        ReleaseInputInterface(&inputInterface_);
        inputInterface_ = nullptr;
    }
}

void InputEventHub::EventCallback(const InputEventPackage **pkgs, uint32_t count, uint32_t devIndex)
{
    if (pkgs == nullptr || readCallback_ == nullptr || count == 0) {
        return;
    }

    RawEvent& data = InputEventHub::GetInstance()->data_;
    for (uint32_t i = 0; i < count; i++) {
        if (pkgs[i]->type == EV_REL) {
            data.type = InputDevType::INDEV_TYPE_MOUSE;
            if (pkgs[i]->code == REL_X)
                data.x += pkgs[i]->value;
            else if (pkgs[i]->code == REL_Y)
                data.y += pkgs[i]->value;
        } else if (pkgs[i]->type == EV_ABS) {
            data.type = InputDevType::INDEV_TYPE_TOUCH;
            if (pkgs[i]->code == ABS_MT_POSITION_X)
                data.x = pkgs[i]->value;
            else if (pkgs[i]->code == ABS_MT_POSITION_Y)
                data.y = pkgs[i]->value;
        } else if (pkgs[i]->type == EV_KEY) {
            if (pkgs[i]->code == BTN_MOUSE || pkgs[i]->code == BTN_TOUCH) {
                if (pkgs[i]->value == 0)
                    data.state = 0;
                else if (pkgs[i]->value == 1)
                    data.state = 1;
            }
        } else if (pkgs[i]->type == EV_SYN) {
            if (pkgs[i]->code == SYN_REPORT) {
                break;
            }
        }
    }

    readCallback_(&data);
}

void InputEventHub::HotPlugCallback(const InputHotPlugEvent *event)
{
    if (event == nullptr) {
        return;
    }

    InputEventHub* hub = GetInstance();
    for (uint8_t i = 0; i < MAX_INPUT_DEVICE_NUM; i++) {
        // Skip idle slots filled with the sentinel value.
        if (hub->mountDevIndex_[i] == UNKNOW_DEV_ID || hub->mountDevIndex_[i] != event->devIndex) {
            continue;
        }

        if (event->status == 1) { // 1: offline
            hub->CloseDevice(i, true);
            GRAPHIC_LOGI("input device offline, dev=%u", event->devIndex);
        } else if (event->status == 0 && (hub->openDev_.load() & (1U << i)) == 0) { // 0: online
            if (!hub->OpenDevice(i)) {
                GRAPHIC_LOGE("recover input device failed, dev=%u", event->devIndex);
            }
        }
        break;
    }

    // Notify the upper layer about the device status change (e.g. WMS hides the
    // mouse cursor when the mouse goes offline).
    if (deviceStatusCallback_ != nullptr) {
        deviceStatusCallback_(GetDeviceType(event->devIndex), event->status);
    }
}

InputDevType InputEventHub::GetDeviceType(uint32_t devIndex)
{
    switch (devIndex) {
        case TOUCH_DEV_ID: {
            return InputDevType::INDEV_TYPE_TOUCH;
        }
        case MOUSE_DEV_ID: {
            return InputDevType::INDEV_TYPE_MOUSE;
        }
        default: {
            return InputDevType::INDEV_TYPE_UNKNOWN;
        }
    }
}

uint8_t InputEventHub::ScanInputDevice()
{
    for (uint8_t i = 0; i < MAX_INPUT_DEVICE_NUM; i++) {
        mountDevIndex_[i] = UNKNOW_DEV_ID;
    }
    /* later will be change get device mode */
    uint32_t deviceNum = 0;
    mountDevIndex_[0] = TOUCH_DEV_ID;
    mountDevIndex_[1] = MOUSE_DEV_ID;
    deviceNum = 2; // 2:Number of current devices
    return deviceNum;
}
} // namespace OHOS
