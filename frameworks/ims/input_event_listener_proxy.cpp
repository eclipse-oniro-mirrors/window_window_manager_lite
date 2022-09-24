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

#include "input_event_listener_proxy.h"
#include "gfx_utils/graphic_log.h"
#include "samgr_lite.h"

namespace OHOS {
static IpcObjectStub objectStub;
InputEventListenerProxy::RawEventListener* InputEventListenerProxy::listener_ = nullptr;
InputEventListenerProxy::~InputEventListenerProxy()
{
    if (proxy_ != nullptr) {
        UnregisterInputEventListener();
        proxy_ = nullptr;
    }
}

InputEventListenerProxy* InputEventListenerProxy::GetInstance()
{
    static InputEventListenerProxy client;
    return &client;
}

bool InputEventListenerProxy::GetIClientProxy()
{
    if (proxy_ == nullptr) {
        IUnknown *iUnknown = SAMGR_GetInstance()->GetDefaultFeatureApi(IMS_SERVICE_NAME);
        if (iUnknown == nullptr) {
            GRAPHIC_LOGE("iUnknown is NULL");
            return false;
        }
        (void)iUnknown->QueryInterface(iUnknown, CLIENT_PROXY_VER, (void **)&proxy_);
        if (proxy_ == nullptr) {
            GRAPHIC_LOGE("QueryInterface failed, IClientProxy is empty!");
            return false;
        }
    }
    return true;
}

int32_t InputEventListenerProxy::ReceiveMsgHandler(uint32_t code, IpcIo* io, IpcIo* reply, MessageOption option)
{
    if (listener_ == nullptr) {
        return -1;
    }
    RawEvent* eventTemp = static_cast<RawEvent*>(ReadRawData(io, sizeof(RawEvent)));
    if (eventTemp == nullptr) {
        GRAPHIC_LOGE("pop raw event failed.");
        return -1;
    }
    RawEvent event = *eventTemp;
    listener_->OnRawEvent(event);
    return 0;
}

bool InputEventListenerProxy::RegisterInputEventListener(RawEventListener* listener)
{
    if (listener == nullptr) {
        GRAPHIC_LOGE("Input event listener is empty.");
        return false;
    }

    if (!GetIClientProxy()) {
        GRAPHIC_LOGE("Get input event client proxy failed.");
        return false;
    }
    IpcIo io;
    uint8_t tmpData[IMS_DEFAULT_IPC_SIZE];
    IpcIoInit(&io, tmpData, IMS_DEFAULT_IPC_SIZE, 1);
    
    SvcIdentity svc;
    objectStub.func = ReceiveMsgHandler;
    objectStub.args = nullptr;
    objectStub.isRemote = false;
    svc.handle = IPC_INVALID_HANDLE;
    svc.token = SERVICE_TYPE_ANONYMOUS;
    svc.cookie = reinterpret_cast<uintptr_t>(&objectStub);
    WriteRemoteObject(&io, &svc);
    WriteBool(&io, listener->IsAlwaysInvoke());
    int32_t ret = proxy_->Invoke(proxy_, LITEIMS_CLIENT_REGISTER, &io, NULL, NULL);
    if (ret != 0) {
        GRAPHIC_LOGE("Client register failed, ret=%d", ret);
        return false;
    }
    listener_ = listener;
    return true;
}

bool InputEventListenerProxy::UnregisterInputEventListener()
{
    if (proxy_ != nullptr) {
        IpcIo io;
        uint8_t tmpData[IMS_DEFAULT_IPC_SIZE];
        IpcIoInit(&io, tmpData, IMS_DEFAULT_IPC_SIZE, 1);
        int32_t ret = proxy_->Invoke(proxy_, LITEIMS_CLIENT_UNREGISTER, &io, NULL, NULL);
        if (ret == 0) {
            listener_ = nullptr;
            return true;
        }
    }
    return false;
}
} // namespace OHOS
