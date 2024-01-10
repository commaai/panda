#!/bin/bash
set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
PANDA_DIR=$DIR/../../

GREEN="\e[1;32m"
NC='\033[0m'

: "${CPPCHECK_DIR:=$DIR/cppcheck/}"

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

cppcheck() {
  build_dir=/tmp/cppcheck_build
  mkdir -p $build_dir

  report="$(mktemp)"
  $CPPCHECK_DIR/cppcheck --enable=all --force --inline-suppr -I $PANDA_DIR/board/ \
          -I $gcc_inc "$(arm-none-eabi-gcc -print-file-name=include)" \
          --suppressions-list=$DIR/suppressions.txt --suppress=*:*inc/* \
          --suppress=*:*include/* --error-exitcode=2 --addon=misra --checkers-report=$report  \
          --cppcheck-build-dir=$build_dir \
          "$@"

  # sanity check the reported coverage
  no="$(grep '^No ' $report | wc -l)"
  yes="$(grep '^Yes' $report | wc -l)"
  echo "$yes checks enabled, $no disabled"
  if [[ $yes -lt 250 ]]; then
    echo "Count of enabled checks seems too low."
    exit 1
  fi
  if [[ $no -ne 99 ]]; then
    echo "Disabled check count threshold doesn't match, update to $no"
    exit 1
  fi
}

printf "\n${GREEN}** PANDA F4 CODE **${NC}\n"
cppcheck -DCAN3 -DPANDA -DSTM32F4 -UPEDAL -DUID_BASE board/main.c

printf "\n${GREEN}** PANDA H7 CODE **${NC}\n"
cppcheck -DCAN3 -DPANDA -DSTM32H7 -UPEDAL -DUID_BASE board/main.c

printf "\n${GREEN}** PEDAL CODE **${NC}\n"
cppcheck -UCAN3 -UPANDA -DSTM32F2 -DPEDAL -UUID_BASE board/pedal/main.c

printf "\n${GREEN}Success!${NC} took $SECONDS seconds\n"
