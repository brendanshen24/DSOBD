import Foundation

public struct SensorReading: Identifiable {
    public let id: String
    public let name: String
    public let pid: String
    public var valueText: String
    public var unit: String
    public var numericValue: Double?
    public var minVal: Double?
    public var maxVal: Double?
    public var lastUpdated: Date

    public init(id: String, name: String, pid: String, valueText: String = "--", unit: String, numericValue: Double? = nil) {
        self.id = id
        self.name = name
        self.pid = pid
        self.valueText = valueText
        self.unit = unit
        self.numericValue = numericValue
        self.minVal = numericValue
        self.maxVal = numericValue
        self.lastUpdated = Date()
    }

    public mutating func update(numeric: Double, formattedText: String) {
        self.numericValue = numeric
        self.valueText = formattedText
        self.lastUpdated = Date()
        if minVal == nil || numeric < minVal! { minVal = numeric }
        if maxVal == nil || numeric > maxVal! { maxVal = numeric }
    }
}

public struct VehicleTelemetry {
    public var rpm: Int = 0
    public var speedKmh: Int = 0
    public var speedMph: Int { Int(Double(speedKmh) * 0.621371) }
    public var coolantC: Int = 0
    public var coolantF: Int { Int(Double(coolantC) * 9.0 / 5.0 + 32.0) }
    public var mapKpa: Int = 0
    public var barometricKpa: Double = 101.3
    public var boostPsi: Double {
        // Atmospheric is ~101.3 kPa, boost above atmospheric
        let boostKpa = Double(mapKpa) - barometricKpa
        return max(0.0, boostKpa * 0.145038)
    }
    public var throttlePct: Double = 0.0
    public var loadPct: Double = 0.0
    public var voltage: String = "--"
    public var voltageNumeric: Double = 0.0
    public var fuelTrimShort: Double = 0.0
    public var fuelTrimLong: Double = 0.0
    public var intakeAirTempC: Int = 0
    public var mafGramsPerSec: Double = 0.0
    public var fuelRateLph: Double = 0.0
    public var elmVersion: String = "ELM327"
    public var protocolName: String = "Auto"
    public var vin: String = "--"

    public var supportedPids01_20: UInt32 = 0
    public var hasSupportedPids: Bool = false

    public var sensorsList: [SensorReading] = [
        SensorReading(id: "rpm", name: "Engine RPM", pid: "010C", unit: "RPM"),
        SensorReading(id: "speed", name: "Vehicle Speed", pid: "010D", unit: "km/h"),
        SensorReading(id: "coolant", name: "Coolant Temperature", pid: "0105", unit: "°C"),
        SensorReading(id: "map", name: "Manifold Pressure (MAP)", pid: "010B", unit: "kPa"),
        SensorReading(id: "tps", name: "Throttle Position (TPS)", pid: "0111", unit: "%"),
        SensorReading(id: "load", name: "Calculated Engine Load", pid: "0104", unit: "%"),
        SensorReading(id: "voltage", name: "Battery Voltage", pid: "ATRV", unit: "V"),
        SensorReading(id: "iat", name: "Intake Air Temp (IAT)", pid: "010F", unit: "°C"),
        SensorReading(id: "maf", name: "Mass Air Flow (MAF)", pid: "0110", unit: "g/s"),
        SensorReading(id: "stft1", name: "Short Term Fuel Trim 1", pid: "0106", unit: "%"),
        SensorReading(id: "ltft1", name: "Long Term Fuel Trim 1", pid: "0107", unit: "%"),
        SensorReading(id: "oil_temp", name: "Engine Oil Temp", pid: "015C", unit: "°C"),
        SensorReading(id: "fuel_rate", name: "Engine Fuel Rate", pid: "015E", unit: "L/h"),
    ]

    public mutating func updateSensor(id: String, numeric: Double, formattedText: String) {
        if let idx = sensorsList.firstIndex(where: { $0.id == id }) {
            sensorsList[idx].update(numeric: numeric, formattedText: formattedText)
        }
    }
}
