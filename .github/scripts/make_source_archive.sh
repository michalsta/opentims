#!/bin/sh
# Archive the repository at a git ref with every symlink replaced by a copy of
# its target, so the result unpacks correctly on Windows too.
# Usage: make_source_archive.sh <ref> <name> <output dir>
set -eu
ref="$1"
name="$2"
out="$3"

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

mkdir "$work/links"
git archive --prefix="$name/" "$ref" | tar -x -C "$work/links"
cp -RL "$work/links/$name" "$work/$name"
if [ -n "$(find "$work/$name" -type l)" ]; then
    echo "symlinks left in the archive:" >&2
    find "$work/$name" -type l >&2
    exit 1
fi

mkdir -p "$out"
tar -czf "$out/$name.tar.gz" -C "$work" "$name"
echo "$out/$name.tar.gz"
