def docker_run(String step_label, int timeout_mins, String cmd) {
  timeout(time: timeout_mins, unit: 'MINUTES') {
    sh script: "docker run --rm --privileged \
          --env PARTIAL_TESTS=${env.PARTIAL_TESTS} \
          --volume /dev/bus/usb:/dev/bus/usb \
          --volume /var/run/dbus:/var/run/dbus \
          --workdir /tmp/openpilot/panda \
          --net host \
          ${env.DOCKER_IMAGE_TAG} \
          bash -c 'scons -j8 && ${cmd}'", \
        label: step_label
  }
}

pipeline {
  agent any
  environment {
    PARTIAL_TESTS = "${env.BRANCH_NAME == 'master' ? ' ' : '1'}"
    DOCKER_IMAGE_TAG = "panda:build-${env.GIT_COMMIT}"
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
        stage('Build Docker Image') {
          steps {
            timeout(time: 20, unit: 'MINUTES') {
              script {
                sh 'git archive -v -o panda.tar.gz --format=tar.gz HEAD'
                dockerImage = docker.build("${env.DOCKER_IMAGE_TAG}")
              }
            }
          }
        }
        stage('prep') {
          steps {
            script {
              docker_run("reset hardware", 3, "python ./tests/ci_reset_hw.py")
            }
          }
        }
        stage('pedal tests') {
          steps {
            script {
              docker_run("test pedal", 1, "PEDAL_JUNGLE=058010800f51363038363036 python ./tests/pedal/test_pedal.py")
            }
          }
        }
        stage('HITL tests') {
          steps {
            script {
              docker_run("HITL tests", 35, 'PANDAS_JUNGLE=23002d000851393038373731 PANDAS_EXCLUDE="1d0002000c51303136383232 2f002e000c51303136383232" ./tests/hitl/test.sh')
            }
          }
        }
        stage('CANFD tests') {
          steps {
            script {
              docker_run("CANFD tets", 6, 'JUNGLE=058010800f51363038363036 H7_PANDAS_EXCLUDE="080021000c51303136383232 33000e001051393133353939" ./tests/canfd/test_canfd.py')
            }
          }
        }
      }
    }
  }
}
