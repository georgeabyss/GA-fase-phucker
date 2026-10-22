#!/bin/bash
# Builds the universal plug-ins and packs them into one macOS installer:
#   dist/FasePhucker-<version>-macOS.zip  =  the .pkg + "READ ME FIRST.txt"
# The installer offers VST3 and Audio Unit as separate choices.
# Not signed with an Apple Developer ID: fine for sharing with friends, who open it
# once via System Settings > Privacy & Security (see installer/READ ME FIRST.txt).
set -e
cd "$(dirname "$0")"

VERSION="$(sed -n 's/^project(FasePhucker VERSION \([0-9.]*\)).*/\1/p' CMakeLists.txt)"
ID="com.ga.fasephucker"
NAME="Fase Phucker"
ARTEFACTS="build/FasePhucker_artefacts/Release"
STAGE="build/installer"
OUT="dist/FasePhucker-$VERSION-macOS"

echo "== Building $NAME $VERSION (universal)"
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release > /dev/null
cmake --build build

rm -rf "$STAGE" "$OUT.pkg" "$OUT.zip"
mkdir -p "$STAGE/pkgs" "$STAGE/scripts" dist
cp installer/postinstall "$STAGE/scripts/postinstall"
chmod +x "$STAGE/scripts/postinstall"

# one component package per format: $1 = short name, $2 = bundle, $3 = install folder, $4 = extra pkgbuild args
component () {
    local root="$STAGE/root-$1"
    mkdir -p "$root"
    ditto "$ARTEFACTS/$2" "$root/$(basename "$2")"
    strip -x "$root/$(basename "$2")/Contents/MacOS/"* 2> /dev/null   # debug symbols: not needed by users
    codesign --force --deep -s - "$root/$(basename "$2")"   # ad-hoc signature, needed on Apple Silicon

    # without this, the installer would "update" any copy it finds elsewhere (e.g. in this build folder)
    pkgbuild --analyze --root "$root" "$STAGE/$1.plist" > /dev/null
    /usr/libexec/PlistBuddy -c "Delete :0:BundleIsRelocatable" "$STAGE/$1.plist" 2> /dev/null || true
    /usr/libexec/PlistBuddy -c "Add :0:BundleIsRelocatable bool false" "$STAGE/$1.plist"

    pkgbuild --root "$root" --component-plist "$STAGE/$1.plist" \
             --identifier "$ID.$1" --version "$VERSION" --install-location "$3" \
             ${4:+$4} "$STAGE/pkgs/$1.pkg" > /dev/null
    echo "   $1 -> $3"
}

echo "== Packing"
component vst3 "VST3/$NAME.vst3"            "/Library/Audio/Plug-Ins/VST3"
component au   "AU/$NAME.component"         "/Library/Audio/Plug-Ins/Components" "--scripts $STAGE/scripts"

cat > "$STAGE/distribution.xml" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
    <title>$NAME $VERSION</title>
    <welcome file="welcome.html" mime-type="text/html"/>
    <readme file="readme.html" mime-type="text/html"/>
    <options customize="always" require-scripts="false" hostArchitectures="arm64,x86_64"/>
    <domains enable_localSystem="true" enable_anywhere="false" enable_currentUserHome="false"/>
    <allowed-os-versions><os-version min="11.0"/></allowed-os-versions>
    <choices-outline>
        <line choice="vst3"/>
        <line choice="au"/>
    </choices-outline>
    <choice id="vst3" title="VST3 plug-in" description="For Ableton Live, Bitwig, Cubase, Reaper, Studio One, FL Studio and others.">
        <pkg-ref id="$ID.vst3"/>
    </choice>
    <choice id="au" title="Audio Unit" description="For Logic Pro, GarageBand and MainStage.">
        <pkg-ref id="$ID.au"/>
    </choice>
    <pkg-ref id="$ID.vst3" version="$VERSION">vst3.pkg</pkg-ref>
    <pkg-ref id="$ID.au" version="$VERSION">au.pkg</pkg-ref>
</installer-gui-script>
EOF

productbuild --distribution "$STAGE/distribution.xml" --resources installer \
             --package-path "$STAGE/pkgs" "$OUT.pkg" > /dev/null

# the .pkg plus the how-to, zipped, ready to share
ZIPDIR="$STAGE/zip/FasePhucker-$VERSION-macOS"
mkdir -p "$ZIPDIR"
cp "$OUT.pkg" "installer/READ ME FIRST.txt" "$ZIPDIR/"
ditto -c -k --norsrc --noextattr --keepParent "$ZIPDIR" "$OUT.zip"

echo "== Done"
echo "   $OUT.pkg"
echo "   $OUT.zip   <- share this one"
