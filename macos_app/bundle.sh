#!/bin/bash
set -e

APP_NAME="DSOBD"
BUNDLE="${APP_NAME}.app"
CONTENTS="${BUNDLE}/Contents"
MACOS="${CONTENTS}/MacOS"
RESOURCES="${CONTENTS}/Resources"

echo "Building release binary..."
swift build -c release

echo "Packaging ${BUNDLE}..."
mkdir -p "${MACOS}/Resources" "${RESOURCES}"
cp .build/release/DSOBDMac "${MACOS}/${APP_NAME}"
cp Resources/dtcdb_obdii.json "${RESOURCES}/"
cp Resources/dtcdb_obdii.json "${MACOS}/Resources/"

cat << 'EOF' > "${CONTENTS}/Info.plist"
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CFBundleExecutable</key>
	<string>DSOBD</string>
	<key>CFBundleIdentifier</key>
	<string>com.dsobd.macos</string>
	<key>CFBundleName</key>
	<string>DSOBD</string>
	<key>CFBundlePackageType</key>
	<string>APPL</string>
	<key>CFBundleShortVersionString</key>
	<string>1.0</string>
	<key>LSMinimumSystemVersion</key>
	<string>13.0</string>
	<key>NSHighResolutionCapable</key>
	<true/>
</dict>
</plist>
EOF

chmod +x "${MACOS}/${APP_NAME}"
echo "Successfully created ${BUNDLE}!"
