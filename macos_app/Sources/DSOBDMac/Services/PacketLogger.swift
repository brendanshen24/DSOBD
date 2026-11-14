import Foundation
import Combine

public final class PacketLogger: ObservableObject {
    public static let shared = PacketLogger()

    @Published public private(set) var records: [PacketRecord] = []
    @Published public var isPaused: Bool = false
    @Published public var filterText: String = ""
    @Published public var selectedDirection: PacketDirection? = nil
    @Published public var selectedCategory: PacketCategory? = nil

    // Stats
    @Published public private(set) var totalPackets: Int = 0
    @Published public private(set) var txCount: Int = 0
    @Published public private(set) var rxCount: Int = 0
    @Published public private(set) var totalTxBytes: Int = 0
    @Published public private(set) var totalRxBytes: Int = 0
    @Published public private(set) var lastLatencyMs: Double = 0.0
    @Published public private(set) var avgLatencyMs: Double = 0.0

    private let maxRecords: Int = 5000
    private var seq: Int = 0
    private var latencies: [Double] = []
    private let queue = DispatchQueue(label: "com.dsobd.packetlogger", qos: .utility)

    private init() {}

    public func logTX(text: String, category: PacketCategory = .pidQuery, notes: String? = nil) {
        guard !isPaused else { return }
        queue.async { [weak self] in
            guard let self = self else { return }
            self.seq += 1
            let rec = PacketRecord(
                sequence: self.seq,
                timestamp: Date(),
                direction: .tx,
                category: category,
                rawText: text,
                latencyMs: nil,
                notes: notes
            )
            DispatchQueue.main.async {
                self.appendRecord(rec)
            }
        }
    }

    public func logRX(text: String, category: PacketCategory = .response, latencyMs: Double? = nil, notes: String? = nil) {
        guard !isPaused else { return }
        queue.async { [weak self] in
            guard let self = self else { return }
            self.seq += 1
            let rec = PacketRecord(
                sequence: self.seq,
                timestamp: Date(),
                direction: .rx,
                category: category,
                rawText: text,
                latencyMs: latencyMs,
                notes: notes
            )
            DispatchQueue.main.async {
                self.appendRecord(rec)
            }
        }
    }

    private func appendRecord(_ record: PacketRecord) {
        records.append(record)
        if records.count > maxRecords {
            records.removeFirst(records.count - maxRecords)
        }

        totalPackets += 1
        if record.direction == .tx {
            txCount += 1
            totalTxBytes += record.byteCount
        } else {
            rxCount += 1
            totalRxBytes += record.byteCount
            if let lat = record.latencyMs {
                lastLatencyMs = lat
                latencies.append(lat)
                if latencies.count > 100 { latencies.removeFirst() }
                avgLatencyMs = latencies.reduce(0, +) / Double(latencies.count)
            }
        }
    }

    public var filteredRecords: [PacketRecord] {
        records.filter { rec in
            if let dir = selectedDirection, rec.direction != dir { return false }
            if let cat = selectedCategory, rec.category != cat { return false }
            if !filterText.isEmpty {
                let term = filterText.lowercased()
                let matchesRaw = rec.rawText.lowercased().contains(term)
                let matchesHex = rec.hexDump.lowercased().contains(term)
                let matchesNotes = rec.notes?.lowercased().contains(term) ?? false
                let matchesCategory = rec.category.rawValue.lowercased().contains(term)
                if !matchesRaw && !matchesHex && !matchesNotes && !matchesCategory {
                    return false
                }
            }
            return true
        }
    }

    public func clear() {
        records.removeAll()
        totalPackets = 0
        txCount = 0
        rxCount = 0
        totalTxBytes = 0
        totalRxBytes = 0
        lastLatencyMs = 0.0
        avgLatencyMs = 0.0
        latencies.removeAll()
    }

    public func exportCSV() -> String {
        var lines: [String] = ["Seq,Timestamp,Direction,Category,Bytes,LatencyMs,RawText,HexDump,Notes"]
        for r in records {
            let safeText = "\"" + r.rawText.replacingOccurrences(of: "\"", with: "\"\"").replacingOccurrences(of: "\n", with: "\\n").replacingOccurrences(of: "\r", with: "\\r") + "\""
            let safeNotes = "\"" + (r.notes ?? "").replacingOccurrences(of: "\"", with: "\"\"") + "\""
            let latStr = r.latencyMs != nil ? String(format: "%.1f", r.latencyMs!) : ""
            lines.append("\(r.sequence),\(r.timestampFormatted),\(r.direction.rawValue),\(r.category.rawValue),\(r.byteCount),\(latStr),\(safeText),\(r.hexDump),\(safeNotes)")
        }
        return lines.joined(separator: "\n")
    }

    public func exportRawLog() -> String {
        var lines: [String] = [
            "# DSOBD Veepeak VP01 Packet Traffic Log",
            "# Captured: \(Date())",
            "# Total Packets: \(totalPackets) (TX: \(txCount), RX: \(rxCount))",
            "# Total Bytes: TX \(totalTxBytes) B, RX \(totalRxBytes) B",
            "--------------------------------------------------------------------------------"
        ]
        for r in records {
            let lat = r.latencyMs != nil ? " [+\(String(format: "%.1f", r.latencyMs!)) ms]" : ""
            let note = r.notes != nil ? " | \(r.notes!)" : ""
            let dirTag = r.direction == .tx ? "TX >>" : "RX <<"
            lines.append("[\(r.timestampFormatted)] \(dirTag) [\(r.category.rawValue)] (\(r.byteCount) B)\(lat)\(note)")
            lines.append("    ASCII: \(r.rawText.replacingOccurrences(of: "\r", with: "\\r").replacingOccurrences(of: "\n", with: "\\n"))")
            lines.append("    HEX:   \(r.hexDump)")
        }
        return lines.joined(separator: "\n")
    }
}
