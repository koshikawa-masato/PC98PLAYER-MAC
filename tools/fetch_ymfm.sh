#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
dest="$root/third_party/ymfm"
# Pin the dependency so local and CI builds use the same source.
revision=81aec25ccbb98f4873a255f7551ac4dadac59b4a
if [ ! -d "$dest/.git" ]; then
  git clone https://github.com/aaronsgiles/ymfm.git "$dest"
fi
git -C "$dest" checkout --detach "$revision"
