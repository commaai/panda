#!/usr/bin/env bash
set -eo pipefail
source ~/.bash_profile
set -euo pipefail

# alfred checks the commit out here on the UP and pushes it to the same path on the DUT
export TEST_DIR=$PWD

up() {
  uvx -q --from 'git+https://github.com/commaai/hardware@e02dc9f#subdirectory=ultimate_provisioning' ultimate_provisioning "$@"
}
trap 'exit 130' INT
trap 'exit 143' HUP TERM
cleanup() {
  local rc=$?
  trap - EXIT
  set +e
  # Wait for the DUT shell's cleanup before releasing the UP for another job.
  if [[ -n ${dut_ip:-} ]] && ! phone_ssh 'flock /tmp/alfred.lock true'; then
    rc=1
    up off
  fi
  up release || rc=$?
  exit "$rc"
}
trap 'cleanup >/tmp/alfred-cleanup.log 2>&1' EXIT

ready() {
  local attempt
  for attempt in 1 2 3; do
    if dut_ip=$(up ready | tail -n1); then return; fi
    echo "Device setup attempt $attempt failed" >&2
  done
  return 1
}

phone_ssh() {
  ssh -T -oBatchMode=yes -oStrictHostKeyChecking=no -oUserKnownHostsFile=/dev/null \
      -oConnectTimeout=30 -oConnectionAttempts=3 -oServerAliveInterval=10 -oServerAliveCountMax=3 \
      "comma@$dut_ip" "$@"
}

# A waiting supervisor handles disconnects immediately, including while a test is running.
# Keep the lock until the child has finished its EXIT cleanup.
supervisor="trap '' PIPE; trap ':' TERM; trap 'trap - HUP; kill -TERM 0; wait \"\$child\"; exit 143' HUP; bash -se <&0 & child=\$!; wait \"\$child\""

phone() {
  {
    cat <<'PROFILE'
source ~/.bash_profile
if [ -f /etc/profile ]; then source /etc/profile; fi
set -euo pipefail
export CI=1 PYTHONWARNINGS=error LOGLEVEL=debug PYTEST_ADDOPTS='-p no:asyncio'
unset PYTHONPATH VIRTUAL_ENV
PROFILE
    printf 'export TEST_DIR=%q UV_PROJECT_ENVIRONMENT=%q\n' "$TEST_DIR" "${TEST_DIR}.venv"
    cat
  } | phone_ssh "exec setpriv --pdeathsig HUP flock --no-fork /tmp/alfred.lock bash -c $(printf %q "$supervisor")"
}

ready
up serve
phone <<'DUT'
install() {
  # Bypass AGNOS's root-only uv shim.
  local uv
  uv=$(type -ap uv | grep -vF /usr/comma/shims | head -n1)
  PYTHONWARNINGS=default "$uv" sync --cache-dir=/data/uv_cache --all-extras --upgrade-package opendbc
  "$uv" cache prune --cache-dir=/data/uv_cache
  source "$UV_PROJECT_ENVIRONMENT/bin/activate"
}

build() {
  scons
  (cd scripts && ./reflash_internal_panda.py)
}

test() {
  cd tests/hitl
  PYTHONPATH=/data/ultimate_provisioning HITL=1 python -m unittest -v test_2*.py test_[5-9]*.py
}

cd "$TEST_DIR"
install
build
test
DUT
