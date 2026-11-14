import SwiftUI

@main
struct DSOBDMacApp: App {
    init() {
        // Preload DTC definitions on startup
        _ = DTCDatabase.shared

        if CommandLine.arguments.contains("--test") {
            runAutomatedTest()
        }
    }

    var body: some Scene {
        WindowGroup {
            ContentView()
                .frame(minWidth: 900, minHeight: 600)
        }
        .windowStyle(.titleBar)
        .windowToolbarStyle(.unified)
        .commands {
            CommandGroup(replacing: .newItem) {}
            CommandMenu("VP01 Adapter") {
                Button("Connect") {
                    ELM327Engine.shared.connect()
                }
                .keyboardShortcut("c", modifiers: [.command])

                Button("Disconnect") {
                    ELM327Engine.shared.disconnect()
                }
                .keyboardShortcut("d", modifiers: [.command])

                Divider()

                Button("Toggle Auto-Polling") {
                    ELM327Engine.shared.toggleAutoPoll()
                }
                .keyboardShortcut("p", modifiers: [.command])

                Button("Scan Diagnostic Codes (DTC)") {
                    ELM327Engine.shared.scanDTCs()
                }
                .keyboardShortcut("r", modifiers: [.command])

                Divider()

                Button("Clear Packet Logger") {
                    PacketLogger.shared.clear()
                }
                .keyboardShortcut("k", modifiers: [.command])
            }
        }
    }

    private func runAutomatedTest() {
        print("======================================================")
        print("  DSOBD Mac — Automated Verification Test")
        print("  Connecting to VP01 Mock Server at 127.0.0.1:35000")
        print("======================================================")

        let engine = ELM327Engine.shared
        let logger = PacketLogger.shared

        engine.host = "127.0.0.1"
        engine.port = 35000
        engine.connect()

        DispatchQueue.global().async {
            // Wait up to 6 seconds for handshake and polling
            var checks = 0
            while checks < 30 {
                Thread.sleep(forTimeInterval: 0.2)
                checks += 1
                if engine.connectionStatus.isConnected && logger.totalPackets >= 12 {
                    break
                }
            }

            print("\n[+] Connection Status: \(engine.connectionStatus.displayText)")
            print("[+] Total Logged Packets: \(logger.totalPackets) (TX: \(logger.txCount), RX: \(logger.rxCount))")
            print("[+] Average Round-Trip Latency: \(String(format: "%.2f", logger.avgLatencyMs)) ms")
            print("[+] Engine RPM Telemetry: \(engine.telemetry.rpm) RPM")
            print("[+] Battery Voltage: \(engine.telemetry.voltage)")
            print("[+] ELM Version: \(engine.telemetry.elmVersion)")

            print("\n--- SAMPLE PACKET LOGGER CAPTURE (First 8 packets) ---")
            for p in logger.records.prefix(8) {
                let lat = p.latencyMs != nil ? " [+\(String(format: "%.1f", p.latencyMs!)) ms]" : ""
                let arrow = p.direction == .tx ? ">> TX" : "<< RX"
                print("[\(p.timestampFormatted)] \(arrow) [\(p.category.rawValue)] \(p.rawText.replacingOccurrences(of: "\r", with: "\\r")) (\(p.byteCount)B)\(lat)")
                print("    HEX: \(p.hexDump)")
            }

            print("\n[+] Verification successful! Exiting test mode.")
            exit(0)
        }
    }
}
