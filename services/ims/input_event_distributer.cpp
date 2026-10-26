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

#include "input_event_distributer.h"
#include "gfx_utils/graphic_log.h"

namespace OHOS {
void InputEventDistributer::Distribute(const RawEvent* events, int32_t size)
{
    if (events == nullptr || size <= 0) {
        return;
    }

    // Copy a snapshot of the listeners under the lock and invoke them outside it,
    // so that adding/removing listeners inside OnRawEvent will not deadlock or
    // invalidate the iteration. Registering this thread in distributingThreads_
    // lets RemoveRawEventListener wait for in-flight loops before the caller
    // destroys a listener.
    std::set<RawEventListener*> listeners;
    pthread_mutex_lock(&listenersLock_);
    listeners = rawEventListeners_;
    distributingThreads_.insert(pthread_self());
    pthread_mutex_unlock(&listenersLock_);
    for (int32_t i = 0; i < size; i++) {
        for (auto listener : listeners) {
            if (listener != nullptr) {
                listener->OnRawEvent(events[i]);
            }
        }
    }
    pthread_mutex_lock(&listenersLock_);
    distributingThreads_.erase(distributingThreads_.find(pthread_self()));
    if (distributingThreads_.empty()) {
        pthread_cond_broadcast(&listenersIdleCond_);
    }
    pthread_mutex_unlock(&listenersLock_);
}

void InputEventDistributer::DistributeDeviceStatus(InputDevType type, uint32_t status)
{
    std::set<RawEventListener*> listeners;
    pthread_mutex_lock(&listenersLock_);
    listeners = rawEventListeners_;
    distributingThreads_.insert(pthread_self());
    pthread_mutex_unlock(&listenersLock_);
    for (auto listener : listeners) {
        if (listener != nullptr) {
            listener->OnDeviceStatus(type, status);
        }
    }
    pthread_mutex_lock(&listenersLock_);
    distributingThreads_.erase(distributingThreads_.find(pthread_self()));
    if (distributingThreads_.empty()) {
        pthread_cond_broadcast(&listenersIdleCond_);
    }
    pthread_mutex_unlock(&listenersLock_);
}
} // namespace OHOS
