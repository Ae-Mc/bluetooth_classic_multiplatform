#include "bluetooth_classic_multiplatform_plugin_winrt.h"

// WinRT includes
#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/standard_method_codec.h>
#include <winrt/Windows.Devices.Bluetooth.Rfcomm.h>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Networking.Sockets.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.System.h>

#include <memory>
#include <sstream>

namespace bluetooth_classic_multiplatform {

std::string TAG = "bluetooth_classic_multiplatform";

// static
void BluetoothClassicMultiplatformPlugin::RegisterWithRegistrar(
    flutter::PluginRegistrarWindows* registrar) {
    auto channel =
        std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
            registrar->messenger(), TAG,
            &flutter::StandardMethodCodec::GetInstance());

    // Register the state channel
    auto state_channel =
        std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
            registrar->messenger(), TAG + "/state",
            &flutter::StandardMethodCodec::GetInstance());

    // Register the discovery channel
    auto discovery_channel =
        std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
            registrar->messenger(), TAG + "/discovery",
            &flutter::StandardMethodCodec::GetInstance());

    // Register the data channel
    auto data_channel =
        std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
            registrar->messenger(), TAG + "/data",
            &flutter::StandardMethodCodec::GetInstance());

    // Register the connection channel
    auto connection_channel =
        std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
            registrar->messenger(), TAG + "/connection",
            &flutter::StandardMethodCodec::GetInstance());

    auto plugin = std::make_unique<BluetoothClassicMultiplatformPlugin>();

    channel->SetMethodCallHandler(
        [plugin_pointer = plugin.get()](const auto& call, auto result) {
            plugin_pointer->HandleMethodCall(call, std::move(result));
        });

    state_channel->SetMethodCallHandler(
        [plugin_pointer = plugin.get()](const auto& call, auto result) {
            plugin_pointer->HandleMethodCall(call, std::move(result));
        });

    discovery_channel->SetMethodCallHandler(
        [plugin_pointer = plugin.get()](const auto& call, auto result) {
            plugin_pointer->HandleMethodCall(call, std::move(result));
        });

    data_channel->SetMethodCallHandler(
        [plugin_pointer = plugin.get()](const auto& call, auto result) {
            plugin_pointer->HandleMethodCall(call, std::move(result));
        });

    connection_channel->SetMethodCallHandler(
        [plugin_pointer = plugin.get()](const auto& call, auto result) {
            plugin_pointer->HandleMethodCall(call, std::move(result));
        });

    registrar->AddPlugin(std::move(plugin));
}

BluetoothClassicMultiplatformPlugin::BluetoothClassicMultiplatformPlugin() {}

BluetoothClassicMultiplatformPlugin::~BluetoothClassicMultiplatformPlugin() {}

