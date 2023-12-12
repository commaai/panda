#!/bin/bash
set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
PANDA_DIR=$DIR/../../

GREEN='\033[0;32m'
NC='\033[0m'

GCC_INC="$(arm-none-eabi-gcc -print-file-name=include)"
: "${CPPCHECK_DIR:=$DIR/cppcheck/}"
CPPCHECK="$CPPCHECK_DIR/cppcheck --dump --enable=all --force --inline-suppr -I $PANDA_DIR/board/ -I $GCC_INC \
          --suppressions-list=$DIR/suppressions.txt --suppress=*:*inc/* \
          --suppress=*:*include/* --error-exitcode=2"

RULES="$DIR/MISRA_C_2012.txt"
MISRA="python $CPPCHECK_DIR/addons/misra.py"
if [ -f "$RULES" ]; then
  MISRA="$MISRA --rule-texts $RULES"
fi

# install cppcheck if missing
if [ ! -d $CPPCHECK_DIR ]; then
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
scons -j8

printf "\n${GREEN}** PANDA F4 CODE **${NC}\n"
$CPPCHECK -DCAN3 -DPANDA -DSTM32F4 -UPEDAL -DUID_BASE board/main.c
$MISRA board/main.c.dump

printf "\n${GREEN}** PANDA H7 CODE **${NC}\n"
$CPPCHECK -DCAN3 -DPANDA -DSTM32H7 -UPEDAL -DUID_BASE board/main.c
$MISRA board/main.c.dump

printf "\n${GREEN}** PEDAL CODE **${NC}\n"
$CPPCHECK -UCAN3 -UPANDA -DSTM32F2 -DPEDAL -UUID_BASE board/pedal/main.c
$MISRA board/pedal/main.c.dump

printf "\n${GREEN}Success!${NC} took $SECONDS seconds\n"
