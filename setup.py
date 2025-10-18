import os
import subprocess
from setuptools import setup, find_packages
from setuptools.command.build import build as _build


class build(_build):
    def run(self):
        env = os.environ.copy()

        project_root = os.path.abspath(os.path.dirname(__file__))

        print("Building firmware with scons...")

        subprocess.check_call(["scons", "-j4"], cwd=project_root, env=env)

        super().run()


setup(
    name="pandacan",
    version="0.0.10",
    packages=find_packages(),
    cmdclass={"build": build},
    package_data={"panda": ["py.typed"]},
    zip_safe=False,
)
