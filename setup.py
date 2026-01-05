from setuptools import setup
from setuptools.command.build_py import build_py
import subprocess
import os
import shutil

class BuildPyWithFW(build_py):
    def run(self):
        # Check if ARM toolchain is available
        toolchain_available = self._check_toolchain()

        if toolchain_available:
            print("ARM toolchain found, building firmware...")
            try:
                result = subprocess.run(['scons'], cwd=os.path.dirname(__file__),
                                      capture_output=True, text=True, timeout=300)
                if result.returncode != 0:
                    print("SCons build failed:")
                    print("STDOUT:", result.stdout)
                    print("STDERR:", result.stderr)
                    print("Warning: Firmware build failed, but continuing with package build.")
                    print("Note: The package will not include firmware binaries.")
                    print("To include firmware, install ARM GCC toolchain and rebuild.")
                else:
                    print("Firmware build completed successfully.")
            except subprocess.TimeoutExpired:
                print("Firmware build timed out after 5 minutes.")
                print("Continuing with package build without firmware.")
            except FileNotFoundError:
                print("SCons not found. Install with: pip install scons")
                print("Continuing with package build without firmware.")
        else:
            print("ARM GCC toolchain not found.")
            print("Continuing with package build without firmware binaries.")
            print("To include firmware, install arm-none-eabi-gcc and rebuild.")

        super().run()

    def _check_toolchain(self):
        """Check if ARM GCC toolchain is available."""
        return shutil.which('arm-none-eabi-gcc') is not None

setup(cmdclass={'build_py': BuildPyWithFW})

