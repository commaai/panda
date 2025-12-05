#!/usr/bin/env bash
set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
PANDA_DIR=$(realpath $DIR/../../)
OPENDBC_ROOT=$(python3 -c "import opendbc; print(opendbc.INCLUDE_PATH)")

GREEN="\e[1;32m"
YELLOW="\e[1;33m"
RED="\e[1;31m"
NC='\033[0m'

: "${CPPCHECK_DIR:=$DIR/cppcheck/}"

# install cppcheck if missing
if [ -z "${SKIP_CPPCHECK_INSTALL}" ]; then
  $DIR/install.sh
fi

# ensure checked in coverage table is up to date
cd $DIR
if [ -z "$SKIP_TABLES_DIFF" ]; then
  python3 $CPPCHECK_DIR/addons/misra.py -generate-table > coverage_table
  if ! git diff --quiet coverage_table; then
    echo -e "${YELLOW}MISRA coverage table doesn't match. Update and commit:${NC}"
    exit 3
  fi
fi

cd $PANDA_DIR
if [ -z "${SKIP_BUILD}" ]; then
  scons -j8
fi

CHECKLIST=$DIR/checkers.txt
echo "Cppcheck checkers list from test_misra.sh:" > $CHECKLIST

cppcheck() {
  # get all gcc defines: arm-none-eabi-gcc -dM -E - < /dev/null
  COMMON_DEFINES="-D__GNUC__=9 -UCMSIS_NVIC_VIRTUAL -UCMSIS_VECTAB_VIRTUAL -UPANDA_JUNGLE -UBOOTSTUB"

  # note that cppcheck build cache results in inconsistent results as of v2.13.0
  OUTPUT=$DIR/.output.log

  echo -e "\n\n\n\n\nTEST variant options:" >> $CHECKLIST
  echo -e ""${@//$PANDA_DIR/}"\n\n" >> $CHECKLIST # (absolute path removed)

  $CPPCHECK_DIR/cppcheck --inline-suppr \
          -I $PANDA_DIR \
          -I "$(arm-none-eabi-gcc -print-file-name=include)" \
          -I $OPENDBC_ROOT \
          --suppress=misra-c2012-5.9:$OPENDBC_ROOT/* \
          --suppressions-list=$DIR/suppressions.txt --suppress=*:*inc/* \
          --suppress=*:*include/* --error-exitcode=2 --check-level=exhaustive --safety \
          --platform=arm32-wchar_t4 $COMMON_DEFINES --checkers-report=$CHECKLIST.tmp \
          --std=c11 "$@" 2>&1 | tee $OUTPUT

  cat $CHECKLIST.tmp >> $CHECKLIST
  rm $CHECKLIST.tmp
  # cppcheck bug: some MISRA errors won't result in the error exit code,
  # so check the output (https://trac.cppcheck.net/ticket/12440#no1)
  if grep -e "misra violation" -e "error" -e "style: " $OUTPUT > /dev/null; then
    printf "${RED}** FAILED: MISRA violations found!${NC}\n"
    exit 1
  fi
}

PANDA_OPTS="--enable=all --addon=misra"

printf "\n${GREEN}** PANDA H7 CODE **${NC}\n"
cppcheck $PANDA_OPTS -DSTM32H7 -DSTM32H725xx -I $PANDA_DIR/board/stm32h7/inc/ -DPANDA \
    $PANDA_DIR/board/libc.c \
    $PANDA_DIR/board/early_init.c \
    $PANDA_DIR/board/critical.c \
    $PANDA_DIR/board/drivers/led.c \
    $PANDA_DIR/board/drivers/pwm.c \
    $PANDA_DIR/board/drivers/gpio.c \
    $PANDA_DIR/board/drivers/fake_siren.c \
    $PANDA_DIR/board/stm32h7/lli2c.c \
    $PANDA_DIR/board/stm32h7/clock.c \
    $PANDA_DIR/board/drivers/clock_source.c \
    $PANDA_DIR/board/stm32h7/sound.c \
    $PANDA_DIR/board/stm32h7/llflash.c \
    $PANDA_DIR/board/stm32h7/stm32h7_config.c \
    $PANDA_DIR/board/drivers/registers.c \
    $PANDA_DIR/board/drivers/interrupts.c \
    $PANDA_DIR/board/provision.c \
    $PANDA_DIR/board/stm32h7/peripherals.c \
    $PANDA_DIR/board/stm32h7/llusb.c \
    $PANDA_DIR/board/drivers/usb.c \
    $PANDA_DIR/board/drivers/spi.c \
    $PANDA_DIR/board/drivers/timers.c \
    $PANDA_DIR/board/stm32h7/lladc.c \
    $PANDA_DIR/board/stm32h7/llspi.c \
    $PANDA_DIR/board/faults.c \
    $PANDA_DIR/board/boards/unused_funcs.c \
    $PANDA_DIR/board/utils.c \
    $PANDA_DIR/board/globals.c \
    $PANDA_DIR/board/obj/gitversion.c \
    $PANDA_DIR/board/can_comms.c \
    $PANDA_DIR/board/drivers/fan.c \
    $PANDA_DIR/board/power_saving.c \
    $PANDA_DIR/board/drivers/uart.c \
    $PANDA_DIR/board/stm32h7/llfdcan.c \
    $PANDA_DIR/board/drivers/harness.c   \
    $PANDA_DIR/board/drivers/bootkick.c \
    $PANDA_DIR/board/stm32h7/llfan.c \
    $PANDA_DIR/board/stm32h7/lluart.c \
    $PANDA_DIR/board/drivers/fdcan.c \
    $PANDA_DIR/board/drivers/can_common.c \
    $PANDA_DIR/board/main_comms.c \
    $PANDA_DIR/board/main.c \
    $PANDA_DIR/board/drivers/simple_watchdog.c \
    $PANDA_DIR/board/stm32h7/board.c \
    $PANDA_DIR/board/boards/tres.c \
    $PANDA_DIR/board/boards/red.c \
    $PANDA_DIR/board/boards/cuatro.c \
    $PANDA_DIR/board/main_definitions.c

printf "\n${GREEN}Success!${NC} took $SECONDS seconds\n"

# ensure list of checkers is up to date
cd $DIR
if [ -z "$SKIP_TABLES_DIFF" ] && ! git diff --quiet $CHECKLIST; then
  echo -e "\n${YELLOW}WARNING: Cppcheck checkers.txt report has changed. Review and commit...${NC}"
  exit 4
fi
