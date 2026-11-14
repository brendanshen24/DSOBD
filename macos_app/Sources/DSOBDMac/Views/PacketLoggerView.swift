import SwiftUI
import AppKit

public struct PacketLoggerView: View {
    @ObservedObject var logger = PacketLogger.shared
    @State private var selectedPacket: PacketRecord? = nil
    @State private var autoScroll: Bool = true
    @State private var showExportSuccess: Bool = false
    @State private var exportMessage: String = ""

    public init() {}

    public var body: some View {
        VStack(spacing: 0) {
            // Stats Header Bar
            statsHeader
                .padding(.horizontal)
                .padding(.vertical, 8)
                .background(Color(NSColor.controlBackgroundColor))
            Divider()

            // Filter & Control Bar
            filterToolbar
                .padding(.horizontal)
                .padding(.vertical, 6)
                .background(Color(NSColor.windowBackgroundColor))
            Divider()

            // Main Content: Split between Packet List and Inspector
            HSplitView {
                packetList
                    .frame(minWidth: 450)

                if let packet = selectedPacket {
                    packetDetailInspector(packet)
                        .frame(minWidth: 260, maxWidth: 400)
                }
            }
        }
        .alert(isPresented: $showExportSuccess) {
            Alert(title: Text("Export Complete"), message: Text(exportMessage), dismissButton: .default(Text("OK")))
        }
    }

    // MARK: - Stats Header

    private var statsHeader: some View {
        HStack(spacing: 20) {
            statItem(label: "TOTAL PACKETS", value: "\(logger.totalPackets)", icon: "shippingbox.fill", color: .purple)
            statItem(label: "TX (LAPTOP)", value: "\(logger.txCount) (\(formatBytes(logger.totalTxBytes)))", icon: "arrow.up.circle.fill", color: .blue)
            statItem(label: "RX (VP01)", value: "\(logger.rxCount) (\(formatBytes(logger.totalRxBytes)))", icon: "arrow.down.circle.fill", color: .green)
            statItem(label: "AVG LATENCY", value: String(format: "%.1f ms", logger.avgLatencyMs), icon: "timer", color: .orange)
            statItem(label: "LAST RTT", value: String(format: "%.1f ms", logger.lastLatencyMs), icon: "bolt.fill", color: .yellow)
            Spacer()
        }
    }

    private func statItem(label: String, value: String, icon: String, color: Color) -> some View {
        HStack(spacing: 6) {
            Image(systemName: icon)
                .foregroundColor(color)
                .font(.system(size: 14))
            VStack(alignment: .leading, spacing: 1) {
                Text(label)
                    .font(.system(size: 9, weight: .bold))
                    .foregroundColor(.secondary)
                Text(value)
                    .font(.system(size: 12, weight: .semibold, design: .monospaced))
            }
        }
    }

    // MARK: - Filter Toolbar

    private var filterToolbar: some View {
        HStack(spacing: 12) {
            // Direction filter
            Picker("Direction", selection: $logger.selectedDirection) {
                Text("All Directions").tag(nil as PacketDirection?)
                Text("TX (Laptop → VP01)").tag(PacketDirection.tx as PacketDirection?)
                Text("RX (VP01 → Laptop)").tag(PacketDirection.rx as PacketDirection?)
            }
            .pickerStyle(.menu)
            .frame(width: 170)

            // Category filter
            Picker("Category", selection: $logger.selectedCategory) {
                Text("All Categories").tag(nil as PacketCategory?)
                ForEach(PacketCategory.allCases, id: \.self) { cat in
                    Text(cat.rawValue).tag(cat as PacketCategory?)
                }
            }
            .pickerStyle(.menu)
            .frame(width: 150)

            // Search
            HStack {
                Image(systemName: "magnifyingglass")
                    .foregroundColor(.secondary)
                TextField("Search ASCII, Hex, notes...", text: $logger.filterText)
                    .textFieldStyle(.plain)
                if !logger.filterText.isEmpty {
                    Button(action: { logger.filterText = "" }) {
                        Image(systemName: "xmark.circle.fill")
                            .foregroundColor(.secondary)
                    }
                    .buttonStyle(.plain)
                }
            }
            .padding(4)
            .background(Color(NSColor.controlBackgroundColor))
            .cornerRadius(6)

            Spacer()

            // Pause / Resume
            Button(action: { logger.isPaused.toggle() }) {
                Label(logger.isPaused ? "Resume" : "Pause", systemImage: logger.isPaused ? "play.fill" : "pause.fill")
            }
            .buttonStyle(.bordered)
            .tint(logger.isPaused ? .green : .secondary)

            // Auto-scroll lock
            Button(action: { autoScroll.toggle() }) {
                Image(systemName: autoScroll ? "arrow.down.to.line.compact" : "arrow.up.and.down")
                    .foregroundColor(autoScroll ? .accentColor : .secondary)
            }
            .help(autoScroll ? "Auto-scroll enabled" : "Auto-scroll paused")

            // Clear
            Button(action: { logger.clear(); selectedPacket = nil }) {
                Label("Clear", systemImage: "trash")
            }
            .buttonStyle(.bordered)

            // Export Menu
            Menu {
                Button("Save as CSV Table...") { exportCSV() }
                Button("Save as Plain Log (.log)...") { exportLog() }
            } label: {
                Label("Export", systemImage: "square.and.arrow.up")
            }
            .buttonStyle(.borderedProminent)
        }
    }

