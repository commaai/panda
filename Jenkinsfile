def docker_run(String cmd) {
  timeout(time: 30, unit: 'MINUTES') {
    sh "docker run --rm --privileged \
          --env PARTIAL_TESTS=${env.PARTIAL_TESTS} \
          --volume ${WORKSPACE.replace('/var/jenkins_home', '/data/docker/volumes/jenkins_jenkins-data/_data')}:/tmp/openpilot/panda \
          --volume /dev/bus/usb:/dev/bus/usb \
          --volume /var/run/dbus:/var/run/dbus \
          --workdir /tmp/openpilot/panda \
          --net host \
          ${env.DOCKER_IMAGE_TAG} \
          bash -c 'pwd && whoami && ls -la && ls -la .. && scons -j8 && ${cmd}'"
  }
}

pipeline {
  agent any
  environment {
    PARTIAL_TESTS = "${env.BRANCH_NAME == 'master' ? ' ' : '1'}"
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
                dockerImage = docker.build("${env.DOCKER_IMAGE_TAG}")
              }
            }
          }
        }
        stage('reset hardware') {
          steps {
            script {
              docker_run("python ./tests/ci_reset_hw.py")
            }
          }
        }
        stage('pedal tests') {
          steps {
            script {
              docker_run("PEDAL_JUNGLE=058010800f51363038363036 python ./tests/pedal/test_pedal.py")
            }
          }
        }
        stage('HITL tests') {
          steps {
            script {
              docker_run("PANDAS_JUNGLE=23002d000851393038373731 PANDAS_EXCLUDE=\"1d0002000c51303136383232 2f002e000c51303136383232\" ./tests/hitl/test.sh")
            }
          }
        }
        stage('CANFD tests') {
          steps {
            script {
              docker_run("scons -j8 && JUNGLE=058010800f51363038363036 H7_PANDAS_EXCLUDE=\"080021000c51303136383232 33000e001051393133353939\" ./tests/canfd/test_canfd.py")
            }
          }
        }
      }

      post {
        always {
          sh "rm -rf ${WORKSPACE} || true"
        }
      }
    }
  }
}