void BluetoothClassicMultiplatformPlugin::HandleMethodCall(
    const flutter::MethodCall<flutter::EncodableValue>& method_call,
    std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
    const auto& method = method_call.method_name();

    if (method == "listen") {
        // Check if this is a data channel listen request
        if (method_call.arguments()) {
            const auto* args =
                std::get_if<flutter::EncodableMap>(method_call.arguments());
            if (args) {
                auto device_it = args->find(flutter::EncodableValue("device"));
                if (device_it != args->end()) {
                    const auto* device_str =
                        std::get_if<std::string>(&device_it->second);
                    if (device_str) {
                        std::string debug_msg =
                            "Data channel listen request for device: " +
                            *device_str + "\n";
                        OutputDebugStringA(debug_msg.c_str());

                        // Always start data listening if device is connected
                        // (like Android)
                        if (connected_sockets_.find(*device_str) !=
                            connected_sockets_.end()) {
                            StartDataListening(*device_str);
                            OutputDebugStringA(
                                "Data listening started for device\n");
                            result->Success(flutter::EncodableValue(true));
                            return;
                        } else {
                            OutputDebugStringA(
                                "Device not connected for data listening\n");
                            result->Success(flutter::EncodableValue(false));
                            return;
                        }
                    }
                }
            }
        }
        // Default listen behavior for other channels
        result->Success(flutter::EncodableValue(true));
    } else if (method == "cancel") {
        CancelDataChannel(method_call.arguments());
        result->Success(flutter::EncodableValue(true));
    } else if (method == "close") {
        CloseDataChannel(method_call.arguments());
        result->Success(flutter::EncodableValue(true));
    }

    // State channel methods
    else if (method == "isAvailable") {
        result->Success(flutter::EncodableValue(IsBluetoothAvailable()));
    } else if (method == "isEnabled") {
        result->Success(flutter::EncodableValue(IsBluetoothEnabled()));
    }

    // Connection channel methods
    else if (method == "enable") {
        result->Success(flutter::EncodableValue(IsBluetoothEnabled()));
    } else if (method == "openSettings") {
        OpenBluetoothSettings();
        result->Success(flutter::EncodableValue(true));
    } else if (method == "getPairedDevices") {
        auto devices = GetPairedDevices();
        result->Success(flutter::EncodableValue(devices));
    } else if (method == "startDiscovery") {
        auto devices = StartDiscovery();
        result->Success(flutter::EncodableValue(devices));
    } else if (method == "stopDiscovery") {
        result->Success(flutter::EncodableValue(true));
    } else if (method == "isDiscovering") {
        result->Success(flutter::EncodableValue(false));
    }

    // Main channel methods
    else if (method == "requestPermissions") {
        result->Success(true);
    } else if (method == "enable") {
        result->Success(flutter::EncodableValue(IsBluetoothEnabled()));
    } else if (method == "connect") {
        OutputDebugStringA("HandleMethodCall: Connect method called\n");
        bool success = ConnectToDevice(method_call.arguments());
        if (success) {
            OutputDebugStringA(
                "HandleMethodCall: Connection successful, notifying state "
                "change\n");
            NotifyConnectionStateChange(method_call.arguments(), true);

            // Don't auto-start data listening here - let Flutter app call
            // listen when ready
            OutputDebugStringA(
                "Connection established, waiting for data channel listen "
                "request\n");
        } else {
            OutputDebugStringA("HandleMethodCall: Connection failed\n");
        }
        result->Success(flutter::EncodableValue(success));
    } else if (method == "disconnect") {
        bool success = DisconnectDevice(method_call.arguments());
        // Send disconnection state change event and cleanup data channels
        if (success) {
            NotifyConnectionStateChange(method_call.arguments(), false);
            CleanupDataChannels(method_call.arguments());
        }
        result->Success(flutter::EncodableValue(success));
    } else if (method == "isConnected") {
        bool connected = IsDeviceConnected(method_call.arguments());
        result->Success(flutter::EncodableValue(connected));
    }

    // Data channel methods
    else if (method == "writeData") {
        bool success = WriteData(method_call.arguments());
        result->Success(flutter::EncodableValue(success));
    } else if (method == "readData") {
        std::string data = ReadData(method_call.arguments());
        result->Success(flutter::EncodableValue(data));
    } else if (method == "available") {
        int available = GetAvailableBytes(method_call.arguments());
        result->Success(flutter::EncodableValue(available));
    } else if (method == "flush") {
        bool success = FlushData(method_call.arguments());
        result->Success(flutter::EncodableValue(success));
    }

    // Generic methods that might be called on any channel
    else if (method == "destroy") {
        result->Success(flutter::EncodableValue(true));
    } else if (method == "finish") {
        result->Success(flutter::EncodableValue(true));
    } else if (method == "getPlatformVersion") {
        result->Success(flutter::EncodableValue("Windows"));
    }

    else {
        result->NotImplemented();
    }
}

bool BluetoothClassicMultiplatformPlugin::IsBluetoothAvailable() {
    try {
        // Check if Bluetooth adapter is available using WinRT
        auto selector = winrt::Windows::Devices::Bluetooth::BluetoothDevice::
            GetDeviceSelector();
        auto devices = winrt::Windows::Devices::Enumeration::DeviceInformation::
                           FindAllAsync(selector)
                               .get();
        return devices.Size() > 0;
    } catch (...) {
        return false;
    }
}

