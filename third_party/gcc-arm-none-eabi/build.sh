#!/usr/bin/env bash
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null && pwd)"
cd "$DIR"

TOOLCHAIN_VERSION="13.2.rel1"
TOOLCHAIN_BASE="arm-gnu-toolchain-${TOOLCHAIN_VERSION}"
GCC_VERSION="13.2.1"

install_toolchain() {
  local PLATFORM_SUFFIX="$1"
  local ARCHNAME="$2"

  local TARBALL="${TOOLCHAIN_BASE}-${PLATFORM_SUFFIX}-arm-none-eabi.tar.xz"
  local URL="https://developer.arm.com/-/media/Files/downloads/gnu/${TOOLCHAIN_VERSION}/binrel/${TARBALL}"
  local INSTALL_DIR="$DIR/$ARCHNAME"

  # download if not cached
  if [ ! -f "$TARBALL" ]; then
    echo "[$ARCHNAME] Downloading $TARBALL ..."
    curl -fSL -o "$TARBALL" "$URL"
  fi

  # extract (find extracted dir by glob since ARM uses mixed case in dir name)
  local EXTRACT_GLOB="arm-gnu-toolchain-*-${PLATFORM_SUFFIX}-arm-none-eabi"
  if ! ls -d $EXTRACT_GLOB 1>/dev/null 2>&1; then
    echo "[$ARCHNAME] Extracting ..."
    tar xf "$TARBALL"
  fi
  local EXTRACT_DIR
  EXTRACT_DIR=$(ls -d $EXTRACT_GLOB)

  # install: copy only what we need
  rm -rf "$INSTALL_DIR"
  mkdir -p "$INSTALL_DIR"

  local SRC="$DIR/$EXTRACT_DIR"

  # --- bin: only the tools we actually use ---
  mkdir -p "$INSTALL_DIR/bin"
  for tool in gcc as ld ld.bfd ar objcopy objdump size ranlib nm strip readelf; do
    if [ -f "$SRC/bin/arm-none-eabi-$tool" ]; then
      cp "$SRC/bin/arm-none-eabi-$tool" "$INSTALL_DIR/bin/"
    fi
  done

  # --- libexec: compiler internals (cc1, collect2) ---
  local LIBEXEC_SRC="$SRC/libexec/gcc/arm-none-eabi/$GCC_VERSION"
  local LIBEXEC_DST="$INSTALL_DIR/libexec/gcc/arm-none-eabi/$GCC_VERSION"
  mkdir -p "$LIBEXEC_DST"
  for f in cc1 collect2 liblto_plugin.so liblto_plugin.0.so; do
    if [ -f "$LIBEXEC_SRC/$f" ]; then
      cp "$LIBEXEC_SRC/$f" "$LIBEXEC_DST/"
    fi
  done

  # --- lib/gcc: compiler support libraries ---
  local LIB_GCC_SRC="$SRC/lib/gcc/arm-none-eabi/$GCC_VERSION"
  local LIB_GCC_DST="$INSTALL_DIR/lib/gcc/arm-none-eabi/$GCC_VERSION"
  mkdir -p "$LIB_GCC_DST"

  # compiler-provided headers (stdint.h, stdbool.h, stddef.h, etc.)
  cp -r "$LIB_GCC_SRC/include" "$LIB_GCC_DST/"
  if [ -d "$LIB_GCC_SRC/include-fixed" ]; then
    cp -r "$LIB_GCC_SRC/include-fixed" "$LIB_GCC_DST/"
  fi

  # target multilib: only thumb/v7e-m+dp/hard (cortex-m7 hard-float)
  local MULTILIB="thumb/v7e-m+dp/hard"
  mkdir -p "$LIB_GCC_DST/$MULTILIB"
  cp "$LIB_GCC_SRC/$MULTILIB"/*.a "$LIB_GCC_DST/$MULTILIB/" 2>/dev/null || true
  cp "$LIB_GCC_SRC/$MULTILIB"/*.o "$LIB_GCC_DST/$MULTILIB/" 2>/dev/null || true

  # base crt files and libgcc
  cp "$LIB_GCC_SRC"/*.o "$LIB_GCC_DST/" 2>/dev/null || true
  cp "$LIB_GCC_SRC"/*.a "$LIB_GCC_DST/" 2>/dev/null || true

  # --- arm-none-eabi/: newlib and binutils support ---
  local ARM_SRC="$SRC/arm-none-eabi"
  local ARM_DST="$INSTALL_DIR/arm-none-eabi"
  mkdir -p "$ARM_DST/bin"

  # binutils bare names (gcc calls these internally)
  for tool in as ld ld.bfd ar nm objcopy objdump ranlib strip readelf; do
    if [ -f "$ARM_SRC/bin/$tool" ]; then
      cp "$ARM_SRC/bin/$tool" "$ARM_DST/bin/"
    fi
  done

  # newlib C headers only (needed for MISRA analysis; build uses -nostdlib so libs aren't needed)
  mkdir -p "$ARM_DST/include"
  find "$ARM_SRC/include" -maxdepth 1 -not -name 'c++' | while read -r item; do
    [ "$item" = "$ARM_SRC/include" ] && continue
    cp -r "$item" "$ARM_DST/include/"
  done

  # --- remove unused libraries (C-only project, no C++/Fortran) ---
  find "$INSTALL_DIR" -name 'libstdc++*' -delete 2>/dev/null || true
  find "$INSTALL_DIR" -name 'libsupc++*' -delete 2>/dev/null || true
  find "$INSTALL_DIR" -name 'libgfortran*' -delete 2>/dev/null || true
  find "$INSTALL_DIR" -name 'libatomic*' -delete 2>/dev/null || true
  # remove large unused compiler headers
  rm -f "$LIB_GCC_DST/include/arm_mve.h" 2>/dev/null || true

  # --- strip host binaries to reduce size ---
  find "$INSTALL_DIR/bin" -type f -executable -exec strip {} + 2>/dev/null || true
  strip "$LIBEXEC_DST/cc1" 2>/dev/null || true
  strip "$LIBEXEC_DST/collect2" 2>/dev/null || true
  find "$ARM_DST/bin" -type f -executable -exec strip {} + 2>/dev/null || true

  # --- clean up download artifacts ---
  rm -rf "$EXTRACT_DIR"
  rm -f "$TARBALL"

  echo "[$ARCHNAME] Installed to $INSTALL_DIR"
  du -sh "$INSTALL_DIR"
}

# Build for all supported platforms
install_toolchain "x86_64" "x86_64"
install_toolchain "aarch64" "aarch64"
install_toolchain "darwin-arm64" "Darwin"
