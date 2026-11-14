// swift-tools-version: 5.9
import PackageDescription

let package = Package(
    name: "DSOBDMac",
    platforms: [
        .macOS(.v13)
    ],
    products: [
        .executable(
            name: "DSOBDMac",
            targets: ["DSOBDMac"]
        )
    ],
    dependencies: [],
    targets: [
        .target(name: "OBDCore", path: "Sources/OBDCore", publicHeadersPath: "include"),
        .executableTarget(
            name: "DSOBDMac",
            dependencies: ["OBDCore"],
            path: "Sources/DSOBDMac"
        ),
        .testTarget(name: "DSOBDMacTests", dependencies: ["DSOBDMac", "OBDCore"])
    ]
)
