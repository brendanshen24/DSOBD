#!/bin/bash
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PBP_PATH="$DIR/psp_app/EBOOT.PBP"

if [ -d "/Applications/PPSSPPSDL.app" ]; then
    PPSSPP_APP="/Applications/PPSSPPSDL.app"
elif [ -d "/Users/brendan/DSCarplay/tools/emulator/PPSSPPSDL.app" ]; then
    PPSSPP_APP="/Users/brendan/DSCarplay/tools/emulator/PPSSPPSDL.app"
else
    echo "[-] Error: PPSSPPSDL.app not found in /Applications or tools/emulator/"
    exit 1
fi

GREEN="\033[92m"
CYAN="\033[96m"
YELLOW="\033[93m"
BOLD="\033[1m"
RESET="\033[0m"

echo -e "\n${CYAN}${BOLD}============================================================${RESET}"
echo -e "${CYAN}${BOLD}  DSOBD PSP - SONY PSP OBDCheck VP01 EMULATION TESTBED      ${RESET}"
echo -e "${CYAN}${BOLD}============================================================${RESET}\n"

# Step 1: Build PSP EBOOT.PBP if needed
if [ ! -f "$PBP_PATH" ]; then
    echo -e "${YELLOW}==> [1/3] Building PSP EBOOT.PBP...${RESET}"
    make -C "$DIR/psp_app"
fi

# Step 2: Start Mock Veepeak VP01 WiFi Server if not running
echo -e "${YELLOW}==> [2/3] Checking Mock Veepeak VP01 WiFi Server (port 35000)...${RESET}"
SERVER_PID=$(lsof -ti:35000 2>/dev/null || true)
if [ -z "$SERVER_PID" ]; then
    echo -e "    Starting tools/mock_vp01.py on port 35000..."
    python3 "$DIR/tools/mock_vp01.py" >/tmp/mock_vp01.log 2>&1 &
    sleep 0.5
    echo -e "${GREEN}    ✓ Mock server running (PID $!)${RESET}"
else
    echo -e "${GREEN}    ✓ Server already active on port 35000 (PID $SERVER_PID)${RESET}"
fi

# Step 3: Launch PPSSPP
echo -e "${YELLOW}==> [3/3] Launching PPSSPP with EBOOT.PBP...${RESET}"
nohup "$PPSSPP_APP/Contents/MacOS/PPSSPPSDL" "$PBP_PATH" >/tmp/ppsspp_dsobd.log 2>&1 &
disown

echo -e "\n${GREEN}${BOLD}✓ PPSSPP launched with EBOOT.PBP!${RESET}\n"
echo -e "${CYAN}PSP Controls:${RESET}"
echo -e "  • ${BOLD}[Cross X]${RESET} Reset ELM327 (ATZ) / Connect in Wi-Fi List"
echo -e "  • ${BOLD}[Circle O]${RESET} Query Engine RPM (010C) / Cancel Wi-Fi List"
echo -e "  • ${BOLD}[Square]${RESET} Query Vehicle Speed (010D) / Refresh Wi-Fi List"
echo -e "  • ${BOLD}[Triangle]${RESET} Read Battery Voltage (ATRV)"
echo -e "  • ${BOLD}[L / R]${RESET} Coolant Temp (0105) / Supported PIDs (0100)"
echo -e "  • ${BOLD}[START]${RESET} Toggle Auto-Polling"
echo -e "  • ${BOLD}[SELECT]${RESET} Open Wi-Fi Network Selector"
