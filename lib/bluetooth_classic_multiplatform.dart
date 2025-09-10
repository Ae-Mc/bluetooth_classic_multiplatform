library;

import 'dart:async';

import 'package:bluetooth_classic_multiplatform/bluetooth_classic_multiplatform_method_channel.dart';
import 'package:bluetooth_classic_multiplatform/bluetooth_classic_multiplatform_platform_interface.dart';
import 'package:flutter/services.dart';

part 'exceptions.dart';
part 'models/bluetooth_bond_state.dart';
part 'models/bluetooth_connection.dart';
part 'models/bluetooth_device.dart';
part 'models/bluetooth_device_type.dart';
part 'models/bluetooth_discovery_result.dart';
part 'models/bluetooth_pairing_request.dart';
part 'models/bluetooth_state.dart';

class BluetoothClassicMultiplatform {
  /* Status */
  /// Checks is the Bluetooth interface avaliable on host device.
  Future<bool> get isAvailable =>
      BluetoothClassicMultiplatformInterface.instance.isAvailable;

  /// Describes is the Bluetooth interface enabled on host device.
  Future<bool> get isEnabled =>
      BluetoothClassicMultiplatformInterface.instance.isEnabled;

  /// Allows monitoring the Bluetooth adapter state changes.
  Stream<BluetoothState> onStateChanged() =>
      BluetoothClassicMultiplatformInterface.instance.onStateChanged();

  /// State of the Bluetooth adapter.
  Future<BluetoothState> get state =>
      BluetoothClassicMultiplatformInterface.instance.state;

  /// Returns the hardware address of the local Bluetooth adapter.
  ///
  /// Does not work for third party applications starting at Android 6.0.
  Future<String?> get address =>
      BluetoothClassicMultiplatformInterface.instance.address;

  /// Returns the friendly Bluetooth name of the local Bluetooth adapter.
  ///
  /// This name is visible to remote Bluetooth devices.
  ///
  /// Does not work for third party applications starting at Android 6.0.
  Future<String?> get name =>
      BluetoothClassicMultiplatformInterface.instance.name;

  /// Sets the friendly Bluetooth name of the local Bluetooth adapter.
  ///
  /// This name is visible to remote Bluetooth devices.
  ///
  /// Valid Bluetooth names are a maximum of 248 bytes using UTF-8 encoding,
  /// although many remote devices can only display the first 40 characters,
  /// and some may be limited to just 20.
  ///
  /// Does not work for third party applications starting at Android 6.0.
  Future<bool> setName(String name) =>
      BluetoothClassicMultiplatformInterface.instance.setName(name);

  /* Adapter settings and general */
  /// Tries to enable Bluetooth interface (if disabled).
  /// Probably results in asking user for confirmation.
  Future<bool> requestEnable() =>
      BluetoothClassicMultiplatformInterface.instance.requestEnable();

  /// Tries to disable Bluetooth interface (if enabled).
  Future<bool> requestDisable() =>
      BluetoothClassicMultiplatformInterface.instance.requestDisable();

  /// Opens the Bluetooth platform system settings.
  Future<void> openSettings() =>
      BluetoothClassicMultiplatformInterface.instance.openSettings();

  /* Discovering and bonding devices */
  /// Checks bond state for given address (might be from system cache).
  Future<BluetoothBondState> getBondStateForAddress(String address) =>
      BluetoothClassicMultiplatformInterface.instance.getBondStateForAddress(
        address,
      );

  /// Starts outgoing bonding (pairing) with device with given address.
  /// Returns true if bonded, false if canceled or failed gracefully.
  ///
  /// `pin` or `passkeyConfirm` could be used to automate the bonding process,
  /// using provided pin or confirmation if necessary. Can be used only if no
  /// pairing request handler is already registered.
  ///
  /// Note: `passkeyConfirm` will probably not work, since 3rd party apps cannot
  /// get `BLUETOOTH_PRIVILEGED` permission (at least on newest Androids).
  Future<bool> bondDeviceAtAddress(
    String address, {
    String? pin,
    bool? passkeyConfirm,
  }) => BluetoothClassicMultiplatformInterface.instance.bondDeviceAtAddress(
    address,
    pin: pin,
    passkeyConfirm: passkeyConfirm,
  );

  /// Removes bond with device with specified address.
  /// Returns true if unbonded, false if canceled or failed gracefully.
  ///
  /// Note: May not work at every Android device!
  Future<bool> removeDeviceBondWithAddress(String address) =>
      BluetoothClassicMultiplatformInterface.instance
          .removeDeviceBondWithAddress(address);

  /// Allows listening and responsing for incoming pairing requests.
  ///
  /// Various variants of pairing requests might require different returns:
  /// * `PairingVariant.Pin` or `PairingVariant.Pin16Digits`
  /// (prompt to enter a pin)
  ///   - return string containing the pin for pairing
  ///   - return `false` to reject.
  /// * `BluetoothDevice.PasskeyConfirmation`
  /// (user needs to confirm displayed passkey, no rewriting necessary)
  ///   - return `true` to accept, `false` to reject.
  ///   - there is `passkey` parameter available.
  /// * `PairingVariant.Consent`
  /// (just prompt with device name to accept without any code or passkey)
  ///   - return `true` to accept, `false` to reject.
  ///
  /// If returned null, the request will be passed for manual pairing
  /// using default Android Bluetooth settings pairing dialog.
  ///
  /// Note: Accepting request variant of `PasskeyConfirmation` and `Consent`
  /// will probably fail, because it require Android `setPairingConfirmation`
  /// which requires `BLUETOOTH_PRIVILEGED` permission that 3rd party apps
  /// cannot acquire (at least on newest Androids) due to security reasons.
  ///
  /// Note: It is necessary to return from handler within 10 seconds, since
  /// Android BroadcastReceiver can wait safely only up to that duration.
  void setPairingRequestHandler(
    Future Function(BluetoothPairingRequest request)? handler,
  ) => BluetoothClassicMultiplatformInterface.instance.setPairingRequestHandler(
    handler,
  );

  /// Returns list of bonded devices.
  Future<List<BluetoothDevice>> getBondedDevices() =>
      BluetoothClassicMultiplatformInterface.instance.getBondedDevices();

  /// Describes is the dicovery process of Bluetooth devices running.
  Future<bool?> get isDiscovering =>
      BluetoothClassicMultiplatformInterface.instance.isDiscovering;

  /// Starts discovery and provides stream of `BluetoothDiscoveryResult`s.
  Stream<BluetoothDiscoveryResult> startDiscovery() =>
      BluetoothClassicMultiplatformInterface.instance.startDiscovery();

  /// Cancels the discovery
  Future<void> cancelDiscovery() =>
      BluetoothClassicMultiplatformInterface.instance.cancelDiscovery();

  /// Describes is the local device in discoverable mode.
  Future<bool> get isDiscoverable =>
      BluetoothClassicMultiplatformInterface.instance.isDiscoverable;

  /// Asks for discoverable mode (probably always prompt for user interaction in fact).
  /// Returns number of seconds acquired or zero if canceled or failed gracefully.
  ///
  /// Duration might be capped to 120, 300 or 3600 seconds on some devices.
  Future<int?> requestDiscoverable(int durationInSeconds) =>
      BluetoothClassicMultiplatformInterface.instance.requestDiscoverable(
        durationInSeconds,
      );
  Future<bool> ensurePermissions() =>
      BluetoothClassicMultiplatformInterface.instance.ensurePermissions();

  Future<BluetoothConnection> connect(String address) =>
      BluetoothConnection.toAddress(address);
}
