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
