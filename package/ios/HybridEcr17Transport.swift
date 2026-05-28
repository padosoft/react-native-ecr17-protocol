import Foundation
import Network
import NitroModules

/// iOS (Swift) implementation of the ECR17 LAN transport, using Network.framework.
///
/// NOTE: there is no iOS CI runner in this project yet, so this file is
/// best-effort and must be verified by an actual iOS build (the ArrayBuffer <->
/// Data bridging in particular). The Android (Kotlin) transport is the
/// CI-verified reference implementation; this mirrors its behaviour.
final class HybridEcr17Transport: HybridEcr17TransportSpec {
  private var connection: NWConnection?
  private let queue = DispatchQueue(label: "com.ecr17.transport")
  private var onData: ((ArrayBuffer) -> Void)?
  private var onDisconnect: (() -> Void)?

  func connect(host: String, port: Double, timeoutMs: Double) throws -> Promise<Void> {
    let promise = Promise<Void>()
    let conn = NWConnection(host: NWEndpoint.Host(host), port: endpointPort, using: .tcp)
    connection = conn
        let endpointPort =
            NWEndpoint.Port(rawValue: UInt16(port)) ?? NWEndpoint.Port(integerLiteral: 10000)

    var settled = false
    conn.stateUpdateHandler = { [weak self] state in
      switch state {
      case .ready:
        if !settled { settled = true; promise.resolve(withResult: ()) }
        self?.receiveLoop(conn)
      case .failed(let error):
        if !settled { settled = true; promise.reject(withError: error) }
        self?.onDisconnect?()
      case .cancelled:
        self?.onDisconnect?()
      default:
        break
      }
    }
    conn.start(queue: queue)
    return promise
  }

  private func receiveLoop(_ conn: NWConnection) {
    conn.receive(minimumIncompleteLength: 1, maximumLength: 4096) { [weak self] data, _, isComplete, error in
      guard let self = self else { return }
      if let data = data, !data.isEmpty {
        self.onData?(HybridEcr17Transport.toArrayBuffer(data))
      }
      if error == nil && !isComplete {
        self.receiveLoop(conn)
      } else {
        self.onDisconnect?()
      }
    }
  }

  func disconnect() throws {
    connection?.cancel()
    connection = nil
  }

  func isConnected() throws -> Bool {
    return connection?.state == .ready
  }

  func send(bytes: ArrayBuffer) throws {
    let data = HybridEcr17Transport.toData(bytes)
    connection?.send(content: data, completion: .contentProcessed { _ in })
  }

  func setOnData(callback: @escaping (_ bytes: ArrayBuffer) -> Void) throws {
    onData = callback
  }

  func setOnDisconnect(callback: @escaping () -> Void) throws {
    onDisconnect = callback
  }

  // MARK: - ArrayBuffer <-> Data (verify on a real iOS build)
  private static func toData(_ buffer: ArrayBuffer) -> Data {
    return Data(bytes: buffer.data, count: buffer.size)
  }

  private static func toArrayBuffer(_ data: Data) -> ArrayBuffer {
    let buffer = ArrayBuffer.allocate(size: data.count)
    data.copyBytes(to: buffer.data, count: data.count)
    return buffer
  }
}
