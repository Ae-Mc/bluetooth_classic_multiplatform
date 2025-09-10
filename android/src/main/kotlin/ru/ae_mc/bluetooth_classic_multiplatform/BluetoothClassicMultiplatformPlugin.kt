package ru.ae_mc.bluetooth_classic_multiplatform

import android.Manifest
import android.annotation.SuppressLint
import android.app.Activity
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothManager
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.pm.PackageManager
import android.os.AsyncTask
import android.os.Build
import android.provider.Settings
import android.util.Log
import android.util.SparseArray
import androidx.annotation.RequiresApi
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import io.flutter.embedding.engine.plugins.FlutterPlugin
import io.flutter.embedding.engine.plugins.FlutterPlugin.FlutterPluginBinding
import io.flutter.embedding.engine.plugins.activity.ActivityAware
import io.flutter.embedding.engine.plugins.activity.ActivityPluginBinding
import io.flutter.plugin.common.BinaryMessenger
import io.flutter.plugin.common.EventChannel
import io.flutter.plugin.common.EventChannel.EventSink
import io.flutter.plugin.common.MethodCall
import io.flutter.plugin.common.MethodChannel
import io.flutter.plugin.common.MethodChannel.MethodCallHandler
import java.io.PrintWriter
import java.io.StringWriter
import java.net.NetworkInterface
import ru.ae_mc.bluetooth_classic_multiplatform.BluetoothConnectionBase.OnDisconnectedCallback
import ru.ae_mc.bluetooth_classic_multiplatform.BluetoothConnectionBase.OnReadCallback

class BluetoothClassicMultiplatformPlugin : FlutterPlugin, ActivityAware {
    companion object {
        // Plugin
        private const val TAG = "BluetoothClassicMultiplatform"
        private const val PLUGIN_NAMESPACE = "bluetooth_classic_multiplatform"

        // Permissions and request constants
        private const val REQUEST_COARSE_LOCATION_PERMISSIONS = 1451
        private const val REQUEST_ENABLE_BLUETOOTH = 1337
        private const val REQUEST_DISCOVERABLE_BLUETOOTH = 2137

        /** Helper function to get string out of exception */
        private fun exceptionToString(ex: Exception): String {
            val sw = StringWriter()
            val pw = PrintWriter(sw)
            ex.printStackTrace(pw)
            return sw.toString()
        }

        /** Helper function to check is device connected */
        private fun checkIsDeviceConnected(device: BluetoothDevice): Boolean {
            try {
                val method = device.javaClass.getMethod("isConnected")
                return method.invoke(device) as Boolean
            } catch (ex: Exception) {
                return false
            }
        }
    }

    private var methodChannel: MethodChannel? = null
    private var pendingResultForActivityResult: MethodChannel.Result? = null

    // General Bluetooth
    private var bluetoothAdapter: BluetoothAdapter? = null

    // State
    private val stateReceiver: BroadcastReceiver
    private var stateSink: EventSink? = null

    // Pairing requests
    private val pairingRequestReceiver: BroadcastReceiver
    private var isPairingRequestHandlerSet = false
    private var bondStateBroadcastReceiver: BroadcastReceiver? = null

    private var discoverySink: EventSink? = null
    private val discoveryReceiver: BroadcastReceiver

    // Connections
    /** Contains all active connections. Maps ID of the connection with plugin data channels. */
    private val connections = SparseArray<BluetoothConnection?>(2)

    /** Last ID given to any connection, used to avoid duplicate IDs */
    private var lastConnectionId = 0
    private var activity: Activity? = null
    private var messenger: BinaryMessenger? = null
    private var activeContext: Context? = null

    override fun onAttachedToEngine(binding: FlutterPluginBinding) {
        Log.v(TAG, "Attached to engine")
        //        if (true) throw new RuntimeException("FlutterBluetoothSerial Attached to engine");
        messenger = binding.binaryMessenger

        methodChannel = MethodChannel(messenger!!, PLUGIN_NAMESPACE + "/methods")
        methodChannel!!.setMethodCallHandler(FlutterBluetoothSerialMethodCallHandler())

        val stateChannel = EventChannel(messenger, PLUGIN_NAMESPACE + "/state")

        stateChannel.setStreamHandler(
                object : EventChannel.StreamHandler {
                    override fun onListen(o: Any, eventSink: EventSink) {
                        stateSink = eventSink

                        activeContext!!.registerReceiver(
                                stateReceiver,
                                IntentFilter(BluetoothAdapter.ACTION_STATE_CHANGED)
                        )
                    }

                    override fun onCancel(o: Any) {
                        stateSink = null
                        try {
                            activeContext!!.unregisterReceiver(stateReceiver)
                        } catch (ex: IllegalArgumentException) {
                            // Ignore `Receiver not registered` exception
                        }
                    }
                }
        )

        // Discovery
        val discoveryChannel = EventChannel(messenger, PLUGIN_NAMESPACE + "/discovery")

        // Ignore `Receiver not registered` exception
        val discoveryStreamHandler: EventChannel.StreamHandler =
                object : EventChannel.StreamHandler {
                    override fun onListen(o: Any?, eventSink: EventSink) {
                        discoverySink = eventSink
                    }

                    override fun onCancel(o: Any?) {
                        Log.d(TAG, "Canceling discovery (stream closed)")
                        try {
                            activeContext!!.unregisterReceiver(discoveryReceiver)
                        } catch (ex: IllegalArgumentException) {
                            // Ignore `Receiver not registered` exception
                        }

                        bluetoothAdapter!!.cancelDiscovery()

                        if (discoverySink != null) {
                            discoverySink!!.endOfStream()
                            discoverySink = null
                        }
                    }
                }
        discoveryChannel.setStreamHandler(discoveryStreamHandler)
    }

