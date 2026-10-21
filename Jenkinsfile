// the DUTs sit on ultimate provisioning (UP) boards, which stand in for the jungle.
// DUTs are reached through their UP, over the USB network between them
UP_DUT_IP = "10.254.254.2"
HARDWARE_REF = "ultimate-provisioning"
SSH_OPTS = "-o ControlMaster=no -o ControlPath=none -o ConnectTimeout=5 -o ServerAliveInterval=5 -o ServerAliveCountMax=12 -o BatchMode=yes -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null"

def upHost(String ip, String step_label, String cmd) {
  withCredentials([file(credentialsId: 'id_rsa', variable: 'key_file')]) {
    sh label: step_label, script: """
ssh ${SSH_OPTS} -i ${key_file} 'comma@${ip}' /usr/bin/bash <<'END'
set -e
source /etc/profile
${cmd}
END"""
  }
}

def upSetup(String ip) {
  upHost(ip, "UP: setup DUT", """
if [ ! -d /data/hardware ]; then
  git clone --depth 1 --filter=blob:none --sparse https://github.com/commaai/hardware.git /data/hardware
  git -C /data/hardware sparse-checkout set ultimate_provisioning
fi
git -C /data/hardware fetch --depth 1 origin ${HARDWARE_REF}
git -C /data/hardware checkout -qf FETCH_HEAD

PYTHONPATH=/data/hardware/ultimate_provisioning python3 -m ultimate_provisioning ready
""")
}

// serve the UP's harness and CAN to the tests on the DUT, see tests/hitl/up.py
def upStart(String ip) {
  upHost(ip, "UP: start", "PYTHONPATH=/data/hardware/ultimate_provisioning python3 -m ultimate_provisioning jungle --serve")
}

def upStop(String ip) {
  upHost(ip, "UP: stop", "PYTHONPATH=/data/hardware/ultimate_provisioning python3 -m ultimate_provisioning release")
}

// run on the DUT, through its UP
def device(String ip, String step_label, String cmd) {
  withCredentials([file(credentialsId: 'id_rsa', variable: 'key_file')]) {
    def ssh_cmd = """
ssh ${SSH_OPTS} -o ProxyCommand='ssh ${SSH_OPTS} -i ${key_file} -W %h:%p comma@${ip}' -i ${key_file} 'comma@${UP_DUT_IP}' exec setpriv --pdeathsig HUP /usr/bin/bash <<'END'

set -e

trap 'kill 0' HUP # stop this process group on SSH disconnect

source ~/.bash_profile
if [ -f /etc/profile ]; then
  source /etc/profile
fi

export CI=1
export TEST_DIR=${env.TEST_DIR}
export SOURCE_DIR=${env.SOURCE_DIR}
export GIT_BRANCH=${env.GIT_BRANCH}
export GIT_COMMIT=${env.GIT_COMMIT}
export PYTHONWARNINGS=error
export LOGLEVEL=debug

unset PYTHONPATH
if [ -f "${env.TEST_DIR}/.venv/bin/activate" ]; then
  source "${env.TEST_DIR}/.venv/bin/activate"
fi

# TODO: this is an agnos issue
export PYTEST_ADDOPTS="-p no:asyncio"

cd ${env.TEST_DIR} || true
${cmd}
exit 0

END"""

    sh script: ssh_cmd, label: step_label
  }
}

def device_steps(steps) {
  lock(resource: "", label: "up", inversePrecedence: true, variable: 'up_ip', quantity: 1, resourceSelectStrategy: 'random') {
    timeout(time: 20, unit: 'MINUTES') {
      retry (3) {
        upSetup(up_ip)
        def date = sh(script: 'date', returnStdout: true).trim()
        device(up_ip, "set time", "date -s '${date}'")
        device(up_ip, "git checkout", readFile("tests/setup_device_ci.sh"))
      }
      try {
        upStart(up_ip)
        steps.each { item ->
          device(up_ip, item[0], item[1])
        }
      } finally {
        upStop(up_ip)
      }
    }
  }
}



pipeline {
  agent any
  environment {
    CI = "1"
    PYTHONWARNINGS= "error"

    TEST_DIR = "/data/panda"
    SOURCE_DIR = "/data/panda_source/"
  }
  options {
    skipDefaultCheckout()
    timeout(time: 3, unit: 'HOURS')
    disableConcurrentBuilds(abortPrevious: env.BRANCH_NAME != 'master')
  }

  stages {
    stage('Setup') {
      steps {
        timeout(time: 20, unit: 'MINUTES') {
          deleteDir()
          script {
            def revision = checkout scm
            env.GIT_COMMIT = revision.GIT_COMMIT
            env.GIT_BRANCH = revision.GIT_BRANCH
          }
        }
      }
    }

    stage('test cuatro') {
      agent { docker { image 'ghcr.io/commaai/alpine-ssh'; args '--user=root'; reuseNode true } }
      steps {
        script {
          device_steps([
            ["build", "scons"],
            ["flash", "cd scripts/ && ./reflash_internal_panda.py"],
            ["test", "cd tests/hitl && HITL=1 python -m unittest -v test_2*.py test_[5-9]*.py"],
          ])
        }
      }
    }
  }
  post {
    always {
      deleteDir()
    }
  }
}
