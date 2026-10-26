# WMS<a name="EN-US_TOPIC_0000001122925147"></a>

-   [Introduction](#section11660541593)
-   [Directory Structure](#section161941989596)
-   [Compilation and Building](#section137768191623)
-   [Description](#section1312121216216)
    -   [Usage](#section129654513264)

-   [Repositories Involved](#section1371113476307)
-   [Changes in This Release](#section_changes_en)

## Introduction<a name="section11660541593"></a>

The graphics service uses the client/server \(C/S\) architecture and is divided into two sub-services: Window Manager Service \(WMS\) and Input Manager Service \(IMS\). The application calls the APIs provided by the client to obtain the window status and process events. The server interacts with the hardware to implement display and input event distribution.

-   WMS: Manages and combines windows of different applications in a unified manner. Each window is bound to a  **RootView**  object.
-   IMS: Connects to the underlying input event driver framework to monitor and distribute input events.

**Figure  1** <a name="fig163546295165"></a>  


![](figures/en-us_image_0000001127903103.png)

## Directory Structure<a name="section161941989596"></a>

```
/foundation/window/window_manager_lite
├── frameworks      # Client
│   ├── ims         # Input event management service client
│   └── wms         # Window management service client
├── interfaces      # APIs
│   ├── innerkits   # APIs between modules
├── services        # Server
│   ├── ims         # Input event management service
│   └── wms         # Window management service
└── test            # Test code
```

## Compilation and Building<a name="section137768191623"></a>

```
# Generate the wms_server and libwms_client.so files in the out directory of the product folder through GN compilation.
hb build window_manager_lite
```

## Description<a name="section1312121216216"></a>

### Usage<a name="section129654513264"></a>

-   WMS provides methods to display various UI components, send event notifications, and process various events. For details, see the related source code.
-   **test/sample\_window**  provides the unit testing for each WMS API. Refer to it for the usage of WMS.

## Repositories Involved<a name="section1371113476307"></a>

[Graphic subsystem](https://gitee.com/openharmony/docs/blob/master/en/readme/graphics.md)

**window_manager_lite**

[graphic_surface_lite](https://gitee.com/openharmony/graphic_surface_lite/blob/master/README.md)

[arkui_ui_lite](https://gitee.com/openharmony/arkui_ui_lite/blob/master/README.md)

[graphic_graphic_utils_lite](https://gitee.com/openharmony/graphic_graphic_utils_lite/blob/master/README.md)

# Changes in This Release<a name="section_changes_en"></a>

This repository is adapted for the HiSpark AiFly (Hi3403V100) development board. For clarity, the changes are described in two parts: **Added Features** and **Enhanced Existing Features**.

## Added Features

1. Added input device hot-plug support
    -   Added the `HotPlugCallback` callback and the `hotPlugCallback_` member in `services/ims/input_event_hub.h` and `services/ims/input_event_hub.cpp`: when a device comes online, it is re-opened and its report callback is registered automatically; when it goes offline, the corresponding device bit is cleared. The offline path can cooperate with the input driver's safe-remove API (`RemoveDisconnectedInputDevice`, controlled by the `DRIVERS_PERIPHERAL_INPUT_FEATURE_HOTPLUG_SAFE_REMOVE` macro).
    -   `SetUp` now registers the hot-plug callback (devices are scanned and opened first, and the callback is registered last), and `TearDown` unregisters it.
    -   Device online/offline status is reported to the upper layer via `InputEventHub::RegisterDeviceStatusCallback` -> `InputEventDistributer::DistributeDeviceStatus` (e.g. WMS hides the mouse pointer when the mouse goes offline).

2. Added hardware copy for the window back buffer
    -   Added `TryHardwareBlit` in `services/wms/lite_win.h` and `services/wms/lite_win.cpp`: when `ENABLE_GFX_ENGINES` is enabled and both buffers have physical addresses, the back-buffer update prefers the hardware graphics engine and falls back to software copy on failure.
    -   Added `DoSoftwareCopy`, which carries the original software copy logic (NEON copy or `memcpy_s` depending on `ARM_NEON_OPT`).

3. Added integration of the input driver hot-plug feature switch
    -   `BUILD.gn` imports `//drivers/peripheral/input/input.gni` and maps the `drivers_peripheral_input_feature_hotplug_safe_remove` switch to the `DRIVERS_PERIPHERAL_INPUT_FEATURE_HOTPLUG_SAFE_REMOVE` macro.

## Enhanced Existing Features

1. The existing input device opening flow is enhanced
    -   Original functionality: `InputEventHub::SetUp` opened devices and registered callbacks inline; a single device failure aborted the initialization of the remaining devices.
    -   Enhancement: the logic is split into `OpenDevice`, with argument validation and null checks of interface pointers; the device is closed when callback registration fails; a single device failure is only logged and no longer aborts the rest.

2. The existing input event distribution logic is enhanced
    -   Original functionality: `InputManagerService::Distribute` dispatched events while holding the lock.
    -   Enhancement: events are now dequeued and the lock released before dispatching, shrinking the critical section and reducing the impact of dispatch time on enqueuing.

3. The existing back-buffer update flow is enhanced
    -   Original functionality: `LiteWindow::UpdateBackBuf` was deeply nested with scattered error handling.
    -   Enhancement: it is flattened with early returns, the buffer is released at a single place, and a two-tier path (hardware copy first, software copy as fallback) is integrated.

4. The existing mouse pointer display logic is enhanced
    -   Original functionality: `LiteWM::OnRawEvent` used a static `firstTime` flag, enabling the pointer only on the first mouse event; non-mouse events afterwards never disabled it.
    -   Enhancement: the event type is now checked per event — the pointer is enabled with `needRedraw` set on the first mouse event, and later redraws are triggered by `UpdateMouseCursor` when the position changes; non-mouse events no longer turn the pointer on by mistake. A pointer-off path is also added: when the mouse device goes offline, the device status notification disables the pointer and redraws the area it occupied (see the device status notification in Added Features 1).

5. The existing hardware composition condition check is enhanced
    -   Original functionality: `LiteWindow::Flush` used `#if ENABLE_GFX_ENGINES`, treating an undefined macro as 0.
    -   Enhancement: it is now `#if defined(ENABLE_GFX_ENGINES) && ENABLE_GFX_ENGINES`, removing the warning and making the check semantically stricter.

6. The existing component description file is enhanced
    -   Original functionality: `bundle.json` used the misspelled `feature` field.
    -   Enhancement: corrected to the standard `features` field.
