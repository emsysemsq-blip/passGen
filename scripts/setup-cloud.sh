#!/usr/bin/env bash
set -euo pipefail
repo_dir="$(cd "$(dirname "$0")/.." && pwd)"
# Debian 13 / amd64. Keep dependency files outside the checkout; no root needed.
tools_dir=/workspace/toolchain
mkdir -p "$tools_dir"/apt/lists/partial "$tools_dir"/apt/cache/archives/partial "$tools_dir"/apt/empty "$tools_dir"/debs "$tools_dir"/root
cat > "$tools_dir/apt/sources.list" <<'SOURCES'
deb [signed-by=/usr/share/keyrings/debian-archive-keyring.gpg] https://deb.debian.org/debian trixie main
SOURCES
cat > "$tools_dir/apt.conf" <<'APTCONF'
Dir::Etc::Parts "/workspace/toolchain/apt/empty";
Dir::Etc::main "/dev/null";
Dir::Etc::sourcelist "/workspace/toolchain/apt/sources.list";
Dir::Etc::sourceparts "/workspace/toolchain/apt/empty";
Dir::State::lists "/workspace/toolchain/apt/lists";
Dir::Cache "/workspace/toolchain/apt/cache";
APT::Get::List-Cleanup "0";
APTCONF
export APT_CONFIG="$tools_dir/apt.conf"
apt-get update
(cd "$tools_dir/debs" && apt-get download g++-mingw-w64-x86-64-posix gcc-mingw-w64-x86-64-posix gcc-mingw-w64-x86-64-posix-runtime gcc-mingw-w64-base binutils-mingw-w64-x86-64 mingw-w64-x86-64-dev mingw-w64-common)
for package in binutils-mingw-w64-x86-64 g++-mingw-w64-x86-64-posix gcc-mingw-w64-base gcc-mingw-w64-x86-64-posix gcc-mingw-w64-x86-64-posix-runtime mingw-w64-common mingw-w64-x86-64-dev; do
 for deb in "$tools_dir/debs/${package}_"*.deb; do dpkg-deb -x "$deb" "$tools_dir/root"; done
done
export PATH="$tools_dir/root/usr/bin:$PATH"
export CXX_WINDOWS="$tools_dir/root/usr/bin/x86_64-w64-mingw32-g++-posix"
cd "$repo_dir"
bash scripts/build-linux.sh
bash scripts/build-windows.sh
