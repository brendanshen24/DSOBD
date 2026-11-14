import SwiftUI

public struct SettingsView: View {
    @ObservedObject var engine = ELM327Engine.shared

    public init() {}

    public var body: some View {
        Form {
            Section("VEEPEAK VP01 ADAPTER CONNECTION") {
                HStack {
                    Text("Target Presets:")
                        .foregroundColor(.secondary)
                    Button("Veepeak VP01 (192.168.0.10:35000)") {
                        engine.host = "192.168.0.10"
                        engine.port = 35000
                    }
                    .buttonStyle(.bordered)

                    Button("Mock Simulator (127.0.0.1:35000)") {
                        engine.host = "127.0.0.1"
                        engine.port = 35000
                    }
                    .buttonStyle(.bordered)
                }

                TextField("Adapter IP Address:", text: $engine.host)
                    .textFieldStyle(.roundedBorder)

                TextField("Port Number:", value: $engine.port, format: .number)
                    .textFieldStyle(.roundedBorder)

                HStack {
                    if engine.connectionStatus.isConnected {
                        Button("Disconnect", role: .destructive) {
                            engine.disconnect()
                        }
                        .buttonStyle(.borderedProminent)
                        .tint(.red)
                    } else {
                        Button("Connect to Adapter") {
                            engine.connect()
                        }
                        .buttonStyle(.borderedProminent)
                    }

                    Text(engine.connectionStatus.displayText)
                        .font(.system(size: 12))
                        .foregroundColor(engine.connectionStatus.isConnected ? .green : .secondary)
                }
            }

            Section("TELEMETRY & POLLING ENGINE") {
                VStack(alignment: .leading, spacing: 6) {
                    HStack {
                        Text("Polling Interval:")
                        Spacer()
                        Text("\(Int(engine.pollingIntervalMs)) ms (\(String(format: "%.1f", 1000.0 / engine.pollingIntervalMs)) Hz)")
                            .font(.system(size: 12, design: .monospaced))
                            .foregroundColor(.secondary)
                    }

                    Slider(value: $engine.pollingIntervalMs, in: 40...1000, step: 10) {
                        Text("Polling Interval")
                    }
                    Text("Controls how frequently PID queries are dispatched. The engine is reply-gated by the '>' prompt to avoid ELM327 STOPPED errors.")
                        .font(.caption)
                        .foregroundColor(.secondary)
                }
            }

            Section("ADAPTER & PROTOCOL IDENTITY") {
                LabeledContent("ELM327 Firmware:", value: engine.telemetry.elmVersion)
                LabeledContent("Active Protocol:", value: engine.telemetry.protocolName)
                LabeledContent("Battery Voltage:", value: engine.telemetry.voltage)
                LabeledContent("TCP Socket Options:", value: "TCP_NODELAY (ON), SO_KEEPALIVE (ON)")
            }

            Section("ABOUT DSOBD NATIVE MAC") {
                Text("Reconstructed from Android Car Scanner v2.1.50 (OVZ) for macOS native execution. Designed to communicate over Wi-Fi with Veepeak OBDCheck VP01 and compatible ELM327 Wi-Fi dongles with high-resolution packet capture.")
                    .font(.caption)
                    .foregroundColor(.secondary)
            }
        }
        .formStyle(.grouped)
        .padding()
    }
}
