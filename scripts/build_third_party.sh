#!/usr/bin/env bash
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null && pwd)"
REPO_DIR="$(cd "$DIR/.." && pwd)"
cd "$REPO_DIR"

TOOLCHAIN_VERSION="13.2.rel1"
TOOLCHAIN_BASE="arm-gnu-toolchain-${TOOLCHAIN_VERSION}"
GCC_VERSION="13.2.1"

install_toolchain() {
  local PLATFORM_SUFFIX="$1"
  local ARCHNAME="$2"

  local TARBALL="${TOOLCHAIN_BASE}-${PLATFORM_SUFFIX}-arm-none-eabi.tar.xz"
  local URL="https://developer.arm.com/-/media/Files/downloads/gnu/${TOOLCHAIN_VERSION}/binrel/${TARBALL}"
  local INSTALL_DIR="$REPO_DIR/$ARCHNAME"

  # download if not cached
  if [ ! -f "$TARBALL" ]; then
    echo "[$ARCHNAME] Downloading $TARBALL ..."
    curl -fSL -o "$TARBALL" "$URL"
  fi

  # extract
  local EXTRACT_GLOB="arm-gnu-toolchain-*-${PLATFORM_SUFFIX}-arm-none-eabi"
  if ! ls -d $EXTRACT_GLOB 1>/dev/null 2>&1; then
    echo "[$ARCHNAME] Extracting ..."
    tar xf "$TARBALL"
  fi
  local EXTRACT_DIR
  EXTRACT_DIR=$(ls -d $EXTRACT_GLOB)

  rm -rf "$INSTALL_DIR"
  mkdir -p "$INSTALL_DIR"

  local SRC="$REPO_DIR/$EXTRACT_DIR"

  # --- bin: only the tools directly used by SConscript ---
  mkdir -p "$INSTALL_DIR/bin"
  for tool in gcc objcopy size; do
    if [ -f "$SRC/bin/arm-none-eabi-$tool" ]; then
      cp "$SRC/bin/arm-none-eabi-$tool" "$INSTALL_DIR/bin/"
    fi
  done

  # --- libexec: cc1 and collect2 (needed by gcc driver) ---
  local LIBEXEC_SRC="$SRC/libexec/gcc/arm-none-eabi/$GCC_VERSION"
  local LIBEXEC_DST="$INSTALL_DIR/libexec/gcc/arm-none-eabi/$GCC_VERSION"
  mkdir -p "$LIBEXEC_DST"
  for f in cc1 collect2 liblto_plugin.so liblto_plugin.0.so; do
    if [ -f "$LIBEXEC_SRC/$f" ]; then
      cp "$LIBEXEC_SRC/$f" "$LIBEXEC_DST/"
    fi
  done

  # --- arm-none-eabi/bin: only tools gcc calls internally ---
  local ARM_SRC="$SRC/arm-none-eabi"
  local ARM_DST="$INSTALL_DIR/arm-none-eabi"
  mkdir -p "$ARM_DST/bin"
  for tool in as ld ld.bfd; do
    if [ -f "$ARM_SRC/bin/$tool" ]; then
      cp "$ARM_SRC/bin/$tool" "$ARM_DST/bin/"
    fi
  done

  # --- newlib C headers (needed for #include_next from GCC's stdint.h etc.) ---
  mkdir -p "$ARM_DST/include"
  find "$ARM_SRC/include" -maxdepth 1 -not -name 'c++' | while read -r item; do
    [ "$item" = "$ARM_SRC/include" ] && continue
    cp -r "$item" "$ARM_DST/include/"
  done

  # --- lib/gcc: compiler support (only the multilib we use) ---
  local LIB_GCC_SRC="$SRC/lib/gcc/arm-none-eabi/$GCC_VERSION"
  local LIB_GCC_DST="$INSTALL_DIR/lib/gcc/arm-none-eabi/$GCC_VERSION"
  local MULTILIB="thumb/v7e-m+dp/hard"
  mkdir -p "$LIB_GCC_DST/$MULTILIB"

  # compiler-provided headers
  cp -r "$LIB_GCC_SRC/include" "$LIB_GCC_DST/"
  if [ -d "$LIB_GCC_SRC/include-fixed" ]; then
    cp -r "$LIB_GCC_SRC/include-fixed" "$LIB_GCC_DST/"
  fi

  # target multilib: only thumb/v7e-m+dp/hard (cortex-m7 hard-float)
  cp "$LIB_GCC_SRC/$MULTILIB"/libgcc.a "$LIB_GCC_DST/$MULTILIB/"
  cp "$LIB_GCC_SRC/$MULTILIB"/crt*.o "$LIB_GCC_DST/$MULTILIB/" 2>/dev/null || true

  # --- remove unused headers ---
  rm -f "$LIB_GCC_DST/include/arm_neon.h"
  rm -f "$LIB_GCC_DST/include/arm_mve_types.h"
  rm -f "$LIB_GCC_DST/include/mmintrin.h"
  rm -f "$LIB_GCC_DST/include/ISO_Fortran_binding.h"
  rm -f "$LIB_GCC_DST/include/gcov.h"
  rm -f "$LIB_GCC_DST/include/arm_cde.h"

  # --- strip host binaries (use llvm-strip for cross-platform support) ---
  local STRIP_CMD="strip"
  if [ "$ARCHNAME" != "$(uname -m)" ] && [ "$ARCHNAME" != "Darwin" ] && command -v llvm-strip &>/dev/null; then
    STRIP_CMD="llvm-strip"
  fi
  find "$INSTALL_DIR" -type f \( -executable -o -name '*.so' \) -exec $STRIP_CMD {} + 2>/dev/null || true

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

# --- Create single compressed tarball ---
echo "Creating compressed tarball..."
XZ_OPT=-9 tar -cJf gcc-arm-none-eabi.tar.xz x86_64/ aarch64/ Darwin/
rm -rf x86_64 aarch64 Darwin

echo "Done."
ls -lh gcc-arm-none-eabi.tar.xz
