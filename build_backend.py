#!/usr/bin/env python3
import os
import subprocess
import shutil
from setuptools import build_meta as _orig
from setuptools.command.build_py import build_py


class BuildPyCommand(build_py):
    """Custom build command that builds firmware before packaging."""

    def run(self):
        # Build firmware
        base_dir = os.path.dirname(os.path.abspath(__file__))
        board_dir = os.path.join(base_dir, "board")

        print("Building Panda firmware...")
        try:
            # Check if scons is available
            if shutil.which("scons") is None:
                print("WARNING: scons not found, skipping firmware build")
                print("Install scons to build firmware: pip install scons")
            else:
                # Build the firmware
                result = subprocess.run(
                    ["scons", "-C", base_dir, "-j", str(os.cpu_count() or 1), board_dir],
                    capture_output=True,
                    text=True
                )
                if result.returncode != 0:
                    print(f"WARNING: Firmware build failed: {result.stderr}")
                    print("Continuing with package build anyway...")
                else:
                    print("Firmware built successfully")
        except Exception as e:
            print(f"WARNING: Failed to build firmware: {e}")
            print("Continuing with package build anyway...")

        # Run the original build_py command
        super().run()


# Expose all the build_meta functions
prepare_metadata_for_build_wheel = _orig.prepare_metadata_for_build_wheel
build_wheel = _orig.build_wheel
build_sdist = _orig.build_sdist
get_requires_for_build_wheel = _orig.get_requires_for_build_wheel
get_requires_for_build_sdist = _orig.get_requires_for_build_sdist


# Override the build command
def _build_with_firmware(config_settings=None):
    """Build wheel with firmware compilation."""
    # Temporarily monkey-patch the build_py command
    from setuptools import setup
    from setuptools.dist import Distribution

    # Create a custom distribution that uses our build command
    class CustomDistribution(Distribution):
        def get_command_class(self, command):
            if command == 'build_py':
                return BuildPyCommand
            return super().get_command_class(command)

    # Patch setuptools to use our custom distribution
    _orig_distribution = setup.Distribution
    setup.Distribution = CustomDistribution

    try:
        # Build the wheel
        return _orig.build_wheel(config_settings)
    finally:
        # Restore original distribution
        setup.Distribution = _orig_distribution


# Override build_wheel to include firmware building
build_wheel = _build_with_firmware