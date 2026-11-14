import SwiftUI

public struct DTCScannerView: View {
    @ObservedObject var engine = ELM327Engine.shared
    @ObservedObject var diagnostics = ELM327Engine.shared.diagnostics
    @State private var showClearConfirmation: Bool = false
    @State private var manualSearchCode: String = ""
    @State private var manualSearchResult: String? = nil

    public init() {}

    public var body: some View {
        VStack(spacing: 0) {
            // Header Bar
            HStack {
                VStack(alignment: .leading, spacing: 2) {
                    Text("DIAGNOSTIC TROUBLE CODES (DTC)")
                        .font(.system(size: 11, weight: .bold))
                        .foregroundColor(.secondary)
                    Text("\(engine.dtcList.count) Fault Code(s) Detected")
                        .font(.system(size: 14, weight: .semibold))
                }

                Spacer()

                // Actions
                HStack(spacing: 12) {
                    Button(action: scanCodes) {
                        Label(diagnostics.scanning ? "Scanning..." : "Read Fault Codes", systemImage: "arrow.triangle.2.circlepath")
                    }
                    .buttonStyle(.borderedProminent)
                    .disabled(!engine.connectionStatus.isConnected || diagnostics.scanning)

                    Button(action: { showClearConfirmation = true }) {
                        Label("Clear Codes (Reset MIL)", systemImage: "trash.fill")
                    }
                    .buttonStyle(.bordered)
                    .tint(.red)
                    .disabled(!engine.connectionStatus.isConnected || diagnostics.scanning)
                }
            }
            .padding()
            .background(Color(NSColor.controlBackgroundColor))

            Divider()

            Text(diagnostics.status).font(.caption).padding(6)

            // Manual Code Lookup Bar
            HStack {
                Image(systemName: "book.fill")
                    .foregroundColor(.secondary)
                TextField("Look up any code in offline database (e.g. P0420, B0001)...", text: $manualSearchCode)
                    .textFieldStyle(.plain)
                    .onSubmit { performManualLookup() }

                Button("Look Up") { performManualLookup() }
                    .buttonStyle(.bordered)
            }
            .padding(8)
            .background(Color(NSColor.windowBackgroundColor))

            if let result = manualSearchResult {
                HStack {
                    Image(systemName: "info.circle.fill")
                        .foregroundColor(.blue)
                    Text(result)
                        .font(.system(size: 12))
                    Spacer()
                    Button("Dismiss") { manualSearchResult = nil }
                        .buttonStyle(.plain)
                        .font(.caption)
                }
                .padding(8)
                .background(Color.blue.opacity(0.12))
            }

            Divider()

            // Fault Codes List
            if engine.dtcList.isEmpty {
                VStack(spacing: 16) {
                    Spacer()
                    Image(systemName: "checkmark.shield.fill")
                        .font(.system(size: 48))
                        .foregroundColor(.green)
                    Text("No codes currently displayed")
                        .font(.headline)
                    Text("Click 'Read Fault Codes' above to query Mode 03 (Stored) and Mode 07 (Pending), and Mode 0A (Permanent) DTCs from the vehicle ECU.")
                        .font(.subheadline)
                        .foregroundColor(.secondary)
                        .multilineTextAlignment(.center)
                        .frame(maxWidth: 420)
                    Spacer()
                }
                .frame(maxWidth: .infinity, maxHeight: .infinity)
            } else {
                List(engine.dtcList) { dtc in
                    dtcRow(dtc)
                        .padding(.vertical, 4)
                }
                .listStyle(.inset(alternatesRowBackgrounds: true))
            }
        }
        .confirmationDialog(
            "Clear Diagnostic Trouble Codes?",
            isPresented: $showClearConfirmation,
            titleVisibility: .visible
        ) {
            Button("Clear All Codes (Mode 04)", role: .destructive) {
                engine.clearDTCs { _ in }
            }
            Button("Cancel", role: .cancel) {}
        } message: {
            Text("This will send OBD-II Mode 04 to clear all stored fault codes, freeze frame data, and reset emissions readiness. Permanent codes remain until the ECU clears them. Use ignition on, engine off.")
        }
    }

    private func dtcRow(_ dtc: DTCRecord) -> some View {
        HStack(alignment: .top, spacing: 14) {
            // Code Badge
            VStack(spacing: 2) {
                Text(dtc.code)
                    .font(.system(size: 15, weight: .bold, design: .monospaced))
                    .padding(.horizontal, 8)
                    .padding(.vertical, 4)
                    .background(Color.red.opacity(0.18))
                    .foregroundColor(.red)
                    .cornerRadius(6)

                Text(dtc.type.rawValue)
                    .font(.system(size: 8, weight: .semibold))
                    .foregroundColor(.secondary)
            }
            .frame(width: 85)

            // Description & System
            VStack(alignment: .leading, spacing: 4) {
                Text(dtc.description)
                    .font(.system(size: 13, weight: .medium))
                    .foregroundColor(.primary)

                HStack(spacing: 8) {
                    Text(dtc.system)
                        .font(.system(size: 11))
                        .foregroundColor(.secondary)

                    Text("•")
                        .foregroundColor(.secondary)

                    Text(dtc.timestamp, style: .time)
                        .font(.system(size: 11, design: .monospaced))
                        .foregroundColor(.secondary)
                }
            }

            Spacer()
        }
    }

    private func scanCodes() {
        engine.scanDTCs()
    }

    private func performManualLookup() {
        let code = manualSearchCode.trimmingCharacters(in: .whitespacesAndNewlines).uppercased()
        guard !code.isEmpty else { return }
        let desc = DTCDatabase.shared.lookup(code: code)
        manualSearchResult = "[\(code)]: \(desc)"
    }
}