    override fun onDetachedFromEngine(binding: FlutterPluginBinding) {
        if (methodChannel != null) methodChannel!!.setMethodCallHandler(null)
    }

    override fun onAttachedToActivity(binding: ActivityPluginBinding) {
        //        if (true) throw new RuntimeException("FlutterBluetoothSerial Attached to
        // activity");
        this.activity = binding.activity
        val bluetoothManager =
                checkNotNull(
                        activity!!.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
                )
        this.bluetoothAdapter = bluetoothManager.adapter

        binding.addActivityResultListener { requestCode: Int, resultCode: Int, data: Intent? ->
            when (requestCode) {
                REQUEST_ENABLE_BLUETOOTH -> {
                    // @TODO - used underlying value of `Activity.RESULT_CANCELED` since we tend to
                    // use `androidx` in which I were not able to find the constant.
                    if (pendingResultForActivityResult != null) {
                        pendingResultForActivityResult!!.success(resultCode != 0)
                    }
                    return@addActivityResultListener true
                }
                REQUEST_DISCOVERABLE_BLUETOOTH -> {
                    pendingResultForActivityResult!!.success(
                            if (resultCode == 0) -1 else resultCode
                    )
                    return@addActivityResultListener true
                }
                else -> return@addActivityResultListener false
            }
        }
        binding.addRequestPermissionsResultListener {
                requestCode: Int,
                permissions: Array<String?>?,
                grantResults: IntArray ->
            when (requestCode) {
                REQUEST_COARSE_LOCATION_PERMISSIONS -> {
                    pendingPermissionsEnsureCallbacks!!.onResult(
                            grantResults[0] == PackageManager.PERMISSION_GRANTED
                    )
                    pendingPermissionsEnsureCallbacks = null
                    return@addRequestPermissionsResultListener true
                }
            }
            false
        }
        activity = binding.activity
        activeContext = binding.activity.applicationContext
    }

    override fun onDetachedFromActivityForConfigChanges() {}

    override fun onReattachedToActivityForConfigChanges(binding: ActivityPluginBinding) {}

    override fun onDetachedFromActivity() {}

    fun interface EnsurePermissionsCallback {
        fun onResult(granted: Boolean)
    }

    var pendingPermissionsEnsureCallbacks: EnsurePermissionsCallback? = null

