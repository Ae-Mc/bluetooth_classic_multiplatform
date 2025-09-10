#ifndef FLUTTER_PLUGIN_BLUETOOTH_CLASSIC_MULTIPLATFORM_PLUGIN_H_
#define FLUTTER_PLUGIN_BLUETOOTH_CLASSIC_MULTIPLATFORM_PLUGIN_H_

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/standard_method_codec.h>

#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>

#include "sink_stream_handler.h"

namespace bluetooth_classic_multiplatform {

class BluetoothClassicMultiplatformPlugin : public flutter::Plugin {
   public:
    static void RegisterWithRegistrar(
        flutter::PluginRegistrarWindows* registrar);

    BluetoothClassicMultiplatformPlugin();

    virtual ~BluetoothClassicMultiplatformPlugin();

    // Disallow copy and assign.
    BluetoothClassicMultiplatformPlugin(
        const BluetoothClassicMultiplatformPlugin&) = delete;
    BluetoothClassicMultiplatformPlugin& operator=(
        const BluetoothClassicMultiplatformPlugin&) = delete;

    // Called when a method is called on this plugin's channel from Dart.
    void HandleMethodCall(
        const flutter::MethodCall<flutter::EncodableValue>& method_call,
        std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result);

   private:
    // Bluetooth helper methods
    bool IsBluetoothAvailable();
    bool IsBluetoothEnabled();
    void OpenBluetoothSettings();
    flutter::EncodableList GetPairedDevices();
    void StartDiscovery();
    bool ConnectToDevice(const flutter::EncodableValue* arguments);
    bool DisconnectDevice(const flutter::EncodableValue* arguments);
    bool IsDeviceConnected(const flutter::EncodableValue* arguments);
    bool WriteData(const flutter::EncodableValue* arguments);
    std::string ReadData(const flutter::EncodableValue* arguments);
    flutter::EncodableList GetConnectedDevices();

    // Data streaming methods
    void StartDataListening(const std::string& device_address);
    void DataListeningThread(const std::string& device_address, SOCKET sock);
    int GetAvailableBytes(const flutter::EncodableValue* arguments);
    bool FlushData(const flutter::EncodableValue* arguments);

    // Connection state management
    void NotifyConnectionStateChange(const flutter::EncodableValue* arguments,
                                     bool connected);

    // Data channel management
    void CleanupDataChannels(const flutter::EncodableValue* arguments);
    void CancelDataChannel(const flutter::EncodableValue* arguments);
    void CloseDataChannel(const flutter::EncodableValue* arguments);

    // Discovery channel
    SinkStreamHandler* discovery_handler_ptr;

    // Store connected sockets and data
    std::map<std::string, SOCKET> connected_sockets_;
    std::set<std::string> listening_devices_;
    std::map<std::string, std::string> received_data_;
    std::mutex data_mutex_;
    flutter::PluginRegistrarWindows* registrar;
};

}  // namespace bluetooth_classic_multiplatform

#endif  // FLUTTER_PLUGIN_BLUETOOTH_CLASSIC_MULTIPLATFORM_PLUGIN_H_
