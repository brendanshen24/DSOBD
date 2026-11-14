#!/usr/bin/env python3
"""
Veepeak OBDCheck VP01 WiFi Adapter Simulator
Listens on TCP port 35000 and responds to ELM327 AT and OBD-II Mode 1 PID commands.
Used for testing the DSOBD apps without needing a physical vehicle or adapter.

By default it behaves like a real ELM327 in a car:
  * replies take 50-300 ms (ATZ about 0.8 s)
  * the first PID query after connect / ATSP0 prints SEARCHING... and takes ~1.5 s
  * any byte received while a reply is pending aborts it with STOPPED
    (exactly what a real ELM327 does)

Run with --fast for the old instant-reply behaviour.
"""

import math
import random
import select
import socket
import sys
import time

HOST = "0.0.0.0"
PORT = 35000
FAST = "--fast" in sys.argv


def get_rpm(t):
    # Oscillate RPM realistically between 800 (idle) and ~3500 RPM
    base = 1800
    amp = 1000 * math.sin(t * 0.5) + 300 * math.sin(t * 1.8)
    return int(max(750, min(5500, base + amp)))


VIN = "1HGCM82633A004352"
CAL_ID = b"37805RBBA530"
ECU_NAME = b"ECM\x00-EngineControl"
MISFIRE_MIDS = [0xA2, 0xA3, 0xA4, 0xA5]  # 4 cylinders
FREEZE = {0x04: [0x45], 0x05: [0x84], 0x0C: [0x1F, 0x40], 0x0D: [0x64]}  # 27 %, 92 C, 2000 rpm, 100 km/h


def mode01(t, stored):
    """Live mode 01 data bytes by PID. Supported-PID bitmaps are derived from this."""
    rpm = get_rpm(t)
    kpa = 36 if rpm < 1200 else int(36 + (rpm - 1200) / 4000.0 * 175)
    word = lambda v: [(v >> 8) & 0xFF, v & 0xFF]
    return {
        # MIL + code count; misfire/fuel/components complete, EVAP incomplete -> 6/7 ready
        0x01: [(0x80 | len(stored)) if stored else 0, 0x07, 0x65, 0x04],
        0x04: [0x45],
        0x05: [0x84],
        0x06: [0x80 + int(4 * math.sin(t))],
        0x07: [0x84],
        0x0B: [max(25, min(240, kpa))],
        0x0C: word(rpm * 4),
        0x0D: [int(max(0, min(120, 55 + 40 * math.sin(t * 0.3))))],
        0x0E: [0x8C],
        0x0F: [0x46],
        0x10: word(int(rpm * 0.6)),
        0x11: [int(max(10, min(95, rpm / 6000.0 * 100)) * 255 / 100)],
        0x14: [0x5A, 0x80],
        0x1C: [0x01],  # OBD-II (CARB)
        0x1F: word(int(t)),
        0x21: word(12),
        0x2F: [0xA0],
        0x30: [3],
        0x31: word(1234),
        0x33: [0x65],
        0x42: word(14100),
        0x46: [0x3C],
        0x5C: [0x7D],
        0x5E: word(int(rpm / 100)),
        0x51: [0x01],  # gasoline
        0x61: [125 + int(rpm / 200)],
        0x62: [125 + int(rpm / 250)],
        0x63: word(250),
        0x77: [0x01, 0x46, 0, 0, 0],
        0x78: [0x01] + word((550 + 40) * 10) + [0] * 6,  # 9 bytes: multi-frame on CAN
        0xA6: [0x00, 0x12, 0xD6, 0x87],  # 123456.7 km
    }


def supported_bitmap(pids, base):
    bits = 0
    for pid in pids:
        if base < pid <= base + 32:
            bits |= 1 << (32 - (pid - base))
    if any(pid > base + 32 for pid in pids):
        bits |= 1  # next range supported
    return " ".join(f"{(bits >> s) & 0xFF:02X}" for s in (24, 16, 8, 0))


def pid_reply(cmd, t, stored=()):
    pids = mode01(t, stored)
    pid = int(cmd[2:4], 16)
    if pid % 0x20 == 0 and pid <= 0xA0:
        return f"41 {pid:02X} {supported_bitmap(pids, pid)}"
    if pid not in pids:
        return None
    return can_reply([0x41, pid] + pids[pid])