    /** Constructs the plugin instance */
    init {
        // State

        stateReceiver =
                object : BroadcastReceiver() {
                    override fun onReceive(context: Context, intent: Intent) {
                        if (stateSink == null) {
                            return
                        }

                        val action = intent.action
                        when (action) {
                            BluetoothAdapter.ACTION_STATE_CHANGED -> {
                                // Disconnect all connections
                                val size = connections.size()
                                var i = 0
                                while (i < size) {
                                    val connection = connections.valueAt(i)
                                    connection!!.disconnect()
                                    i++
                                }
                                connections.clear()

                                stateSink!!.success(
                                        intent.getIntExtra(
                                                BluetoothAdapter.EXTRA_STATE,
                                                BluetoothDevice.ERROR
                                        )
                                )
                            }
                        }
                    }
                }

        // Pairing requests
        pairingRequestReceiver =
                object : BroadcastReceiver() {
                    override fun onReceive(context: Context, intent: Intent) {
                        when (intent.action) {
                            BluetoothDevice.ACTION_PAIRING_REQUEST -> {
                                val device =
                                        intent.getParcelableExtra<BluetoothDevice>(
                                                BluetoothDevice.EXTRA_DEVICE
                                        )
                                val pairingVariant =
                                        intent.getIntExtra(
                                                BluetoothDevice.EXTRA_PAIRING_VARIANT,
                                                BluetoothDevice.ERROR
                                        )
                                Log.d(
                                        TAG,
                                        "Pairing request (variant " +
                                                pairingVariant +
                                                ") incoming from " +
                                                device!!.address
                                )
                                when (pairingVariant) {
                                    BluetoothDevice
                                            .PAIRING_VARIANT_PIN -> // Simplest method - 4 digit
                                        // number
                                    {
                                        val broadcastResult = this.goAsync()

                                        val arguments: MutableMap<String, Any> = HashMap()
                                        arguments["address"] = device.address
                                        arguments["variant"] = pairingVariant

                                        methodChannel!!.invokeMethod(
                                                "handlePairingRequest",
                                                arguments,
                                                object : MethodChannel.Result {
                                                    override fun success(handlerResult: Any?) {
                                                        Log.d(TAG, handlerResult.toString())
                                                        if (handlerResult is String) {
                                                            try {
                                                                val passkeyString = handlerResult
                                                                val passkey =
                                                                        passkeyString.toByteArray()
                                                                Log.d(
                                                                        TAG,
                                                                        "Trying to set passkey for pairing to $passkeyString"
                                                                )
                                                                device.setPin(passkey)
                                                                broadcastResult.abortBroadcast()
                                                            } catch (ex: Exception) {
                                                                Log.e(TAG, ex.message!!)
                                                                ex.printStackTrace()
                                                                // @TODO , passing the error
                                                                // result.error("bond_error",
                                                                // "Setting passkey for pairing
                                                                // failed", exceptionToString(ex));
                                                            }
                                                        } else {
                                                            Log.d(
                                                                    TAG,
                                                                    "Manual pin pairing in progress"
                                                            )
                                                            // Intent intent = new
                                                            // Intent(BluetoothAdapter.ACTION_PAIRING_REQUEST);
                                                            // intent.putExtra(BluetoothDevice.EXTRA_DEVICE, device);
                                                            // intent.putExtra(BluetoothDevice.EXTRA_PAIRING_VARIANT, pairingVariant)
                                                            ActivityCompat.startActivity(
                                                                    activity!!,
                                                                    intent,
                                                                    null
                                                            )
                                                        }
                                                        broadcastResult.finish()
                                                    }

                                                    override fun notImplemented() {
                                                        throw UnsupportedOperationException()
                                                    }

                                                    override fun error(
                                                            code: String,
                                                            message: String?,
                                                            details: Any?
                                                    ) {
                                                        throw UnsupportedOperationException()
                                                    }
                                                }
                                        )
                                    }
                                    BluetoothDevice.PAIRING_VARIANT_PASSKEY_CONFIRMATION,
                                    3 -> // The simplest, but much less secure method - just yes or
                                        // no, without any auth.
                                        // Consent type can use same code as passkey confirmation
                                        // since passed passkey,
                                        // which is 0 or error at the moment, should not be used
                                        // anyway by common code.
                                    {
                                        val pairingKey =
                                                intent.getIntExtra(
                                                        BluetoothDevice.EXTRA_PAIRING_KEY,
                                                        BluetoothDevice.ERROR
                                                )

                                        val arguments: MutableMap<String, Any> = HashMap()
                                        arguments["address"] = device.address
                                        arguments["variant"] = pairingVariant
                                        arguments["pairingKey"] = pairingKey

                                        val broadcastResult = this.goAsync()
                                        methodChannel!!.invokeMethod(
                                                "handlePairingRequest",
                                                arguments,
                                                object : MethodChannel.Result {
                                                    @SuppressLint("MissingPermission")
                                                    override fun success(handlerResult: Any?) {
                                                        if (handlerResult is Boolean) {
                                                            try {
                                                                val confirm = handlerResult
                                                                Log.d(
                                                                        TAG,
                                                                        "Trying to set pairing confirmation to $confirm (key: $pairingKey)"
                                                                )
                                                                // @WARN `BLUETOOTH_PRIVILEGED`
                                                                // permission required, but might be
                                                                // unavailable for thrid party apps
                                                                // on newer versions of Androids.
                                                                device.setPairingConfirmation(
                                                                        confirm
                                                                )
                                                                broadcastResult.abortBroadcast()
                                                            } catch (ex: Exception) {
                                                                Log.e(TAG, ex.message!!)
                                                                ex.printStackTrace()
                                                                // @TODO , passing the error
                                                                // result.error("bond_error",
                                                                // "Auto-confirming pass key
                                                                // failed", exceptionToString(ex));
                                                            }
                                                        } else {
                                                            Log.d(
                                                                    TAG,
                                                                    "Manual passkey confirmation pairing in progress (key: $pairingKey)"
                                                            )
                                                            ActivityCompat.startActivity(
                                                                    activity!!,
                                                                    intent,
                                                                    null
                                                            )
                                                        }
                                                        broadcastResult.finish()
                                                    }

                                                    override fun notImplemented() {
                                                        throw UnsupportedOperationException()
                                                    }

                                                    override fun error(
                                                            code: String,
                                                            message: String?,
                                                            details: Any?
                                                    ) {
                                                        Log.e(TAG, "$code $message")
                                                        throw UnsupportedOperationException()
                                                    }
                                                }
                                        )
                                    }
                                    4, 5 -> // Same as previous, but for 4 digit pin.
                                    {
                                        val pairingKey =
                                                intent.getIntExtra(
                                                        BluetoothDevice.EXTRA_PAIRING_KEY,
                                                        BluetoothDevice.ERROR
                                                )

                                        val arguments: MutableMap<String, Any> = HashMap()
                                        arguments["address"] = device.address
                                        arguments["variant"] = pairingVariant
                                        arguments["pairingKey"] = pairingKey

                                        methodChannel!!.invokeMethod(
                                                "handlePairingRequest",
                                                arguments
                                        )
                                    }
                                    else -> // Only log other pairing variants
                                    Log.w(TAG, "Unknown pairing variant: $pairingVariant")
                                }
                            }
                            else -> {}
                        }
                    }
                }

        // Discovery
        discoveryReceiver =
                object : BroadcastReceiver() {
                    override fun onReceive(context: Context, intent: Intent) {
                        val action = intent.action
                        when (action) {
                            BluetoothDevice.ACTION_FOUND -> {
                                val device =
                                        intent.getParcelableExtra<BluetoothDevice>(
                                                BluetoothDevice.EXTRA_DEVICE
                                        )
                                // final BluetoothClass deviceClass =
                                // intent.getParcelableExtra(BluetoothDevice.EXTRA_CLASS); // @TODO
                                // . !BluetoothClass!
                                // final String extraName =
                                // intent.getStringExtra(BluetoothDevice.EXTRA_NAME); // @TODO ?
                                // !EXTRA_NAME!
                                val deviceRSSI =
                                        intent.getShortExtra(
                                                        BluetoothDevice.EXTRA_RSSI,
                                                        Short.MIN_VALUE
                                                )
                                                .toInt()

                                val discoveryResult: MutableMap<String, Any> = HashMap()
                                discoveryResult["address"] = device!!.address
                                discoveryResult["name"] = device.name
                                discoveryResult["type"] = device.type
                                // discoveryResult.put("class", deviceClass); // @TODO . it isn't my
                                // priority for now !BluetoothClass!
                                discoveryResult["isConnected"] = checkIsDeviceConnected(device)
                                discoveryResult["bondState"] = device.bondState
                                discoveryResult["rssi"] = deviceRSSI

                                Log.d(TAG, "Discovered " + device.address)
                                if (discoverySink != null) {
                                    discoverySink!!.success(discoveryResult)
                                }
                            }
                            BluetoothAdapter.ACTION_DISCOVERY_FINISHED -> {
                                Log.d(TAG, "Discovery finished")
                                try {
                                    context.unregisterReceiver(this)
                                } catch (ex: IllegalArgumentException) {
                                    // Ignore `Receiver not registered` exception
                                }

                                bluetoothAdapter!!.cancelDiscovery()

                                if (discoverySink != null) {
                                    discoverySink!!.endOfStream()
                                    discoverySink = null
                                }
                            }
                            else -> {}
                        }
                    }
                }
    }

