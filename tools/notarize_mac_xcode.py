#!/usr/bin/env python3
"""Notarize using the account already signed in to Xcode; never read credentials."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import plistlib
import re
import subprocess


def run(*args):
    subprocess.run([str(a) for a in args], check=True)


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--app', type=Path)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--resume', action='store_true')
args = parser.parse_args()
output = args.output.resolve()
output.mkdir(parents=True, exist_ok=True)
state_path = output / 'xcode-notary-state.json'
if args.resume:
    state = json.loads(state_path.read_text())
    archive = Path(state['archive'])
    version = state['version']
else:
    if not args.app:
        parser.error('--app is required for a new submission')
    app = args.app.resolve()
    info = plistlib.loads((app / 'Contents/Info.plist').read_bytes())
    signature = subprocess.run(['codesign', '-dv', '--verbose=4', str(app)],
                               check=True, capture_output=True, text=True).stderr
    team = re.search(r'^TeamIdentifier=(.+)$', signature, re.MULTILINE).group(1)
    identity = re.search(r'^Authority=(Developer ID Application:.+)$', signature, re.MULTILINE).group(1)
    version = info['CFBundleShortVersionString']
    now = datetime.now(timezone.utc)
    archive = output / ('PC98PLAYER-' + now.strftime('%Y%m%d-%H%M%S-%f') + '.xcarchive')
    destination = archive / 'Products/Applications/PC98PLAYER.app'
    destination.parent.mkdir(parents=True)
    run('ditto', app, destination)
    metadata = {
        'ArchiveVersion': 2, 'CreationDate': now.replace(tzinfo=None),
        'Name': 'PC98PLAYER', 'SchemeName': 'PC98PLAYER',
        'ApplicationProperties': {
            'ApplicationPath': 'Applications/PC98PLAYER.app',
            'CFBundleIdentifier': info['CFBundleIdentifier'],
            'CFBundleShortVersionString': version,
            'CFBundleVersion': info['CFBundleVersion'],
            'SigningIdentity': identity, 'Team': team,
        },
    }
    (archive / 'Info.plist').write_bytes(plistlib.dumps(metadata))
    options = archive / 'ExportOptions.plist'
    options.write_bytes(plistlib.dumps({
        'method': 'developer-id', 'destination': 'upload',
        'signingStyle': 'manual', 'signingCertificate': 'Developer ID Application',
        'teamID': team, 'manageAppVersionAndBuildNumber': False, 'uploadSymbols': False,
    }))
    run('xcodebuild', '-exportArchive', '-archivePath', archive,
        '-exportPath', archive / 'Upload', '-exportOptionsPlist', options,
        '-allowProvisioningUpdates')
    state_path.write_text(json.dumps({'archive': str(archive), 'version': version}, indent=2) + '\n')

export = archive / 'NotarizedExport'
result = subprocess.run(['xcodebuild', '-exportNotarizedApp', '-archivePath', str(archive),
                         '-exportPath', str(export)])
if result.returncode:
    raise SystemExit('Notarized export is not ready. Check this archive in Xcode Organizer, '
                     'then run tools/package_mac.sh --resume-xcode; this does not resubmit.')
app = export / 'PC98PLAYER.app'
run('codesign', '--verify', '--strict', '--all-architectures', app)
run('xcrun', 'stapler', 'validate', app)
run('spctl', '--assess', '--type', 'execute', '--verbose=2', app)
run('lipo', '-verify_arch', 'arm64', app / 'Contents/MacOS/PC98PLAYER')
run('lipo', '-verify_arch', 'x86_64', app / 'Contents/MacOS/PC98PLAYER')
package = output / f'PC98PLAYER-{version}-macOS-universal.zip'
run('ditto', '-c', '-k', '--sequesterRsrc', '--keepParent', app, package)
digest = hashlib.sha256(package.read_bytes()).hexdigest() + '  ' + package.name + '\n'
Path(str(package) + '.sha256').write_text(digest)
print('Ready for distribution:', package)
