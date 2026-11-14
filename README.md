# DSOBD — Nintendo DS, 3DS, PSP & macOS OBD-II Suite

Connects to a Veepeak OBDCheck VP01 Wi-Fi adapter (or compatible ELM327 Wi-Fi dongle) to display vehicle telemetry.

## Nintendo DS gauge app

The main DS app lives in `source/` and `include/`. `make` builds `dsobd.nds` directly from those folders. The former `turboxs-dtec/` source tree has been consolidated here.

- **Multi-gauge:** tachometer, speed, throttle, coolant, engine load, voltage, and packet count.
- **Instrument graphics:** metal dial sprites, beveled controls, and antialiased Barlow text/numerals. The multi-gauge screen always shows speed; its final tile shows load until boost is detected.
- **Sound effects:** short navigation, confirmation, connected, and link-lost cues. Tap the gear/mode title on the touch screen to open **Settings**, or cycle SELECT to Settings. Tap **SOUND EFFECTS** (or press **A**) to mute/unmute, **X / TEST SOUND** to preview, and **B / BACK** to return. Muting stops the current cue immediately. Sound defaults on; the setting is saved as `dsobd-settings.ini` at the root of the flashcart/DSi SD card. Without writable storage, the toggle still works for the session and Settings shows **SESSION ONLY / SD UNAVAILABLE**.
- **Analog:** Tach on the left, Speed on the right.
- **Metric units:** km/h, °C, and kPa throughout.
- **Automatic boost display:** the multi-gauge load tile becomes boost after three consecutive MAP readings exceed ambient pressure by 5 kPa. Ambient is the car's barometric PID 33, read once at connect, or 101 kPa if the car does not report it. Boost then stays enabled through idle for that connection; reconnecting resets detection. The scope and session stats also hide boost until detected. A car idling without boost is not classified as naturally aspirated.
- **Trouble codes (CODES screen):** **A / SCAN** reads the check-engine lamp, ECU code count and emissions readiness (mode 01 PID 01), stored, pending and permanent codes (modes 03, 07, 0A), the freeze frame (the code that stored it plus load, coolant, RPM and speed), VIN (09 02), protocol (ATDP), and distance since codes were cleared / with the lamp on. Works on CAN and on older ISO 9141, KWP and J1850 cars. **Y / CLEAR** sends mode 04 after a second press; this also erases the freeze frame and resets readiness monitors, so do it with the engine off and ignition on. Permanent codes cannot be cleared by a scan tool; the ECU removes them after the fault's monitor passes. Codes are shown as P/C/B/U numbers without descriptions.
- **Sensors screen:** every supported mode 01 PID from a 43-entry table (fuel trims, MAF, timing, intake/ambient/oil/catalyst temperatures, O2 sensors, fuel level and rate, rail pressure, ECU voltage, run time, engine torque, intercooler, exhaust gas and DPF temperatures, odometer, fuel type, OBD standard, and more), eight per page. Only the visible page is polled, so its rows refresh several times a second. The list comes from the car's supported-PID bitmaps, read at connect: 01 00, then each further range (01 20 ... 01 A0) only if the previous one says it exists.
- **ECU INFO screen:** **A / SCAN** (the same scan as CODES) shows the VIN, the engine computer's name and calibration ID (09 0A, 09 04), OBD standard, fuel type, odometer (PID A6, mostly 2019+ cars), and misfire counts per cylinder (mode 06): this drive cycle and the average of the last 10. Misfire counts are CAN-only; older cars and cars without the standard misfire monitor IDs show NOT REPORTED.
- **CSV logging:** **R** starts and stops a log on any screen except Settings and the vehicle picker. The SD card gets `dsobd-log-001.csv`, `-002`, ... (never overwritten): a `time_s` column plus one column per sensor, one row per 100 ms holding every value read in that window, blank where a sensor was not read. It logs whatever the current screen polls: RPM, speed, MAP, throttle, coolant and load on the gauges, or the visible page on SENSORS. A red REC badge and the row count show while recording. Data is flushed every 5 seconds, so switching off without stopping loses at most the last few seconds.
- **Priority polling:** RPM, speed, one rotating secondary reading, then repeat. Each request waits for the previous ELM327 `>` prompt, with no added polling delay. Unsupported PIDs are skipped, and manual requests wait for the current reply. Timeouts use elapsed milliseconds rather than rendered frames. Actual refresh rate depends on the adapter and vehicle protocol.
- **Vehicle picker:** D-pad movement keeps the selected row visible. Touch PREV/NEXT changes page and selection together. A tap acts once; tap an already selected vehicle or CONNECT to link.

