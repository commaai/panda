import os
import subprocess
from setuptools import setup
from setuptools.command.build import build as _build

class build(_build):
    def run(self):
        subprocess.check_call(["scons", f"-j{os.cpu_count() or 4}"])
        super().run()

setup(cmdclass={"build": build})
