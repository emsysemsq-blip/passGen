#!/usr/bin/env bash
# Test-only PoCL CPU OpenCL runtime for the Debian 13 amd64 cloud base.
# This is not a GPU runtime/benchmark. No host package installation is performed.
# Normal APT signed repository and package hash verification remain enabled.
set -euo pipefail

passgen_toolchain_root="${PASSGEN_TOOLCHAIN_ROOT:-/workspace/toolchain}"
case "$(dpkg --print-architecture)" in
  amd64) ;;
  *) printf 'This recorded setup targets Debian amd64.\n' >&2; exit 1 ;;
esac
for required_tool in apt-get dpkg-deb python3; do
  command -v "$required_tool" >/dev/null
done
keyring=/usr/share/keyrings/debian-archive-keyring.gpg
test -r "$keyring"
mkdir -p "$passgen_toolchain_root"/{ocl-debs,ocl-root,ocl-icd,ocl-cache,ocl-apt/empty,ocl-apt/lists/partial,ocl-apt/cache/archives/partial}

cat > "$passgen_toolchain_root/ocl-apt/sources.list" <<EOF
deb [arch=amd64 signed-by=$keyring] https://deb.debian.org/debian trixie main
EOF
cat > "$passgen_toolchain_root/ocl-apt.conf" <<EOF
Dir::Etc::Parts "$passgen_toolchain_root/ocl-apt/empty";
Dir::Etc::main "/dev/null";
Dir::Etc::sourcelist "$passgen_toolchain_root/ocl-apt/sources.list";
Dir::Etc::sourceparts "$passgen_toolchain_root/ocl-apt/empty";
Dir::State::lists "$passgen_toolchain_root/ocl-apt/lists";
Dir::Cache "$passgen_toolchain_root/ocl-apt/cache";
APT::Get::List-Cleanup "0";
EOF
export APT_CONFIG="$passgen_toolchain_root/ocl-apt.conf"
apt-get update

# Versions actually used for the validated PoCL source-build/dispatch tests.
# Dependencies already supplied by the fixed Debian cloud base include libc6,
# libstdc++6, libhwloc15 and ocl-icd-libopencl1. Keep normal dynamic-link checks
# below so a changed base fails visibly instead of claiming a usable runtime.
packages=(
  'clang-18=1:18.1.8-18+b1'
  'clinfo=3.0.25.02.14-1'
  'libclang-common-18-dev=1:18.1.8-18+b1'
  'libclang-cpp18=1:18.1.8-18+b1'
  'libclang1-18=1:18.1.8-18+b1'
  'libllvm18=1:18.1.8-18+b1'
  'libllvmspirvlib18.1=18.1.11-1'
  'libobjc-14-dev=14.2.0-19'
  'libobjc4=14.2.0-19'
  'libpocl2-common=6.0-6'
  'libpocl2t64=6.0-6'
  'llvm-18-linker-tools=1:18.1.8-18+b1'
  'llvm-spirv-18=18.1.11-1'
  'pocl-opencl-icd=6.0-6'
)
(
  cd "$passgen_toolchain_root/ocl-debs"
  apt-get download "${packages[@]}"
)
# Only extract exactly the recorded package/version pairs. Other existing files
# in the toolchain directories are preserved.
for specification in "${packages[@]}"; do
  package_name="${specification%%=*}"
  package_version="${specification#*=}"
  found=0
  for package_file in "$passgen_toolchain_root/ocl-debs/"*.deb; do
    if [[ "$(dpkg-deb -f "$package_file" Package)" == "$package_name" && \
          "$(dpkg-deb -f "$package_file" Version)" == "$package_version" ]]; then
      dpkg-deb -x "$package_file" "$passgen_toolchain_root/ocl-root"
      found=1
      break
    fi
  done
  if [[ "$found" != 1 ]]; then
    printf 'Missing downloaded package: %s\n' "$specification" >&2
    exit 1
  fi
done

printf '%s\n' "$passgen_toolchain_root/ocl-root/usr/lib/x86_64-linux-gnu/libpocl.so.2" > "$passgen_toolchain_root/ocl-icd/pocl.icd"
export LD_LIBRARY_PATH="$passgen_toolchain_root/ocl-root/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export OCL_ICD_VENDORS="$passgen_toolchain_root/ocl-icd"
export POCL_CACHE_DIR="$passgen_toolchain_root/ocl-cache"
if ldd "$passgen_toolchain_root/ocl-root/usr/lib/x86_64-linux-gnu/libpocl.so.2" | grep -q 'not found'; then
  ldd "$passgen_toolchain_root/ocl-root/usr/lib/x86_64-linux-gnu/libpocl.so.2" >&2
  printf 'Runtime dependencies are missing from this Debian base.\n' >&2
  exit 1
fi
{
  printf 'export LD_LIBRARY_PATH=%q\n' "$LD_LIBRARY_PATH"
  printf 'export OCL_ICD_VENDORS=%q\n' "$OCL_ICD_VENDORS"
  printf 'export POCL_CACHE_DIR=%q\n' "$POCL_CACHE_DIR"
} > "$passgen_toolchain_root/ocl-env.sh"
"$passgen_toolchain_root/ocl-root/usr/bin/clinfo" -l
printf 'PoCL CPU test runtime ready. Source %s/ocl-env.sh before software OpenCL tests.\n' "$passgen_toolchain_root"
