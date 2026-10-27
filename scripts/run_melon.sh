#!/bin/bash
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ROM_PATH="$DIR/dsobd.nds"

if [ -d "/Applications/melonDS.app" ]; then
    MELONDS_APP="/Applications/melonDS.app"
elif [ -d "/Users/brendan/DSCarplay/tools/emulator/melonDS.app" ]; then
    MELONDS_APP="/Users/brendan/DSCarplay/tools/emulator/melonDS.app"
else
    echo "[-] Error: melonDS.app not found in /Applications or tools/emulator/"
    exit 1
fi

GREEN="\033[92m"
CYAN="\033[96m"
YELLOW="\033[93m"
BOLD="\033[1m"
RESET="\033[0m"

echo -e "\n${CYAN}${BOLD}============================================================${RESET}"
echo -e "${CYAN}${BOLD}  DSOBD - NINTENDO DS OBDCheck VP01 EMULATION TESTBED       ${RESET}"
echo -e "${CYAN}${BOLD}============================================================${RESET}\n"

# Step 1: Build ROM if needed
if [ ! -f "$ROM_PATH" ]; then
    echo -e "${YELLOW}==> [1/3] Building dsobd.nds...${RESET}"
    make -C "$DIR"
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

# Step 3: Launch melonDS
echo -e "${YELLOW}==> [3/3] Launching melonDS with dsobd.nds...${RESET}"
nohup "$MELONDS_APP/Contents/MacOS/melonDS" "$ROM_PATH" >/tmp/melonds_dsobd.log 2>&1 &
disown

echo -e "\n${GREEN}${BOLD}✓ melonDS launched with dsobd.nds!${RESET}\n"
echo -e "${CYAN}DS Controls (dtec gauges):${RESET}"
echo -e "  • ${BOLD}[SELECT]${RESET} Next display mode   • ${BOLD}[START]${RESET} Toggle auto-poll"
echo -e "  • ${BOLD}[A]${RESET} ATRV  ${BOLD}[B]${RESET} 010C  ${BOLD}[X]${RESET} 010D  ${BOLD}[Y]${RESET} 010B  ${BOLD}[L]${RESET} 0105  ${BOLD}[R]${RESET} 0104"
