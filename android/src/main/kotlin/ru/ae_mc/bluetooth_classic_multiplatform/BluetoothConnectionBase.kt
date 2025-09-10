package ru.ae_mc.bluetooth_classic_multiplatform

abstract class BluetoothConnectionBase(
    val onReadCallback: OnReadCallback,
    val onDisconnectedCallback: OnDisconnectedCallback
) : BluetoothConnection {
    interface OnReadCallback {
        fun onRead(data: ByteArray)
    }
    interface OnDisconnectedCallback {
        fun onDisconnected(byRemote: Boolean)
    }


    override fun onRead(data: ByteArray) {
        onReadCallback.onRead(data);
    }

    override fun onDisconnected(byRemote: Boolean) {
        onDisconnectedCallback.onDisconnected(byRemote);
    }
}
