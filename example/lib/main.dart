import 'package:bluetooth_classic_multiplatform_example/widgets/future_button.dart';
import 'package:flutter/material.dart';
import 'dart:async';
import 'package:bluetooth_classic_multiplatform/bluetooth_classic_multiplatform.dart';

void main() {
  runApp(const MyApp());
}

class MyApp extends StatefulWidget {
  const MyApp({super.key});

  @override
  State<MyApp> createState() => _MyAppState();
}

class _MyAppState extends State<MyApp> {
  bool? _bluetoothAvailable;
  bool? _bluetoothEnabled;
  final _bluetoothClassicMultiplatformPlugin = BluetoothClassicMultiplatform();
  final Set<BluetoothDevice> discoveredDevices = {};
  late final Stream<BluetoothDevice> discoveryStream;

  StreamSubscription<BluetoothDevice>? subs;

  BluetoothConnection? connection;

  bool _isScanning = false;

  @override
  void initState() {
    super.initState();
    discoveryStream = _bluetoothClassicMultiplatformPlugin.scanResults;
    _bluetoothClassicMultiplatformPlugin.scanResults.listen((device) {
      print("Discovered device: $device");
      setState(() {
        discoveredDevices.add(device);
      });
    });
    _bluetoothClassicMultiplatformPlugin.isScanning.listen((isScanning) {
      print("Is scanning: $isScanning");
      setState(() {
        _isScanning = isScanning;
      });
    });
    initPlatformState();
    print('State initialized');
  }

  // Platform messages are asynchronous, so we initialize in an async method.
  Future<void> initPlatformState() async {
    // Platform messages may fail, so we use a try/catch PlatformException.
    // We also handle the message potentially returning null.
    final bluetoothAvailable =
        await _bluetoothClassicMultiplatformPlugin.isSupported;
    final bluetoothEnabled =
        await _bluetoothClassicMultiplatformPlugin.isEnabled;

    // If the widget was removed from the tree while the asynchronous platform
    // message was in flight, we want to discard the reply rather than calling
    // setState to update our non-existent appearance.
    if (!mounted) return;

    setState(() {
      _bluetoothAvailable = bluetoothAvailable;
      _bluetoothEnabled = bluetoothEnabled;
    });
  }

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      home: Scaffold(
        appBar: AppBar(title: const Text('Plugin example app')),
        body: Container(
          alignment: Alignment(0.5, 0.5),
          padding: EdgeInsets.only(bottom: 16, left: 16, right: 16),
          child: Column(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              Text('''Bluetooth available: $_bluetoothAvailable
Bluetooth enabled: $_bluetoothEnabled''', textAlign: TextAlign.center),
              FutureButton(
                text: _bluetoothEnabled == true
                    ? "Выключить Bluetooth"
                    : "Включить Bluetooth",
                onPressed: () async {
                  final enabled =
                      await _bluetoothClassicMultiplatformPlugin.isEnabled;
                  _bluetoothClassicMultiplatformPlugin.requestEnable();
                  await _bluetoothClassicMultiplatformPlugin.isEnabled;
                  setState(() {
                    _bluetoothEnabled = enabled;
                  });
                },
              ),
              SizedBox(height: 16),
              Expanded(
                child: Builder(
                  builder: (context) {
                    if (_isScanning) {
                      return Center(
                        child: CircularProgressIndicator.adaptive(),
                      );
                    }
                    return ListView(
                      children: discoveredDevices
                          .map(
                            (e) => ListTile(
                              onTap: () async {
                                connection =
                                    await _bluetoothClassicMultiplatformPlugin
                                        .connect(e.address);
                              },
                              title: Text(e.name ?? ''),
                              subtitle: Text(e.address),
                              leading: Icon(
                                Icons.circle,
                                color: e.bondState == BluetoothBondState.bonded
                                    ? Colors.amber
                                    : Colors.blueGrey,
                              ),
                              trailing: Icon(
                                Icons.bluetooth,
                                color: connection?.address == e.address
                                    ? Colors.green
                                    : Colors.blueGrey,
                              ),
                            ),
                          )
                          .toList(),
                    );
                  },
                ),
              ),
              FutureButton(
                text: 'Поиск устройств',
                onPressed: () async {
                  print("Device search starting...");
                  setState(() {
                    discoveredDevices.clear();
                    _bluetoothClassicMultiplatformPlugin.startScan();
                    print("Discovery started");
                  });
                },
              ),
            ],
          ),
        ),
      ),
    );
  }
}
