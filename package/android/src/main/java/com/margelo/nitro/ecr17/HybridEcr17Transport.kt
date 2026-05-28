package com.margelo.nitro.ecr17

import com.margelo.nitro.core.ArrayBuffer
import com.margelo.nitro.core.Promise
import java.io.PushbackInputStream
import java.net.InetSocketAddress
import java.net.Socket
import java.net.SocketTimeoutException
import java.util.concurrent.atomic.AtomicBoolean
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

  // The reader and the liveness probe ([isConnected]) share one input stream;
  // PushbackInputStream lets the probe peek a byte (looking only for EOF) and push
  // it back so it is never consumed from the protocol stream.
  private var input: PushbackInputStream? = null

  @Volatile private var running = false

  // True while a caller-initiated disconnect is in progress, so the reader's
  // finally block does not emit a spurious onDisconnect for an intentional close.
  @Volatile private var intentionalDisconnect = false

  // Single-shot guard so a given drop fires onDisconnect exactly once, no matter
  // whether the reader thread or the liveness probe observes it first.
  private val disconnectEmitted = AtomicBoolean(false)

  // Serializes access to the shared input stream between the reader thread and the
  // liveness probe so the two never `read()` concurrently.
  private val ioLock = Any()

  private var onDataCallback: ((ArrayBuffer) -> Unit)? = null
  private var onDisconnectCallback: (() -> Unit)? = null

  override fun connect(host: String, port: Double, timeoutMs: Double): Promise<Unit> {
    return Promise.parallel {
      // Tear down any previous connection (and join its reader) before reconnecting.
      closeCurrent()
      intentionalDisconnect = false
      disconnectEmitted.set(false)

      val s = Socket()
      s.tcpNoDelay = true
      s.connect(InetSocketAddress(host, port.toInt()), timeoutMs.toInt())
      // A short read timeout lets the reader loop release `ioLock` periodically so
      // the liveness probe ([isConnected]) can run between reads; a timeout is not
      // an error, just "no data yet".
      s.soTimeout = READ_TIMEOUT_MS
      socket = s
      input = PushbackInputStream(s.getInputStream(), 1)
      running = true
      startReader()
      Unit
    }
  }

  private fun startReader() {
    readerThread =
      thread(name = "ecr17-reader", isDaemon = true) {
        val buffer = ByteArray(4096)
        try {
          while (running) {
            val read =
              synchronized(ioLock) {
                if (!running) return@synchronized SENTINEL_STOP
                try {
                  input?.read(buffer) ?: -1 // null stream -> treat as EOF
                } catch (_: SocketTimeoutException) {
                  SENTINEL_TIMEOUT // no data within READ_TIMEOUT_MS — keep looping
                }
              }
            when {
              read == SENTINEL_STOP -> break
              read == SENTINEL_TIMEOUT -> continue
              read < 0 -> break // EOF: peer closed the connection
              read > 0 ->
                onDataCallback?.invoke(ArrayBuffer.copy(buffer.copyOfRange(0, read)))
            }
          }
        } catch (_: Throwable) {
          // socket closed or read error
        } finally {
          // Promptly close the socket so `isConnected()`'s `isClosed` check is an
          // immediate, reliable signal of the drop (no reliance on a write probe).
          markDropped()
        }
      }
  }

  /**
   * Non-destructive liveness probe used to detect a peer-closed / half-open socket
   * BEFORE a command is sent. ECR17/Nexi terminals routinely close the TCP socket
   * between transactions; `Socket.isConnected` stays `true` for such a half-open
   * socket, and the reader thread may not have observed the EOF yet — so without a
   * probe a command could be sent on a dead socket, fail MID-exchange, and (because
   * the money-safety RetryPolicy correctly refuses to replay a financial command)
   * surface a FALSE "transport disconnected" error.
   *
   * The probe peeks ONE byte with a tiny read timeout and immediately pushes it
   * back, so it NEVER consumes a protocol byte and NEVER writes anything to the
   * peer (unlike `sendUrgentData`, which can place an inline 0xFF before the next
   * STX frame on terminals with SO_OOBINLINE and corrupt a financial command). A
   * returned `-1` means the peer closed; a read timeout means the (idle) socket is
   * alive. It runs under `ioLock` so it never races the reader's `read()`.
   *
   * Money-safety is unchanged: this only removes the FALSE drop from a stale
   * pre-send socket; a genuine mid-exchange drop still surfaces and is recovered
   * via sendLastResult ('G').
   */
  override fun isConnected(): Boolean {
    val s = socket
    val pin = input
    if (!running || s == null || pin == null || !s.isConnected || s.isClosed) {
      return false
    }
    return synchronized(ioLock) {
      if (!running || s.isClosed) return@synchronized false
      try {
        val b = pin.read() // EOF (-1) if peer closed; SocketTimeoutException if idle+alive
        if (b < 0) {
          markDropped()
          false
        } else {
          pin.unread(b) // push the (unexpected) byte back — never consumed/dropped
          true
        }
      } catch (_: SocketTimeoutException) {
        true // idle but alive (no FIN received within the timeout)
      } catch (_: Throwable) {
        markDropped() // I/O error: treat as dropped
        false
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
    input = null
    readerThread?.let { t ->
      try {
        t.join(1000)
      } catch (_: Throwable) {
        // ignore
      }
    }
    readerThread = null
  }

  /**
   * Marks the connection as dropped, closes the socket so the drop is synchronously
   * observable, and fires onDisconnect exactly once. Safe to call from both the
   * reader thread and the liveness probe (single-shot via [disconnectEmitted]).
   */
  private fun markDropped() {
    running = false
    try {
      socket?.close()
    } catch (_: Throwable) {
      // ignore
    }
    // Only signal disconnect for unexpected drops, not caller-initiated closes, and
    // only once per drop.
    if (!intentionalDisconnect && disconnectEmitted.compareAndSet(false, true)) {
      onDisconnectCallback?.invoke()
    }
  }

  override fun disconnect() {
    closeCurrent()
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

  private companion object {
    // Reader-loop read timeout: short enough that the liveness probe never waits
    // long for `ioLock`, long enough to avoid busy-spinning.
    private const val READ_TIMEOUT_MS = 100

    // Sentinels returned by the synchronized read block; kept below the real EOF
    // value (-1) so `read < 0` still catches a genuine EOF after these are handled.
    private const val SENTINEL_TIMEOUT = -2
    private const val SENTINEL_STOP = -3
  }
}