    // MARK: - Packet List Table

    private var packetList: some View {
        ScrollViewReader { proxy in
            List(selection: $selectedPacket) {
                ForEach(logger.filteredRecords) { rec in
                    packetRow(rec)
                        .tag(rec)
                        .id(rec.id)
                }
            }
            .listStyle(.inset(alternatesRowBackgrounds: true))
            .onChange(of: logger.records.count) { _ in
                if autoScroll, let last = logger.filteredRecords.last {
                    withAnimation(.easeOut(duration: 0.1)) {
                        proxy.scrollTo(last.id, anchor: .bottom)
                    }
                }
            }
        }
    }

    private func packetRow(_ rec: PacketRecord) -> some View {
        HStack(spacing: 10) {
            // Sequence #
            Text("#\(rec.sequence)")
                .font(.system(size: 10, design: .monospaced))
                .foregroundColor(.secondary)
                .frame(width: 44, alignment: .trailing)

            // Timestamp
            Text(rec.timestampFormatted)
                .font(.system(size: 11, design: .monospaced))
                .foregroundColor(.secondary)
                .frame(width: 85, alignment: .leading)

            // Direction Pill
            HStack(spacing: 3) {
                Image(systemName: rec.direction.icon)
                Text(rec.direction.rawValue)
                    .fontWeight(.bold)
            }
            .font(.system(size: 10))
            .padding(.horizontal, 6)
            .padding(.vertical, 2)
            .background(rec.direction == .tx ? Color.blue.opacity(0.18) : Color.green.opacity(0.18))
            .foregroundColor(rec.direction == .tx ? .blue : .green)
            .cornerRadius(4)
            .frame(width: 60)

            // Category
            Text(rec.category.rawValue)
                .font(.system(size: 10, weight: .medium))
                .padding(.horizontal, 5)
                .padding(.vertical, 2)
                .background(Color.secondary.opacity(0.12))
                .cornerRadius(4)
                .frame(width: 85, alignment: .center)

            // ASCII Payload
            Text(displayAscii(rec.rawText))
                .font(.system(size: 12, weight: .regular, design: .monospaced))
                .lineLimit(1)
                .foregroundColor(.primary)

            Spacer()

            // Latency / Notes
            if let lat = rec.latencyMs {
                Text(String(format: "+%.1f ms", lat))
                    .font(.system(size: 10, design: .monospaced))
                    .foregroundColor(.orange)
            }

            if let notes = rec.notes {
                Text(notes)
                    .font(.system(size: 10))
                    .foregroundColor(.secondary)
                    .lineLimit(1)
            }

            // Byte count
            Text("\(rec.byteCount)B")
                .font(.system(size: 10, design: .monospaced))
                .foregroundColor(.secondary)
                .frame(width: 32, alignment: .trailing)
        }
        .padding(.vertical, 2)
    }

    // MARK: - Packet Detail Inspector

