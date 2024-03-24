#!/bin/bash
set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
PANDA_DIR=$(realpath $DIR/../../)

GREEN="\e[1;32m"
NC='\033[0m'

: "${CPPCHECK_DIR:=$DIR/cppcheck/}"

# install cppcheck if missing
if [ -z "${SKIP_BUILD}" ]; then
  $DIR/install.sh
fi

# ensure checked in coverage table is up to date
cd $DIR
python $CPPCHECK_DIR/addons/misra.py -generate-table > new_table
if ! cmp -s new_table coverage_table; then
  echo "MISRA coverage table doesn't match. Update and commit:"
  echo "mv new_table coverage_table && git add . && git commit -m 'update table'"
  exit 1
fi

cd $PANDA_DIR
if [ -z "${SKIP_BUILD}" ]; then
  scons -j8
fi

cppcheck() {
  # get all gcc defines: arm-none-eabi-gcc -dM -E - < /dev/null
  COMMON_DEFINES="-D__GNUC__=9 -UCMSIS_NVIC_VIRTUAL -UCMSIS_VECTAB_VIRTUAL"

  # note that cppcheck build cache results in inconsistent results as of v2.13.0
  OUTPUT=$DIR/.output.log
  $CPPCHECK_DIR/cppcheck --inline-suppr -I $PANDA_DIR/board/ \
          -I "$(arm-none-eabi-gcc -print-file-name=include)" \
          -I $PANDA_DIR/board/stm32f4/inc/ -I $PANDA_DIR/board/stm32h7/inc/ \
          --suppressions-list=$DIR/suppressions.txt --suppress=*:*inc/* \
          --suppress=*:*include/* --error-exitcode=2 --check-level=exhaustive \
          --platform=arm32-wchar_t4 $COMMON_DEFINES \
          "$@" |& tee $OUTPUT

  # cppcheck bug: some MISRA errors won't result in the error exit code,
  # so check the output (https://trac.cppcheck.net/ticket/12440#no1)
  if grep -e "misra violation" -e "error" -e "style: " $OUTPUT > /dev/null; then
    exit 1
  fi
}

PANDA_OPTS="--enable=all --disable=unusedFunction -DPANDA --addon=misra"

printf "\n${GREEN}** PANDA F4 CODE **${NC}\n"
cppcheck $PANDA_OPTS -DSTM32F4 -DSTM32F413xx $PANDA_DIR/board/main.c

printf "\n${GREEN}** PANDA H7 CODE **${NC}\n"
cppcheck $PANDA_OPTS -DSTM32H7 -DSTM32H725xx $PANDA_DIR/board/main.c

# unused needs to run globally
#printf "\n${GREEN}** UNUSED ALL CODE **${NC}\n"
#cppcheck --enable=unusedFunction --quiet $PANDA_DIR/board/

printf "\n${GREEN}Success!${NC} took $SECONDS seconds\n"

