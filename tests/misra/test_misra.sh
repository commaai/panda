#!/usr/bin/env bash
set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
PANDA_DIR=$(realpath $DIR/../../)
OPENDBC_ROOT=${OPENDBC_ROOT:-$(python3 -c "import opendbc; print(opendbc.INCLUDE_PATH)")}

GREEN="\e[1;32m"
RED="\e[1;31m"
NC='\033[0m'

cd $PANDA_DIR
if [ -z "${SKIP_BUILD}" ]; then
  scons
fi

misra() {
  # exits with 1 on findings and 2 on analysis or usage errors
  # opendbc's safety code is checked by opendbc's own MISRA test
  # CMSIS picks its compiler support by __GNUC__, and scons generates the version defines
  # --inline-suppr honors the cppcheck-suppress comments in the sources, misrable reads cppcheck's syntax
  if ! misrable -I $PANDA_DIR -isystem $OPENDBC_ROOT -isystem $PANDA_DIR/board/stm32h7/inc \
         -D__GNUC__=9 -DGITVERSION='"misra"' -DHEALTH_PACKET_VERSION=0x1U -DCAN_PACKET_VERSION_HASH=0x1U \
         --inline-suppr \
         --suppressions-list $DIR/suppressions.txt \
         --external-api $DIR/external_api.txt \
         "$@"; then
    printf "${RED}** FAILED: MISRA violations or analysis errors!${NC}\n"
    exit 1
  fi
}

printf "\n${GREEN}** PANDA H7 CODE **${NC}\n"
misra -DSTM32H7 -DSTM32H725xx $PANDA_DIR/board/main.c
printf "\n${GREEN}** PANDA H7 CODE with ALLOW_DEBUG **${NC}\n"
misra -DSTM32H7 -DSTM32H725xx -DALLOW_DEBUG $PANDA_DIR/board/main.c

printf "\n${GREEN}Success!${NC} took $SECONDS seconds\n"
