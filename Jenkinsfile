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



pipeline {
  agent any
  environment {
    CI = "1"

    TEST_DIR = "/data/panda"
    SOURCE_DIR = "/data/panda_source/"
  }
  options {
    timeout(time: 3, unit: 'HOURS')
    disableConcurrentBuilds(abortPrevious: env.BRANCH_NAME != 'master')
  }

  stages {
    stage ('Acquire resource locks') {
      options {
        lock(resource: "pandas")
      }
      stages {
        stage('jungle tests') {
          agent {
            docker {
              image 'python:3.12'
              args '--user=root --privileged --volume /dev/bus/usb:/dev/bus/usb --volume /var/run/dbus:/var/run/dbus --net host'
              reuseNode true
            }
          }
          steps {
            timeout(time: 10, unit: 'MINUTES') {
              retry (3) {
                sh './setup.sh && . .venv/bin/activate && export PYTHONWARNINGS=error && scons && python3 ./tests/hitl/reset_jungles.py'
              }
            }
          }
          post {
            always {
              sh 'chmod -R 777 . || true'
            }
          }
        }

        stage('parallel tests') {
          parallel {
            stage('test cuatro') {
              agent { docker { image 'ghcr.io/commaai/alpine-ssh'; args '--user=root'; reuseNode true } }
              steps {
                phone_steps("panda-cuatro", [
                  ["build", "scons"],
                  ["flash", "cd scripts/ && ./reflash_internal_panda.py"],
                  ["flash jungle", "cd board/jungle && ./flash.py --all"],
                  ["test", "cd tests/hitl && pytest --durations=0 2*.py [5-9]*.py"],
                ])
              }
            }

            stage('test tres') {
              agent { docker { image 'ghcr.io/commaai/alpine-ssh'; args '--user=root'; reuseNode true } }
              steps {
                phone_steps("panda-tres", [
                  ["build", "scons"],
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
