#!/usr/bin/env bash
set -eo pipefail

source ~/.bash_profile
set -euo pipefail

up() {
  uvx -q --refresh-package comma-ultimate-provisioning \
    --from 'git+https://github.com/commaai/hardware@master#subdirectory=ultimate_provisioning' \
    ultimate_provisioning "$@"
}
trap 'exit 130' INT
trap 'exit 143' HUP TERM
trap 'up release' EXIT

phone() {
  {
    cat <<'PROFILE'
source ~/.bash_profile
if [ -f /etc/profile ]; then
  source /etc/profile
fi
export CI=1 PYTHONWARNINGS=error LOGLEVEL=debug PYTEST_ADDOPTS='-p no:asyncio'
unset PYTHONPATH
PROFILE
    printf 'export TEST_DIR=%q SOURCE_DIR=%q GIT_COMMIT=%q GIT_BRANCH=%q\n' "$TEST_DIR" "$SOURCE_DIR" "$GIT_COMMIT" "$GIT_BRANCH"
    cat
  } | ssh -T -oBatchMode=yes -oStrictHostKeyChecking=no -oUserKnownHostsFile=/dev/null \
      -oConnectTimeout=30 -oConnectionAttempts=3 -oServerAliveInterval=10 -oServerAliveCountMax=3 \
      "comma@$dut_ip" bash -se
}

ready=0
for attempt in 1 2 3; do
  if dut_ip=$(up ready | tail -n1) && {
    printf 'date -s %q\n' "$(date)"
    cat tests/setup_device_ci.sh
  } | phone; then
    ready=1
    break
  fi
  echo "Device setup attempt $attempt failed" >&2
done
[[ $ready == 1 ]]

up serve
phone <<'DUT'
cd "$TEST_DIR"
source .venv/bin/activate
scons
(cd scripts && ./reflash_internal_panda.py)
cd tests/hitl
PYTHONPATH=/data/ultimate_provisioning HITL=1 python -m unittest -v test_2*.py test_[5-9]*.py
DUT