def can_reply(data):
    """ISO 15765 with headers off: one line, or a byte count plus numbered frames."""
    hexes = [f"{b:02X}" for b in data]
    if len(data) <= 7:
        return " ".join(hexes)
    lines = [f"{len(data):03X}", "0: " + " ".join(hexes[:6])]
    rest = hexes[6:]
    for i in range(0, len(rest), 7):
        lines.append(f"{(i // 7 + 1) % 16:X}: " + " ".join(rest[i:i + 7]))
    return "\r".join(lines)


def misfire_reply(mid, drive, avg):
    """Mode 06 CAN records: MID TID UASID value min max; TID 0B = 10-drive avg, 0C = this drive."""
    rec = lambda tid, v: [mid, tid, 0x24, v >> 8, v & 0xFF, 0, 0, 0xFF, 0xFF]
    return can_reply([0x46] + rec(0x0B, avg) + rec(0x0C, drive))


def codes_reply(mode, codes):
    data = [0x40 + mode, len(codes)]
    for code in codes:
        data += [int(code[1:3], 16), int(code[3:5], 16)]  # P-codes only
    return can_reply(data)


def get_voltage(t):
    return f"{13.8 + 0.4 * math.sin(t * 0.1):.1f}V"


class Session:
    def __init__(self, conn):
        self.conn = conn
        self.echo = True
        self.start_t = time.time()
        self.need_search = True   # first PID after connect / ATSP0 searches
        self.pending = None       # (due_time, payload, label)
        self.buffer = ""
        self.discard_line = False
        self.stats = {"cmds": 0, "stopped": 0}
        self.stored = ["P0133", "P0420"]
        self.pending_codes = ["P0171"]
        self.permanent = ["P0420"]  # stays until the monitor passes, like a real ECU
        self.misfires = {0xA4: (14, 6)}  # cylinder 3: this drive, 10-drive average

    def obd(self, cmd, t):
        if len(cmd) == 5 and cmd.startswith("01"):
            cmd = cmd[:4]  # ELM327 v1.3+ response-count suffix
        if len(cmd) == 4 and cmd.startswith("01"):
            return pid_reply(cmd, t, self.stored)
        if cmd == "03":
            return codes_reply(0x03, self.stored)
        if cmd == "07":
            return codes_reply(0x07, self.pending_codes)
        if cmd == "0A":
            return codes_reply(0x0A, self.permanent)
        if cmd == "04":
            self.stored, self.pending_codes, self.misfires = [], [], {}
            return "44"
        if cmd == "0902":
            return can_reply([0x49, 0x02, 0x01] + list(VIN.encode("ascii")))
        if cmd == "0904":
            return can_reply([0x49, 0x04, 0x01] + list(CAL_ID.ljust(16, b"\x00")))
        if cmd == "090A":
            return can_reply([0x49, 0x0A, 0x01] + list(ECU_NAME.ljust(20, b"\x00")))
        if cmd == "06A0":
            bits = sum(1 << (0xC0 - mid) for mid in [0xA1] + MISFIRE_MIDS)
            return can_reply([0x46, 0xA0] + list(bits.to_bytes(4, "big")))
        if len(cmd) == 4 and cmd.startswith("06") and int(cmd[2:], 16) in MISFIRE_MIDS:
            mid = int(cmd[2:], 16)
            return misfire_reply(mid, *self.misfires.get(mid, (0, 0)))
        if len(cmd) == 6 and cmd.startswith("02") and self.stored:
            pid = int(cmd[2:4], 16)
            if pid == 0x02:
                code = self.stored[0]
                return f"42 02 00 {code[1:3]} {code[3:5]}"
            if pid in FREEZE:
                return f"42 {pid:02X} 00 " + " ".join(f"{b:02X}" for b in FREEZE[pid])
        return None

    def send(self, text):
        self.conn.sendall(text.encode("ascii"))

    def respond(self, line):
        cmd = line.upper().replace(" ", "")
        t = time.time() - self.start_t
        self.stats["cmds"] += 1
        delay = 0.0 if FAST else 0.02
        prefix = ""

        if cmd.startswith("ATZ"):
            self.echo = True
            self.need_search = True
            resp = "\r\rELM327 v1.5\r"
            delay = 0.02 if FAST else 0.8
        elif cmd == "ATE0":
            self.echo = False
            resp = "\rOK\r"
        elif cmd == "ATE1":
            self.echo = True
            resp = "\rOK\r"
        elif cmd.startswith("ATSP"):
            self.need_search = True
            resp = "\rOK\r"
        elif cmd in ("ATL0", "ATL1", "ATH0", "ATH1", "ATAT1", "ATAT2"):
            resp = "\rOK\r"
        elif cmd == "ATRV":
            resp = f"\r{get_voltage(t)}\r"
        elif cmd == "ATDP":
            resp = "\rAUTO, ISO 15765-4 (CAN 11/500)\r"
        elif cmd == "ATDPN":
            resp = "\rA6\r"
        elif cmd.startswith("AT"):
            resp = "\r?\r"
        else:
            reply = self.obd(cmd, t)
            resp = f"\r{reply}\r" if reply else "\rNO DATA\r"
            if reply and not FAST:
                delay = random.uniform(0.05, 0.30)
                if self.need_search:
                    self.need_search = False
                    prefix = "SEARCHING...\r"
                    delay = 1.5

        # A real ELM327 echoes characters immediately, before the reply.
        now_out = (f"{line}\r" if self.echo else "") + prefix
        if now_out:
            self.send(now_out)
        self.pending = (time.time() + delay, f"{resp}\r>", line)

    def interrupt(self):
        self.stats["stopped"] += 1
        print(f"[!] '{self.pending[2]}' interrupted by new data -> STOPPED")
        self.pending = None
        self.send("\rSTOPPED\r\r>")

    def feed(self, text):
        if self.pending is not None:
            # Data arrived while busy: abort the current command. The bytes
            # that caused the interrupt are consumed, so drop that line.
            self.interrupt()
            self.discard_line = True

        self.buffer += text
        while True:
            cut = min((i for i in (self.buffer.find("\r"), self.buffer.find("\n")) if i != -1),
                      default=-1)
            if cut == -1:
                return
            line = self.buffer[:cut].strip()
            self.buffer = self.buffer[cut + 1:]
            if self.discard_line:
                self.discard_line = False
                continue
            if not line:
                continue
            if self.pending is not None:
                # Two commands in one burst: the second one interrupts the first.
                self.interrupt()
                continue
            print(f"[RX] '{line}'")
            self.respond(line)

    def run(self):
        self.send(">")
        while True:
            timeout = None
            if self.pending is not None:
                timeout = max(0.0, self.pending[0] - time.time())
            ready, _, _ = select.select([self.conn], [], [], timeout)
            if ready:
                data = self.conn.recv(256)
                if not data:
                    break
                self.feed(data.decode("ascii", errors="replace"))
            elif self.pending is not None:
                _, payload, _ = self.pending
                self.pending = None
                self.send(payload)
                print(f"[TX] {payload!r}")


