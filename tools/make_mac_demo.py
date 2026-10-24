#!/usr/bin/env python3
"""Create a tiny original DOS COM program for macOS port verification (no game data)."""
from pathlib import Path
import sys

folder = Path(sys.argv[1] if len(sys.argv) > 1 else 'out/smoke-game')
folder.mkdir(parents=True, exist_ok=True)
text = ('PC98PLAYER for Mac\r\n\r\nCocoa / CoreText / AudioQueue\r\n\r\n'
        '日本語表示テスト：ＰＣ－９８ 漢字 ひらがな カタカナ\r\n\r\n'
        'Keyboard test: press a key.  F12 releases mouse.\r\n$').encode('cp932')
# Print the CP932 string; read and echo keys in a loop.
program = bytes.fromhex('BA 13 01 B4 09 CD 21 B4 08 CD 21 B4 02 88 C2 CD 21 EB F4')
for name, content in {
    'DEMO.COM': program + text,
    'PC98PLAYER.INI': b'[PC98PLAYER]\nStart=DEMO.COM\nMIDI=0\nScale=1\nRhythmROM=0\nExitOnEnd=0\n',
}.items():
    path = folder / name
    with path.open('xb') as file:
        file.write(content)
print(folder.resolve())
