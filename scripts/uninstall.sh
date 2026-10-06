#!/bin/sh
set -eu

home=${HOME:-}
[ -n "$home" ] || { printf 'TermNav uninstaller: HOME is not set.\n' >&2; exit 1; }
prefix=${TERMNAV_PREFIX:-"$home/.local"}
case $(uname -s) in
    Linux) ;;
    *) printf 'TermNav uninstaller: this installation is for Linux only.\n' >&2; exit 1 ;;
esac
case "$prefix" in
    /*) ;;
    *) printf 'TermNav uninstaller: TERMNAV_PREFIX must be an absolute path.\n' >&2; exit 1 ;;
esac
[ "$prefix" != / ] || { printf 'TermNav uninstaller: TERMNAV_PREFIX cannot be the filesystem root.\n' >&2; exit 1; }

rm -f "$prefix/bin/termnav" \
    "$prefix/share/termnav/preview_helper.py" \
    "$prefix/share/man/man1/termnav.1"
printf 'Removed TermNav files from %s.\n' "$prefix"
