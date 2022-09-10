#!/bin/bash
set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
PANDA_DIR=$DIR/../../

CPPCHECK_DIR=$DIR/cppcheck
CPPCHECK=$CPPCHECK_DIR/cppcheck

RULES="$DIR/MISRA_C_2012.txt"
MISRA="python $CPPCHECK_DIR/addons/misra.py"
if [ -f "$RULES" ]; then
  MISRA="$MISRA --rule-texts $RULES"
fi

mkdir -p /tmp/misra
ERROR_CODE=0

# install cppcheck if missing
if [ ! -d cppcheck/ ]; then
  $DIR/install.sh
fi

# generate coverage matrix
#python tests/misra/cppcheck/addons/misra.py -generate-table > tests/misra/coverage_table

printf "\nPANDA F4 CODE\n"
$CPPCHECK -DPANDA -DSTM32F4 -UPEDAL -DCAN3 -DUID_BASE \
          --suppressions-list=$DIR/suppressions.txt --suppress=*:*inc/* \
          -I $PANDA_DIR/board/ --dump --enable=all --inline-suppr --force \
          $PANDA_DIR/board/main.c 2>/tmp/misra/cppcheck_f4_output.txt

$MISRA $PANDA_DIR/board/main.c.dump 2> /tmp/misra/misra_f4_output.txt || true

# strip (information) lines
cppcheck_f4_output=$( cat /tmp/misra/cppcheck_f4_output.txt | grep -v ": information: " ) || true
misra_f4_output=$( cat /tmp/misra/misra_f4_output.txt | grep -v ": information: " ) || true


printf "\nPANDA H7 CODE\n"
$CPPCHECK -DPANDA -DSTM32H7 -UPEDAL -DUID_BASE \
          --suppressions-list=$DIR/suppressions.txt --suppress=*:*inc/* \
          -I $PANDA_DIR/board/ --dump --enable=all --inline-suppr --force \
          $PANDA_DIR/board/main.c 2>/tmp/misra/cppcheck_h7_output.txt

$MISRA $PANDA_DIR/board/main.c.dump 2> /tmp/misra/misra_h7_output.txt || true

# strip (information) lines
cppcheck_h7_output=$( cat /tmp/misra/cppcheck_h7_output.txt | grep -v ": information: " ) || true
misra_h7_output=$( cat /tmp/misra/misra_h7_output.txt | grep -v ": information: " ) || true


printf "\nPEDAL CODE\n"
$CPPCHECK -UPANDA -DSTM32F2 -DPEDAL -UCAN3 \
          --suppressions-list=$DIR/suppressions.txt --suppress=*:*inc/* \
          -I $PANDA_DIR/board/ --dump --enable=all --inline-suppr --force \
          $PANDA_DIR/board/pedal/main.c 2>/tmp/misra/cppcheck_pedal_output.txt

$MISRA $PANDA_DIR/board/pedal/main.c.dump 2> /tmp/misra/misra_pedal_output.txt || true

# strip (information) lines
cppcheck_pedal_output=$( cat /tmp/misra/cppcheck_pedal_output.txt | grep -v ": information: " ) || true
misra_pedal_output=$( cat /tmp/misra/misra_pedal_output.txt | grep -v ": information: " ) || true

if [[ -n "$misra_f4_output" ]] || [[ -n "$cppcheck_f4_output" ]]
then
  echo "Failed! found Misra violations in panda F4 code:"
  echo "$misra_f4_output"
  echo "$cppcheck_f4_output"
  ERROR_CODE=1
fi

if [[ -n "$misra_h7_output" ]] || [[ -n "$cppcheck_h7_output" ]]
then
  echo "Failed! found Misra violations in panda H7 code:"
  echo "$misra_h7_output"
  echo "$cppcheck_h7_output"
  ERROR_CODE=1
fi

if [[ -n "$misra_pedal_output" ]] || [[ -n "$cppcheck_pedal_output" ]]
then
  echo "Failed! found Misra violations in pedal code:"
  echo "$misra_pedal_output"
  echo "$cppcheck_pedal_output"
  ERROR_CODE=1
fi

if [[ $ERROR_CODE > 0 ]]
then
  exit 1
fi

echo "Success"
