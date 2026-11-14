import XCTest
import OBDCore
@testable import DSOBDMac

final class DiagnosticsTests: XCTestCase {
    @MainActor
    func testReadingsAndCSV() throws {
        let d = Diagnostics()
        d.receive("0100", "41 00 FF FF FF FF\r>")
        d.receive("0902", "014\r0: 49 02 01 31 48 47\r1: 43 4D 38 32 36 33 33\r2: 41 30 30 34 33 35 32\r>")
        d.receive("06A4", "013\r0: 46 A4 0B 24 00 06\r1: 00 00 FF FF A4 0C 24\r2: 00 0E 00 00 FF FF\r>")
        d.receive("0A", "4A 01 04 20\r>")
        XCTAssertTrue(d.info.contains("VIN: 1HGCM82633A004352"))
        XCTAssertEqual(d.codes.first?.code, "P0420")
        XCTAssertEqual(d.codes.first?.type, .permanent)
        XCTAssertEqual(d.misfires[2], "Cylinder 3: 14 / 6")
        XCTAssertEqual(d.misfires[1], "Cylinder 2: -- / --")
        d.receive("0151", "41 51 01\r>")
        XCTAssertEqual(d.sensors.first(where: { $0.pid == "0151" })?.valueText, "Gasoline")
        let folder = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        try FileManager.default.createDirectory(at: folder, withIntermediateDirectories: true)
        defer { try? FileManager.default.removeItem(at: folder) }
        d.startRecording(directory: folder)
        XCTAssertTrue(d.recording)
        let path = d.recordingStatus
        d.stopRecording()
        XCTAssertFalse(d.recording)
        XCTAssertTrue(try String(contentsOfFile: path).contains("ODOMETER (km)"))
        d.startRecording(directory: folder)
        XCTAssertNotEqual(path, d.recordingStatus)
        d.reset()
        XCTAssertFalse(d.recording)
        XCTAssertTrue(d.codes.isEmpty)
        XCTAssertEqual(d.status, "Not scanned")
    }

    @MainActor
    func testMockConnection() async throws {
        guard let portText = ProcessInfo.processInfo.environment["DSOBD_MOCK_PORT"], let port = Int(portText) else {
            throw XCTSkip("Run with tests/ports_mock.py and DSOBD_MOCK_PORT")
        }
        let e = ELM327Engine.shared
        e.connect(host: "127.0.0.1", port: port)
        defer { e.disconnect() }
        for _ in 0..<200 where !e.connectionStatus.isConnected { try await Task.sleep(nanoseconds: 100_000_000) }
        XCTAssertTrue(e.connectionStatus.isConnected)
        e.stopAutoPoll()
        e.scanDTCs()
        for _ in 0..<400 where e.diagnostics.scanning { try await Task.sleep(nanoseconds: 100_000_000) }
        XCTAssertFalse(e.diagnostics.scanning)
        XCTAssertEqual(e.dtcList.count, 4)
        XCTAssertTrue(e.diagnostics.info.contains("ECU: ECM-EngineControl"))
        XCTAssertEqual(e.diagnostics.misfires[2], "Cylinder 3: 14 / 6")
        XCTAssertTrue(e.diagnostics.freeze.contains { $0.pid == "010C" && $0.numericValue == 2000 })
        XCTAssertEqual(e.diagnostics.sensors.count, 30)
        let folder = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        try FileManager.default.createDirectory(at: folder, withIntermediateDirectories: true)
        defer { try? FileManager.default.removeItem(at: folder) }
        e.diagnostics.startRecording(directory: folder)
        for sensor in e.diagnostics.sensors {
            e.sendCustomCommand(sensor.pid)
            try await Task.sleep(nanoseconds: 400_000_000)
        }
        try await Task.sleep(nanoseconds: 500_000_000)
        e.diagnostics.stopRecording()
        XCTAssertTrue(e.diagnostics.sensors.allSatisfy { $0.numericValue != nil })
        let log = try String(contentsOfFile: e.diagnostics.recordingStatus)
        XCTAssertGreaterThan(log.components(separatedBy: "\n").count, 20)
        XCTAssertEqual(e.diagnostics.sensors.first(where: { $0.pid == "0178" })?.valueText, "550")
        var acknowledged = false
        e.clearDTCs { acknowledged = $0 }
        for _ in 0..<400 where e.diagnostics.scanning { try await Task.sleep(nanoseconds: 100_000_000) }
        XCTAssertTrue(acknowledged)
        XCTAssertEqual(e.dtcList.count, 1)
        XCTAssertEqual(e.dtcList.first?.type, .permanent)
        XCTAssertEqual(e.diagnostics.misfires[2], "Cylinder 3: 0 / 0")
    }
}