The gauge app defaults to `WiFi_OBDII`, `192.168.0.10:35000`; these are defined in `include/dtec_common.h`. It uses DHCP with a static `192.168.0.11` fallback for VP01 adapters. In melonDS, a `10.0.2.2` gateway automatically selects the mock server on the host. The gauge app currently uses these built-in network defaults; `dsobd.ini` is used by the terminal apps.

### DS gauge controls

- **SELECT:** cycle display modes, including vehicle picker and session stats.
- **START:** toggle auto-polling; confirm vehicle when in the picker.
- **A:** voltage; confirm vehicle when in the picker.
- **B:** RPM. **X:** speed. **Y:** manifold pressure.
- **L:** coolant. **R:** start/stop CSV log.
- **D-pad up/down:** select vehicle in the picker.
- **Touch:** choose a page, select vehicle, toggle polling, query RPM/MAP, or reconnect.
- **Settings:** tap the gear/mode title; A toggles sound, X tests it, B returns. These settings controls do not send OBD commands.
- **CODES:** A or the SCAN button reads everything; Y or CLEAR twice erases codes; R still starts/stops the log. Other buttons do nothing on this screen.
- **SENSORS:** D-pad up/down or the UP/DOWN buttons change page; the REC button starts/stops the log.
- **ECU INFO:** A or the SCAN button reads everything; the REC button starts/stops the log.

## Other apps

- `terminal_app/`: optional DS terminal; `make terminal` builds `dsobd_terminal.nds`.
- `3ds_app/`: `make build-3ds` builds `dsobd.3dsx`, `dsobd.smdh`, and optionally `dsobd.cia`.
- `psp_app/`: `make build-psp` builds `EBOOT.PBP`.
- `macos_app/`: `make build-mac` builds `DSOBD.app`.
- `core/`: shared ELM327 scheduler for the handheld terminals, plus the portable diagnostics parser and CSV writer used by 3DS, PSP and macOS. The DS gauge app has its own scheduler and parser.

The terminal apps read `dsobd.ini` for network and polling settings. SELECT opens their Wi-Fi selector; D-pad moves selection, A/Cross connects, B/Circle cancels, and X/Square refreshes. In terminal mode, A/Cross resets ELM327, B/Circle queries RPM, X/Square queries speed, Y/Triangle queries voltage, and L queries coolant. R records CSV on 3DS/PSP (on the optional DS terminal it still queries supported PIDs). START toggles polling on DS/PSP; on 3DS, RIGHT toggles polling and START exits.

### 3DS, PSP and macOS diagnostics

All three now expose stored/pending/permanent codes, MIL/readiness, freeze frame, VIN, calibration ID, ECU name, standard CAN misfire monitors for up to 12 cylinders, and the 43-entry sensor table through PID A6. Missing replies are shown as unavailable; they are not zero misfires or a clean bill of health. Misfire history is the ECU's 10-drive exponentially weighted average, and the drive column is the last/current cycle reported by the ECU. With headers off, replies are combined rather than labelled by module.

