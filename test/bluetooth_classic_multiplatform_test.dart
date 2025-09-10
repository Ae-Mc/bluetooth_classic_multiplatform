import 'package:flutter_test/flutter_test.dart';
import 'package:bluetooth_classic_multiplatform/bluetooth_classic_multiplatform.dart';
import 'package:bluetooth_classic_multiplatform/bluetooth_classic_multiplatform_platform_interface.dart';
import 'package:bluetooth_classic_multiplatform/bluetooth_classic_multiplatform_method_channel.dart';
import 'package:plugin_platform_interface/plugin_platform_interface.dart';

class MockBluetoothClassicMultiplatformPlatform
    with MockPlatformInterfaceMixin
    implements BluetoothClassicMultiplatformInterface {
  @override
  Future<String?> getPlatformVersion() => Future.value('42');
}

void main() {
  final BluetoothClassicMultiplatformInterface initialPlatform =
      BluetoothClassicMultiplatformInterface.instance;

  test(
    '$MethodChannelBluetoothClassicMultiplatform is the default instance',
    () {
      expect(
        initialPlatform,
        isInstanceOf<MethodChannelBluetoothClassicMultiplatform>(),
      );
    },
  );

  test('getPlatformVersion', () async {
    BluetoothClassicMultiplatform bluetoothClassicMultiplatformPlugin =
        BluetoothClassicMultiplatform();
    MockBluetoothClassicMultiplatformPlatform fakePlatform =
        MockBluetoothClassicMultiplatformPlatform();
    BluetoothClassicMultiplatformInterface.instance = fakePlatform;

    expect(
      await bluetoothClassicMultiplatformPlugin.getPlatformVersion(),
      '42',
    );
  });
}
