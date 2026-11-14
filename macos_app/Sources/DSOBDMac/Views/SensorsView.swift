import SwiftUI

public struct SensorsView: View {
    @ObservedObject var engine = ELM327Engine.shared
    @ObservedObject var diagnostics = ELM327Engine.shared.diagnostics
    @State private var filterText: String = ""

    public init() {}

    private var filteredSensors: [SensorReading] {
        if filterText.isEmpty {
            return diagnostics.sensors
        }
        let term = filterText.lowercased()
        return diagnostics.sensors.filter {
            $0.name.lowercased().contains(term) || $0.pid.lowercased().contains(term) || $0.unit.lowercased().contains(term)
        }
    }

    public var body: some View {
        VStack(spacing: 0) {
            // Header Bar
            HStack {
                VStack(alignment: .leading, spacing: 2) {
                    Text("ALL SENSORS & LIVE DATA STREAM")
                        .font(.system(size: 11, weight: .bold))
                        .foregroundColor(.secondary)
                    Text("\(diagnostics.sensors.count) Monitored Parameters")
                        .font(.system(size: 14, weight: .semibold))
                }

                Spacer()

                HStack {
                    Image(systemName: "magnifyingglass")
                        .foregroundColor(.secondary)
                    TextField("Filter sensors...", text: $filterText)
                        .textFieldStyle(.plain)
                        .frame(width: 180)
                }
                .padding(6)
                .background(Color(NSColor.controlBackgroundColor))
                .cornerRadius(6)
            }
            .padding()
            .background(Color(NSColor.controlBackgroundColor))

            Divider()

            // Sensor Table
            Table(filteredSensors) {
                TableColumn("PID") { sensor in
                    Text(sensor.pid)
                        .font(.system(size: 11, weight: .bold, design: .monospaced))
                        .foregroundColor(.accentColor)
                }
                .width(min: 60, ideal: 70, max: 90)

                TableColumn("Parameter Name") { sensor in
                    Text(sensor.name)
                        .font(.system(size: 12, weight: .medium))
                }
                .width(min: 180, ideal: 240)

                TableColumn("Current Value") { sensor in
                    Text(sensor.valueText)
                        .font(.system(size: 13, weight: .bold, design: .monospaced))
                }
                .width(min: 120, ideal: 140)

                TableColumn("Min") { sensor in
                    if let min = sensor.minVal {
                        Text(String(format: "%.1f", min))
                            .font(.system(size: 11, design: .monospaced))
                            .foregroundColor(.secondary)
                    } else {
                        Text("--").foregroundColor(.secondary)
                    }
                }
                .width(min: 60, ideal: 70)

                TableColumn("Max") { sensor in
                    if let max = sensor.maxVal {
                        Text(String(format: "%.1f", max))
                            .font(.system(size: 11, design: .monospaced))
                            .foregroundColor(.secondary)
                    } else {
                        Text("--").foregroundColor(.secondary)
                    }
                }
                .width(min: 60, ideal: 70)

                TableColumn("Unit") { sensor in
                    Text(sensor.unit)
                        .font(.system(size: 11))
                        .foregroundColor(.secondary)
                }
                .width(min: 60, ideal: 80)
            }
        }
        .onAppear { engine.pollAllSensors = true }
        .onDisappear { engine.pollAllSensors = false }
    }
}
