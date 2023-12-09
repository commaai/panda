#!/bin/bash
set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
PANDA_DIR=$DIR/../../

GREEN='\033[0;32m'
NC='\033[0m'

: "${CPPCHECK_DIR:=$DIR/cppcheck/}"
CPPCHECK="$CPPCHECK_DIR/cppcheck --dump --enable=all --force --inline-suppr -I $PANDA_DIR/board/ \
          --suppressions-list=$DIR/suppressions.txt --suppress=*:*inc/* --error-exitcode=2"

RULES="$DIR/MISRA_C_2012.txt"
MISRA="python $CPPCHECK_DIR/addons/misra.py"
if [ -f "$RULES" ]; then
  MISRA="$MISRA --rule-texts $RULES"
fi
MISRA="$MISRA $PANDA_DIR/board/main.c.dump"

# install cppcheck if missing
if [ ! -d cppcheck/ ]; then
  $DIR/install.sh
fi

# generate coverage matrix
#python tests/misra/cppcheck/addons/misra.py -generate-table > tests/misra/coverage_table

printf "\n${GREEN}** PANDA F4 CODE **${NC}\n"
$CPPCHECK -DPANDA -DSTM32F4 -UPEDAL -DUID_BASE $PANDA_DIR/board/main.c
$MISRA

printf "\n${GREEN}** PANDA H7 CODE **${NC}\n"
$CPPCHECK -DPANDA -DSTM32H7 -UPEDAL -DUID_BASE $PANDA_DIR/board/main.c
$MISRA

printf "\n${GREEN}** PEDAL CODE **${NC}\n"
$CPPCHECK -UPANDA -DSTM32F2 -DPEDAL -UUID_BASE $PANDA_DIR/board/main.c
$MISRA

echo "${GREEN}Success${NC}"
