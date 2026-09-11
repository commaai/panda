#!/usr/bin/env bash
set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
PANDA_DIR=$(realpath $DIR/../../)
OPENDBC_ROOT=$(python3 -c "import opendbc; print(opendbc.INCLUDE_PATH)")

GREEN="\e[1;32m"
YELLOW="\e[1;33m"
RED="\e[1;31m"
NC='\033[0m'

: "${CPPCHECK_DIR:=$(python3 -c "import cppcheck; print(cppcheck.DIR)")}"

RUN_DIR=$(mktemp -d)
trap 'rm -rf "$RUN_DIR"' EXIT

# ensure checked in coverage table is up to date
cd $DIR
if [ -z "$SKIP_TABLES_DIFF" ]; then
  python3 $CPPCHECK_DIR/addons/misra.py -generate-table > "$RUN_DIR/coverage_table"
  if ! cmp -s coverage_table "$RUN_DIR/coverage_table"; then
    cp "$RUN_DIR/coverage_table" coverage_table.new
    echo -e "${YELLOW}MISRA coverage table doesn't match. Review coverage_table.new and update coverage_table.${NC}"
    exit 3
  fi
fi

cd $PANDA_DIR
if [ -z "${SKIP_BUILD}" ]; then
  scons
fi
scons compile_commands.json

CHECKLIST=$RUN_DIR/checkers.txt
echo "Cppcheck checkers list from test_misra.sh:" > $CHECKLIST

cppcheck() {
  # get all gcc defines: arm-none-eabi-gcc -dM -E - < /dev/null
  COMMON_DEFINES="-D__GNUC__=9 -UCMSIS_NVIC_VIRTUAL -UCMSIS_VECTAB_VIRTUAL -UPANDA_JUNGLE -UBOOTSTUB"

  # Whole-program analysis needs a build directory with parallel checking.
  # Never reuse cached results: cppcheck v2.13.0 can report inconsistent results.
  BUILD_DIR=$RUN_DIR/build
  mkdir -p "$BUILD_DIR"
  OUTPUT=$DIR/.output.log

  echo -e "\n\n\n\n\nTEST variant options:" >> $CHECKLIST
  echo -e ""${@//$PANDA_DIR/}"\n\n" >> $CHECKLIST # (absolute path removed)

  $CPPCHECK_DIR/cppcheck --inline-suppr -j4 --cppcheck-build-dir="$BUILD_DIR" \
          -I $PANDA_DIR \
          -I "$(arm-none-eabi-gcc -print-file-name=include)" \
          -I $OPENDBC_ROOT \
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

PANDA_OPTS="--enable=all --disable=unusedFunction --addon=misra"

printf "\n${GREEN}** PANDA H7 CODE **${NC}\n"
PANDA_SOURCE_LIST=$(python3 "$DIR/panda_sources.py")
PANDA_SOURCES=()
while IFS= read -r source; do
  PANDA_SOURCES+=("$source")
done <<< "$PANDA_SOURCE_LIST"
cppcheck $PANDA_OPTS -DSTM32H7 -DSTM32H725xx -I $PANDA_DIR/board/stm32h7/inc/ "${PANDA_SOURCES[@]}"

# unused needs to run globally
#printf "\n${GREEN}** UNUSED ALL CODE **${NC}\n"
#cppcheck --enable=unusedFunction --quiet $PANDA_DIR/board/

printf "\n${GREEN}Success!${NC} took $SECONDS seconds\n"

# ensure list of checkers is up to date
cd $DIR
if [ -z "$SKIP_TABLES_DIFF" ] && ! cmp -s "$DIR/checkers.txt" "$CHECKLIST"; then
  cp "$CHECKLIST" "$DIR/checkers.txt.new"
  echo -e "\n${YELLOW}Cppcheck report has changed. Review checkers.txt.new and update checkers.txt.${NC}"
  exit 4
fi
