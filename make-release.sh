#!/bin/bash

make clean
LDFLAGS='-static -s' make build/native/bin/greeny-cli
LDFLAGS='-s' make
mingw64-make clean
LDFLAGS='-s' mingw64-make

release_name="greeny-$(date '+%Y-%m-%d-%M:%S')"
release_dir="build/$release_name"
release_tarball="$release_dir.tar.gz"
mkdir -p "$release_dir"
cp build/native/bin/{greeny,greeny-cli} build/windows/bin/{greeny.exe,greeny-cli.exe} "$release_dir"
tar czf "$release_tarball" -C build "$release_name"
