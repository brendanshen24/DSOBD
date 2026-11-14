import Foundation
import Combine

public final class ELM327Engine: ObservableObject {
    public static let shared = ELM327Engine()

    @Published public var connectionStatus: ConnectionStatus = .disconnected
    @Published public var telemetry: VehicleTelemetry = VehicleTelemetry()
    @Published public var dtcList: [DTCRecord] = []
    @Published public var isAutoPolling: Bool = false
    @Published public var pollingIntervalMs: Double = 100.0 // Default 100ms
    @Published public var terminalLogs: [String] = []

    // Settings
    @Published public var host: String = "192.168.0.10"
    @Published public var port: Int = 35000

    let diagnostics = Diagnostics()
    var pollAllSensors = false

    private let transport = OBDConnection()
    private let logger = PacketLogger.shared
    private let decoder = PIDDecoder.shared

    // Reply gating & queue
    private var commandQueue: [QueuedCommand] = []
    private var inFlightCommand: QueuedCommand? = nil
    private var sentTimestamp: Date? = nil
    private var rxBuffer: String = ""
    private var initStep: Int = 0

    private var pollTimer: Timer?
    private var pollIndex: Int = 0
    private let coreQueue = DispatchQueue.main

    private struct QueuedCommand {
        let text: String
        let category: PacketCategory
        let notes: String?
        let onComplete: ((String) -> Void)?
    }

    private let initCommands: [(cmd: String, desc: String)] = [
        ("ATZ", "Reset"),
        ("ATE0", "Echo Off"),
        ("ATL0", "Linefeeds Off"),
        ("ATH0", "Headers Off"),
        ("ATSP0", "Set Protocol Auto"),
        ("ATDP", "Describe Protocol"),
        ("ATRV", "Read Voltage"),
        ("0100", "Query Supported PIDs"),
        ("0120", "PIDs 21–40"), ("0140", "PIDs 41–60"),
        ("0160", "PIDs 61–80"), ("0180", "PIDs 81–A0"),
        ("01A0", "PIDs A1–C0"), ("0133", "Barometric pressure")
    ]

    private init() {
        transport.onBytesReceived = { [weak self] data in
            self?.handleBytesReceived(data)
        }
        transport.onDisconnected = { [weak self] reason in
            DispatchQueue.main.async {
                self?.handleDisconnected(reason: reason)
            }
        }
    }

    public func connect(host: String? = nil, port: Int? = nil) {
        if let h = host { self.host = h }
        if let p = port { self.port = p }

        guard !connectionStatus.isConnected else { return }
        diagnostics.reset(); telemetry = VehicleTelemetry(); dtcList = []
        connectionStatus = .connecting(self.host, self.port)
        logger.logTX(text: "TCP CONNECT \(self.host):\(self.port)", category: .status, notes: "Opening non-blocking TCP socket with TCP_NODELAY")

        DispatchQueue.global(qos: .userInitiated).async { [weak self] in
            guard let self = self else { return }
            let res = self.transport.connect(host: self.host, port: self.port, timeoutSeconds: 5.0)
            DispatchQueue.main.async {
                switch res {
                case .success:
                    self.startHandshake()
                case .failure(let err):
                    self.connectionStatus = .error(err.localizedDescription)
                    self.logger.logRX(text: "CONNECT FAILED: \(err.localizedDescription)", category: .error)
                }
            }
        }
    }

    public func disconnect() {
        stopAutoPoll()
        transport.disconnect(reason: "User requested disconnect")
        handleDisconnected(reason: "Disconnected")
    }

    private func handleDisconnected(reason: String) {
        stopAutoPoll(); diagnostics.stopRecording(); diagnostics.scanning = false
        connectionStatus = .disconnected
        commandQueue.removeAll()
        inFlightCommand = nil
        sentTimestamp = nil
        rxBuffer = ""
        isAutoPolling = false
        logger.logRX(text: "DISCONNECTED: \(reason)", category: .status)
    }

    // MARK: - Handshake Sequence

