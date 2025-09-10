#include "include/bluetooth_classic_multiplatform/bluetooth_classic_multiplatform_plugin_c_api.h"

#include <flutter/plugin_registrar_windows.h>

#include "bluetooth_classic_multiplatform_plugin.h"

void BluetoothClassicMultiplatformPluginCApiRegisterWithRegistrar(
    FlutterDesktopPluginRegistrarRef registrar) {
    bluetooth_classic_multiplatform::BluetoothClassicMultiplatformPlugin::
        RegisterWithRegistrar(
            flutter::PluginRegistrarManager::GetInstance()
                ->GetRegistrar<flutter::PluginRegistrarWindows>(registrar));
}
