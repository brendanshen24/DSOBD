# DSOBD Mac — Reconstructed Native macOS OBD-II Application & VP01 Packet Logger

A high-performance native macOS OBD-II diagnostic and telemetry application reconstructed from the Android **Car Scanner ELM OBD2 (OVZ)** app (`com.ovz.carscanner` v2.1.50) using static reverse-engineering techniques, featuring an integrated real-time **Packet Logger & Network Sniffer** designed specifically for the **Veepeak OBDCheck VP01 Wi-Fi** adapter.

---

## Reverse Engineering & Reconstruction Highlights

From static disassembly of `libassemblies.armeabi-v7a.blob.so` (`CarScannerMaui.dll` and `CarScannerMaui.Droid.dll`):
1. **Transport Layer (`TCPConnectionV2`):**
   - Connects to `192.168.0.10:35000` via non-blocking TCP socket.
   - Socket flags `TCP_NODELAY` and `SO_KEEPALIVE` are required to prevent Nagle coalescing that stalls clone adapters.
2. **Init Handshake Protocol (`OBDDataReader`):**
   - Reset with `ATZ`, then wait for the prompt before sending `ATE0`.
   - Setup sequence: `ATE0` (echo off), `ATL0` (linefeeds off), `ATH0` (headers off), `ATSP0` (protocol automatic).
   - Reply gating: every command is terminated by `\r`, and the subsequent command is only dispatched once the `>` (0x3E) prompt is received or timed out.
3. **DTC Database Extraction:**
   - 5,602 standard OBD-II Diagnostic Trouble Code definitions were reverse-engineered and extracted from the Android app's binary container (`dtcdb.OBDII.db`) into `Resources/dtcdb_obdii.json`.

---

## Key Features

### 📡 Real-Time Packet Logger (VP01 Traffic Sniffer)
- **High-Resolution Timing:** Captures every frame with millisecond-precision timestamps and calculates round-trip latency ($\Delta t$) for every command/prompt transaction.
- **Directional Inspection:** Distinguishes **TX (Laptop → VP01)** and **RX (VP01 → Laptop)** with color-coded badges.
- **Dual Representation:** Live view of both raw ASCII text (with escaped control characters `\r`, `\n`) and complete Hex byte dumps (`41 0C 1A F8...`).
- **Classification & Notes:** Automatically classifies packets into Reset/Init, AT Command, PID Query, DTC Command, Response, and Prompt (`>`).
- **Filtering & Search:** Real-time filter by direction, category, or search string across ASCII, Hex, and notes.
- **Exporting:** One-click export to **CSV table** or **Raw Traffic Log (`.log`)**.

### 🎛️ Live Instrument Dashboard
- **Radial Analog Dials:** Dynamic Engine RPM dial (0–7000 RPM with redline warning) and Vehicle Speedometer (MPH & km/h).
- **Metric Cards:** Coolant Temp (°C & °F), Manifold Absolute Pressure (MAP kPa & Boost PSI), Battery Voltage (ATRV), Throttle Position (TPS %), Calculated Engine Load (%), and Short-Term Fuel Trim (STFT %).
- **Adaptive Auto-Polling:** Reply-gated auto-polling loop with configurable interval (40 ms – 1000 ms) preventing adapter `STOPPED` errors.

### ⚠️ DTC Diagnostics (Check Engine Light)
- Reads stored (03), pending (07) and permanent (0A) codes using the shared validated parser.
- Instant offline code lookup against the 5,602 reverse-engineered descriptions.
- Clear DTCs (Mode 04) with confirmation safeguard to reset the Check Engine Light (MIL).
- Offline DTC dictionary search bar for looking up any generic or manufacturer code.

### 📊 All Sensors Stream
- Discovers supported PIDs through A0 and polls the 43-entry sensor table while this tab is visible, with min/max values and units.
- ECU Info shows VIN, calibration ID, ECU name, fuel type, OBD standard, odometer, readiness, freeze frame and CAN misfire counts (last/current drive and 10-drive weighted average).
- Record CSV in the top bar chooses a folder and streams fresh sensor readings; Stop CSV closes the file. Existing files are never overwritten. Disconnect and normal app termination stop recording.
- Missing data remains unavailable. Clear succeeds only after a mode 04 acknowledgement and automatically rescans, including permanent codes.

### 💻 Raw ELM327 Terminal
- Interactive console for sending arbitrary AT commands or raw hex PIDs with color-coded syntax and quick shortcut chips.

---

## Building and Running

### Requirements
- macOS 13.0 or later
- Swift 5.9+ (Xcode or Command Line Tools installed)

### Commands

```bash
# Build the application
make build

# Run the app directly
make run

# Build the standalone macOS app bundle (DSOBD.app)
make app

# Run parser/model/CSV tests
make test
```

---

## Connecting to Your Adapter

1. **Physical Veepeak OBDCheck VP01:**
   - Plug the VP01 adapter into your car's OBD-II port.
   - On your Mac, connect to the VP01 Wi-Fi network (typically named `WiFi_OBDII` or `V-LINK`).
   - Launch DSOBD Mac and click **"Connect VP01"** (default IP: `192.168.0.10`, port: `35000`).

2. **Mock Simulator (for testing without vehicle):**
   - From the repository root, start the mock server:
     ```bash
     python3 tools/mock_vp01.py --fast
     ```
   - In DSOBD Mac, open **Settings**, select the **"Mock Simulator (127.0.0.1:35000)"** preset, and click **Connect**.
