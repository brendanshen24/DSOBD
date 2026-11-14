import Foundation

public final class PIDDecoder {
    public static let shared = PIDDecoder()

    private init() {}

    /// Strips spaces, newlines, and carriage returns, returns clean uppercase hex or string
    public static func cleanLine(_ line: String) -> String {
        line.replacingOccurrences(of: "\r", with: "")
            .replacingOccurrences(of: "\n", with: "")
            .replacingOccurrences(of: ">", with: "")
            .trimmingCharacters(in: .whitespaces)
    }

    /// Check if line is negative response or error
    public static func isErrorOrNoData(_ line: String) -> Bool {
        let upper = line.uppercased()
        return upper.contains("NO DATA") ||
               upper.contains("ERROR") ||
               upper.contains("STOPPED") ||
               upper.contains("UNABLE TO CONNECT") ||
               upper.contains("BUS INIT: ERROR") ||
               upper.contains("CAN ERROR") ||
               upper.contains("?")
    }

    /// Decodes Mode 01 PID response into engineering values
    public func decodeMode01(cmd: String, rawResponse: String, telemetry: inout VehicleTelemetry) {
        let cleanCmd = cmd.uppercased().replacingOccurrences(of: " ", with: "")
        guard cleanCmd.count >= 4 else { return }
        let pidHex = String(cleanCmd.suffix(2))

        let lines = rawResponse.replacingOccurrences(of: "\r", with: "\n").components(separatedBy: "\n")
        for rawLine in lines {
            let clean = Self.cleanLine(rawLine)
            if clean.isEmpty || Self.isErrorOrNoData(clean) { continue }

            // Remove spaces for byte extraction: e.g. "41 0C 1A F8" -> "410C1AF8"
            let compact = clean.replacingOccurrences(of: " ", with: "").uppercased()
            guard compact.starts(with: "41"), compact.count >= 4,
                  compact[compact.index(compact.startIndex, offsetBy: 2)..<compact.index(compact.startIndex, offsetBy: 4)] == pidHex else {
                continue
            }

            let byteHexes = stride(from: 4, to: compact.count, by: 2).compactMap { i -> UInt8? in
                let end = min(compact.count, i + 2)
                guard end - i == 2 else { return nil }
                let startIndex = compact.index(compact.startIndex, offsetBy: i)
                let endIndex = compact.index(compact.startIndex, offsetBy: end)
                return UInt8(compact[startIndex..<endIndex], radix: 16)
            }
            guard !byteHexes.isEmpty else { continue }

            switch pidHex {
            case "00": // Supported PIDs 01-20 (OR across all responding ECUs)
                if byteHexes.count >= 4 {
                    let val = (UInt32(byteHexes[0]) << 24) |
                              (UInt32(byteHexes[1]) << 16) |
                              (UInt32(byteHexes[2]) << 8)  |
                               UInt32(byteHexes[3])
                    telemetry.supportedPids01_20 |= val
                    telemetry.hasSupportedPids = true
                }

            case "04": // Calculated engine load
                let pct = Double(byteHexes[0]) * 100.0 / 255.0
                telemetry.loadPct = pct
                telemetry.updateSensor(id: "load", numeric: pct, formattedText: String(format: "%.1f %%", pct))

            case "05": // Engine coolant temperature: A - 40
                let tempC = Int(byteHexes[0]) - 40
                telemetry.coolantC = tempC
                telemetry.updateSensor(id: "coolant", numeric: Double(tempC), formattedText: "\(tempC) °C (\(telemetry.coolantF) °F)")

            case "06": // Short term fuel trim Bank 1: (A - 128) * 100 / 128
                let stft = (Double(byteHexes[0]) - 128.0) * 100.0 / 128.0
                telemetry.fuelTrimShort = stft
                telemetry.updateSensor(id: "stft1", numeric: stft, formattedText: String(format: "%+.1f %%", stft))

            case "07": // Long term fuel trim Bank 1: (A - 128) * 100 / 128
                let ltft = (Double(byteHexes[0]) - 128.0) * 100.0 / 128.0
                telemetry.fuelTrimLong = ltft
                telemetry.updateSensor(id: "ltft1", numeric: ltft, formattedText: String(format: "%+.1f %%", ltft))

            case "0B": // Intake manifold absolute pressure (MAP): A kPa
                let kpa = Int(byteHexes[0])
                telemetry.mapKpa = kpa
                telemetry.updateSensor(id: "map", numeric: Double(kpa), formattedText: "\(kpa) kPa (\(String(format: "%.1f", telemetry.boostPsi)) PSI)")

            case "0C": // Engine RPM: ((A * 256) + B) / 4
                if byteHexes.count >= 2 {
                    let rpm = (Int(byteHexes[0]) * 256 + Int(byteHexes[1])) / 4
                    telemetry.rpm = rpm
                    telemetry.updateSensor(id: "rpm", numeric: Double(rpm), formattedText: "\(rpm) RPM")
                }

            case "0D": // Vehicle speed: A km/h
                let speed = Int(byteHexes[0])
                telemetry.speedKmh = speed
                telemetry.updateSensor(id: "speed", numeric: Double(speed), formattedText: "\(speed) km/h (\(telemetry.speedMph) mph)")

            case "0F": // Intake Air Temp: A - 40
                let iat = Int(byteHexes[0]) - 40
                telemetry.intakeAirTempC = iat
                telemetry.updateSensor(id: "iat", numeric: Double(iat), formattedText: "\(iat) °C")

            case "10": // MAF: ((A * 256) + B) / 100
                if byteHexes.count >= 2 {
                    let maf = Double(Int(byteHexes[0]) * 256 + Int(byteHexes[1])) / 100.0
                    telemetry.mafGramsPerSec = maf
                    telemetry.updateSensor(id: "maf", numeric: maf, formattedText: String(format: "%.2f g/s", maf))
                }

            case "11": // Throttle position: A * 100 / 255
                let tps = Double(byteHexes[0]) * 100.0 / 255.0
                telemetry.throttlePct = tps
                telemetry.updateSensor(id: "tps", numeric: tps, formattedText: String(format: "%.1f %%", tps))

            case "5C": // Engine oil temp: A - 40
                let oilTemp = Int(byteHexes[0]) - 40
                telemetry.updateSensor(id: "oil_temp", numeric: Double(oilTemp), formattedText: "\(oilTemp) °C")

            case "5E": // Engine fuel rate: ((A * 256) + B) / 20 L/h
                if byteHexes.count >= 2 {
                    let rate = Double(Int(byteHexes[0]) * 256 + Int(byteHexes[1])) / 20.0
                    telemetry.fuelRateLph = rate
                    telemetry.updateSensor(id: "fuel_rate", numeric: rate, formattedText: String(format: "%.2f L/h", rate))
                }

            default:
                break
            }
        }
    }

