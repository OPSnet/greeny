#!/bin/bash

make clean
LDFLAGS='-static -s' make build/native/bin/greeny-cli
LDFLAGS='-s' make
mingw64-make clean
# for some reason, you cannot specify ldflags to mingw64-make without an override, which I don't really want
mingw64-make
x86_64-w64-mingw32-strip build/windows/bin/greeny{,-cli}.exe

release_name="greeny-$(date '+%Y-%m-%d-%H:%M')"
release_dir="build/$release_name"
mkdir -p "$release_dir"
cp build/native/bin/greeny{,-cli} build/windows/bin/greeny{,-cli}.exe "$release_dir"
for i in $release_dir/greeny*; do gpg -u greeny-signing -b "$i"; done
gpg --export greeny-signing > "$release_dir/greeny-signing.pub"
