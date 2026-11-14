import Foundation

public enum ConnectionStatus: Equatable {
    case disconnected
    case connecting(String, Int)
    case initializing(String)
    case connected(String, String) // version, protocol
    case error(String)

    public var isConnected: Bool {
        if case .connected = self { return true }
        return false
    }

    public var displayText: String {
        switch self {
        case .disconnected:
            return "Disconnected"
        case .connecting(let host, let port):
            return "Connecting to \(host):\(port)..."
        case .initializing(let step):
            return "Initializing: \(step)"
        case .connected(let version, _):
            return "Connected (\(version))"
        case .error(let msg):
            return "Error: \(msg)"
        }
    }
}
