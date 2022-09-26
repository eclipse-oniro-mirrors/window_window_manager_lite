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

#include "lite_wms.h"
#include "gfx_utils/graphic_log.h"
#include "lite_wm.h"
#include "lite_wm_type.h"
#include "pms_interface.h"
#include "surface.h"
#include "surface_impl.h"

namespace OHOS {
LiteWMS* LiteWMS::GetInstance()
{
    static LiteWMS wms;
    return &wms;
}

void LiteWMS::WMSRequestHandle(int funcId, void* origin, IpcIo* req, IpcIo* reply)
{
    switch (funcId) {
        case LiteWMS_GetSurface:
            OHOS::LiteWMS::GetInstance()->GetSurface(req, reply);
            break;
        case LiteWMS_Show:
            LiteWMS::GetInstance()->Show(req, reply);
            break;
        case LiteWMS_Hide:
            LiteWMS::GetInstance()->Hide(req, reply);
            break;
        case LiteWMS_RaiseToTop:
            LiteWMS::GetInstance()->RaiseToTop(req, reply);
            break;
        case LiteWMS_LowerToBottom:
            LiteWMS::GetInstance()->LowerToBottom(req, reply);
            break;
        case LiteWMS_MoveTo:
            LiteWMS::GetInstance()->MoveTo(req, reply);
            break;
        case LiteWMS_Resize:
            LiteWMS::GetInstance()->Resize(req, reply);
            break;
        case LiteWMS_Update:
            LiteWMS::GetInstance()->Update(req, reply);
            break;
        case LiteWMS_CreateWindow:
            LiteWMS::GetInstance()->CreateWindow(req, reply);
            break;
        case LiteWMS_RemoveWindow:
            LiteWMS::GetInstance()->RemoveWindow(req, reply);
            break;
        case LiteWMS_GetEventData:
            LiteWMS::GetInstance()->GetEventData(req, reply);
            break;
        case LiteWMS_Screenshot:
            LiteWMS::GetInstance()->Screenshot(req, reply);
            break;
        case LiteWMS_ClientRegister:
            LiteWMS::GetInstance()->ClientRegister(req, reply);
            break;
        case LiteWMS_GetLayerInfo:
            LiteWMS::GetInstance()->GetLayerInfo(req, reply);
            break;
        default:
            GRAPHIC_LOGW("code not support:%d!", funcId);
            break;
    }
}

int32_t LiteWMS::SurfaceRequestHandler(uint32_t code, IpcIo* data, IpcIo* reply, MessageOption option)
{
    LiteWindow* window = reinterpret_cast<LiteWindow*>(option.args);
    if (code == 0) {
        GRAPHIC_LOGI("requestBuffer");
        window->UpdateBackBuf();
    }

    Surface* surface = window->GetSurface();
    SurfaceImpl* liteSurface = reinterpret_cast<SurfaceImpl*>(surface);
    liteSurface->DoIpcMsg(code, data, reply, option);
    return 0;
}

void LiteWMS::GetSurface(IpcIo* req, IpcIo* reply)
{
    int32_t id;
    ReadInt32(req, &id);
    GRAPHIC_LOGI("GetSurface,id=%d", id);
    LiteWindow* window = LiteWM::GetInstance()->GetWindowById(id);
    if (window == nullptr) {
        GRAPHIC_LOGE("window not found, id = %d", id);
        return;
    }
    IpcObjectStub* objectStub = new IpcObjectStub();
    if (objectStub == nullptr) {
        return;
    }
    SvcIdentity svc;
    objectStub->func = SurfaceRequestHandler;
    objectStub->args = window;
    objectStub->isRemote = false;
    svc.handle = IPC_INVALID_HANDLE;
    svc.token = SERVICE_TYPE_ANONYMOUS;
    svc.cookie = reinterpret_cast<uintptr_t>(objectStub);
    WriteInt32(reply, 0);
    window->SetSid(svc);
    WriteRemoteObject(reply, &svc);
}

void LiteWMS::Show(IpcIo* req, IpcIo* reply)
{
    GRAPHIC_LOGI("Show");
    int32_t id;
    ReadInt32(req, &id);
    LiteWM::GetInstance()->Show(id);
}

void LiteWMS::Hide(IpcIo* req, IpcIo* reply)
{
    GRAPHIC_LOGI("Hide");
    int32_t id;
    ReadInt32(req, &id);
    LiteWM::GetInstance()->Hide(id);
}

void LiteWMS::RaiseToTop(IpcIo* req, IpcIo* reply)
{
    GRAPHIC_LOGI("RaiseToTop");
    int32_t id;
    ReadInt32(req, &id);
    LiteWM::GetInstance()->RaiseToTop(id);
}

void LiteWMS::LowerToBottom(IpcIo* req, IpcIo* reply)
{
    GRAPHIC_LOGI("LowerToBottom");
    int32_t id;
    ReadInt32(req, &id);
    LiteWM::GetInstance()->LowerToBottom(id);
}

void LiteWMS::MoveTo(IpcIo* req, IpcIo* reply)
{
    GRAPHIC_LOGI("MoveTo");
    int32_t id;
    ReadInt32(req, &id);
    uint32_t x;
    ReadUint32(req, &x);
    uint32_t y;
    ReadUint32(req, &y);
    LiteWM::GetInstance()->MoveTo(id, x, y);
}

void LiteWMS::Resize(IpcIo* req, IpcIo* reply)
{
    GRAPHIC_LOGI("Resize");
    int32_t id;
    ReadInt32(req, &id);
    uint32_t width;
    ReadUint32(req, &width);
    uint32_t height;
    ReadUint32(req, &height);
    LiteWM::GetInstance()->Resize(id, width, height);
}

void LiteWMS::Update(IpcIo* req, IpcIo* reply)
{
    GRAPHIC_LOGI("Update");
    int32_t id;
    ReadInt32(req, &id);
    LiteWM::GetInstance()->UpdateWindow(id);
}

void LiteWMS::CreateWindow(IpcIo* req, IpcIo* reply)
{
    GRAPHIC_LOGI("CreateWindow");
    LiteWinConfig* config = static_cast<LiteWinConfig*>(ReadRawData(req, sizeof(LiteWinConfig)));
    if (config != nullptr) {
        pid_t pid = GetCallingPid();
        LiteWindow* window = LiteWM::GetInstance()->CreateWindow(*config, pid);
        if (window != nullptr) {
            WriteInt32(reply, window->GetWindowId());
            return;
        }
    }
    WriteInt32(reply, INVALID_WINDOW_ID);
}

void LiteWMS::RemoveWindow(IpcIo* req, IpcIo* reply)
{
    GRAPHIC_LOGI("RemoveWindow");
    int32_t id;
    ReadInt32(req, &id);
    LiteWM::GetInstance()->RemoveWindow(id);
}

void LiteWMS::GetEventData(IpcIo* req, IpcIo* reply)
{
    DeviceData data;
    LiteWM::GetInstance()->GetEventData(&data);
    WriteRawData(reply, &data, sizeof(DeviceData));
}

void LiteWMS::Screenshot(IpcIo* req, IpcIo* reply)
{
    const char *writeMediaImagePermissionName = "ohos.permission.WRITE_MEDIA_IMAGES";
    pid_t uid = GetCallingUid();
    if (CheckPermission(uid, writeMediaImagePermissionName) != GRANTED) {
        GRAPHIC_LOGE("permission denied");
        WriteInt32(reply, LiteWMS_EUNKNOWN);
        return;
    }
    Surface* surface = SurfaceImpl::GenericSurfaceByIpcIo(*req);
    bool ret = LiteWM::GetInstance()->OnScreenshot(surface);
    WriteInt32(reply, ret ? LiteWMS_EOK : LiteWMS_EUNKNOWN);
}

void LiteWMS::ClientRegister(IpcIo* req, IpcIo* reply)
{
    pid_t pid = GetCallingPid();
    SvcIdentity sid;
    bool ret = ReadRemoteObject(req, &sid);
    if (!ret) {
        return;
    }

    DeathCallbackArg* arg = new DeathCallbackArg;
    arg->pid = pid;
    arg->sid = sid;
    uint32_t cbId = -1;
    if (AddDeathRecipient(arg->sid, DeathCallback, arg, &cbId) != 0) {
        GRAPHIC_LOGE("AddDeathRecipient failed!");
    }
}

void LiteWMS::DeathCallback(void* arg)
{
    if (arg != nullptr) {
        DeathCallbackArg* cbArg = static_cast<DeathCallbackArg*>(arg);
        LiteWM::GetInstance()->OnClientDeathNotify(cbArg->pid);
        ReleaseSvc(cbArg->sid);
        delete cbArg;
    }
}

void LiteWMS::GetLayerInfo(IpcIo* req, IpcIo* reply)
{
    LiteLayerInfo layerInfo = {};
    LiteWM::GetInstance()->GetLayerInfo(layerInfo);
    WriteRawData(reply, &layerInfo, sizeof(LiteLayerInfo));
}
} // namespace OHOS