- **3DS / PSP:** LEFT cycles Terminal → Codes → Sensors → ECU Info. UP/DOWN pages through codes or sensors. A/Cross scans on a diagnostic screen. On Codes, Y/Triangle twice within 10 seconds clears codes; B/Circle or changing screens cancels confirmation. Clearing also resets readiness and freeze frame; use ignition on, engine off. Permanent codes remain until the ECU clears them. SELECT still opens Wi-Fi; START exits on 3DS and toggles polling on PSP.
- **Handheld CSV:** R starts/stops recording to the SD or Memory Stick root (`sdmc:/dsobd-log-0001.csv` or `ms0:/dsobd-log-0001.csv`). Files are created exclusively and never overwrite existing logs. Sensors polls the visible eight rows; Terminal polls RPM, speed, coolant, MAP, throttle, load and voltage. The CSV contains fresh mode 01 sensor values only, with blanks for samples not read during that row. Errors stop recording and appear in the terminal. Normal exit or disconnection closes the file.
- **macOS:** DTC Diagnostics reads all three code types; ECU Info shows readiness, identity, freeze frame and misfires. All Sensors polls the supported sensor table while visible. Record CSV in the top bar opens a folder chooser; Stop CSV closes it. The output path or write error remains visible. The existing packet log remains separate from sensor CSV recording.
- **Storage:** CSV rows are sampled at most every 100 ms and flushed every five seconds. Stop recording before power-off/removing media. A sudden power loss can lose buffered data or damage the open file; flushing is not a guarantee against that.

The selected temperature layouts were checked against [DashLogic's PID reference](https://www.dashlogic.com/docs/technical/obdii_pids); the misfire labels follow [Ford's J1979 mode 06 documentation](https://www.fordservicecontent.com/ford_content/catalog/motorcraft/OBDSM800_HEV.pdf). The portable parser checks each temperature sensor's presence bit and validates numbered frames before decoding.

## Build and verify

```sh
make                 # DS gauge ROM (BlocksDS)
make terminal        # Optional DS terminal ROM
make test            # DS + shared C regression checks, with ASan/UBSan
swift test --package-path macos_app  # macOS diagnostics/CSV tests
make all-platforms   # Also needs devkitARM/libctru and pspdev
make sd              # Refresh drag-ready payloads under sd/
make emu             # DS emulator with mock adapter
make emu-psp          # PSP emulator with mock adapter
```

`make test` covers polling, boost detection, trouble-code parsing (CAN single/multi-frame and ISO 9141/KWP), VIN, calibration ID, ECU name, misfire counts, freeze frame, clearing, supported-range skipping, multi-frame sensor replies, sensor paging, CSV log contents, vehicle navigation, sound/mute behavior, settings persistence, and rendering all 17 screens with missing/live/extreme telemetry. To write native-resolution sample screenshots, run `mkdir -p /tmp/dsobd-previews` then `sh tests/run.sh /tmp/dsobd-previews`. These previews use fixture telemetry, not a vehicle connection.

Instrument sprites and fonts are compiled into the ROM: no asset folders need to be copied to the SD card. Font sources and licenses are in `assets/`. To regenerate the checked-in sprite/font arrays, install Pillow and run `python3 tools/build_art.py`; ordinary ROM builds do not need Pillow. The sound cues are original PCM tones generated once at startup and played asynchronously through libnds.

`tools/mock_vp01.py` simulates a Wi-Fi ELM327 on CAN with delayed replies, first-request protocol search, and STOPPED when a request is interrupted. It reports two stored codes, one pending and one permanent code, a freeze frame, a VIN, calibration ID and ECU name, misfires on cylinder 3 of 4, and 30 sensor PIDs; clearing removes the stored and pending codes and the misfire counts. Run `make mock` to start it; `--fast` gives immediate replies.

For a flashcart, copy `dsobd.nds` to its microSD card. `sd/nintendo-ds/dsobd.nds` is the same built ROM after refreshing the payload. See `sd/README.txt` for other platforms.

For repeatable TCP integration tests (no car required):

```sh
cc -std=c11 -D_DARWIN_C_SOURCE -Icore/include tests/ports_mock.c core/source/elm_core.c core/source/obd_diag.c -o /tmp/dsobd-ports-mock
python3 tests/ports_mock.py /tmp/dsobd-ports-mock --fast
python3 tests/ports_mock.py /tmp/dsobd-ports-mock  # realistic reply delays
swift test --package-path macos_app
python3 tests/ports_mock.py --mac --fast
```

The harness chooses a free localhost port, starts the same VP01 mock used by the emulators, checks scan/sensor/clear behavior and requires zero STOPPED interruptions. The macOS integration test also records and reads a sensor CSV. Physical vehicle, SD/Memory Stick performance and on-device UI testing remain necessary.