bool BluetoothClassicMultiplatformPlugin::IsBluetoothEnabled() {
    try {
        // Check if Bluetooth is enabled by trying to get the default adapter
        auto adapter = winrt::Windows::Devices::Bluetooth::BluetoothAdapter::
                           GetDefaultAsync()
                               .get();
        return adapter != nullptr;
    } catch (...) {
        return false;
    }
}

void BluetoothClassicMultiplatformPlugin::OpenBluetoothSettings() {
    try {
        winrt::Windows::System::Launcher::LaunchUriAsync(
            winrt::Windows::Foundation::Uri(L"ms-settings:bluetooth"));
    } catch (...) {
        // Fallback to ShellExecute if WinRT fails
        ShellExecute(NULL, L"open", L"ms-settings:bluetooth", NULL, NULL,
                     SW_SHOWNORMAL);
    }
}

flutter::EncodableList BluetoothClassicMultiplatformPlugin::GetPairedDevices() {
    flutter::EncodableList devices;

    try {
        // Get paired Bluetooth devices
        auto selector = winrt::Windows::Devices::Bluetooth::BluetoothDevice::
            GetDeviceSelector();
        auto deviceInfos = winrt::Windows::Devices::Enumeration::
                               DeviceInformation::FindAllAsync(selector)
                                   .get();

        for (const auto& deviceInfo : deviceInfos) {
            try {
                auto device = winrt::Windows::Devices::Bluetooth::
                                  BluetoothDevice::FromIdAsync(deviceInfo.Id())
                                      .get();

                if (device != nullptr) {
                    flutter::EncodableMap device_map;

                    // Get device name
                    std::wstring name = device.Name().c_str();
                    std::string name_str(name.begin(), name.end());

                    // Get device address
                    auto address = device.BluetoothAddress();
                    char addressStr[18];
                    sprintf_s(addressStr, "%012llX", address);

                    // Format as XX:XX:XX:XX:XX:XX
                    std::string formatted_address;
                    for (int i = 0; i < 12; i += 2) {
                        if (i > 0) formatted_address += ":";
                        formatted_address += std::string(addressStr + i, 2);
                    }

                    device_map[flutter::EncodableValue("name")] =
                        flutter::EncodableValue(name_str);
                    device_map[flutter::EncodableValue("address")] =
                        flutter::EncodableValue(formatted_address);
                    device_map[flutter::EncodableValue("type")] =
                        flutter::EncodableValue("classic");
                    device_map[flutter::EncodableValue("isConnected")] =
                        flutter::EncodableValue(
                            device.ConnectionStatus() ==
                            winrt::Windows::Devices::Bluetooth::
                                BluetoothConnectionStatus::Connected);

                    devices.push_back(flutter::EncodableValue(device_map));
                }
            } catch (...) {
                // Skip devices that can't be accessed
                continue;
            }
        }
    } catch (...) {
        // Return empty list if enumeration fails
    }

    return devices;
}

flutter::EncodableList BluetoothClassicMultiplatformPlugin::StartDiscovery() {
    // For WinRT, discovery is typically done through device enumeration
    // This is similar to GetPairedDevices but may include more devices
    return GetPairedDevices();
}

winrt::Windows::Devices::Bluetooth::BluetoothDevice
BluetoothClassicMultiplatformPlugin::GetBluetoothDevice(
    const std::string& address) {
    try {
        // Convert address format from XX:XX:XX:XX:XX:XX to raw format
        std::string clean_address = address;
        clean_address.erase(
            std::remove(clean_address.begin(), clean_address.end(), ':'),
            clean_address.end());

        uint64_t bt_address = std::stoull(clean_address, nullptr, 16);

        // Get device by address
        auto selector = winrt::Windows::Devices::Bluetooth::BluetoothDevice::
            GetDeviceSelectorFromBluetoothAddress(bt_address);
        auto deviceInfos = winrt::Windows::Devices::Enumeration::
                               DeviceInformation::FindAllAsync(selector)
                                   .get();

        if (deviceInfos.Size() > 0) {
            return winrt::Windows::Devices::Bluetooth::BluetoothDevice::
                FromIdAsync(deviceInfos.GetAt(0).Id())
                    .get();
        }
    } catch (...) {
        // Return null device if not found
    }

    return nullptr;
}

