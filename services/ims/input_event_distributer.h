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

#ifndef GRAPHIC_LITE_INPUT_EVENT_DISTRIBUTER_H
#define GRAPHIC_LITE_INPUT_EVENT_DISTRIBUTER_H

#include <pthread.h>
#include <set>
#include "gfx_utils/input_event_info.h"

namespace OHOS {
/**
 * @brief Distributer distribute all input events to specific window.
 */
class InputEventDistributer {
public:
    InputEventDistributer()
    {
        pthread_mutex_init(&listenersLock_, nullptr);
        pthread_cond_init(&listenersIdleCond_, nullptr);
    }
    ~InputEventDistributer()
    {
        pthread_cond_destroy(&listenersIdleCond_);
        pthread_mutex_destroy(&listenersLock_);
    }

    /**
     * @brief Distribute input events.
     *
     * @param [in] events events waiting for distribution.
     * @param [in] size total size of events.
     *
     */
    void Distribute(const RawEvent* events, int32_t size);

    /**
     * @brief Distribute device status to listeners.
     *
     * @param [in] type device type.
     * @param [in] status device status, 1: offline, 0: online.
     *
     */
    void DistributeDeviceStatus(InputDevType type, uint32_t status);

    /**
     * @brief Listener of raw event.
    */
    class RawEventListener {
    public:
        virtual void OnRawEvent(const RawEvent& event) = 0;
        /**
         * @brief Called when an input device is plugged in or unplugged.
         *
         * @param [in] type device type.
         * @param [in] status device status, 1: offline, 0: online.
         */
        virtual void OnDeviceStatus(InputDevType type, uint32_t status) {}
    };

    /**
     * @brief Add a raw event listener.
     *
     * @param listener raw event listener.
     */
    void AddRawEventListener(RawEventListener* listener)
    {
        if (listener == nullptr) {
            return;
        }
        pthread_mutex_lock(&listenersLock_);
        rawEventListeners_.insert(listener);
        pthread_mutex_unlock(&listenersLock_);
    }

    /**
     * @brief Remove a raw event listener.
     *
     * The listener is unregistered first, then this function blocks until all
     * in-flight callback loops (which may still hold a snapshot containing the
     * listener) have finished, so the caller can safely destroy the listener
     * object right after this function returns.
     *
     * Calling this function from inside OnRawEvent/OnDeviceStatus is allowed:
     * the current thread is recognized as a dispatching thread and will not
     * wait on itself. In that case the listener must not be destroyed before
     * its callback returns.
     *
     * @param listener raw event listener.
     */
    void RemoveRawEventListener(RawEventListener* listener)
    {
        if (listener == nullptr) {
            return;
        }
        pthread_mutex_lock(&listenersLock_);
        rawEventListeners_.erase(listener);
        // Quiesce wait: callback loops run outside the lock on a snapshot, so a
        // concurrently removed listener may still be invoked by an in-flight loop.
        // Skip the wait when the removal itself comes from inside a callback,
        // otherwise the thread would wait on itself and deadlock.
        if (distributingThreads_.find(pthread_self()) == distributingThreads_.end()) {
            while (!distributingThreads_.empty()) {
                pthread_cond_wait(&listenersIdleCond_, &listenersLock_);
            }
        }
        pthread_mutex_unlock(&listenersLock_);
    }
private:
    std::set<RawEventListener*> rawEventListeners_;
    // pthread_self() of every thread running a callback loop, one entry per
    // in-flight loop so reentrant loops nest naturally. RemoveRawEventListener
    // waits until this set is empty before letting the caller free a listener.
    std::multiset<pthread_t> distributingThreads_;
    pthread_mutex_t listenersLock_;
    pthread_cond_t listenersIdleCond_;
};
} // namespace OHOS
#endif // GRAPHIC_LITE_INPUT_EVENT_DISTRIBUTER_H
