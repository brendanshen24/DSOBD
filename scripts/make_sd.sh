#!/bin/sh
# Assemble ./sd — one folder per device. The CONTENTS of each device folder
# map to the root of that device's SD card / Memory Stick, so you can drag
# them straight on. Re-run after a rebuild (`make sd`) to refresh the binaries.
set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
SD="$ROOT/sd"

rm -rf "$SD"
mkdir -p "$SD/nintendo-ds" \
         "$SD/nintendo-3ds/3ds/dsobd" "$SD/nintendo-3ds/cias" \
         "$SD/sony-psp/PSP/GAME/DSOBD"

copy() {
    if [ -f "$1" ]; then
        cp "$1" "$2"
        echo "  + ${2#"$SD"/}$(basename "$1")"
    else
        echo "  ! missing $1 — build it first (make / make build-3ds / make build-psp)"
    fi
}

# --- Nintendo DS (R4 / flashcart, or DSi SD) -> microSD root ---
copy dsobd.nds "$SD/nintendo-ds/"
copy dsobd_terminal.nds "$SD/nintendo-ds/"
copy dsobd.ini "$SD/nintendo-ds/"

# --- Nintendo 3DS (Homebrew Launcher) -> SD root ---
# App at /3ds/dsobd/, config at sdmc:/dsobd.ini, optional CIA for FBI install.
copy dsobd.3dsx "$SD/nintendo-3ds/3ds/dsobd/"
copy dsobd.smdh "$SD/nintendo-3ds/3ds/dsobd/"
copy dsobd.cia  "$SD/nintendo-3ds/cias/"
copy dsobd.ini  "$SD/nintendo-3ds/"

# --- Sony PSP -> Memory Stick root ---
copy EBOOT.PBP "$SD/sony-psp/PSP/GAME/DSOBD/"
copy dsobd.ini "$SD/sony-psp/PSP/GAME/DSOBD/"

cat > "$SD/README.txt" <<'EOF'
DSOBD SD card payloads
======================
Each folder below is one device. Drag the CONTENTS of the folder (not the
folder itself) onto the root of that device's SD card / Memory Stick.
Edit dsobd.ini to match your adapter (SSID / IP / port); defaults are the
Veepeak VP01 (WiFi_OBDII, 192.168.0.10:35000).

nintendo-ds/   -> microSD root of your DS flashcart (R4/DSTWO) or DSi SD
                    dsobd.nds      the ROM (launch it from the card menu)
                    dsobd.ini      config (read from card root)

nintendo-3ds/  -> SD card root of the 3DS
                    3ds/dsobd/dsobd.3dsx   launch via Homebrew Launcher
                    3ds/dsobd/dsobd.smdh   icon/metadata
                    dsobd.ini              config (read from sdmc:/dsobd.ini)
                    cias/dsobd.cia         OPTIONAL: install with FBI instead
                                           of using the .3dsx, then delete

sony-psp/      -> Memory Stick root of the PSP
                    PSP/GAME/DSOBD/EBOOT.PBP   shows up under Game > Memory Stick
                    PSP/GAME/DSOBD/dsobd.ini   config
EOF

echo "sd/ ready at $SD"
