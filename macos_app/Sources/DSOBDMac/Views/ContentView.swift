import SwiftUI

public enum NavigationTab: String, CaseIterable, Identifiable {
    case dashboard = "Dashboard"
    case packetLogger = "Packet Logger"
    case dtc = "DTC Diagnostics"
    case ecu = "ECU Info"
    case sensors = "All Sensors"
    case terminal = "Terminal"
    case settings = "Settings"

    public var id: String { rawValue }

    public var icon: String {
        switch self {
        case .dashboard: return "gauge.medium"
        case .packetLogger: return "point.3.connected.trianglepath.dotted"
        case .dtc: return "exclamationmark.triangle.fill"
        case .ecu: return "cpu"
        case .sensors: return "list.bullet.rectangle.portrait"
        case .terminal: return "terminal.fill"
        case .settings: return "gearshape.fill"
        }
    }
}

public struct ContentView: View {
    @ObservedObject var engine = ELM327Engine.shared
    @ObservedObject var diagnostics = ELM327Engine.shared.diagnostics
    @ObservedObject var logger = PacketLogger.shared
    @State private var selectedTab: NavigationTab = .dashboard

    public init() {}

    public var body: some View {
        NavigationSplitView {
            List(NavigationTab.allCases, selection: $selectedTab) { tab in
                NavigationLink(value: tab) {
                    Label {
                        HStack {
                            Text(tab.rawValue)
                            Spacer()
                            if tab == .packetLogger && logger.totalPackets > 0 {
                                Text("\(logger.totalPackets)")
                                    .font(.caption2.bold())
                                    .padding(.horizontal, 6)
                                    .padding(.vertical, 1)
                                    .background(Color.blue.opacity(0.2))
                                    .foregroundColor(.blue)
                                    .cornerRadius(8)
                            } else if tab == .dtc && engine.dtcList.count > 0 {
                                Text("\(engine.dtcList.count)")
                                    .font(.caption2.bold())
                                    .padding(.horizontal, 6)
                                    .padding(.vertical, 1)
                                    .background(Color.red.opacity(0.2))
                                    .foregroundColor(.red)
                                    .cornerRadius(8)
                            }
                        }
                    } icon: {
                        Image(systemName: tab.icon)
                    }
                }
            }
            .listStyle(.sidebar)
            .navigationTitle("DSOBD Mac")
        } detail: {
            VStack(spacing: 0) {
                // Top Global Status Bar
                topGlobalBar
                Divider()
                if !diagnostics.recordingStatus.isEmpty { Text(diagnostics.recordingStatus).font(.caption).textSelection(.enabled).padding(4) }

                // Tab Content
                switch selectedTab {
                case .dashboard:
                    DashboardView()
                case .packetLogger:
                    PacketLoggerView()
                case .dtc:
                    DTCScannerView()
                case .ecu:
                    ECUInfoView()
                case .sensors:
                    SensorsView()
                case .terminal:
                    TerminalView()
                case .settings:
                    SettingsView()
                }
            }
        }
    }

    private var topGlobalBar: some View {
        HStack(spacing: 12) {
            // Connection Status Pill
            HStack(spacing: 6) {
                Circle()
                    .fill(engine.connectionStatus.isConnected ? Color.green : Color.red)
                    .frame(width: 8, height: 8)

                Text(engine.connectionStatus.displayText)
                    .font(.system(size: 12, weight: .medium))
            }
            .padding(.horizontal, 10)
            .padding(.vertical, 4)
            .background(Color(NSColor.controlBackgroundColor))
            .cornerRadius(6)

            // Battery Voltage Badge
            if engine.connectionStatus.isConnected {
                HStack(spacing: 4) {
                    Image(systemName: "bolt.fill")
                        .foregroundColor(.yellow)
                    Text(engine.telemetry.voltage)
                        .font(.system(size: 11, weight: .bold, design: .monospaced))
                }
                .padding(.horizontal, 8)
                .padding(.vertical, 4)
                .background(Color(NSColor.controlBackgroundColor))
                .cornerRadius(6)

                // Protocol Badge
                Text(engine.telemetry.protocolName)
                    .font(.system(size: 11, design: .monospaced))
                    .foregroundColor(.secondary)
                    .padding(.horizontal, 8)
                    .padding(.vertical, 4)
                    .background(Color(NSColor.controlBackgroundColor))
                    .cornerRadius(6)
            }

            Spacer()

            Button(diagnostics.recording ? "Stop CSV" : "Record CSV") {
                if diagnostics.recording { diagnostics.stopRecording() }
                else { diagnostics.chooseRecordingFolder() }
            }
            .disabled(!engine.connectionStatus.isConnected && !diagnostics.recording)
            .help(diagnostics.recordingStatus)
            if diagnostics.recording { Text("REC").foregroundColor(.red) }

            // Quick Connect / Disconnect button
            if engine.connectionStatus.isConnected {
                Button("Disconnect") {
                    engine.disconnect()
                }
                .buttonStyle(.bordered)
            } else {
                Button(action: { engine.connect() }) {
                    Label("Connect VP01", systemImage: "wifi")
                }
                .buttonStyle(.borderedProminent)
            }
        }
        .padding(.horizontal, 16)
        .padding(.vertical, 8)
        .background(Color(NSColor.windowBackgroundColor))
    }
}
