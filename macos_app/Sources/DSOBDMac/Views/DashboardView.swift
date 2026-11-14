import SwiftUI

public struct DashboardView: View {
    @ObservedObject var engine = ELM327Engine.shared

    public init() {}

    public var body: some View {
        ScrollView {
            VStack(spacing: 20) {
                // Top Telemetry Header
                HStack {
                    VStack(alignment: .leading, spacing: 2) {
                        Text("LIVE VEHICLE TELEMETRY")
                            .font(.system(size: 11, weight: .bold))
                            .foregroundColor(.secondary)
                        Text(engine.connectionStatus.displayText)
                            .font(.system(size: 14, weight: .semibold))
                            .foregroundColor(engine.connectionStatus.isConnected ? .green : .secondary)
                    }

                    Spacer()

                    // Auto-poll toggle & interval
                    HStack(spacing: 12) {
                        Toggle("Auto-Polling", isOn: Binding(
                            get: { engine.isAutoPolling },
                            set: { _ in engine.toggleAutoPoll() }
                        ))
                        .toggleStyle(.switch)
                        .disabled(!engine.connectionStatus.isConnected)

                        HStack(spacing: 4) {
                            Text("Rate:")
                                .font(.caption)
                                .foregroundColor(.secondary)
                            Text("\(Int(engine.pollingIntervalMs))ms")
                                .font(.caption.monospacedDigit())
                        }
                    }
                    .padding(8)
                    .background(Color(NSColor.controlBackgroundColor))
                    .cornerRadius(8)
                }
                .padding(.horizontal)
                .padding(.top, 12)

                // Primary Gauges: RPM & Speed
                HStack(spacing: 24) {
                    // RPM Gauge
                    RadialGaugeView(
                        value: Double(engine.telemetry.rpm),
                        minValue: 0,
                        maxValue: 7000,
                        title: "ENGINE RPM",
                        unit: "RPM",
                        primaryColor: .red,
                        warningThreshold: 6000
                    )
                    .frame(width: 240, height: 240)

                    // Speed Gauge
                    RadialGaugeView(
                        value: Double(engine.telemetry.speedMph),
                        minValue: 0,
                        maxValue: 140,
                        title: "VEHICLE SPEED",
                        unit: "MPH",
                        primaryColor: .blue,
                        secondaryValueText: "\(engine.telemetry.speedKmh) km/h"
                    )
                    .frame(width: 240, height: 240)
                }

                // Secondary Metric Cards
                LazyVGrid(columns: [GridItem(.adaptive(minimum: 180))], spacing: 14) {
                    metricCard(
                        title: "COOLANT TEMP",
                        value: "\(engine.telemetry.coolantC) °C",
                        subtitle: "\(engine.telemetry.coolantF) °F",
                        icon: "thermometer.medium",
                        color: engine.telemetry.coolantC > 105 ? .red : .orange
                    )

                    metricCard(
                        title: "MANIFOLD (MAP / BOOST)",
                        value: "\(engine.telemetry.mapKpa) kPa",
                        subtitle: String(format: "+%.1f PSI Boost", engine.telemetry.boostPsi),
                        icon: "gauge.medium",
                        color: .purple
                    )

                    metricCard(
                        title: "BATTERY VOLTAGE",
                        value: engine.telemetry.voltage,
                        subtitle: engine.telemetry.voltageNumeric >= 13.2 ? "Alternator OK" : "Battery",
                        icon: "bolt.batteryblock.fill",
                        color: .yellow
                    )

                    metricCard(
                        title: "THROTTLE (TPS)",
                        value: String(format: "%.1f %%", engine.telemetry.throttlePct),
                        subtitle: "Pedal Position",
                        icon: "speedometer",
                        color: .green,
                        progress: engine.telemetry.throttlePct / 100.0
                    )

                    metricCard(
                        title: "CALCULATED LOAD",
                        value: String(format: "%.1f %%", engine.telemetry.loadPct),
                        subtitle: "Engine Workload",
                        icon: "scalemass.fill",
                        color: .cyan,
                        progress: engine.telemetry.loadPct / 100.0
                    )

                    metricCard(
                        title: "FUEL TRIM (STFT)",
                        value: String(format: "%+.1f %%", engine.telemetry.fuelTrimShort),
                        subtitle: "Bank 1 Short Term",
                        icon: "fuelpump.fill",
                        color: abs(engine.telemetry.fuelTrimShort) > 10 ? .orange : .blue
                    )
                }
                .padding(.horizontal)

                // Quick Action Bar
                HStack(spacing: 12) {
                    Button(action: { engine.sendCustomCommand("ATRV") }) {
                        Label("Check Battery (ATRV)", systemImage: "bolt.fill")
                    }
                    .buttonStyle(.bordered)
                    .disabled(!engine.connectionStatus.isConnected)

                    Button(action: { engine.sendCustomCommand("0100") }) {
                        Label("Supported PIDs (0100)", systemImage: "checklist")
                    }
                    .buttonStyle(.bordered)
                    .disabled(!engine.connectionStatus.isConnected)

                    Button(action: { engine.sendCustomCommand("ATZ") }) {
                        Label("Reset ELM327 (ATZ)", systemImage: "arrow.clockwise")
                    }
                    .buttonStyle(.bordered)
                    .disabled(!engine.connectionStatus.isConnected)

                    Spacer()
                }
                .padding(.horizontal)
                .padding(.bottom, 16)
            }
        }
    }

