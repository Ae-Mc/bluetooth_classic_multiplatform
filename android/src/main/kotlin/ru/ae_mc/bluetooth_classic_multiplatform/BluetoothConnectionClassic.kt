package ru.ae_mc.bluetooth_classic_multiplatform

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.UUID;
import java.util.Arrays;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothSocket;
import ru.ae_mc.bluetooth_classic_multiplatform.BluetoothConnectionBase

/// Universal Bluetooth serial connection class (for Java)
class BluetoothConnectionClassic(
    onReadCallback: OnReadCallback,
    onDisconnectedCallback: OnDisconnectedCallback,
    protected val bluetoothAdapter: BluetoothAdapter,
    protected var connectionThread: ConnectionThread? = null
) : BluetoothConnectionBase(onReadCallback, onDisconnectedCallback)
{
    companion object {
        val DEFAULT_UUID: UUID = UUID.fromString("00001101-0000-1000-8000-00805F9B34FB")
    }


    override fun isConnected(): Boolean {
        return connectionThread != null && connectionThread?.requestedClosing != true;
    }

    // @TODO . `connect` could be done perfored on the other thread
    // @TODO . `connect` parameter: timeout
    // @TODO . `connect` other methods than `createRfcommSocketToServiceRecord`, including hidden one raw `createRfcommSocket` (on channel).
    // @TODO ? how about turning it into factoried?
    override fun connect(address: String, uuid: UUID) {
        if (isConnected()) {
            throw IOException("already connected");
        }

        val device: BluetoothDevice = bluetoothAdapter.getRemoteDevice(address)
            ?: throw IOException("device not found");

        var socket: BluetoothSocket = device.createRfcommSocketToServiceRecord(uuid)
            ?: throw IOException("socket connection not established"); // @TODO . introduce ConnectionMethod

        // Cancel discovery, even though we didn't start it
        bluetoothAdapter.cancelDiscovery();

        try {
            socket.connect();
        } catch (e: IOException) {
            try {
                // Newer versions of android may require voodoo; see https://stackoverflow.com/a/25647197
                socket = device::class.java.getMethod("createRfcommSocket", Int::class.javaPrimitiveType).invoke(device,1) as BluetoothSocket
                socket.connect();
            } catch (e2: Exception) {
                throw IOException("Failed to connect", e2);
            }
        }

        connectionThread = ConnectionThread(socket);
        connectionThread?.start();
    }

    override fun connect(address: String) {
        connect(address, DEFAULT_UUID);
    }
    
    override fun disconnect() {
        if (isConnected()) {
            connectionThread?.cancel();
            connectionThread = null;
        }
    }

    override fun write(data: ByteArray) {
        if (!isConnected()) {
            throw IOException("not connected");
        }

        connectionThread?.write(data);
    }

    /// Thread to handle connection I/O
    inner class ConnectionThread(socket: BluetoothSocket) : Thread() {
        private val socket: BluetoothSocket
        private val input: InputStream?
        private val output: OutputStream?
        var requestedClosing: Boolean = false

        init {
            this.socket = socket
            try {
                input = socket.inputStream;
                output = socket.outputStream;
            } catch (e: IOException) {
                e.printStackTrace();
                throw e
            }
        }

        /// Thread main code
        override fun run() {
            val buffer = ByteArray(1024)
            var bytes: Int

            while (!requestedClosing) {
                try {
                    bytes = input?.read(buffer) ?: 0

                    onRead(buffer.copyOf(bytes))
                } catch (e: IOException) {
                    // `input.read` throws when closed by remote device
                    break;
                }
            }

            // Make sure output stream is closed
            if (output != null) {
                try {
                    output.close()
                } catch (_: Exception) {}
            }

            // Make sure input stream is closed
            if (input != null) {
                try {
                    input.close()
                } catch (_: Exception) {}
            }

            // Callback on disconnected, with information which side is closing
            onDisconnected(!requestedClosing)

            // Just prevent unnecessary `cancel`ing
            requestedClosing = true
        }

        /// Writes to output stream
        fun write(bytes: ByteArray) {
            try {
                output?.write(bytes);
            } catch (e: IOException) {
                e.printStackTrace();
            }
        }

        /// Stops the thread, disconnects
        fun cancel() {
            if (requestedClosing) {
                return;
            }
            requestedClosing = true;

            // Flush output buffers before closing
            try {
                output?.flush();
            } catch (_: Exception) {}

            // Close the connection socket
            try {
                // Might be useful (see https://stackoverflow.com/a/22769260/4880243)
                sleep(111);

                socket.close();
            } catch (_: Exception ) {}
        }
    }
}