    private func packetDetailInspector(_ packet: PacketRecord) -> some View {
        VStack(alignment: .leading, spacing: 12) {
            HStack {
                Text("Packet #\(packet.sequence)")
                    .font(.headline)
                Spacer()
                Button(action: { selectedPacket = nil }) {
                    Image(systemName: "xmark")
                        .foregroundColor(.secondary)
                }
                .buttonStyle(.plain)
            }

            Divider()

            // Meta Info
            VStack(alignment: .leading, spacing: 6) {
                detailField(label: "Direction", value: packet.direction.label)
                detailField(label: "Category", value: packet.category.rawValue)
                detailField(label: "Timestamp", value: packet.timestampFormatted)
                detailField(label: "Length", value: "\(packet.byteCount) bytes")
                if let lat = packet.latencyMs {
                    detailField(label: "Latency", value: String(format: "%.2f ms", lat))
                }
                if let notes = packet.notes {
                    detailField(label: "Decoded Info", value: notes)
                }
            }
            .font(.system(size: 11))

            Divider()

            // ASCII View
            VStack(alignment: .leading, spacing: 4) {
                HStack {
                    Text("ASCII PAYLOAD")
                        .font(.system(size: 10, weight: .bold))
                        .foregroundColor(.secondary)
                    Spacer()
                    Button("Copy") {
                        NSPasteboard.general.clearContents()
                        NSPasteboard.general.setString(packet.rawText, forType: .string)
                    }
                    .font(.system(size: 10))
                }

                ScrollView {
                    Text(displayAscii(packet.rawText))
                        .font(.system(size: 11, design: .monospaced))
                        .padding(8)
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .background(Color(NSColor.textBackgroundColor))
                        .cornerRadius(6)
                }
                .frame(maxHeight: 120)
            }

            // Hex Dump View
            VStack(alignment: .leading, spacing: 4) {
                HStack {
                    Text("HEX DUMP")
                        .font(.system(size: 10, weight: .bold))
                        .foregroundColor(.secondary)
                    Spacer()
                    Button("Copy") {
                        NSPasteboard.general.clearContents()
                        NSPasteboard.general.setString(packet.hexDump, forType: .string)
                    }
                    .font(.system(size: 10))
                }

                ScrollView {
                    Text(packet.hexDump)
                        .font(.system(size: 11, design: .monospaced))
                        .padding(8)
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .background(Color(NSColor.textBackgroundColor))
                        .cornerRadius(6)
                }
            }

            Spacer()
        }
        .padding()
        .background(Color(NSColor.controlBackgroundColor))
    }

    private func detailField(label: String, value: String) -> some View {
        HStack(alignment: .top) {
            Text(label + ":")
                .foregroundColor(.secondary)
                .frame(width: 80, alignment: .leading)
            Text(value)
                .fontWeight(.medium)
        }
    }

    private func displayAscii(_ s: String) -> String {
        return s.replacingOccurrences(of: "\r", with: "\\r")
                .replacingOccurrences(of: "\n", with: "\\n")
    }

    private func formatBytes(_ b: Int) -> String {
        if b < 1024 { return "\(b) B" }
        if b < 1024 * 1024 { return String(format: "%.1f KB", Double(b) / 1024.0) }
        return String(format: "%.1f MB", Double(b) / 1_048_576.0)
    }

    // MARK: - Export Handlers

    private func exportCSV() {
        let panel = NSSavePanel()
        panel.allowedContentTypes = [.commaSeparatedText]
        panel.nameFieldStringValue = "VP01_Traffic_\(Int(Date().timeIntervalSince1970)).csv"
        if panel.runModal() == .OK, let url = panel.url {
            let csv = logger.exportCSV()
            try? csv.write(to: url, atomically: true, encoding: .utf8)
            exportMessage = "Saved \(logger.records.count) packets to \(url.lastPathComponent)"
            showExportSuccess = true
        }
    }

    private func exportLog() {
        let panel = NSSavePanel()
        panel.allowedContentTypes = [.plainText]
        panel.nameFieldStringValue = "VP01_Traffic_\(Int(Date().timeIntervalSince1970)).log"
        if panel.runModal() == .OK, let url = panel.url {
            let log = logger.exportRawLog()
            try? log.write(to: url, atomically: true, encoding: .utf8)
            exportMessage = "Saved raw traffic log to \(url.lastPathComponent)"
            showExportSuccess = true
        }
    }
}