winrt::Windows::Devices::Bluetooth::Rfcomm::RfcommDeviceService
BluetoothClassicMultiplatformPlugin::GetRfcommService(
    const winrt::Windows::Devices::Bluetooth::BluetoothDevice& device) {
    try {
        // Get RFCOMM services
        auto services = device.GetRfcommServicesAsync().get();

        if (services.Services().Size() > 0) {
            // Return the first available service
            return services.Services().GetAt(0);
        }
    } catch (...) {
        // Return null service if not found
    }

    return nullptr;
}

flutter::EncodableList
BluetoothClassicMultiplatformPlugin::GetConnectedDevices() {
    flutter::EncodableList devices;

    for (const auto& pair : connected_sockets_) {
        flutter::EncodableMap device;
        device[flutter::EncodableValue("address")] =
            flutter::EncodableValue(pair.first);
        device[flutter::EncodableValue("name")] =
            flutter::EncodableValue("Connected Device");
        device[flutter::EncodableValue("type")] =
            flutter::EncodableValue("classic");
        device[flutter::EncodableValue("isConnected")] =
            flutter::EncodableValue(true);

        devices.push_back(flutter::EncodableValue(device));
    }

    return devices;
}

bool BluetoothClassicMultiplatformPlugin::ConnectToDevice(
    const flutter::EncodableValue* arguments) {
    if (!arguments) {
        OutputDebugStringA("ConnectToDevice: No arguments provided\n");
        return false;
    }

    const auto* args = std::get_if<flutter::EncodableMap>(arguments);
    if (!args) {
        OutputDebugStringA("ConnectToDevice: Invalid arguments format\n");
        return false;
    }

    auto address_it = args->find(flutter::EncodableValue("address"));
    if (address_it == args->end()) {
        OutputDebugStringA("ConnectToDevice: No address provided\n");
        return false;
    }

    const auto* address_str = std::get_if<std::string>(&address_it->second);
    if (!address_str) {
        OutputDebugStringA("ConnectToDevice: Invalid address format\n");
        return false;
    }

    std::string debug_msg =
        "ConnectToDevice: Attempting to connect to " + *address_str + "\n";
    OutputDebugStringA(debug_msg.c_str());

    // Check if already connected
    if (connected_sockets_.find(*address_str) != connected_sockets_.end()) {
        OutputDebugStringA("ConnectToDevice: Device already connected\n");
        return true;
    }

    try {
        // Get Bluetooth device
        auto device = GetBluetoothDevice(*address_str);
        if (device == nullptr) {
            OutputDebugStringA("ConnectToDevice: Device not found\n");
            return false;
        }

        // Get RFCOMM service
        auto service = GetRfcommService(device);
        if (service == nullptr) {
            OutputDebugStringA("ConnectToDevice: No RFCOMM service found\n");
            return false;
        }

        // Create socket and connect
        auto socket = winrt::Windows::Networking::Sockets::StreamSocket();

        // Connect to the service
        socket
            .ConnectAsync(service.ConnectionHostName(),
                          service.ConnectionServiceName())
            .get();

        // Store successful connection
        connected_sockets_[*address_str] = socket;
        OutputDebugStringA("ConnectToDevice: Connection stored successfully\n");

        return true;
    } catch (const winrt::hresult_error& ex) {
        std::string error_msg =
            "ConnectToDevice: WinRT error: " + winrt::to_string(ex.message()) +
            "\n";
        OutputDebugStringA(error_msg.c_str());
        return false;
    } catch (...) {
        OutputDebugStringA("ConnectToDevice: Unknown error occurred\n");
        return false;
    }
}

bool BluetoothClassicMultiplatformPlugin::DisconnectDevice(
    const flutter::EncodableValue* arguments) {
    if (!arguments) {
        // Disconnect all devices
        for (auto& pair : connected_sockets_) {
            try {
                pair.second.Close();
            } catch (...) {
                // Ignore errors during cleanup
            }
        }
        connected_sockets_.clear();
        return true;
    }

    const auto* args = std::get_if<flutter::EncodableMap>(arguments);
    if (!args) return false;

    auto address_it = args->find(flutter::EncodableValue("address"));
    if (address_it == args->end()) return false;

    const auto* address_str = std::get_if<std::string>(&address_it->second);
    if (!address_str) return false;

    auto sock_it = connected_sockets_.find(*address_str);
    if (sock_it != connected_sockets_.end()) {
        try {
            sock_it->second.Close();
        } catch (...) {
            // Ignore errors during cleanup
        }
        connected_sockets_.erase(sock_it);
        return true;
    }

    return true;  // Return true even if not found (already disconnected)
}

