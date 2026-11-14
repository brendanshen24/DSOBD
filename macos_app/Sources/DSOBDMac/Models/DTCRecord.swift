import Foundation

public enum DTCType: String, Codable {
    case stored = "Stored (Mode 03)"
    case pending = "Pending (Mode 07)"
    case permanent = "Permanent (Mode 0A)"
}

public struct DTCRecord: Identifiable, Codable {
    public let id: UUID
    public let code: String
    public let system: String
    public let description: String
    public let type: DTCType
    public let timestamp: Date

    public init(id: UUID = UUID(), code: String, description: String, type: DTCType, timestamp: Date = Date()) {
        self.id = id
        self.code = code
        self.description = description
        self.type = type
        self.timestamp = timestamp

        // Parse system category from code prefix
        let prefix = code.prefix(1)
        switch prefix {
        case "P": self.system = "Powertrain (Engine/Transmission)"
        case "C": self.system = "Chassis (Brakes/Suspension)"
        case "B": self.system = "Body (Airbags/AC/Lighting)"
        case "U": self.system = "Network (CAN/Communication)"
        default:  self.system = "Diagnostic Code"
        }
    }
}
