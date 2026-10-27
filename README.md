# DSOBD — Nintendo DS, 3DS, Sony PSP & macOS Native OBDCheck VP01 Suite

A multi-platform diagnostic suite for **Nintendo DS / DSi** (`dsobd.nds`), **Nintendo 3DS** (`dsobd.3dsx` / `dsobd.cia`), **Sony PSP** (`EBOOT.PBP`), and **macOS Native** (`DSOBD.app` in `macos_app/`) that connects to a **Veepeak OBDCheck VP01 Wi-Fi** adapter (and other ELM327 Wi-Fi OBD-II dongles) over wireless network, displaying live telemetry and logging all protocol traffic.

---

## Built Binaries (in `/Users/brendan/DSOBD`)

| Platform | Output Binaries | Source Directory |
|---|---|---|
| **macOS Native App** | `DSOBD.app` | `macos_app/` |
| **Nintendo DS / DSi** | `dsobd.nds` (the dtec gauge cluster, copied from `turboxs-dtec/`) | `turboxs-dtec/` |
| **Nintendo 3DS** | `dsobd.3dsx`, `dsobd.cia`, `dsobd.smdh` | `3ds_app/` |
| **Sony PSP** | `EBOOT.PBP`, `psp_app/dsobd_psp.prx` | `psp_app/` |
| **DS gauge cluster** | `turboxs-dtec/dsobd_gauges.nds` | `turboxs-dtec/` |

All four apps share one ELM327 engine in `core/` (`elm_core.c` for the protocol, `ini.c` for `dsobd.ini`). Each app's `obd.c` is a thin adapter that copies the parsed values into its own screen state, so a protocol fix only has to be made once.

---

## Features (All 3 Platforms)

- **Direct Wi-Fi & TCP Socket:** Connects over Wi-Fi to your OBD-II adapter (`WiFi_OBDII`) and opens a non-blocking TCP socket to `192.168.0.10:35000`.
- **Interactive Wi-Fi Network Selector (`[SELECT]`):** Choose a Wi-Fi network or profile from an on-screen list if your adapter uses a different SSID (`WiFi_OBDII`, `OBDII`, `V-LINK`, `CLKDevices`, saved system slots, or emulator loopback).
- **Live Raw Data Terminal:** Displays every incoming byte/character from the ELM327 chip as scrolling text in real time with highlighted `>` prompts and `TX>` command tags.
- **Reply-Gated Polling:** A command is only sent after the adapter's `>` prompt for the previous one (or a timeout), so the ELM327 never aborts a request with `STOPPED`. Pressing a button queues the command behind whatever is in flight. Slow protocol detection (`SEARCHING...`) gets a longer timeout.
- **Supported-PID Aware:** The `0100` answer is decoded, and auto-poll skips PIDs the car does not report.
- **Live Telemetry Dashboard:** Parses and displays battery voltage (`ATRV`), engine RPM (`010C`), vehicle speed in km/h & mph (`010D`), coolant temperature (`0105`), and ELM327 version string.
- **Persistent Config (`dsobd.ini`):** Remembers your chosen Wi-Fi SSID, target IP, port, and polling settings across reboots.

---

## Controls by Platform

### Nintendo DS / DSi & Nintendo 3DS
| Button | Terminal Mode Action | Wi-Fi Selector Mode (`[SELECT]`) |
|---|---|---|
| **`[A]`** | Send `ATZ` (Reset ELM327) | Connect to highlighted network |
| **`[B]`** | Query Engine RPM (`010C`) | Cancel and return to terminal |
| **`[X]`** | Query Vehicle Speed (`010D`) | Refresh / Rescan Wi-Fi networks |
| **`[Y]`** | Read Battery Voltage (`ATRV`) | — |
| **`[L]` / `[R]`** | Coolant Temp (`0105`) / Supported PIDs (`0100`) | — |
| **`[SELECT]`** | **Open Wi-Fi Network Selector** | — |
| **`[START]`** | Toggle Auto-Polling (DS) / Exit to HB Menu (3DS; use `[RIGHT]` on 3DS for Auto-Poll) | — |
| **`[D-Pad UP / DOWN]`** | Clear terminal log (`[UP]`) | Move selection cursor up / down |
| **`[Touch Screen]`** | Not used | Tap any network row to select / connect |

### Sony PSP
| Button | Terminal Mode Action | Wi-Fi Selector Mode (`[SELECT]`) |
|---|---|---|
| **`[Cross X]`** | Send `ATZ` (Reset ELM327) | Connect to highlighted network |
| **`[Circle O]`** | Query Engine RPM (`010C`) | Cancel and return to terminal |
| **`[Square]`** | Query Vehicle Speed (`010D`) | Refresh network list |
| **`[Triangle]`** | Read Battery Voltage (`ATRV`) | — |
| **`[L]` / `[R]`** | Coolant Temp (`0105`) / Supported PIDs (`0100`) | — |
| **`[START]`** | Toggle Auto-Polling | — |
| **`[SELECT]`** | **Open Wi-Fi Network Selector** | — |
| **`[D-Pad UP / DOWN]`** | Clear terminal log (`[UP]`) | Move selection cursor up / down |

---

## Building & Running in Emulators

```bash
# Build the DS ROM (needs BlocksDS only):
make

# Build DS + 3DS + PSP (also needs devkitARM/libctru and pspdev):
make all-platforms

# Launch Nintendo DS in melonDS with Mock VP01 Server:
make emu

# Launch Sony PSP in PPSSPP with Mock VP01 Server:
make emu-psp
```

### Mock adapter

`tools/mock_vp01.py` (started by `make emu` / `make emu-psp`, or `make mock`) behaves like a real ELM327 in a car: replies take 50 to 300 ms, the first PID after connect prints `SEARCHING...` and takes about 1.5 s, and any byte sent while a reply is pending aborts it with `STOPPED`. It prints a `STOPPED` count when the client disconnects, which should stay at 0. Run `python3 tools/mock_vp01.py --fast` for instant replies.

### Git

Build output, `.DS_Store` and `turboxs-dtec/reference_images/` are listed in `.gitignore`. To start version control: `git init && git add . && git commit -m "Initial commit"`.