    @RequiresApi(Build.VERSION_CODES.S)
    private fun ensurePermissions(callbacks: EnsurePermissionsCallback) {
        if ((ContextCompat.checkSelfPermission(
                        activity!!,
                        Manifest.permission.ACCESS_COARSE_LOCATION
                ) != PackageManager.PERMISSION_GRANTED) ||
                        (ContextCompat.checkSelfPermission(
                                activity!!,
                                Manifest.permission.ACCESS_FINE_LOCATION
                        ) != PackageManager.PERMISSION_GRANTED) ||
                        (ContextCompat.checkSelfPermission(
                                activity!!,
                                Manifest.permission.BLUETOOTH_SCAN
                        ) != PackageManager.PERMISSION_GRANTED) ||
                        (ContextCompat.checkSelfPermission(
                                activity!!,
                                Manifest.permission.BLUETOOTH_ADVERTISE
                        ) != PackageManager.PERMISSION_GRANTED) ||
                        (ContextCompat.checkSelfPermission(
                                activity!!,
                                Manifest.permission.BLUETOOTH_CONNECT
                        ) != PackageManager.PERMISSION_GRANTED)
        ) {
            ActivityCompat.requestPermissions(
                    activity!!,
                    arrayOf(
                            Manifest.permission.ACCESS_COARSE_LOCATION,
                            Manifest.permission.ACCESS_FINE_LOCATION,
                            Manifest.permission.BLUETOOTH_SCAN,
                            Manifest.permission.BLUETOOTH_ADVERTISE,
                            Manifest.permission.BLUETOOTH_CONNECT
                    ),
                    REQUEST_COARSE_LOCATION_PERMISSIONS
            )

            pendingPermissionsEnsureCallbacks = callbacks
        } else {
            callbacks.onResult(true)
        }
    }

