import SwiftUI

public struct TerminalView: View {
    @ObservedObject var engine = ELM327Engine.shared
    @State private var inputCommand: String = ""
    @State private var commandHistory: [String] = []
    @State private var historyIndex: Int = -1

    public init() {}

    private let quickCommands = ["ATZ", "ATE0", "ATRV", "ATDP", "0100", "010C", "010D", "0105", "03", "07", "0902"]

    public var body: some View {
        VStack(spacing: 0) {
            // Header Bar
            HStack {
                VStack(alignment: .leading, spacing: 2) {
                    Text("RAW ELM327 / OBD-II TERMINAL")
                        .font(.system(size: 11, weight: .bold))
                        .foregroundColor(.secondary)
                    Text("Direct serial ASCII stream with VP01")
                        .font(.system(size: 13))
                }

                Spacer()

                Button("Clear Console") {
                    engine.clearTerminal()
                }
                .buttonStyle(.bordered)
            }
            .padding()
            .background(Color(NSColor.controlBackgroundColor))

            Divider()

            // Quick Shortcut Chips
            ScrollView(.horizontal, showsIndicators: false) {
                HStack(spacing: 8) {
                    Text("Quick:")
                        .font(.caption)
                        .foregroundColor(.secondary)

                    ForEach(quickCommands, id: \.self) { cmd in
                        Button(cmd) {
                            sendCommand(cmd)
                        }
                        .buttonStyle(.bordered)
                        .font(.system(size: 11, design: .monospaced))
                        .disabled(!engine.connectionStatus.isConnected)
                    }
                }
                .padding(.horizontal)
                .padding(.vertical, 6)
            }
            .background(Color(NSColor.windowBackgroundColor))

            Divider()

            // Terminal Logs Area
            ScrollViewReader { proxy in
                ScrollView {
                    LazyVStack(alignment: .leading, spacing: 4) {
                        ForEach(Array(engine.terminalLogs.enumerated()), id: \.offset) { index, line in
                            terminalLineView(line)
                                .id(index)
                        }
                    }
                    .padding(10)
                    .frame(maxWidth: .infinity, alignment: .leading)
                }
                .background(Color.black)
                .onChange(of: engine.terminalLogs.count) { _ in
                    if let lastIndex = engine.terminalLogs.indices.last {
                        proxy.scrollTo(lastIndex, anchor: .bottom)
                    }
                }
            }

            Divider()

            // Input Bar
            HStack(spacing: 8) {
                Text(">")
                    .font(.system(size: 14, weight: .bold, design: .monospaced))
                    .foregroundColor(.green)

                TextField("Type command (e.g. ATRV, 010C, 0902)...", text: $inputCommand)
                    .textFieldStyle(.plain)
                    .font(.system(size: 13, design: .monospaced))
                    .onSubmit {
                        sendCommand(inputCommand)
                    }
                    .disabled(!engine.connectionStatus.isConnected)

                Button("Send") {
                    sendCommand(inputCommand)
                }
                .buttonStyle(.borderedProminent)
                .disabled(!engine.connectionStatus.isConnected || inputCommand.trimmingCharacters(in: .whitespaces).isEmpty)
            }
            .padding(10)
            .background(Color(NSColor.controlBackgroundColor))
        }
    }

    private func terminalLineView(_ line: String) -> some View {
        let isTx = line.starts(with: "TX>")
        let isRx = line.starts(with: "RX>")
        let isSystem = line.starts(with: "===")

        let color: Color
        if isTx { color = .cyan }
        else if isRx { color = .green }
        else if isSystem { color = .yellow }
        else { color = .white }

        return Text(line)
            .font(.system(size: 12, design: .monospaced))
            .foregroundColor(color)
            .textSelection(.enabled)
    }

    private func sendCommand(_ cmd: String) {
        let trimmed = cmd.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmed.isEmpty else { return }
        commandHistory.append(trimmed)
        historyIndex = commandHistory.count
        engine.sendCustomCommand(trimmed)
        inputCommand = ""
    }
}
