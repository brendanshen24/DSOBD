# `make` builds the Nintendo DS gauge app from source/ and include/.
# The optional terminal app is built with `make terminal`.
.PHONY: all ds terminal test all-platforms build-3ds build-psp build-mac macos sd clean clean-all mock emu emu-psp

all: ds

ds:
	@$(MAKE) -f Makefile.gauges
	@# The original R4 kernel's DLDI auto-patch breaks BlocksDS ROMs (white screen); pre-patch its driver.
	@$(MAKE) -f Makefile.gauges dldipatch

terminal:
	@$(MAKE) -f Makefile.terminal

test:
	@sh tests/run.sh

all-platforms: ds terminal build-3ds build-psp build-mac

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
	@$(MAKE) -f Makefile.gauges clean
	@rm -f dsobd.nds

clean-all: clean
	@$(MAKE) -f Makefile.terminal clean
	@$(MAKE) -C 3ds_app clean
	@$(MAKE) -C psp_app clean
	@rm -f dsobd.3dsx dsobd.smdh dsobd.cia EBOOT.PBP

mock:
	@python3 tools/mock_vp01.py

emu: ds
	@./scripts/run_melon.sh

emu-psp: build-psp
	@./scripts/run_ppsspp.sh