bool BluetoothClassicMultiplatformPlugin::IsDeviceConnected(
    const flutter::EncodableValue* arguments) {
    if (!arguments) return false;

    const auto* args = std::get_if<flutter::EncodableMap>(arguments);
    if (!args) return false;

    auto address_it = args->find(flutter::EncodableValue("address"));
    if (address_it == args->end()) return false;

    const auto* address_str = std::get_if<std::string>(&address_it->second);
    if (!address_str) return false;

    return connected_sockets_.find(*address_str) != connected_sockets_.end();
}

void BluetoothClassicMultiplatformPlugin::NotifyConnectionStateChange(
    const flutter::EncodableValue* arguments, bool connected) {
    // This would typically send an event through a method channel
    // For now, we'll just update internal state
    // In a full implementation, you'd need to store channel references to send
    // events
}

void BluetoothClassicMultiplatformPlugin::StartDataListening(
    const std::string& device_address) {
    std::string debug_msg =
        "StartDataListening called for: " + device_address + "\n";
    OutputDebugStringA(debug_msg.c_str());

    // Check if already listening for this device
    if (listening_devices_.find(device_address) != listening_devices_.end()) {
        OutputDebugStringA("Data listening already active for device\n");
        return;
    }

    auto sock_it = connected_sockets_.find(device_address);
    if (sock_it == connected_sockets_.end()) {
        OutputDebugStringA(
            "Cannot start data listening - device not connected\n");
        return;
    }

    auto socket = sock_it->second;

    // Store device for data monitoring
    listening_devices_.insert(device_address);

    // Clear any existing data buffer for fresh start
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        received_data_[device_address].clear();
    }

    // Start background thread for data monitoring
    std::thread data_thread([this, device_address, socket]() {
        this->DataListeningThread(device_address, socket);
    });
    data_thread.detach();

    OutputDebugStringA("Data listening thread started for device\n");
}

void BluetoothClassicMultiplatformPlugin::DataListeningThread(
    const std::string& device_address,
    winrt::Windows::Networking::Sockets::StreamSocket socket) {
    std::string thread_debug_msg =
        "DataListeningThread: Started for device: " + device_address + "\n";
    OutputDebugStringA(thread_debug_msg.c_str());

    try {
        auto input_stream = socket.InputStream();
        auto reader =
            winrt::Windows::Storage::Streams::DataReader(input_stream);

        while (listening_devices_.find(device_address) !=
                   listening_devices_.end() &&
               connected_sockets_.find(device_address) !=
                   connected_sockets_.end()) {
            try {
                // Load data from stream
                auto load_task = reader.LoadAsync(1024);
                load_task.get();  // Wait for completion

                if (load_task.Status() ==
                    winrt::Windows::Foundation::AsyncStatus::Completed) {
                    uint32_t bytes_read = reader.UnconsumedBufferLength();

                    if (bytes_read > 0) {
                        // Read the data
                        std::vector<uint8_t> buffer(bytes_read);
                        reader.ReadBytes(buffer);

                        // Store raw received data
                        {
                            std::lock_guard<std::mutex> lock(data_mutex_);
                            received_data_[device_address].append(
                                reinterpret_cast<const char*>(buffer.data()),
                                bytes_read);
                        }

                        // Debug: Log received data
                        std::string recv_debug_msg =
                            "Received " + std::to_string(bytes_read) +
                            " bytes from " + device_address + ": ";
                        int max_chars = bytes_read < 50 ? bytes_read : 50;
                        for (int j = 0; j < max_chars; j++) {
                            if (buffer[j] >= 32 && buffer[j] <= 126) {
                                recv_debug_msg += static_cast<char>(buffer[j]);
                            } else {
                                recv_debug_msg +=
                                    "[" + std::to_string(buffer[j]) + "]";
                            }
                        }
                        if (bytes_read > 50) recv_debug_msg += "...";
                        recv_debug_msg += "\n";
                        OutputDebugStringA(recv_debug_msg.c_str());
                    }
                } else {
                    // No data available or connection closed
                    break;
                }
            } catch (...) {
                // Connection likely closed
                break;
            }

            // Use same delay as Android (10ms)
            Sleep(10);
        }
    } catch (...) {
        // Handle any exceptions
    }

    OutputDebugStringA("DataListeningThread: Ending for device\n");
    listening_devices_.erase(device_address);
}

