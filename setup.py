import os
import subprocess
from setuptools import setup
from setuptools.command.build import build as _build

class build(_build):
    def run(self):
        env = os.environ.copy()
        env['FINAL_PROVISIONING'] = '1'

        project_root = os.path.abspath(os.path.dirname(__file__))

        print("Building firmware with scons...")
        
        subprocess.check_call(
            ["scons", "-j4"],
            cwd=project_root,
            env=env
        )

        super().run()

setup(
    cmdclass={"build": build},
)