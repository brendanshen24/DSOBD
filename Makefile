# The DS ROM is the dtec gauge cluster (turboxs-dtec/). `make` builds it and
# copies it to ./dsobd.nds. `make all-platforms` also builds the 3DS
# (devkitARM) and PSP (pspdev) terminal apps.
.PHONY: all dtec all-platforms build-3ds build-psp build-mac macos sd clean clean-all mock emu emu-psp

all: dtec

dtec:
	@$(MAKE) -C turboxs-dtec
	@$(MAKE) -f Makefile.terminal
	@# Clean DLDI ROM (for TwilightMenu, Wood R4, YSMenu, DSTWO, R4 SDHC, modern flashcarts)
	@cp turboxs-dtec/dsobd_gauges.nds ./dsobd.nds
	@rm -f ./dsobd_r4.nds

all-platforms: dtec build-3ds build-psp build-mac

build-mac:
	@echo "==> Building macOS Native OBD-II App (macos_app)..."
	@$(MAKE) -C macos_app app
	@rm -rf ./DSOBD.app
	@cp -R macos_app/DSOBD.app ./DSOBD.app

macos: build-mac

build-3ds:
	@echo "==> Building Nintendo 3DS OBD-II App (3ds_app)..."
	@$(MAKE) -C 3ds_app
	@cp 3ds_app/dsobd.3dsx ./dsobd.3dsx
	@cp 3ds_app/dsobd.smdh ./dsobd.smdh
	@if [ -f 3ds_app/dsobd.cia ]; then cp 3ds_app/dsobd.cia ./dsobd.cia; fi

build-psp:
	@echo "==> Building Sony PSP OBD-II App (psp_app)..."
	@$(MAKE) -C psp_app
	@cp psp_app/EBOOT.PBP ./EBOOT.PBP

# Collect the built payloads into ./sd, one drag-ready folder per device.
sd:
	@sh scripts/make_sd.sh

clean:
	@$(MAKE) -C turboxs-dtec clean
	@rm -f dsobd.nds

clean-all: clean
	@$(MAKE) -C 3ds_app clean
	@$(MAKE) -C psp_app clean
	@rm -f dsobd.3dsx dsobd.smdh dsobd.cia EBOOT.PBP

mock:
	@python3 tools/mock_vp01.py

emu: dtec
	@./scripts/run_melon.sh

emu-psp: build-psp
	@./scripts/run_ppsspp.sh