    private func startHandshake() {
        initStep = 0
        connectionStatus = .initializing("Starting ELM327 Reset...")
        logger.logTX(text: "CONNECTED TO \(host):\(port)", category: .status, notes: "Beginning CarScanner handshake")
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.5) { [weak self] in
            guard let self, case .initializing = self.connectionStatus else { return }
            self.sendNextInitStep()
        }
    }

    private func sendNextInitStep() {
        guard initStep < initCommands.count else {
            // Handshake complete!
            connectionStatus = .connected(telemetry.elmVersion, telemetry.protocolName)
            logger.logRX(text: "READY: Handshake Complete! Connected to \(telemetry.elmVersion)", category: .status)
            appendTerminal("=== Handshake complete! ELM327 ready. ===")
            startAutoPoll()
            return
        }

        let item = initCommands[initStep]
        if !isPidSupported(item.cmd) { initStep += 1; sendNextInitStep(); return }
        connectionStatus = .initializing(item.desc)

        queueCommand(item.cmd, category: .reset, notes: item.desc) { [weak self] response in
            guard let self = self else { return }

            if response == "TIMEOUT" || response == "SEND ERROR" {
                self.transport.disconnect(reason: "Initialization failed")
                self.handleDisconnected(reason: response)
                return
            }
            if item.cmd.starts(with: "ATZ") {
                let lines = response.components(separatedBy: "\r").map { $0.trimmingCharacters(in: .whitespaces) }
                for line in lines where line.contains("ELM327") || line.contains("OBD") {
                    self.telemetry.elmVersion = line
                }
            } else if item.cmd == "ATDP" {
                let clean = PIDDecoder.cleanLine(response)
                if !clean.isEmpty && !clean.contains("?") {
                    self.telemetry.protocolName = clean
                }
            } else if item.cmd == "ATRV" {
                self.decoder.decodeATRV(raw: response, telemetry: &self.telemetry)
            } else if item.cmd == "0100" {
                self.decoder.decodeMode01(cmd: "0100", rawResponse: response, telemetry: &self.telemetry)
            }

            self.initStep += 1
            self.sendNextInitStep()
        }
    }

    // MARK: - Command Queue & Reply Gating

    public func sendCustomCommand(_ cmd: String) {
        let clean = cmd.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !clean.isEmpty else { return }
        let category: PacketCategory
        if clean.uppercased().starts(with: "AT") {
            category = .atCommand
        } else if clean.starts(with: "03") || clean.starts(with: "04") || clean.starts(with: "07") {
            category = .dtcCommand
        } else {
            category = .pidQuery
        }

        queueCommand(clean, category: category, notes: "User command") { [weak self] response in
            self?.appendTerminal("RX> \(response.trimmingCharacters(in: .whitespacesAndNewlines))")
        }
    }

    private func queueCommand(
        _ cmd: String,
        category: PacketCategory = .pidQuery,
        notes: String? = nil,
        onComplete: ((String) -> Void)? = nil
    ) {
        coreQueue.async { [weak self] in
            guard let self = self else { return }
            let item = QueuedCommand(text: cmd, category: category, notes: notes, onComplete: onComplete)
            self.commandQueue.append(item)
            self.pumpQueue()
        }
    }

    private func pumpQueue() {
        guard inFlightCommand == nil, !commandQueue.isEmpty else { return }
        let cmd = commandQueue.removeFirst()
        inFlightCommand = cmd
        sentTimestamp = Date()
        rxBuffer = ""

        // Ensure proper '\r' termination
        let wireCmd = cmd.text.hasSuffix("\r") ? cmd.text : "\(cmd.text)\r"

        logger.logTX(text: wireCmd, category: cmd.category, notes: cmd.notes)
        DispatchQueue.main.async {
            self.appendTerminal("TX> \(cmd.text)")
        }

        if !transport.send(string: wireCmd) {
            DispatchQueue.main.async {
                self.logger.logRX(text: "SEND ERROR", category: .error, notes: "Failed to write to socket")
            }
            inFlightCommand = nil
            cmd.onComplete?("SEND ERROR")
            return
        }
        let stamp = sentTimestamp
        coreQueue.asyncAfter(deadline: .now() + 20) { [weak self] in
            guard let self, self.inFlightCommand != nil, self.sentTimestamp == stamp else { return }
            self.inFlightCommand = nil; self.sentTimestamp = nil; self.rxBuffer = ""
            self.diagnostics.data.scan_error = true
            cmd.onComplete?("TIMEOUT")
            self.pumpQueue()
        }
    }

    private func handleBytesReceived(_ data: Data) {
        guard let text = String(data: data, encoding: .ascii) ?? String(data: data, encoding: .utf8) else {
            return
        }

        coreQueue.async { [weak self] in
            guard let self = self else { return }
            self.rxBuffer.append(text)

            guard self.rxBuffer.utf8.count < 16384 else {
                self.transport.disconnect(reason: "Oversized adapter response")
                self.handleDisconnected(reason: "Oversized adapter response")
                return
            }
            while let prompt = self.rxBuffer.firstIndex(of: ">") {
                let payload = String(self.rxBuffer[..<prompt])
                self.rxBuffer.removeSubrange(...prompt)
                guard let cmd = self.inFlightCommand else { continue }
                self.inFlightCommand = nil
                let latency = self.sentTimestamp.map { Date().timeIntervalSince($0) * 1000 }
                self.sentTimestamp = nil
                self.logger.logRX(text: payload, category: .response, latencyMs: latency, notes: cmd.notes)
                let clean = cmd.text.uppercased().replacingOccurrences(of: " ", with: "")
                self.diagnostics.receive(clean, payload)
                if clean.hasPrefix("01") {
                    self.decoder.decodeMode01(cmd: clean, rawResponse: payload, telemetry: &self.telemetry)
                } else if clean == "ATRV" {
                    self.decoder.decodeATRV(raw: payload, telemetry: &self.telemetry)
                }
                if let baro = self.diagnostics.sensors.first(where: { $0.pid == "0133" })?.numericValue {
                    self.telemetry.barometricKpa = baro
                }
                cmd.onComplete?(payload)
            }
            self.pumpQueue()
        }
    }

    // MARK: - Auto Polling Loop

    public func toggleAutoPoll() {
        if isAutoPolling {
            stopAutoPoll()
        } else {
            startAutoPoll()
        }
    }

    public func startAutoPoll() {
        guard !isAutoPolling else { return }
        isAutoPolling = true

        let interval = max(0.05, pollingIntervalMs / 1000.0)
        pollTimer = Timer.scheduledTimer(withTimeInterval: interval, repeats: true) { [weak self] _ in
            self?.pollNextTelemetry()
        }
    }

    public func stopAutoPoll() {
        isAutoPolling = false
        pollTimer?.invalidate()
        pollTimer = nil
    }

    private let pollingPids: [(pid: String, desc: String)] = [
        ("010C", "Engine RPM"),
        ("010D", "Vehicle Speed"),
        ("010B", "Manifold Absolute Pressure"),
        ("0111", "Throttle Position"),
        ("0105", "Coolant Temperature"),
        ("0104", "Engine Load"),
        ("ATRV", "Battery Voltage"),
        ("010F", "Intake Air Temp"),
        ("0106", "Short Term Fuel Trim")
    ]

    private func isPidSupported(_ cmd: String) -> Bool {
        guard cmd.count == 4, cmd.hasPrefix("01"), let pid = UInt8(cmd.suffix(2), radix: 16) else { return true }
        return diagnostics.supported(pid)
    }

    private func pollNextTelemetry() {
        guard connectionStatus.isConnected, !diagnostics.scanning, commandQueue.isEmpty, inFlightCommand == nil else { return }
        let commands = pollAllSensors ? diagnostics.sensors.map { ($0.pid, $0.name) } : pollingPids.map { ($0.pid, $0.desc) }
        guard !commands.isEmpty else { return }
        for _ in commands {
            let item = commands[pollIndex % commands.count]; pollIndex += 1
            if isPidSupported(item.0) { queueCommand(item.0, notes: item.1); return }
        }
    }

    // MARK: - DTC Scanning & Clearing

    public func scanDTCs() {
        guard connectionStatus.isConnected, !diagnostics.scanning else { return }
        diagnostics.beginScan(); dtcList = []
        scanNext(0)
    }

    private func scanNext(_ index: Int) {
        guard connectionStatus.isConnected else { diagnostics.scanning = false; return }
        guard let cmd = diagnostics.job(index) else {
            diagnostics.finishScan(); dtcList = diagnostics.codes; return
        }
        if cmd.isEmpty { scanNext(index + 1); return }
        queueCommand(cmd, category: .dtcCommand, notes: "Diagnostic scan") { [weak self] response in
            guard let self else { return }
            if response == "TIMEOUT" || response == "SEND ERROR" { self.diagnostics.data.scan_error = true }
            self.dtcList = self.diagnostics.codes
            self.scanNext(index + 1)
        }
    }

    public func clearDTCs(completion: @escaping (Bool) -> Void) {
        guard connectionStatus.isConnected, !diagnostics.scanning else { completion(false); return }
        diagnostics.scanning = true; diagnostics.data.clear_ok = false
        queueCommand("04", category: .dtcCommand, notes: "Clear codes and readiness") { [weak self] _ in
            guard let self else { return }
            self.diagnostics.scanning = false
            let success = self.diagnostics.data.clear_ok
            self.appendTerminal(success ? "Clear acknowledged; rescanning" : "Clear NOT acknowledged")
            completion(success)
            if success { self.scanDTCs() }
        }
    }

    private func appendTerminal(_ msg: String) {
        terminalLogs.append(msg)
        if terminalLogs.count > 1000 {
            terminalLogs.removeFirst(terminalLogs.count - 1000)
        }
    }

    public func clearTerminal() {
        terminalLogs.removeAll()
    }
}