    private inner class FlutterBluetoothSerialMethodCallHandler : MethodCallHandler {
        /** Provides access to the plugin methods */
        @RequiresApi(Build.VERSION_CODES.S)
        override fun onMethodCall(call: MethodCall, result: MethodChannel.Result) {
            if (bluetoothAdapter == null) {
                if ("isAvailable" == call.method) {
                    result.success(false)
                } else {
                    result.error("bluetooth_unavailable", "bluetooth is not available", null)
                }
                return
            }

            fun getAddress(): String {
                try {
                    val addr = call.argument("address") as String?
                    if (!BluetoothAdapter.checkBluetoothAddress(addr)) {
                        throw ClassCastException()
                    }
                    return addr!!
                } catch (ex: ClassCastException) {
                    result.error(
                            "invalid_argument",
                            "'address' argument is required to be string containing remote MAC address",
                            null
                    )
                    throw ex
                }
            }

            methodCallDispatching@ when (call.method) {
                "isAvailable" -> result.success(true)
                "isOn", "isEnabled" -> result.success(bluetoothAdapter!!.isEnabled)
                "openSettings" -> {
                    ContextCompat.startActivity(
                            activity!!,
                            Intent(Settings.ACTION_BLUETOOTH_SETTINGS),
                            null
                    )
                    result.success(null)
                }
                "requestEnable" ->
                        if (!bluetoothAdapter!!.isEnabled) {
                            pendingResultForActivityResult = result
                            val intent = Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE)
                            ActivityCompat.startActivityForResult(
                                    activity!!,
                                    intent,
                                    REQUEST_ENABLE_BLUETOOTH,
                                    null
                            )
                        } else {
                            result.success(true)
                        }
                "requestDisable" ->
                        if (bluetoothAdapter!!.isEnabled) {
                            bluetoothAdapter!!.disable()
                            result.success(true)
                        } else {
                            result.success(false)
                        }
                "ensurePermissions" ->
                        ensurePermissions(
                                EnsurePermissionsCallback { innerResult: Boolean ->
                                    result.success(innerResult)
                                }
                        )
                "getState" -> result.success(bluetoothAdapter!!.state)
                "getAddress" -> {
                    @SuppressLint("MissingPermission", "HardwareIds")
                    var address = bluetoothAdapter!!.address

                    if (address == "02:00:00:00:00:00") {
                        Log.w(
                                TAG,
                                "Local Bluetooth MAC address is hidden by system, trying other options..."
                        )

                        do {
                            Log.d(TAG, "Trying to obtain address using Settings Secure bank")
                            try {
                                // Requires `LOCAL_MAC_ADDRESS` which could be unavailible for third
                                // party applications...
                                val value =
                                        Settings.Secure.getString(
                                                activeContext!!.contentResolver,
                                                "bluetooth_address"
                                        )
                                if (value == null) {
                                    throw NullPointerException(
                                            "null returned, might be no permissions problem"
                                    )
                                }
                                address = value
                                break
                            } catch (ex: Exception) {
                                // Ignoring failure (since it isn't critical API for most
                                // applications)
                                Log.d(TAG, "Obtaining address using Settings Secure bank failed")
                                // result.error("hidden_address", "obtaining address using Settings
                                // Secure bank failed", exceptionToString(ex));
                            }

                            Log.d(
                                    TAG,
                                    "Trying to obtain address using reflection against internal Android code"
                            )
                            try {
                                // This will most likely work, but well, it is unsafe
                                val mServiceField =
                                        bluetoothAdapter!!.javaClass.getDeclaredField("mService")
                                mServiceField.isAccessible = true

                                val bluetoothManagerService = mServiceField[bluetoothAdapter]
                                if (bluetoothManagerService == null) {
                                    if (!bluetoothAdapter!!.isEnabled) {
                                        Log.d(
                                                TAG,
                                                "Probably failed just because adapter is disabled!"
                                        )
                                    }
                                    throw NullPointerException()
                                }
                                val getAddressMethod =
                                        bluetoothManagerService.javaClass.getMethod("getAddress")
                                val value =
                                        getAddressMethod.invoke(bluetoothManagerService) as String
                                                ?: throw NullPointerException()
                                address = value
                                Log.d(TAG, "Probably succed: $address ✨ :F")
                                break
                            } catch (ex: Exception) {
                                // Ignoring failure (since it isn't critical API for most
                                // applications)
                                Log.d(
                                        TAG,
                                        "Obtaining address using reflection against internal Android code failed"
                                )
                                // result.error("hidden_address", "obtaining address using
                                // reflection agains internal Android code failed",
                                // exceptionToString(ex));
                            }

                            Log.d(
                                    TAG,
                                    "Trying to look up address by network interfaces - might be invalid on some devices"
                            )
                            try {
                                // This method might return invalid MAC address (since Bluetooth
                                // might use other address than WiFi).
                                // @TODO . further testing: 1) check is while open connection, 2)
                                // check other devices
                                val interfaces = NetworkInterface.getNetworkInterfaces()
                                var value: String? = null
                                while (interfaces.hasMoreElements()) {
                                    val networkInterface = interfaces.nextElement()
                                    val name = networkInterface.name

                                    if (!name.equals("wlan0", ignoreCase = true)) {
                                        continue
                                    }

                                    val addressBytes = networkInterface.hardwareAddress
                                    if (addressBytes != null) {
                                        val addressBuilder = StringBuilder(18)
                                        for (b in addressBytes) {
                                            addressBuilder.append(String.format("%02X:", b))
                                        }
                                        addressBuilder.setLength(17)
                                        value = addressBuilder.toString()
                                        //     Log.v(TAG, "-> '" + name + "' : " + value);
                                        // }
                                        // else {
                                        //    Log.v(TAG, "-> '" + name + "' : <no hardware
                                        // address>");
                                    }
                                }
                                if (value == null) {
                                    throw NullPointerException()
                                }
                                address = value
                            } catch (ex: Exception) {
                                // Ignoring failure (since it isn't critical API for most
                                // applications)
                                Log.w(TAG, "Looking for address by network interfaces failed")
                                // result.error("hidden_address", "looking for address by network
                                // interfaces failed", exceptionToString(ex));
                            }
                        } while (false)
                    }
                    result.success(address)
                }
                "getName" -> result.success(bluetoothAdapter!!.name)
                "setName" -> {
                    if (!call.hasArgument("name")) {
                        result.error("invalid_argument", "argument 'name' not found", null)
                    }

                    val name: String?
                    try {
                        name = call.argument("name")
                        result.success(bluetoothAdapter!!.setName(name))
                    } catch (ex: ClassCastException) {
                        result.error(
                                "invalid_argument",
                                "'name' argument is required to be string",
                                null
                        )
                    }
                }
                "getDeviceBondState" -> {
                    if (!call.hasArgument("address")) {
                        result.error("invalid_argument", "argument 'address' not found", null)
                    }

                    val address = getAddress()

                    val device = bluetoothAdapter!!.getRemoteDevice(address)
                    result.success(device.bondState)
                }
                "removeDeviceBond" -> {
                    if (!call.hasArgument("address")) {
                        result.error("invalid_argument", "argument 'address' not found", null)
                    }

                    val address = getAddress()
                    val device = bluetoothAdapter!!.getRemoteDevice(address)
                    when (device.bondState) {
                        BluetoothDevice.BOND_BONDING -> {
                            result.error("bond_error", "device already bonding", null)
                        }
                        BluetoothDevice.BOND_NONE -> {
                            result.error("bond_error", "device already unbonded", null)
                        }
                        else -> {}
                    }

                    try {
                        val method = device.javaClass.getMethod("removeBond")
                        val value = method.invoke(device) as Boolean
                        result.success(value)
                    } catch (ex: Exception) {
                        result.error("bond_error", "error while unbonding", exceptionToString(ex))
                    }
                }
                "bondDevice" -> {
                    if (!call.hasArgument("address")) {
                        result.error("invalid_argument", "argument 'address' not found", null)
                    }

                    val address = getAddress()

                    if (bondStateBroadcastReceiver != null) {
                        result.error(
                                "bond_error",
                                "another bonding process is ongoing from local device",
                                null
                        )
                    }

                    val device = bluetoothAdapter!!.getRemoteDevice(address)
                    when (device.bondState) {
                        BluetoothDevice.BOND_BONDING -> {
                            result.error("bond_error", "device already bonding", null)
                        }
                        BluetoothDevice.BOND_BONDED -> {
                            result.error("bond_error", "device already bonded", null)
                        }
                        else -> {}
                    }

                    bondStateBroadcastReceiver =
                            object : BroadcastReceiver() {
                                override fun onReceive(context: Context, intent: Intent) {
                                    when (intent.action) {
                                        BluetoothDevice.ACTION_BOND_STATE_CHANGED -> {
                                            val someDevice =
                                                    intent.getParcelableExtra<BluetoothDevice>(
                                                            BluetoothDevice.EXTRA_DEVICE
                                                    )
                                            if (someDevice != device) {
                                                return
                                            }

                                            val newBondState =
                                                    intent.getIntExtra(
                                                            BluetoothDevice.EXTRA_BOND_STATE,
                                                            BluetoothDevice.ERROR
                                                    )
                                            when (newBondState) {
                                                BluetoothDevice
                                                        .BOND_BONDING -> // Wait for true bond
                                                        // result :F
                                                        return
                                                BluetoothDevice.BOND_BONDED -> result.success(true)
                                                BluetoothDevice.BOND_NONE -> result.success(false)
                                                else ->
                                                        result.error(
                                                                "bond_error",
                                                                "invalid bond state while bonding",
                                                                null
                                                        )
                                            }
                                            activeContext!!.unregisterReceiver(this)
                                            bondStateBroadcastReceiver = null
                                        }
                                        else -> {}
                                    }
                                }
                            }

                    val filter = IntentFilter(BluetoothDevice.ACTION_BOND_STATE_CHANGED)
                    // filter.setPriority(pairingRequestReceiverPriority + 1);
                    activeContext!!.registerReceiver(bondStateBroadcastReceiver, filter)

                    if (!device.createBond()) {
                        result.error("bond_error", "error starting bonding process", null)
                    }
                }
                "pairingRequestHandlingEnable" -> {
                    if (this@BluetoothClassicMultiplatformPlugin.isPairingRequestHandlerSet) {
                        result.error(
                                "logic_error",
                                "pairing request handling is already enabled",
                                null
                        )
                        return
                    }
                    Log.d(TAG, "Starting listening for pairing requests to handle")

                    this@BluetoothClassicMultiplatformPlugin.isPairingRequestHandlerSet = true
                    val filter = IntentFilter(BluetoothDevice.ACTION_PAIRING_REQUEST)
                    // filter.setPriority(pairingRequestReceiverPriority);
                    activeContext!!.registerReceiver(pairingRequestReceiver, filter)
                }
                "pairingRequestHandlingDisable" -> {
                    this@BluetoothClassicMultiplatformPlugin.isPairingRequestHandlerSet = false
                    try {
                        activeContext!!.unregisterReceiver(pairingRequestReceiver)
                        Log.d(TAG, "Stopped listening for pairing requests to handle")
                    } catch (ex: IllegalArgumentException) {
                        // Ignore `Receiver not registered` exception
                    }
                }
                "getBondedDevices" ->
                        ensurePermissions { granted: Boolean ->
                            if (!granted) {
                                result.error(
                                        "no_permissions",
                                        "discovering other devices requires location access permission",
                                        null
                                )
                                return@ensurePermissions
                            }
                            val list: MutableList<Map<String, Any>> = ArrayList()
                            for (device in bluetoothAdapter!!.bondedDevices) {
                                val entry: MutableMap<String, Any> = HashMap()
                                entry["address"] = device.address
                                entry["name"] = device.name
                                entry["type"] = device.type
                                entry["isConnected"] = checkIsDeviceConnected(device)
                                entry["bondState"] = BluetoothDevice.BOND_BONDED
                                list.add(entry)
                            }
                            result.success(list)
                        }
                "isDiscovering" -> result.success(bluetoothAdapter!!.isDiscovering)
                "startDiscovery" ->
                        ensurePermissions { granted: Boolean ->
                            if (!granted) {
                                result.error(
                                        "no_permissions",
                                        "discovering other devices requires location access permission",
                                        null
                                )
                                return@ensurePermissions
                            }
                            Log.d(TAG, "Starting discovery")
                            val intent = IntentFilter()
                            intent.addAction(BluetoothAdapter.ACTION_DISCOVERY_FINISHED)
                            intent.addAction(BluetoothDevice.ACTION_FOUND)
                            activeContext!!.registerReceiver(discoveryReceiver, intent)

                            bluetoothAdapter!!.startDiscovery()
                            result.success(null)
                        }
                "cancelDiscovery" -> {
                    Log.d(TAG, "Canceling discovery")
                    try {
                        activeContext!!.unregisterReceiver(discoveryReceiver)
                    } catch (ex: IllegalArgumentException) {
                        // Ignore `Receiver not registered` exception
                    }

                    bluetoothAdapter!!.cancelDiscovery()

                    if (discoverySink != null) {
                        discoverySink!!.endOfStream()
                        discoverySink = null
                    }

                    result.success(null)
                }
                "isDiscoverable" ->
                        result.success(
                                bluetoothAdapter!!.scanMode ==
                                        BluetoothAdapter.SCAN_MODE_CONNECTABLE_DISCOVERABLE
                        )
                "requestDiscoverable" -> {
                    val intent = Intent(BluetoothAdapter.ACTION_REQUEST_DISCOVERABLE)

                    if (call.hasArgument("duration")) {
                        try {
                            val duration = call.argument<Any>("duration") as Int
                            intent.putExtra(BluetoothAdapter.EXTRA_DISCOVERABLE_DURATION, duration)
                        } catch (ex: ClassCastException) {
                            result.error(
                                    "invalid_argument",
                                    "'duration' argument is required to be integer",
                                    null
                            )
                            return
                        }
                    }

                    pendingResultForActivityResult = result
                    ActivityCompat.startActivityForResult(
                            activity!!,
                            intent,
                            REQUEST_DISCOVERABLE_BLUETOOTH,
                            null
                    )
                }
                "connect" -> {
                    if (!call.hasArgument("address")) {
                        result.error("invalid_argument", "argument 'address' not found", null)
                        return
                    }

                    val isLE =
                            call.hasArgument("isLE") &&
                                    java.lang.Boolean.TRUE == call.argument<Boolean>("isLE")

                    val address: String?
                    try {
                        address = call.argument("address")
                        if (!BluetoothAdapter.checkBluetoothAddress(address)) {
                            throw ClassCastException()
                        }
                    } catch (ex: ClassCastException) {
                        result.error(
                                "invalid_argument",
                                "'address' argument is required to be string containing remote MAC address",
                                null
                        )
                        return
                    }

                    val connection: BluetoothConnection
                    val connection0 = arrayOf<BluetoothConnection?>(null)

                    val id = ++lastConnectionId

                    val readSink = arrayOf<EventSink?>(null)

                    // I think this code is to effect disconnection when the plugin is unloaded or
                    // something?
                    val readChannel = EventChannel(messenger, PLUGIN_NAMESPACE + "/read/" + id)
                    // If canceled by local, disconnects - in other case, by remote, does nothing
                    // True dispose
                    val readStreamHandler: EventChannel.StreamHandler =
                            object : EventChannel.StreamHandler {
                                override fun onListen(o: Any?, eventSink: EventSink) {
                                    readSink[0] = eventSink
                                }

                                override fun onCancel(o: Any?) {
                                    // If canceled by local, disconnects - in other case, by remote,
                                    // does nothing
                                    connection0[0]!!.disconnect()

                                    // True dispose
                                    AsyncTask.execute {
                                        readChannel.setStreamHandler(null)
                                        connections.remove(id)
                                        Log.d(TAG, "Disconnected (id: $id)")
                                    }
                                }
                            }
                    readChannel.setStreamHandler(
                            readStreamHandler
                    ) // LEAK //THINK I don't know if this should go after the BluetoothConnection
                    // is created or not

                    val orc: OnReadCallback =
                            object : OnReadCallback {
                                override fun onRead(data: ByteArray) {
                                    activity!!.runOnUiThread {
                                        if (readSink[0] != null) {
                                            readSink[0]!!.success(data)
                                        }
                                    }
                                }
                            }

                    val odc: OnDisconnectedCallback =
                            object : OnDisconnectedCallback {
                                override fun onDisconnected(byRemote: Boolean) {
                                    activity!!.runOnUiThread {
                                        if (byRemote) {
                                            Log.d(TAG, "onDisconnected by remote (id: $id)")
                                            if (readSink[0] != null) {
                                                readSink[0]!!.endOfStream()
                                                readSink[0] = null
                                            }
                                        } else {
                                            Log.d(TAG, "onDisconnected by local (id: $id)")
                                        }
                                    }
                                }
                            }

                    connection0[0] = BluetoothConnectionClassic(orc, odc, bluetoothAdapter!!)
                    connection = connection0[0]!!
                    connections.put(id, connection)

                    Log.d(TAG, "Connecting to $address (id: $id)")

                    AsyncTask.execute {
                        try {
                            connection.connect(address!!)
                            activity!!.runOnUiThread { result.success(id) }
                        } catch (ex: Exception) {
                            activity!!.runOnUiThread {
                                result.error("connect_error", ex.message, exceptionToString(ex))
                            }
                            connections.remove(id)
                        }
                    }
                }
                "write" -> {
                    if (!call.hasArgument("id")) {
                        result.error("invalid_argument", "argument 'id' not found", null)
                        return
                    }

                    val id: Int
                    try {
                        id = call.argument("id")!!
                    } catch (ex: ClassCastException) {
                        result.error(
                                "invalid_argument",
                                "'id' argument is required to be integer id of connection",
                                null
                        )
                        return
                    }

                    val connection = connections[id]
                    if (connection == null) {
                        result.error(
                                "invalid_argument",
                                "there is no connection with provided id",
                                null
                        )
                        return
                    }

                    if (call.hasArgument("string")) {
                        val string = call.argument<String>("string")
                        AsyncTask.execute {
                            try {
                                connection.write(string!!.toByteArray())
                                activity!!.runOnUiThread { result.success(null) }
                            } catch (ex: Exception) {
                                activity!!.runOnUiThread {
                                    result.error("write_error", ex.message, exceptionToString(ex))
                                }
                            }
                        }
                    } else if (call.hasArgument("bytes")) {
                        val bytes = call.argument<ByteArray>("bytes")
                        AsyncTask.execute {
                            try {
                                connection.write(bytes!!)
                                activity!!.runOnUiThread { result.success(null) }
                            } catch (ex: Exception) {
                                activity!!.runOnUiThread {
                                    result.error("write_error", ex.message, exceptionToString(ex))
                                }
                            }
                        }
                    } else {
                        result.error(
                                "invalid_argument",
                                "there must be 'string' or 'bytes' argument",
                                null
                        )
                    }
                }
                else -> result.notImplemented()
            }
        }
    }
}
