import 'dart:io';

import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';

import 'bluetooth_classic_multiplatform.dart';
import 'bluetooth_classic_multiplatform_platform_interface.dart';
import 'model/bluetooth_connection.dart';
import 'model/bluetooth_device.dart';

/// An implementation of [BluetoothClassicMultiplatformPlatformInterface] that uses method channels.
class BluetoothClassicMultiplatformMethodChannel
    implements BluetoothClassicMultiplatformPlatformInterface {
  static const String namespace = "bluetooth_classic_multiplatform";

  /// The method channel used to interact with the native platform
  @visibleForTesting
  final MethodChannel methodChannel = const MethodChannel("$namespace/methods");

  /// The event channel used to get updates for the adapter state
  @visibleForTesting
  final EventChannel adapterStateEventChannel = const EventChannel(
    "$namespace/adapterState",
  );

  /// The event channel used to get updates for new discovered devices
  @visibleForTesting
  final EventChannel scanStateEventChannel = const EventChannel(
    "$namespace/discoveryState",
  );

  /// The event channel used to get updates for new discovered devices
  @visibleForTesting
  final EventChannel scanResultEventChannel = const EventChannel(
    "$namespace/scanResults",
  );

  @override
  Future<bool> isSupported() async =>
      await methodChannel.invokeMethod<bool>("isSupported") ?? false;

  @override
  Future<bool> isEnabled() async =>
      await methodChannel.invokeMethod<bool>("isEnabled") ?? false;

  @override
  Future<BluetoothAdapterState> adapterStateNow() async {
    String? state = await methodChannel.invokeMethod<String>("getAdapterState");
    return BluetoothAdapterState.values.firstWhere(
      (e) => e.name == state,
      orElse: () => BluetoothAdapterState.unknown,
    );
  }

  @override
  Stream<BluetoothAdapterState> adapterState() {
    return adapterStateEventChannel.receiveBroadcastStream().map(
      (event) => BluetoothAdapterState.values.firstWhere(
        (e) => e.name == event,
        orElse: () => BluetoothAdapterState.unknown,
      ),
    );
  }

  @override
  Future<List<BluetoothDevice>?> bondedDevices() async {
    return Platform.isAndroid
        ? (await methodChannel.invokeMethod<List>("bondedDevices") ?? [])
              .map((e) => BluetoothDevice.fromMap(e))
              .toList()
        : null;
  }

  @override
  Stream<bool> isScanning() {
    return scanStateEventChannel.receiveBroadcastStream().map(
      (event) => event == true ? true : false,
    );
  }

  @override
  Future<bool> isScanningNow() async =>
      await methodChannel.invokeMethod<bool>("isScanningNow") ?? false;

  @override
  Stream<BluetoothDevice> scanResults() {
    return scanResultEventChannel.receiveBroadcastStream().map(
      (event) => BluetoothDevice.fromMap(event),
    );
  }

  @override
  void startScan(bool usesFineLocation) {
    methodChannel.invokeMethod("startScan", {
      "usesFineLocation": usesFineLocation,
    });
  }

  @override
  void stopScan() {
    methodChannel.invokeMethod("stopScan");
  }

  @override
  Future<bool> bondDevice(String address) async => Platform.isAndroid
      ? await methodChannel.invokeMethod<bool>("bondDevice", {
              "address": address,
            }) ??
            false
      : false;

  @override
  Future<BluetoothConnection?> connect(String address, {String? uuid}) async {
    int? id = await methodChannel.invokeMethod<int>("connect", {
      "address": address,
      "uuid": uuid,
    });
    return id != null
        ? BluetoothConnection.fromConnectionId(id, address)
        : null;
  }

  @override
  Future<void> write(int id, Uint8List data) {
    return methodChannel.invokeMethod<void>("write", {"id": id, "bytes": data});
  }

  /* Adapter settings and general */
  /// Tries to enable Bluetooth interface (if disabled).
  /// Probably results in asking user for confirmation.
  @override
  void requestEnable() => methodChannel.invokeMethod('requestEnable');
}
