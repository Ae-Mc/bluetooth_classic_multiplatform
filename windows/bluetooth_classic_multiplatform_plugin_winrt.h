#ifndef FLUTTER_PLUGIN_BLUETOOTH_CLASSIC_MULTIPLATFORM_PLUGIN_H_
#define FLUTTER_PLUGIN_BLUETOOTH_CLASSIC_MULTIPLATFORM_PLUGIN_H_

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/standard_method_codec.h>

// WinRT includes
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Bluetooth.Rfcomm.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Networking.Sockets.h>
#include <winrt/Windows.Storage.Streams.h>

#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>

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
    flutter::EncodableList StartDiscovery();
    bool ConnectToDevice(const flutter::EncodableValue* arguments);
    bool DisconnectDevice(const flutter::EncodableValue* arguments);
    bool IsDeviceConnected(const flutter::EncodableValue* arguments);
    bool WriteData(const flutter::EncodableValue* arguments);
    std::string ReadData(const flutter::EncodableValue* arguments);
    flutter::EncodableList GetConnectedDevices();

    // Data streaming methods
    void StartDataListening(const std::string& device_address);
    void DataListeningThread(const std::string& device_address, 
                           winrt::Windows::Networking::Sockets::StreamSocket socket);
    int GetAvailableBytes(const flutter::EncodableValue* arguments);
    bool FlushData(const flutter::EncodableValue* arguments);

    // Connection state management
    void NotifyConnectionStateChange(const flutter::EncodableValue* arguments,
                                     bool connected);

    // Data channel management
    void CleanupDataChannels(const flutter::EncodableValue* arguments);
    void CancelDataChannel(const flutter::EncodableValue* arguments);
    void CloseDataChannel(const flutter::EncodableValue* arguments);

    // WinRT helper methods
    winrt::Windows::Devices::Bluetooth::BluetoothDevice GetBluetoothDevice(const std::string& address);
    winrt::Windows::Devices::Bluetooth::Rfcomm::RfcommDeviceService GetRfcommService(
        const winrt::Windows::Devices::Bluetooth::BluetoothDevice& device);

    // Store connected sockets and data using WinRT types
    std::map<std::string, winrt::Windows::Networking::Sockets::StreamSocket> connected_sockets_;
    std::set<std::string> listening_devices_;
    std::map<std::string, std::string> received_data_;
    std::mutex data_mutex_;
};

}  // namespace bluetooth_classic_multiplatform

#endif  // FLUTTER_PLUGIN_BLUETOOTH_CLASSIC_MULTIPLATFORM_PLUGIN_H_
