import Foundation
import Combine
import AppKit
import OBDCore

final class Diagnostics: ObservableObject {
    @Published var sensors: [SensorReading] = []
    @Published var freeze: [SensorReading] = []
    @Published var codes: [DTCRecord] = []
    @Published var info: [String] = []
    @Published var misfires: [String] = []
    @Published var status = "Not scanned"
    @Published var scanning = false
    @Published var recording = false
    @Published var recordingStatus = ""
    var data = ObdDiagnostics()
    private var csv = ObdCsv()
    private var timer: Timer?
    private var terminationObserver: NSObjectProtocol?
    private var now: UInt32 { UInt32(truncatingIfNeeded: UInt64(ProcessInfo.processInfo.systemUptime * 1000)) }

    init() {
        terminationObserver = NotificationCenter.default.addObserver(forName: NSApplication.willTerminateNotification, object: nil, queue: .main) { [weak self] _ in self?.stopRecording() }
    }
    func reset() { stopRecording(); data = ObdDiagnostics(); scanning = false; status = "Not scanned"; refresh() }
    func beginScan() { obd_scan_reset(&data); scanning = true; status = "Reading ECU…"; refresh() }
    func job(_ index: Int) -> String? { obd_scan_job(&data, Int32(index)).map { String(cString: $0) } }
    func supported(_ pid: UInt8) -> Bool { obd_supported(&data, pid) }
    func receive(_ command: String, _ response: String) {
        obd_response(&data, command, response)
        refresh()
    }
    func finishScan() {
        scanning = false
        status = data.scan_error ? "Scan incomplete — some requests failed" : "Scan finished; unavailable readings show --"
        refresh()
    }
    func reading(_ i: Int32, freeze: Bool) -> SensorReading {
        let p = obd_sensor(i)!.pointee
        let pid = String(format: "01%02X", p.pid)
        var result = SensorReading(id: pid, name: String(cString: p.name), pid: pid, unit: String(cString: p.unit))
        if obd_has(&data, i, freeze) {
            let v = obd_value(&data, i, freeze)
            let formatted = obd_enum(p.pid, Int32(v)).map { String(cString: $0) } ?? String(format: "%.*f", Int32(p.decimals), v)
            result.update(numeric: v, formattedText: formatted)
        }
        return result
    }
    private func refresh() {
        sensors = (0..<Int32(OBD_SENSOR_COUNT)).filter { supported(obd_sensor($0)!.pointee.pid) }.map { i in
            var next = reading(i, freeze: false)
            if let previous = sensors.first(where: { $0.id == next.id }), let value = next.numericValue {
                next.minVal = min(previous.minVal ?? value, value)
                next.maxVal = max(previous.maxVal ?? value, value)
            }
            return next
        }
        freeze = (0..<Int32(OBD_SENSOR_COUNT)).filter { obd_has(&data, $0, true) }.map { reading($0, freeze: true) }
        codes = (0..<data.code_count).compactMap { i in
            guard let c = obd_code(&data, i)?.pointee else { return nil }
            var out = [CChar](repeating: 0, count: 6)
            obd_format_code(c.value, &out)
            let code = String(cString: out)
            return DTCRecord(code: code, description: DTCDatabase.shared.lookup(code: code), type: c.kind == 0 ? .stored : c.kind == 1 ? .pending : .permanent)
        }
        let names = ["VIN", "Calibration ID", "ECU", "Protocol"]
        info = (0..<4).map { i in
            let value = String(cString: obd_text(&data, Int32(i)))
            return "\(names[i]): \(value.isEmpty ? "--" : value)"
        }
        let ready = (data.ready_supported & ~data.incomplete).nonzeroBitCount
        info.append(data.monitors ? "Check-engine light: \(data.mil ? "ON" : "OFF") · Readiness: \(ready)/\(data.ready_supported.nonzeroBitCount)" : "MIL / readiness: not reported")
        if data.freeze_code != 0 {
            var code = [CChar](repeating: 0, count: 6); obd_format_code(data.freeze_code, &code)
            info.append("Freeze-frame code: \(String(cString: code))")
        }
        misfires = (0..<12).map { c in
            let drive = obd_misfire(&data, Int32(c), false), average = obd_misfire(&data, Int32(c), true)
            return "Cylinder \(c+1): \(drive < 0 ? "--" : String(drive)) / \(average < 0 ? "--" : String(average))"
        }
    }
    func startRecording(directory: URL) {
        guard !recording else { return }
        if obd_csv_start(&csv, directory.path, &data, now) {
            recording = true
            recordingStatus = withUnsafePointer(to: &csv.path) { String(cString: UnsafeRawPointer($0).assumingMemoryBound(to: CChar.self)) }
            timer = Timer.scheduledTimer(withTimeInterval: 0.1, repeats: true) { [weak self] _ in
                guard let self else { return }
                if !obd_csv_tick(&self.csv, &self.data, self.now) { self.recordingFailed() }
            }
        } else { recordingFailed() }
    }
    func chooseRecordingFolder() {
        let panel = NSOpenPanel()
        panel.canChooseDirectories = true; panel.canChooseFiles = false; panel.canCreateDirectories = true
        panel.prompt = "Record CSV Here"
        if panel.runModal() == .OK, let url = panel.url { startRecording(directory: url) }
    }
    func stopRecording() {
        timer?.invalidate(); timer = nil
        if !obd_csv_stop(&csv) { recordingFailed() }
        recording = false
    }
    private func recordingFailed() {
        timer?.invalidate(); timer = nil; recording = false
        recordingStatus = withUnsafePointer(to: &csv.error) { String(cString: UnsafeRawPointer($0).assumingMemoryBound(to: CChar.self)) }
    }
}
