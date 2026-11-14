import SwiftUI

struct ECUInfoView: View {
    @ObservedObject var engine = ELM327Engine.shared
    @ObservedObject var diagnostics = ELM327Engine.shared.diagnostics
    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 12) {
                HStack {
                    Text("ECU INFO & EMISSIONS").font(.headline)
                    Spacer()
                    Button(diagnostics.scanning ? "Scanning…" : "Scan ECU") { engine.scanDTCs() }
                        .disabled(!engine.connectionStatus.isConnected || diagnostics.scanning)
                }
                Text(diagnostics.status).foregroundColor(.secondary)
                ForEach(diagnostics.info, id: \.self) { Text($0).textSelection(.enabled) }
                ForEach(diagnostics.sensors.filter { ["011C", "0151", "01A6", "0131", "0121"].contains($0.pid) }) { s in
                    Text("\(s.name): \(s.valueText) \(s.unit)")
                }
                Divider()
                Text("Freeze frame").font(.headline)
                if diagnostics.freeze.isEmpty { Text("Not reported").foregroundColor(.secondary) }
                ForEach(diagnostics.freeze) { s in Text("\(s.name): \(s.valueText) \(s.unit)") }
                Divider()
                Text("Misfires: last/current drive / 10-drive weighted average").font(.headline)
                Text("-- means unavailable. Standard CAN monitor IDs only.").font(.caption).foregroundColor(.secondary)
                ForEach(diagnostics.misfires, id: \.self) { Text($0).monospaced() }
            }.padding()
        }
    }
}
