package ru.ae_mc.bluetooth_classic_multiplatform;

import java.io.IOException;
import java.util.UUID;

interface BluetoothConnection {
    fun isConnected(): Boolean
    /// Connects to given device by hardware address
    /** @throws IOException */
    fun connect(address: String, uuid: UUID)
    /// Connects to given device by hardware address (default UUID used)
    /** @throws IOException */
    fun connect(address: String)
    /// Disconnects current session (ignore if not connected)
    fun disconnect()
    /// Writes to connected remote device
    /** @throws IOException */
    fun write(data: ByteArray)
    /// Callback for reading data.
    fun onRead(data: ByteArray)
    /// Callback for disconnection.
    fun onDisconnected(byRemote: Boolean)
}
