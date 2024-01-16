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
  hashed_args=$(echo -n "$@$DIR" | md5sum | awk '{print $1}')
  build_dir=/tmp/cppcheck_build/$hashed_args
  mkdir -p $build_dir

  $CPPCHECK_DIR/cppcheck --enable=all --force --inline-suppr -I $PANDA_DIR/board/ \
          -I $gcc_inc "$(arm-none-eabi-gcc -print-file-name=include)" \
          --suppressions-list=$DIR/suppressions.txt --suppress=*:*inc/* \
          --suppress=*:*include/* --error-exitcode=2 --addon=misra \
          --cppcheck-build-dir=$build_dir \
          "$@"
}

printf "\n${GREEN}** PANDA F4 CODE **${NC}\n"
cppcheck -DCAN3 -DPANDA -DSTM32F4 -UPEDAL -DUID_BASE $PANDA_DIR/board/main.c

printf "\n${GREEN}** PANDA H7 CODE **${NC}\n"
cppcheck -DCAN3 -DPANDA -DSTM32H7 -UPEDAL -DUID_BASE $PANDA_DIR/board/main.c

printf "\n${GREEN}** PEDAL CODE **${NC}\n"
cppcheck -UCAN3 -UPANDA -DSTM32F2 -DPEDAL -UUID_BASE $PANDA_DIR/board/pedal/main.c

printf "\n${GREEN}Success!${NC} took $SECONDS seconds\n"