    /// Decodes ATRV (battery voltage) e.g. "13.8V"
    public func decodeATRV(raw: String, telemetry: inout VehicleTelemetry) {
        let clean = Self.cleanLine(raw)
        if clean.contains("V") {
            let numStr = clean.replacingOccurrences(of: "V", with: "").trimmingCharacters(in: .whitespaces)
            if let num = Double(numStr) {
                telemetry.voltage = String(format: "%.1fV", num)
                telemetry.voltageNumeric = num
                telemetry.updateSensor(id: "voltage", numeric: num, formattedText: telemetry.voltage)
            }
        }
    }

    /// Decodes Mode 03 / Mode 07 DTC hex strings into an array of DTC codes (e.g. ["P0300", "P0171"])
    public func decodeDTCs(raw: String, type: DTCType) -> [DTCRecord] {
        var results: [DTCRecord] = []
        let clean = raw.replacingOccurrences(of: "\r", with: "\n")
        let lines = clean.components(separatedBy: "\n").map { $0.trimmingCharacters(in: .whitespaces) }

        for line in lines {
            let compact = line.replacingOccurrences(of: " ", with: "").uppercased()
            // Look for Mode 03 response prefix "43" or Mode 07 response prefix "47"
            guard compact.starts(with: "43") || compact.starts(with: "47") else { continue }

            let payload: Substring
            if compact.count >= 4 {
                // e.g. "43 01 03 00 00 00" -> compact: "430103000000"
                // byte 1 is count or first DTC byte
                payload = compact.dropFirst(2)
            } else {
                continue
            }

            // Extract pairs of 2 bytes (4 hex chars)
            var idx = payload.startIndex
            // If the next byte is count (e.g. 01, 02), check if total length aligns
            var hexPairs: [String] = []
            while idx < payload.endIndex {
                let nextIdx = payload.index(idx, offsetBy: 4, limitedBy: payload.endIndex) ?? payload.endIndex
                let chunk = String(payload[idx..<nextIdx])
                if chunk.count == 4 && chunk != "0000" {
                    hexPairs.append(chunk)
                }
                idx = nextIdx
            }

            for chunk in hexPairs {
                if let code = parseSingleDTC(chunk) {
                    let desc = DTCDatabase.shared.lookup(code: code)
                    results.append(DTCRecord(code: code, description: desc, type: type))
                }
            }
        }

        return results
    }

    /// Converts 4 hex digits into standard SAE DTC string (e.g. "0300" -> "P0300")
    private func parseSingleDTC(_ hex4: String) -> String? {
        guard hex4.count == 4, let firstByte = UInt8(hex4.prefix(2), radix: 16) else { return nil }
        let highNibble = (firstByte >> 6) & 0x03
        let secondNibble = (firstByte >> 4) & 0x03

        let prefix: Character
        switch highNibble {
        case 0: prefix = "P" // Powertrain
        case 1: prefix = "C" // Chassis
        case 2: prefix = "B" // Body
        case 3: prefix = "U" // Network
        default: prefix = "P"
        }

        let num1 = String(secondNibble)
        let rest = hex4.suffix(3)
        return "\(prefix)\(num1)\(rest)"
    }
}
