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
    // when the stream ends (or on an intentional close); but there is a RACE: an
    // ECR17/Nexi terminal typically closes the TCP socket between transactions, and
    // the reader thread may not have observed the EOF (`read() < 0`) yet when the
    // NEXT command checks `isConnected()`. If we returned `true` there, the command
    // would be SENT on a half-open (peer-closed) socket; the read would then fail
    // MID-exchange — and the money-safety RetryPolicy (correctly) refuses to replay
    // a financial command, so a FALSE "transport disconnected" error surfaces.
    //
    // Fix: detect a peer-closed/half-open socket PROACTIVELY here, BEFORE the send,
    // with a non-destructive liveness probe. `sendUrgentData` writes a single TCP
    // out-of-band byte; on a socket the peer has already closed (FIN/RST received)
    // it throws IOException, while on a live socket it is harmless and does not
    // touch the normal data stream (so it cannot corrupt an STX/ETX frame and does
    // not race the reader thread, which only reads the ordinary input stream). When
    // the probe trips, we treat the connection as dead so `ensureConnected()`
    // reconnects BEFORE sending — turning the old reactive (post-send) drop
    // detection into proactive (pre-send) recovery. Money-safety is unchanged: we
    // only remove the FALSE drop caused by a stale pre-send socket; a genuine
    // mid-exchange drop still surfaces and is recovered via sendLastResult ('G').
    val s = socket
    if (!running || s == null || !s.isConnected || s.isClosed) {
      return false
    }
    return try {
      s.sendUrgentData(0xFF)
      true
    } catch (_: Throwable) {
      // Peer has closed the socket (or it is otherwise unusable): mark it dead and
      // notify listeners, mirroring the reader thread's EOF path, so the next
      // ensureConnected() establishes a fresh socket before any command is sent.
      markDropped()
      false
    }
  }

  /** Marks the connection as dropped and fires onDisconnect (unexpected drop). */
  private fun markDropped() {
    if (!running) return
    running = false
    if (!intentionalDisconnect) {
      onDisconnectCallback?.invoke()
    }
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
