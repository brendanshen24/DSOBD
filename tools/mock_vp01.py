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


def pid_reply(cmd, t):
    rpm = get_rpm(t)
    if cmd == "0100":
        return "41 00 BE 3E B8 11"
    if cmd == "010C":
        raw = rpm * 4
        return f"41 0C {(raw >> 8) & 0xFF:02X} {raw & 0xFF:02X}"
    if cmd == "010D":
        return f"41 0D {int(max(0, min(120, 55 + 40 * math.sin(t * 0.3)))):02X}"
    if cmd == "0105":
        return "41 05 84"  # 92 C
    if cmd == "010B":
        kpa = 36 if rpm < 1200 else int(36 + (rpm - 1200) / 4000.0 * 175)
        return f"41 0B {max(25, min(240, kpa)):02X}"
    if cmd == "0111":
        pct = max(10, min(95, rpm / 6000.0 * 100))
        return f"41 11 {int(pct * 255 / 100):02X}"
    if cmd == "0104":
        return "41 04 45"  # 27 %
    if cmd == "03":
        return "43 00"
    return None


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
        else:
            reply = pid_reply(cmd, t)
            resp = f"\r{reply}\r" if reply else "\r?\r"
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
    print("  PIDs: 0100 010C 010D 0105 010B 0111 0104, ATRV")
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
