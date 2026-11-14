import Foundation

public final class DTCDatabase {
    public static let shared = DTCDatabase()

    private var database: [String: String] = [:]

    private init() {
        loadDatabase()
    }

    private func loadDatabase() {
        var data: Data? = nil

        // 1. Try Bundle.main resources (works in .app bundle Contents/Resources)
        if let url = Bundle.main.url(forResource: "dtcdb_obdii", withExtension: "json") {
            data = try? Data(contentsOf: url)
        }

        // 2. Try alongside the executable
        if data == nil, let exeURL = Bundle.main.executableURL {
            let alongside = exeURL.deletingLastPathComponent().appendingPathComponent("Resources/dtcdb_obdii.json")
            data = try? Data(contentsOf: alongside)
        }

        // 3. Fallback: check relative and absolute paths
        if data == nil {
            let possiblePaths = [
                "Resources/dtcdb_obdii.json",
                "../Resources/dtcdb_obdii.json",
                "Sources/DSOBDMac/Resources/dtcdb_obdii.json",
                "/Users/brendan/DSOBD/macos_app/Resources/dtcdb_obdii.json",
                "/tmp/dtcdb_obdii.json"
            ]
            for path in possiblePaths {
                if let fileData = try? Data(contentsOf: URL(fileURLWithPath: path)) {
                    data = fileData
                    break
                }
            }
        }

        if let data = data,
           let dict = try? JSONDecoder().decode([String: String].self, from: data) {
            self.database = dict
            print("[DTCDatabase] Loaded \(dict.count) DTC definitions.")
        } else {
            print("[DTCDatabase] Warning: Could not load DTC database file, using standard core fallback.")
            self.database = Self.defaultFallbackDatabase
        }
    }

    public func lookup(code: String) -> String {
        let cleaned = code.trimmingCharacters(in: .whitespacesAndNewlines).uppercased()
        if let desc = database[cleaned] {
            return desc
        }
        // Fallback generic descriptions by code family
        if cleaned.hasPrefix("P0") {
            return "Standard ISO/SAE Powertrain Fault"
        } else if cleaned.hasPrefix("P1") {
            return "Manufacturer-Specific Powertrain Fault"
        } else if cleaned.hasPrefix("C0") {
            return "Standard Chassis (Braking/Steering) Fault"
        } else if cleaned.hasPrefix("B0") {
            return "Standard Body System Fault"
        } else if cleaned.hasPrefix("U0") {
            return "Standard Network / CAN Communication Fault"
        }
        return "Unknown Diagnostic Trouble Code"
    }

    public static let defaultFallbackDatabase: [String: String] = [
        "P0100": "Mass or Volume Air Flow Circuit Malfunction",
        "P0101": "Mass or Volume Air Flow Circuit Range/Performance Problem",
        "P0102": "Mass or Volume Air Flow Circuit Low Input",
        "P0103": "Mass or Volume Air Flow Circuit High Input",
        "P0115": "Engine Coolant Temperature Circuit Malfunction",
        "P0116": "Engine Coolant Temperature Circuit Range/Performance",
        "P0117": "Engine Coolant Temperature Circuit Low Input",
        "P0118": "Engine Coolant Temperature Circuit High Input",
        "P0120": "Throttle Position Sensor/Switch A Circuit Malfunction",
        "P0121": "Throttle Position Sensor/Switch A Circuit Range/Performance",
        "P0122": "Throttle Position Sensor/Switch A Circuit Low Input",
        "P0123": "Throttle Position Sensor/Switch A Circuit High Input",
        "P0130": "O2 Sensor Circuit Malfunction (Bank 1 Sensor 1)",
        "P0131": "O2 Sensor Circuit Low Voltage (Bank 1 Sensor 1)",
        "P0132": "O2 Sensor Circuit High Voltage (Bank 1 Sensor 1)",
        "P0133": "O2 Sensor Circuit Slow Response (Bank 1 Sensor 1)",
        "P0134": "O2 Sensor Circuit No Activity Detected (Bank 1 Sensor 1)",
        "P0171": "System Too Lean (Bank 1)",
        "P0172": "System Too Rich (Bank 1)",
        "P0300": "Random/Multiple Cylinder Misfire Detected",
        "P0301": "Cylinder 1 Misfire Detected",
        "P0302": "Cylinder 2 Misfire Detected",
        "P0303": "Cylinder 3 Misfire Detected",
        "P0304": "Cylinder 4 Misfire Detected",
        "P0420": "Catalyst System Efficiency Below Threshold (Bank 1)",
        "P0430": "Catalyst System Efficiency Below Threshold (Bank 2)",
        "P0440": "Evaporative Emission Control System Malfunction",
        "P0442": "Evaporative Emission System Leak Detected (Small Leak)",
        "P0500": "Vehicle Speed Sensor Malfunction"
    ]
}
