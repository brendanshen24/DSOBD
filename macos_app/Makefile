.PHONY: all build release run test app clean

all: build

build:
	swift build

release:
	swift build -c release

run:
	swift run DSOBDMac

test:
	swift test

app:
	bash bundle.sh

clean:
	swift package clean
	rm -rf DSOBD.app .build
