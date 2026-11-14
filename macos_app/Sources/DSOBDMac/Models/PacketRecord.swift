import Foundation

public enum PacketDirection: String, Codable, CaseIterable {
    case tx = "TX"
    case rx = "RX"

    public var icon: String {
        switch self {
        case .tx: return "arrow.up.circle.fill"
        case .rx: return "arrow.down.circle.fill"
        }
    }

    public var label: String {
        switch self {
        case .tx: return "Laptop → VP01"
        case .rx: return "VP01 → Laptop"
        }
    }
}

public enum PacketCategory: String, Codable, CaseIterable {
    case atCommand = "AT Command"
    case pidQuery = "PID Query"
    case dtcCommand = "DTC Command"
    case response = "Response"
    case prompt = "Prompt (>)"
    case reset = "Reset / Init"
    case error = "Error"
    case status = "Status"
}

public struct PacketRecord: Identifiable, Codable, Hashable {
    public let id: UUID
    public let sequence: Int
    public let timestamp: Date
    public let direction: PacketDirection
    public let category: PacketCategory
    public let rawText: String
    public let hexDump: String
    public let byteCount: Int
    public let latencyMs: Double?
    public let notes: String?

    public init(
        id: UUID = UUID(),
        sequence: Int,
        timestamp: Date = Date(),
        direction: PacketDirection,
        category: PacketCategory,
        rawText: String,
        latencyMs: Double? = nil,
        notes: String? = nil
    ) {
        self.id = id
        self.sequence = sequence
        self.timestamp = timestamp
        self.direction = direction
        self.category = category
        self.rawText = rawText
        self.latencyMs = latencyMs
        self.notes = notes

        let data = Data(rawText.utf8)
        self.byteCount = data.count
        self.hexDump = data.map { String(format: "%02X", $0) }.joined(separator: " ")
    }

    public var timestampFormatted: String {
        let formatter = DateFormatter()
        formatter.dateFormat = "HH:mm:ss.SSS"
        return formatter.string(from: timestamp)
    }
}
