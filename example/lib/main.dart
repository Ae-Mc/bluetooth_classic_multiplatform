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
  final Set<BluetoothDiscoveryResult> discoveredDevices = {};
  Stream<BluetoothDiscoveryResult>? discoveryStream;

  StreamSubscription<BluetoothDevice>? subs;

  @override
  void initState() {
    super.initState();
    initPlatformState();
  }

  // Platform messages are asynchronous, so we initialize in an async method.
  Future<void> initPlatformState() async {
    // Platform messages may fail, so we use a try/catch PlatformException.
    // We also handle the message potentially returning null.
    final bluetoothAvailable =
        await _bluetoothClassicMultiplatformPlugin.isAvailable;
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
                  await _bluetoothClassicMultiplatformPlugin
                      .ensurePermissions();
                  final enabled =
                      await _bluetoothClassicMultiplatformPlugin.isEnabled;
                  if (enabled) {
                    final enabled = await _bluetoothClassicMultiplatformPlugin
                        .requestDisable();
                    setState(() {
                      _bluetoothEnabled = enabled;
                    });
                  } else {
                    final enabled = await _bluetoothClassicMultiplatformPlugin
                        .requestEnable();
                    setState(() {
                      _bluetoothEnabled = enabled;
                    });
                  }
                },
              ),
              SizedBox(height: 16),
              Expanded(
                child: StreamBuilder(
                  stream: discoveryStream,
                  builder: (context, snapshot) {
                    print(
                      "Device: ${snapshot.data}. ConnectionState: ${snapshot.connectionState}",
                    );
                    if (snapshot.connectionState == ConnectionState.waiting) {
                      return Center(
                        child: CircularProgressIndicator.adaptive(),
                      );
                    }
                    if (snapshot.hasData) {
                      discoveredDevices.add(snapshot.requireData);
                    }
                    return ListView(
                      children: discoveredDevices
                          .map(
                            (e) => ListTile(
                              onTap: () {
                                _bluetoothClassicMultiplatformPlugin.connect(
                                  e.device.address,
                                );
                              },
                              title: Text(e.device.name ?? ''),
                              subtitle: Text(e.device.address),
                              leading: Icon(
                                Icons.circle,
                                color: e.device.isBonded
                                    ? Colors.amber
                                    : Colors.blueGrey,
                              ),
                              trailing: Icon(
                                Icons.bluetooth,
                                color: e.device.isConnected
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
                    discoveryStream = _bluetoothClassicMultiplatformPlugin
                        .startDiscovery();
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
