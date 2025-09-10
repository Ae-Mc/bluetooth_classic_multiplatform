#pragma comment(lib, "Bthprops.lib")

// clang-format off
#include <windows.h>
#include <bluetoothapis.h>
// clang-format on

#include <initguid.h>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/windows.Devices.Radios.h>
#include <winrt/windows.Foundation.Collections.h>
#include <ws2bth.h>

#include <chrono>
#include <coroutine>
#include <future>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <thread>

using namespace std;
using namespace winrt;
using namespace Windows::Devices::Radios;
using namespace Windows::Devices::Enumeration;
using namespace Windows::Devices::Bluetooth;
using namespace Windows::Foundation;

wostream& operator<<(wostream& out, const RadioState& value) {
#define MAPENTRY(p) \
    {               \
        p, L#p      \
    }
    const struct MapEntry {
        RadioState value;
        wstring_view str;
    } entries[] = {
        MAPENTRY(RadioState::Unknown), MAPENTRY(RadioState::On),
        MAPENTRY(RadioState::Off), MAPENTRY(RadioState::Disabled)
        // doesn't matter what is used instead of ErrorA here...
    };
#undef MAPENTRY
    const wstring_view* s = 0;
    for (const auto& entry : entries) {
        if (entry.value == value) {
            s = &entry.str;
            break;
        }
    }

    if (s == 0) {
        return out << "null";
    }
    return out << *s;
}

wostream& operator<<(wostream& out, const RadioKind& value) {
#define MAPENTRY(p) \
    {               \
        p, L#p      \
    }
    const struct MapEntry {
        RadioKind value;
        wstring_view str;
    } entries[] = {
        MAPENTRY(RadioKind::Other), MAPENTRY(RadioKind::WiFi),
        MAPENTRY(RadioKind::MobileBroadband), MAPENTRY(RadioKind::Bluetooth),
        MAPENTRY(RadioKind::FM)
        // doesn't matter what is used instead of ErrorA here...
    };
#undef MAPENTRY
    const wstring_view* s = 0;
    for (const auto& entry : entries) {
        if (entry.value == value) {
            s = &entry.str;
            break;
        }
    }

    if (s == 0) {
        return out << "null";
    }
    return out << *s;
}

// Windows::Foundation::IAsyncOperation<
//     Windows::Foundation::Collections::IVectorView<Radio>>
// get_radios_async() {
//     co_return co_await Radio::GetRadiosAsync();  // просто «пробрасываем»
//                                                  // результат
// }

void StartDiscovery() {
    BLUETOOTH_DEVICE_SEARCH_PARAMS searchParams = {0};
    searchParams.dwSize = sizeof(BLUETOOTH_DEVICE_SEARCH_PARAMS);
    searchParams.fReturnAuthenticated = TRUE;
    searchParams.fReturnRemembered = TRUE;
    searchParams.fReturnConnected = TRUE;
    searchParams.fReturnUnknown = TRUE;
    searchParams.fIssueInquiry = TRUE;
    searchParams.cTimeoutMultiplier = 1;

    BLUETOOTH_DEVICE_INFO deviceInfo = {0};
    deviceInfo.dwSize = sizeof(BLUETOOTH_DEVICE_INFO);

    HBLUETOOTH_DEVICE_FIND hFind =
        BluetoothFindFirstDevice(&searchParams, &deviceInfo);

    if (hFind != NULL) {
        do {
            // Convert wide string to UTF-8 properly
            std::wstring wname(deviceInfo.szName);
            int len = WideCharToMultiByte(CP_UTF8, 0, wname.c_str(), -1, NULL,
                                          0, NULL, NULL);
            std::string name(len - 1, 0);
            WideCharToMultiByte(CP_UTF8, 0, wname.c_str(), -1, &name[0], len,
                                NULL, NULL);

            char addressStr[18];
            sprintf_s(
                addressStr, "%02X:%02X:%02X:%02X:%02X:%02X",
                deviceInfo.Address.rgBytes[5], deviceInfo.Address.rgBytes[4],
                deviceInfo.Address.rgBytes[3], deviceInfo.Address.rgBytes[2],
                deviceInfo.Address.rgBytes[1], deviceInfo.Address.rgBytes[0]);

            cout << "Found:\n\tName: " << name << "\n\tAddress: " << addressStr
                 << "\n\tConnected: " << (deviceInfo.fConnected == TRUE)
                 << "\n\tPaired: " << (deviceInfo.fAuthenticated == TRUE)
                 << endl;

        } while (BluetoothFindNextDevice(hFind, &deviceInfo));

        BluetoothFindDeviceClose(hFind);
    }
}

IAsyncAction StartDiscoveryAsync() {
    auto selector = BluetoothDevice::GetDeviceSelector();
    auto devices = co_await DeviceInformation::FindAllAsync(selector);

    for (auto const& deviceInfo : devices) {
        auto name = to_string(deviceInfo.Name().c_str());
        auto id = to_string(deviceInfo.Id().c_str());
        bool paired = deviceInfo.Pairing().IsPaired();

        cout << "Found:\n\tName: " << name << "\n\tId: " << id
             << "\n\tPaired: " << paired << endl;

        // Если нужно — можно получить BluetoothDevice по Id
        try {
            BluetoothDevice btDevice =
                co_await BluetoothDevice::FromIdAsync(deviceInfo.Id());
            bool connected = btDevice.ConnectionStatus() ==
                             BluetoothConnectionStatus::Connected;

            cout << "\tConnected: " << connected << endl;
        } catch (...) {
            cerr << "\tFailed to access device." << endl;
        }
    }
}

int main() {
    winrt::init_apartment();
    auto radios_vec = Radio::GetRadiosAsync().get();
    wcout << L"Radios count: " << radios_vec.Size() << endl;
    for (int i = 0; i < radios_vec.Size(); i++) {
        auto radio = radios_vec.GetAt(i);
        wcout << L"\"" << radio.Name().c_str() << L"\" \"" << radio.Kind()
              << L"\" \"" << radio.State() << L"\"" << endl;
    }
    cout << "Default search" << endl;
    StartDiscovery();
    cout << "Paired" << endl;
    StartDiscoveryAsync().get();
    return 0;
}