std::string BluetoothClassicMultiplatformPlugin::ReadData(
    const flutter::EncodableValue* arguments) {
    if (!arguments) return "";

    const auto* args = std::get_if<flutter::EncodableMap>(arguments);
    if (!args) return "";

    auto address_it = args->find(flutter::EncodableValue("address"));
    if (address_it == args->end()) return "";

    const auto* address_str = std::get_if<std::string>(&address_it->second);
    if (!address_str) return "";

    std::lock_guard<std::mutex> lock(data_mutex_);
    auto data_it = received_data_.find(*address_str);
    if (data_it != received_data_.end()) {
        std::string data = data_it->second;
        if (!data.empty()) {
            data_it->second.clear();  // Clear after reading

            // Debug: Log what's being returned to Flutter
            std::string read_debug_msg = "ReadData returning " +
                                         std::to_string(data.length()) +
                                         " bytes: ";
            size_t max_chars = data.length() < 50 ? data.length() : 50;
            for (size_t k = 0; k < max_chars; k++) {
                if (data[k] >= 32 && data[k] <= 126) {
                    read_debug_msg += data[k];
                } else {
                    read_debug_msg +=
                        "[" + std::to_string((unsigned char)data[k]) + "]";
                }
            }
            if (data.length() > 50) read_debug_msg += "...";
            read_debug_msg += "\n";
            OutputDebugStringA(read_debug_msg.c_str());

            // Return raw data exactly as received - no processing
            return data;
        }
    }

    return "";
}

int BluetoothClassicMultiplatformPlugin::GetAvailableBytes(
    const flutter::EncodableValue* arguments) {
    if (!arguments) return 0;

    const auto* args = std::get_if<flutter::EncodableMap>(arguments);
    if (!args) return 0;

    auto address_it = args->find(flutter::EncodableValue("address"));
    if (address_it == args->end()) return 0;

    const auto* address_str = std::get_if<std::string>(&address_it->second);
    if (!address_str) return 0;

    std::lock_guard<std::mutex> lock(data_mutex_);
    auto data_it = received_data_.find(*address_str);
    if (data_it != received_data_.end()) {
        int available = (int)data_it->second.length();
        if (available > 0) {
            std::string debug_msg =
                "GetAvailableBytes: " + std::to_string(available) +
                " bytes available\n";
            OutputDebugStringA(debug_msg.c_str());
        }
        return available;
    }

    return 0;
}

bool BluetoothClassicMultiplatformPlugin::FlushData(
    const flutter::EncodableValue* arguments) {
    if (!arguments) return false;

    const auto* args = std::get_if<flutter::EncodableMap>(arguments);
    if (!args) return false;

    auto address_it = args->find(flutter::EncodableValue("address"));
    if (address_it == args->end()) return false;

    const auto* address_str = std::get_if<std::string>(&address_it->second);
    if (!address_str) return false;

    std::lock_guard<std::mutex> lock(data_mutex_);
    received_data_[*address_str].clear();
    return true;
}

