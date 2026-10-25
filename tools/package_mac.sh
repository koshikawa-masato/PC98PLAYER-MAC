#!/bin/sh
# Developer ID distribution, outside the Mac App Store.
set -eu
cd "$(dirname "$0")/.."
mode=notarize
case "${1:-}" in
  --sign-only) mode=sign-only ;;
  '') ;;
  *) echo 'Usage: tools/package_mac.sh [--sign-only]' >&2; exit 2 ;;
esac
if [ "$mode" = notarize ] && [ -z "${MAC_NOTARY_PROFILE:-}" ]; then
  echo 'Set MAC_NOTARY_PROFILE to an existing notarytool keychain profile.' >&2
  echo 'Use --sign-only to prepare an explicitly unnotarized package.' >&2
  exit 2
fi
identity=${MAC_SIGN_IDENTITY:-}
if [ -z "$identity" ]; then
  identities=$(security find-identity -v -p codesigning | sed -n '/"Developer ID Application:/s/.*) \([A-F0-9]*\) .*/\1/p')
  count=$(printf '%s\n' "$identities" | awk 'NF {n++} END {print n+0}')
  if [ "$count" -ne 1 ]; then
    echo 'Set MAC_SIGN_IDENTITY to the Developer ID Application identity to use.' >&2
    exit 2
  fi
  identity=$identities
fi
./build_mac.sh
source_app=build/PC98PLAYER.app
binary="$source_app/Contents/MacOS/PC98PLAYER"
lipo -verify_arch x86_64 "$binary"
lipo -verify_arch arm64 "$binary"
version=$(/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' "$source_app/Contents/Info.plist")
output="$PWD/out/distribution"
mkdir -p "$output"
stage=$(mktemp -d "$output/stage.XXXXXX")
trap 'rm -rf "$stage"' EXIT HUP INT TERM
app="$stage/PC98PLAYER.app"
ditto "$source_app" "$app"
codesign --force --sign "$identity" --options runtime --timestamp "$app"
codesign --verify --strict --all-architectures --verbose=2 "$app"
# Reject development/ad-hoc signing even if an explicit identity was supplied.
codesign --verify -R='anchor apple generic and certificate leaf[field.1.2.840.113635.100.6.1.13] exists' "$app"
base="PC98PLAYER-$version-macOS-universal"
archive="$output/$base-signed-not-notarized.zip"
ditto -c -k --sequesterRsrc --keepParent "$app" "$archive"
if [ "$mode" = sign-only ]; then
  shasum -a 256 "$archive" > "$archive.sha256"
  printf '\nSigned, NOT notarized: %s\n' "$archive"
  exit 0
fi
report="$output/$base-notarization.json"
xcrun notarytool submit "$archive" --keychain-profile "$MAC_NOTARY_PROFILE" --wait --output-format json > "$report"
python3 - "$report" <<'PY'
import json, sys
report = json.load(open(sys.argv[1]))
print('Notarization:', report.get('status'), 'Submission:', report.get('id'))
if report.get('status') != 'Accepted':
    raise SystemExit('Notarization was not accepted; inspect the submission log before distribution.')
PY
xcrun stapler staple "$app"
xcrun stapler validate "$app"
spctl --assess --type execute --verbose=2 "$app"
archive="$output/$base.zip"
ditto -c -k --sequesterRsrc --keepParent "$app" "$archive"
shasum -a 256 "$archive" > "$archive.sha256"
printf '\nReady for distribution: %s\n' "$archive"
