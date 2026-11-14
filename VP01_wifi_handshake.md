# CarScanner (OVZ) WiFi / ELM327 Handshake — reverse-engineering reference

Source: `com.ovz.carscanner` v2.1.50 (build 22001050), armeabi-v7a APK, confirmed working with the VP01.
Method: static disassembly only. The APK is a **.NET 9 MAUI/Xamarin** app — real logic is managed IL in
`lib/armeabi-v7a/libassemblies.armeabi-v7a.blob.so` (`XABA` AssemblyStore, LZ4/`XALZ` per assembly).
Key assemblies: `CarScannerMaui.dll` (protocol) and `CarScannerMaui.Droid.dll` (socket).
All values below are read from IL; the app was **not** run and **no live traffic was captured**.

## Transport handshake — `CarScannerMaui.Droid.TCP.TCPConnectionV2`

1. TCP connect to adapter, default **`192.168.0.10:35000`**
   (in-app: "99% of ELM327 devices uses default ip (192.168.0.10) and port (35000)").
2. Socket options: **`TCP_NODELAY` (NoDelay)** + **`SO_KEEPALIVE` (KeepAlive)**.
3. **`BindToWiFiNetwork` → `ConnectivityManager.BindProcessToNetwork(wifi)`** — pins the socket to the WiFi
   interface so traffic doesn't leak to cellular. Gated by `SharedSettings.AndroidWiFiMode`.
4. No application greeting / magic bytes on connect — it goes straight into ELM init.
   (`TCPConnectionV2` uses `Java.Net.Socket`; the older `TCPConnection` used .NET `TcpClient`.)

## ELM327 init handshake — `CarScannerMaui.OBD2.OBDDataReader`

| Step | Method | Detail (from IL) |
|------|--------|------------------|
| Command terminator | `SendString` | every command ends with **`\r`** (0x0D) |
| Reset | `SendATZ` | sends literally **`"ATZ\rATE0\r\r\r\r\r\r\r\r\r\r"`** (ATE0 pipelined after ATZ + 10 CRs); timeout **6000 ms**, 2048-byte buffer |
| Identity/prompt check | `CheckIsELM327` | reads until byte **`>` (0x3E)**, budget **500 ms** |
| General read loop | `ReadData` | timeouts **2500 / 7500 ms**; on no terminator logs `"[NoFinishCharacter, timeElapsed=… loopCounter=… res.length=…]"` |
| Setup commands | `SendInitCommand` | per-command delays ~200 ms (skipped when `NoDelayELM327Init` is set) |
| Protocol select | `SearchForProtocol` | runs after init |

The entire exchange is **prompt-terminated**: a response is complete when the adapter sends `>`.

## Why a WiFi handshake stalls (most → least likely)

1. **Not reading until `>` (0x3E).** Accumulate bytes; treat `>` as end-of-response. Do not read a fixed
   length or wait on `\r\n`. This is the #1 cause of "NoFinishCharacter"-style stalls.
2. **Nagle left on.** Set `TCP_NODELAY`; otherwise small AT writes coalesce and some adapters won't reply
   until a flush, so no prompt returns.
3. **Socket routed over cellular, not WiFi.** On Android with data+WiFi the OS prefers cellular (adapter AP
   has no internet). Must `BindProcessToNetwork(wifi)`.
4. **ATZ/ATE0 framing.** The app fires `ATZ\rATE0\r…` then reads until `>`, swallowing the reset banner
   rather than parsing ATZ's reply. Waiting for a specific ATZ response string desyncs on clone banners.
5. **Wrong IP/port.**

## Other notable findings

- **Active clone detection** (`CheckIsELM327`, strings "ATTENTION! You have chosen a BAD ELM327 device!",
  "Coding not supported or faulty ELM327 clone"): low-quality adapters get flagged and have coding features
  disabled — basic reads still work.
- Same `OBDDataReader` protocol layer sits on all transports: `USBConnectionV1`, `BluetoothConnectionV3`,
  `BTLEConnection`. The AT handshake is identical regardless of transport.
- `ConnectionSettingsTester` / `ConnectionDetector.CheckAndSetWiFi` auto-probe connection settings.
- `WriteDataTimeoutException` is thrown if a socket write blocks (adapter not draining).

## Not verified / caveats

- `ReadData`'s exact per-byte finish-char compare goes through a helper/field; the `>`=0x3E contract is
  taken from `CheckIsELM327` (explicit in IL), not read branch-by-branch in `ReadData`.
- The full `SendInitCommand` AT-command list was not enumerated.
- Static evidence from v2.1.50 only — confirm against a real packet capture before hard-coding timeouts.
