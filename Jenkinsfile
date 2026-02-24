def phone(String ip, String step_label, String cmd) {
  withCredentials([file(credentialsId: 'id_rsa', variable: 'key_file')]) {
    def ssh_cmd = """
ssh -tt -o StrictHostKeyChecking=no -o ConnectTimeout=30 -o ConnectionAttempts=3 -o ServerAliveInterval=10 -o ServerAliveCountMax=3 -i ${key_file} 'comma@${ip}' /usr/bin/bash <<'END'

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
export PYTHONPATH=${env.TEST_DIR}/../
export PYTHONWARNINGS=error
export LOGLEVEL=debug
ln -sf /data/openpilot/opendbc_repo/opendbc /data/opendbc

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
      retry (3) {
        def date = sh(script: 'date', returnStdout: true).trim()
        phone(device_ip, "set time", "date -s '${date}'")
        phone(device_ip, "git checkout", readFile("tests/setup_device_ci.sh"))
      }
      steps.each { item ->
        phone(device_ip, item[0], item[1])
      }
    }
  }
}

def docker_run(String step_label, int timeout_mins, String cmd) {
  timeout(time: timeout_mins, unit: 'MINUTES') {
    def branch = env.CHANGE_BRANCH ?: env.BRANCH_NAME
    sh script: "docker run --rm --privileged \
          --env PYTHONWARNINGS=error \
          --volume /dev/bus/usb:/dev/bus/usb \
          --volume /var/run/dbus:/var/run/dbus \
          --net host \
          python:3 \
          bash -c 'cd /tmp && git clone --depth=1 https://github.com/commaai/panda.git -b ${branch} panda && cd panda && PYTHONWARNINGS= ./setup.sh && . .venv/bin/activate && scons -j8 && ${cmd}'", \
        label: step_label
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
    timeout(time: 3, unit: 'HOURS')
    disableConcurrentBuilds(abortPrevious: env.BRANCH_NAME != 'master')
    skipDefaultCheckout()
  }

  stages {
    stage ('Acquire resource locks') {
      options {
        lock(resource: "pandas")
      }
      stages {
        stage('jungle tests') {
          steps {
            script {
              retry (3) {
                docker_run("reset hardware", 3, "python3 ./tests/hitl/reset_jungles.py")
              }
            }
          }
        }

        stage('parallel tests') {
          parallel {
            stage('test cuatro') {
              agent { docker { image 'ghcr.io/commaai/alpine-ssh'; args '--user=root' } }
              steps {
                sh 'rm -rf .venv pandacan.egg-info .eggs || true'
                script {
                  def scmVars = checkout scm
                  env.GIT_COMMIT = scmVars.GIT_COMMIT
                  env.GIT_BRANCH = scmVars.GIT_BRANCH
                }
                phone_steps("panda-cuatro", [
                  ["build", "scons -j4"],
                  ["flash", "cd scripts/ && ./reflash_internal_panda.py"],
                  ["flash jungle", "cd board/jungle && ./flash.py --all"],
                  ["test", "cd tests/hitl && pytest --durations=0 2*.py [5-9]*.py"],
                ])
              }
            }

            stage('test tres') {
              agent { docker { image 'ghcr.io/commaai/alpine-ssh'; args '--user=root' } }
              steps {
                sh 'rm -rf .venv pandacan.egg-info .eggs || true'
                script {
                  def scmVars = checkout scm
                  env.GIT_COMMIT = scmVars.GIT_COMMIT
                  env.GIT_BRANCH = scmVars.GIT_BRANCH
                }
                phone_steps("panda-tres", [
                  ["build", "scons -j4"],
                  ["flash", "cd scripts/ && ./reflash_internal_panda.py"],
                  ["flash jungle", "cd board/jungle && ./flash.py --all"],
                  ["test", "cd tests/hitl && pytest --durations=0 2*.py [5-9]*.py"],
                ])
              }
            }
          }
        }
      }
    }
  }
}
