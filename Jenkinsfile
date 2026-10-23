// the DUTs are plugged into ultimate provisioning (UP) boards, which stand in for the jungles.
// a DUT is only reachable through its UP, over the USB network between them
// the UP library, fetched on demand. uv's caches go on /data, the UP's /home is tiny
UP = "UV_CACHE_DIR=/data/uv_cache UV_TOOL_DIR=/data/uv_tools uvx -q --refresh-package comma-ultimate-provisioning --from git+https://github.com/commaai/hardware@master#subdirectory=ultimate_provisioning ultimate_provisioning"

def up(String ip, String step_label, String cmd, boolean returnStdout = false) {
  withCredentials([file(credentialsId: 'id_rsa', variable: 'key_file')]) {
    return sh(label: step_label, returnStdout: returnStdout, script: """
ssh -o StrictHostKeyChecking=no -o ConnectTimeout=30 -i ${key_file} 'comma@${ip}' /usr/bin/bash <<'END'
set -e
${cmd}
END""")
  }
}

def phone(String ip, String dut_ip, String step_label, String cmd) {
  withCredentials([file(credentialsId: 'id_rsa', variable: 'key_file')]) {
    // every DUT has the same IP behind its UP, so don't keep their host keys
    def ssh_cmd = """
ssh -tt -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o ConnectTimeout=30 -o ConnectionAttempts=3 -o ServerAliveInterval=10 -o ServerAliveCountMax=3 -o ProxyCommand='ssh -o StrictHostKeyChecking=no -i ${key_file} -W %h:%p comma@${ip}' -i ${key_file} 'comma@${dut_ip}' /usr/bin/bash <<'END'

set -e


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

def phone_steps(String device_type, steps) {
  lock(resource: "", label: device_type, inversePrecedence: true, variable: 'device_ip', quantity: 1) {
    timeout(time: 20, unit: 'MINUTES') {
      def dut_ip
      retry (3) {
        // ready prints the DUT's IP on the UP's USB network
        dut_ip = up(device_ip, "UP: ready", "${UP} ready", true).trim().readLines().last()
        def date = sh(script: 'date', returnStdout: true).trim()
        phone(device_ip, dut_ip, "set time", "date -s '${date}'")
        phone(device_ip, dut_ip, "git checkout", readFile("tests/setup_device_ci.sh"))
      }
      // serve the UP's harness and CAN to the tests on the DUT, see tests/hitl/base.py
      up(device_ip, "UP: serve", "${UP} serve")
      try {
        steps.each { item ->
          phone(device_ip, dut_ip, item[0], item[1])
        }
      } finally {
        up(device_ip, "UP: release", "${UP} release")
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
    stage ('Acquire resource locks') {
      options {
        lock(resource: "pandas")
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

        stage('parallel tests') {
          parallel {
            stage('test cuatro') {
              agent { docker { image 'ghcr.io/commaai/alpine-ssh'; args '--user=root'; reuseNode true } }
              steps {
                phone_steps("up-mici", [
                  ["build", "scons"],
                  ["flash", "cd scripts/ && ./reflash_internal_panda.py"],
                  ["test", "cd tests/hitl && PYTHONPATH=/data/ultimate_provisioning HITL=1 python -m unittest -v test_2*.py test_[5-9]*.py"],
                ])
              }
            }

            stage('test tres') {
              agent { docker { image 'ghcr.io/commaai/alpine-ssh'; args '--user=root'; reuseNode true } }
              steps {
                phone_steps("up-tres", [
                  ["build", "scons"],
                  ["flash", "cd scripts/ && ./reflash_internal_panda.py"],
                  ["test", "cd tests/hitl && PYTHONPATH=/data/ultimate_provisioning HITL=1 python -m unittest -v test_2*.py test_[5-9]*.py"],
                ])
              }
            }

          }
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