def handle_client(conn, addr):
    print(f"\n[+] Connected by client at {addr[0]}:{addr[1]}")
    s = Session(conn)
    try:
        s.run()
        print("[-] Client disconnected.")
    except (ConnectionResetError, BrokenPipeError):
        print("[-] Client connection reset.")
    finally:
        print(f"[*] Session: {s.stats['cmds']} commands, {s.stats['stopped']} STOPPED")
        conn.close()


def main():
    print("======================================================")
    print("  Veepeak OBDCheck VP01 WiFi Simulator")
    print(f"  Listening on {HOST}:{PORT}  ({'fast' if FAST else 'realistic'} mode)")
    print("  Mode 01 PIDs, codes 03/07/0A, clear 04, freeze frame 02, misfire 06,")
    print("  VIN / calibration ID / ECU name 0902/0904/090A")
    print("  AT: Z E0/1 L0/1 H0/1 AT1/2 SPn RV DP DPN")
    print("======================================================")

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind((HOST, PORT))
    sock.listen(1)

    print("[*] Ready. Start the DS / 3DS / PSP app (emulator or real hardware).")
    try:
        while True:
            conn, addr = sock.accept()
            handle_client(conn, addr)
    except KeyboardInterrupt:
        print("\n[*] Exiting server.")
    finally:
        sock.close()


if __name__ == "__main__":
    main()