bool BluetoothClassicMultiplatformPlugin::WriteData(
    const flutter::EncodableValue* arguments) {
    if (!arguments) return false;

    const auto* args = std::get_if<flutter::EncodableMap>(arguments);
    if (!args) return false;

    auto address_it = args->find(flutter::EncodableValue("address"));
    auto data_it = args->find(flutter::EncodableValue("data"));

    if (address_it == args->end() || data_it == args->end()) return false;

    const auto* address_str = std::get_if<std::string>(&address_it->second);
    if (!address_str) return false;

    auto sock_it = connected_sockets_.find(*address_str);
    if (sock_it == connected_sockets_.end()) return false;

    try {
        // Handle both string and binary data
        std::vector<uint8_t> data_buffer;

        if (const auto* data_str = std::get_if<std::string>(&data_it->second)) {
            data_buffer.assign(data_str->begin(), data_str->end());
        } else if (const auto* data_list =
                       std::get_if<flutter::EncodableList>(&data_it->second)) {
            // Handle binary data as list of bytes
            data_buffer.reserve(data_list->size());
            for (const auto& byte_val : *data_list) {
                if (const auto* byte_int = std::get_if<int>(&byte_val)) {
                    data_buffer.push_back(static_cast<uint8_t>(*byte_int));
                }
            }
        }

        if (!data_buffer.empty()) {
            // Create data writer and write data
            auto output_stream = sock_it->second.OutputStream();
            auto writer =
                winrt::Windows::Storage::Streams::DataWriter(output_stream);

            writer.WriteBytes(data_buffer);
            auto store_task = writer.StoreAsync();
            store_task.get();  // Wait for completion

            if (store_task.Status() ==
                winrt::Windows::Foundation::AsyncStatus::Completed) {
                std::string debug_msg = "WriteData: Sent " +
                                        std::to_string(data_buffer.size()) +
                                        " bytes to " + *address_str + "\n";
                OutputDebugStringA(debug_msg.c_str());
                return true;
            }
        }
    } catch (...) {
        OutputDebugStringA("WriteData: Error writing data\n");
    }

    return false;
}

void BluetoothClassicMultiplatformPlugin::CleanupDataChannels(
    const flutter::EncodableValue* arguments) {
    if (arguments) {
        const auto* args = std::get_if<flutter::EncodableMap>(arguments);
        if (args) {
            auto address_it = args->find(flutter::EncodableValue("address"));
            if (address_it != args->end()) {
                const auto* address_str =
                    std::get_if<std::string>(&address_it->second);
                if (address_str) {
                    // Stop data listening for this device
                    listening_devices_.erase(*address_str);

                    // Clear buffered data
                    std::lock_guard<std::mutex> lock(data_mutex_);
                    received_data_.erase(*address_str);

                    OutputDebugStringA(
                        "CleanupDataChannels: Cleaned up data channels\n");
                }
            }
        }
    } else {
        // Clean up all data channels if no specific device
        listening_devices_.clear();
        std::lock_guard<std::mutex> lock(data_mutex_);
        received_data_.clear();
        OutputDebugStringA(
            "CleanupDataChannels: Cleaned up all data channels\n");
    }
}

void BluetoothClassicMultiplatformPlugin::CancelDataChannel(
    const flutter::EncodableValue* arguments) {
    if (arguments) {
        const auto* args = std::get_if<flutter::EncodableMap>(arguments);
        if (args) {
            auto address_it = args->find(flutter::EncodableValue("address"));
            if (address_it != args->end()) {
                const auto* address_str =
                    std::get_if<std::string>(&address_it->second);
                if (address_str) {
                    // Stop listening for this device
                    listening_devices_.erase(*address_str);
                    OutputDebugStringA(
                        "CancelDataChannel: Cancelled data channel\n");
                }
            }
        }
    }
}

void BluetoothClassicMultiplatformPlugin::CloseDataChannel(
    const flutter::EncodableValue* arguments) {
    if (arguments) {
        const auto* args = std::get_if<flutter::EncodableMap>(arguments);
        if (args) {
            auto address_it = args->find(flutter::EncodableValue("address"));
            if (address_it != args->end()) {
                const auto* address_str =
                    std::get_if<std::string>(&address_it->second);
                if (address_str) {
                    // Stop listening and clear data
                    listening_devices_.erase(*address_str);

                    std::lock_guard<std::mutex> lock(data_mutex_);
                    received_data_.erase(*address_str);

                    OutputDebugStringA(
                        "CloseDataChannel: Closed data channel\n");
                }
            }
        }
    }
}

}  // namespace bluetooth_classic_multiplatform
