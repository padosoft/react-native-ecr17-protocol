package com.margelo.nitro.ecr17

import com.margelo.nitro.core.ArrayBuffer
import com.margelo.nitro.core.Promise
import java.net.InetSocketAddress
import java.net.Socket
import kotlin.concurrent.thread

/**
 * Android (Kotlin) implementation of the ECR17 LAN transport. Plain TCP socket
 * with a background reader thread that forwards received bytes to the C++ core
 * via the [onDataCallback]. Nitro generates the C++<->Kotlin JNI bridge for this
 * HybridObject, so no manual JNI is needed.
 */
class HybridEcr17Transport : HybridEcr17TransportSpec() {
  private var socket: Socket? = null
  private var readerThread: Thread? = null

  @Volatile private var running = false

  // True while a caller-initiated disconnect is in progress, so the reader's
  // finally block does not emit a spurious onDisconnect for an intentional close.
  @Volatile private var intentionalDisconnect = false

  private var onDataCallback: ((ArrayBuffer) -> Unit)? = null
  private var onDisconnectCallback: (() -> Unit)? = null

  override fun connect(host: String, port: Double, timeoutMs: Double): Promise<Unit> {
    return Promise.parallel {
      // Tear down any previous connection (and join its reader) before reconnecting.
      closeCurrent()
      intentionalDisconnect = false

      val s = Socket()
      s.tcpNoDelay = true
      s.connect(InetSocketAddress(host, port.toInt()), timeoutMs.toInt())
      socket = s
      running = true
      startReader(s)
      Unit
    }
  }

  private fun startReader(s: Socket) {
    readerThread =
      thread(name = "ecr17-reader", isDaemon = true) {
        val buffer = ByteArray(4096)
        try {
          val input = s.getInputStream()
          while (running) {
            val read = input.read(buffer)
            if (read < 0) break
            if (read > 0) {
              onDataCallback?.invoke(ArrayBuffer.copy(buffer.copyOfRange(0, read)))
            }
          }
        } catch (_: Throwable) {
          // socket closed or read error
        } finally {
          running = false
          // Only signal disconnect for unexpected drops, not caller-initiated closes.
          if (!intentionalDisconnect) {
            onDisconnectCallback?.invoke()
          }
        }
      }
  }

  /** Closes the current socket and joins its reader thread (intentional close). */
  private fun closeCurrent() {
    intentionalDisconnect = true
    running = false
    try {
      socket?.close()
    } catch (_: Throwable) {
      // ignore
    }
    socket = null
    readerThread?.let { t ->
      try {
        t.join(1000)
      } catch (_: Throwable) {
        // ignore
      }
    }
    readerThread = null
  }

  override fun disconnect() {
    closeCurrent()
  }

  override fun isConnected(): Boolean {
    // `Socket.isConnected` stays true after the peer closes the connection, so it
    // alone can't detect an unexpected drop. The reader thread clears `running`
    // when the stream ends (or on an intentional close), making it the source of
    // truth for an active, usable connection.
    val s = socket
    return running && s != null && s.isConnected && !s.isClosed
  }

  override fun send(bytes: ArrayBuffer) {
    val s = socket ?: throw IllegalStateException("ECR17 transport is not connected")
    val out = s.getOutputStream()
    out.write(bytes.toByteArray())
    out.flush()
  }

  override fun setOnData(callback: (bytes: ArrayBuffer) -> Unit) {
    onDataCallback = callback
  }

  override fun setOnDisconnect(callback: () -> Unit) {
    onDisconnectCallback = callback
  }
}
