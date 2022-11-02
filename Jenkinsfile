pipeline {
  agent any
  environment {
    DOCKER_IMAGE_TAG = "panda:build-${env.GIT_COMMIT}"
  }
  stages {
    stage ('Acquire resource locks') {
      options {
        lock(resource: "pandas")
      }
      stages {
        stage('Build Docker Image') {
          steps {
            timeout(time: 60, unit: 'MINUTES') {
              script {
                sh 'git archive -v -o panda.tar.gz --format=tar.gz HEAD'
                dockerImage = docker.build("${env.DOCKER_IMAGE_TAG}")
              }
            }
          }
        }
        stage('reset hardware') {
          steps {
            timeout(time: 10, unit: 'MINUTES') {
              script {
                sh "docker run --rm --privileged \
                      --volume /dev/bus/usb:/dev/bus/usb \
                      --volume /var/run/dbus:/var/run/dbus \
                      --net host \
                      ${env.DOCKER_IMAGE_TAG} \
                      bash -c 'cd /tmp/panda && scons -j8 && python ./tests/ci_reset_hw.py'"
              }
            }
          }
        }
        stage('pedal tests') {
          steps {
            timeout(time: 10, unit: 'MINUTES') {
              script {
                sh "docker run --rm --privileged \
                      --volume /dev/bus/usb:/dev/bus/usb \
                      --volume /var/run/dbus:/var/run/dbus \
                      --net host \
                      ${env.DOCKER_IMAGE_TAG} \
                      bash -c 'cd /tmp/panda && PEDAL_JUNGLE=058010800f51363038363036 python ./tests/pedal/test_pedal.py'"
              }
            }
          }
        }
        stage('HITL tests') {
          steps {
            timeout(time: 30, unit: 'MINUTES') {
              script {
                sh "docker run --rm --privileged \
                      --volume /dev/bus/usb:/dev/bus/usb \
                      --volume /var/run/dbus:/var/run/dbus \
                      --net host \
                      ${env.DOCKER_IMAGE_TAG} \
                      bash -c 'cd /tmp/panda && scons -j8 && PANDAS_JUNGLE=23002d000851393038373731 PANDAS_EXCLUDE=\"1d0002000c51303136383232 2f002e000c51303136383232\" ./tests/automated/test.sh'"
              }
            }
          }
        }
        stage('CANFD tests') {
          steps {
            timeout(time: 10, unit: 'MINUTES') {
              script {
                sh "docker run --rm --privileged \
                      --volume /dev/bus/usb:/dev/bus/usb \
                      --volume /var/run/dbus:/var/run/dbus \
                      --net host \
                      ${env.DOCKER_IMAGE_TAG} \
                      bash -c 'cd /tmp/panda && scons -j8 && JUNGLE=058010800f51363038363036 H7_PANDAS_EXCLUDE=\"080021000c51303136383232 33000e001051393133353939\" ./tests/canfd/test_canfd.py'"
              }
            }
          }
        }
      }
    }
  }
}
