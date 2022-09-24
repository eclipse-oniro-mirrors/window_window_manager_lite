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

#include "input_event_client_proxy.h"
#include "gfx_utils/graphic_log.h"
#include "samgr_lite.h"

namespace OHOS {
pthread_mutex_t InputEventClientProxy::lock_;
InputEventClientProxy* InputEventClientProxy::GetInstance()
{
    static InputEventClientProxy ims;
    return &ims;
}

void InputEventClientProxy::ClientRequestHandle(int funcId, void* origin, IpcIo* req, IpcIo* reply)
{
    switch (funcId) {
        case LITEIMS_CLIENT_REGISTER: {
            InputEventClientProxy::GetInstance()->AddListener(origin, req, reply);
            break;
        }
        case LITEIMS_CLIENT_UNREGISTER: {
            InputEventClientProxy::GetInstance()->RemoveListener(origin, req, reply);
            break;
        }
        default: {
            break;
        }
    }
}

void InputEventClientProxy::AddListener(const void* origin, IpcIo* req, IpcIo* reply)
{
    if (clientInfoMap_.size() >= MAX_CLIENT_SIZE) {
        GRAPHIC_LOGE("Exceeded the maximum number!");
        return;
    }
    pid_t pid = GetCallingPid();
    SvcIdentity svc = {0};
    bool ret = ReadRemoteObject(req, &svc);
    bool alwaysInvoke;
    ReadBool(req, &alwaysInvoke);
    if (!ret) {
        GRAPHIC_LOGE("ReadRemoteObject failed.");
        return;
    }
    uint32_t cbId = 0;
    if (AddDeathRecipient(svc, DeathCallback, nullptr, &cbId) != 0) {
        GRAPHIC_LOGE("Register death callback failed!");
        return;
    }
    struct ClientInfo clientInfo = { svc, cbId, alwaysInvoke };
    pthread_mutex_lock(&lock_);
    clientInfoMap_.insert(std::make_pair(pid, clientInfo));
    pthread_mutex_unlock(&lock_);
}

void InputEventClientProxy::DeathCallback(void* origin)
{
    InputEventClientProxy::GetInstance()->RemoveListener(origin, nullptr, nullptr);
}

void InputEventClientProxy::RemoveListener(const void* origin, IpcIo* req, IpcIo* reply)
{
    pid_t pid = GetCallingPid();
    if (clientInfoMap_.count(pid) > 0) {
        ReleaseSvc(clientInfoMap_[pid].svc);
        pthread_mutex_lock(&lock_);
        clientInfoMap_.erase(pid);
        pthread_mutex_unlock(&lock_);
    }
}

void InputEventClientProxy::OnRawEvent(const RawEvent& event)
{
    IpcIo io;
    uint8_t tmpData[IMS_DEFAULT_IPC_SIZE];
    IpcIoInit(&io, tmpData, IMS_DEFAULT_IPC_SIZE, 1);
    WriteRawData(&io, static_cast<const void*>(&event), sizeof(RawEvent));
    pthread_mutex_lock(&lock_);
    std::map<pid_t, ClientInfo>::iterator it;
    for (it = clientInfoMap_.begin(); it != clientInfoMap_.end(); ++it) {
        if (it->second.alwaysInvoke || (event.state != lastState_)) {
            MessageOption option;
            MessageOptionInit(&option);
            option.flags = TF_OP_ASYNC;
            SendRequest(it->second.svc, 0, &io, nullptr, option, nullptr);
        }
    }
    lastState_ = event.state;
    pthread_mutex_unlock(&lock_);
}
} // namespace OHOS
