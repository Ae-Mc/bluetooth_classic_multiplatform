part of 'bluetooth_classic_multiplatform.dart';

abstract class BluetoothClassicMultiplatformException implements Exception {
  final String message;

  BluetoothClassicMultiplatformException(this.message);
}

class PermissionDeniedException extends BluetoothClassicMultiplatformException {
  PermissionDeniedException([
    super.message = "Bluetooth permissions not granted",
  ]);
}

class BluetoothUnavailableException
    extends BluetoothClassicMultiplatformException {
  BluetoothUnavailableException([super.message = "Bluetooth not available"]);
}