    private func metricCard(
        title: String,
        value: String,
        subtitle: String,
        icon: String,
        color: Color,
        progress: Double? = nil
    ) -> some View {
        VStack(alignment: .leading, spacing: 8) {
            HStack {
                Text(title)
                    .font(.system(size: 10, weight: .bold))
                    .foregroundColor(.secondary)
                Spacer()
                Image(systemName: icon)
                    .foregroundColor(color)
                    .font(.system(size: 14))
            }

            Text(value)
                .font(.system(size: 20, weight: .bold, design: .monospaced))

            if let p = progress {
                ProgressView(value: max(0.0, min(1.0, p)))
                    .tint(color)
            }

            Text(subtitle)
                .font(.system(size: 11))
                .foregroundColor(.secondary)
        }
        .padding(14)
        .background(Color(NSColor.controlBackgroundColor))
        .cornerRadius(10)
    }
}

// MARK: - Radial Gauge Component

public struct RadialGaugeView: View {
    let value: Double
    let minValue: Double
    let maxValue: Double
    let title: String
    let unit: String
    let primaryColor: Color
    var warningThreshold: Double? = nil
    var secondaryValueText: String? = nil

    private var fraction: Double {
        let clamped = max(minValue, min(maxValue, value))
        return (clamped - minValue) / (maxValue - minValue)
    }

    public var body: some View {
        ZStack {
            // Background Track
            Circle()
                .trim(from: 0.15, to: 0.85)
                .stroke(Color.secondary.opacity(0.18), style: StrokeStyle(lineWidth: 14, lineCap: .round))
                .rotationEffect(.degrees(90))

            // Value Progress Arc
            Circle()
                .trim(from: 0.15, to: 0.15 + fraction * 0.70)
                .stroke(
                    warningThreshold != nil && value >= warningThreshold! ? Color.red : primaryColor,
                    style: StrokeStyle(lineWidth: 14, lineCap: .round)
                )
                .rotationEffect(.degrees(90))
                .animation(.easeOut(duration: 0.15), value: fraction)

            // Center Text
            VStack(spacing: 3) {
                Text(title)
                    .font(.system(size: 10, weight: .bold))
                    .foregroundColor(.secondary)

                Text("\(Int(value))")
                    .font(.system(size: 38, weight: .heavy, design: .monospaced))

                Text(unit)
                    .font(.system(size: 12, weight: .semibold))
                    .foregroundColor(.secondary)

                if let sec = secondaryValueText {
                    Text(sec)
                        .font(.system(size: 11, design: .monospaced))
                        .foregroundColor(.primary.opacity(0.75))
                        .padding(.top, 2)
                }
            }
        }
        .padding(10)
    }
}
