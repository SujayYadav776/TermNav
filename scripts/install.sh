#!/bin/sh
set -eu

repo='https://github.com/SujayYadav776/TermNav'
archive_url="$repo/archive/refs/heads/main.tar.gz"
home=${HOME:-}
[ -n "$home" ] || { printf 'TermNav installer: HOME is not set.\n' >&2; exit 1; }
prefix=${TERMNAV_PREFIX:-"$home/.local"}

fail() { printf 'TermNav installer: %s\n' "$*" >&2; exit 1; }

case $(uname -s) in
    Linux) ;;
    *) fail 'TermNav currently supports Linux only. Windows users can run it inside WSL; native macOS and Windows are not supported.' ;;
esac

case "$prefix" in
    /*) ;;
    *) fail 'TERMNAV_PREFIX must be an absolute path.' ;;
esac
[ "$prefix" != / ] || fail 'TERMNAV_PREFIX cannot be the filesystem root.'

missing=0
for tool in make pkg-config tar; do
    command -v "$tool" >/dev/null 2>&1 || missing=1
done
if ! command -v cc >/dev/null 2>&1 && ! command -v gcc >/dev/null 2>&1 && ! command -v clang >/dev/null 2>&1; then missing=1; fi
if ! command -v curl >/dev/null 2>&1 && ! command -v wget >/dev/null 2>&1; then missing=1; fi
if command -v pkg-config >/dev/null 2>&1 && ! pkg-config --exists ncursesw; then missing=1; fi

if [ "$missing" -eq 1 ]; then
    [ -r /etc/os-release ] || fail "Install a C compiler, make, pkg-config, curl, tar, and ncurses wide-character development files, then retry. See $repo#build-on-linux-ubuntu-wsl."
    . /etc/os-release
    printf 'Installing build tools and ncurses with the Linux package manager...\n'
    install_packages() {
        if [ "$(id -u)" -eq 0 ]; then "$@"
        elif command -v sudo >/dev/null 2>&1; then sudo "$@"
        else fail 'Package setup needs root or sudo. Install the build tools and ncurses development files, then retry.'
        fi
    }
    case " ${ID:-} ${ID_LIKE:-} " in
        *' debian '*|*' ubuntu '*) install_packages apt-get update; install_packages apt-get install -y build-essential pkg-config libncurses-dev curl ca-certificates tar ;;
        *' alpine '*) install_packages apk add build-base pkgconf ncurses-dev curl ca-certificates tar ;;
        *' fedora '*|*' rhel '*|*' centos '*) install_packages dnf install -y gcc make pkgconf-pkg-config ncurses-devel curl ca-certificates tar ;;
        *' arch '*|*' manjaro '*) install_packages pacman -S --needed --noconfirm base-devel pkgconf ncurses curl ca-certificates tar ;;
        *' opensuse '*|*' suse '*) install_packages zypper --non-interactive install gcc make pkg-config ncurses-devel curl ca-certificates tar ;;
        *) fail "Could not select a package manager for ${ID:-this Linux distribution}. Install a C compiler, make, pkg-config, curl, tar, and ncurses wide-character development files, then retry." ;;
    esac
fi

work=$(mktemp -d "${TMPDIR:-/tmp}/termnav-install.XXXXXX") || fail 'Could not create a temporary build directory.'
trap 'rm -rf "$work"' 0
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 143' TERM
mkdir "$work/source"

printf 'Downloading TermNav source...\n'
if command -v curl >/dev/null 2>&1; then
    curl -fsSL "$archive_url" -o "$work/source.tar.gz" || fail 'Could not download the TermNav source archive.'
else
    wget -q "$archive_url" -O "$work/source.tar.gz" || fail 'Could not download the TermNav source archive.'
fi
tar -xzf "$work/source.tar.gz" --strip-components=1 -C "$work/source" || fail 'Could not unpack the TermNav source archive.'

printf 'Building TermNav...\n'
make -s -C "$work/source" all || fail 'Build failed. Check the compiler and ncurses development packages.'
printf 'Installing to %s...\n' "$prefix"
make -s -C "$work/source" PREFIX="$prefix" install || fail "Install failed. Check permissions for $prefix or choose another absolute path with TERMNAV_PREFIX."

printf '\nTermNav installed at %s/bin/termnav\n' "$prefix"
case ":${PATH:-}:" in
    *":$prefix/bin:"*) ;;
    *) printf 'Add it to PATH with:\n  export PATH="%s/bin:%s"\n' "$prefix" "${PATH:-}" ;;
esac
printf 'Start it with: termnav [directory]\n'
printf 'Optional previews need Python 3; images also need Pillow, and PDFs need pdftotext.\n'
