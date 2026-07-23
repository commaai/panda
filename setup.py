import os
import platform
import subprocess

from setuptools import setup
from setuptools.command.build_py import build_py


class BuildPyWithFirmware(build_py):
  def run(self):
    # Build the firmware into panda/fw/ so it ships with the package.
    # Skipped for editable installs, where scons is run directly,
    # and on Windows, where the toolchain isn't available.
    if not getattr(self, "editable_mode", False) and platform.system() != "Windows":
      subprocess.check_call(["scons", f"-j{os.cpu_count()}", "panda/fw"], cwd=os.path.dirname(os.path.abspath(__file__)))
    super().run()


setup(cmdclass={"build_py": BuildPyWithFirmware})
