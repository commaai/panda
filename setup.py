"""Build hook for panda firmware compilation."""
import os
import subprocess
from distutils.errors import DistutilsExecError
from setuptools import setup
from setuptools.command.build import build
from setuptools import Command

REPO_ROOT = os.path.dirname(os.path.abspath(__file__))

REQUIRED_OUTPUTS = [
    "board/obj/panda_h7.bin.signed",
    "board/obj/bootstub.panda_h7.bin",
    "board/obj/panda_jungle_h7.bin.signed",
    "board/obj/bootstub.panda_jungle_h7.bin",
    "board/obj/body_h7.bin.signed",
    "board/obj/bootstub.body_h7.bin",
]


class BuildFirmware(Command):
    """Compile panda firmware using SCons."""

    description = "compile panda firmware"
    user_options = [
        ("jobs=", "j", "number of parallel jobs"),
        ("release", "r", "build release firmware (default: debug)"),
    ]
    editable_mode = False
    build_lib = None

    def initialize_options(self):
        self.jobs = None
        self.release = False

    def finalize_options(self):
        if self.jobs is None:
            self.jobs = os.cpu_count() or 1
        self.set_undefined_options("build_py", ("build_lib", "build_lib"))

    def run(self):
        cmd = ["scons", "-j", str(self.jobs)]
        env = os.environ.copy()
        if self.release:
            env["RELEASE"] = "1"
            print(f"Building RELEASE firmware")
        else:
            print(f"Building DEBUG firmware")
        print(f"Running: {' '.join(cmd)} in {REPO_ROOT}")
        result = subprocess.run(cmd, cwd=REPO_ROOT, env=env)
        if result.returncode != 0:
            raise DistutilsExecError(f"SCons firmware build failed (exit {result.returncode})")

        missing = [f for f in REQUIRED_OUTPUTS if not os.path.exists(os.path.join(REPO_ROOT, f))]
        if missing:
            raise DistutilsExecError(f"Missing firmware outputs: {missing}")

    def get_source_files(self):
        return []

    def get_outputs(self):
        if self.build_lib is None:
            return []
        return [os.path.join(self.build_lib, "panda", f) for f in REQUIRED_OUTPUTS]

    def get_output_mapping(self):
        if self.build_lib is None:
            return {}
        return {
            os.path.join(self.build_lib, "panda", f): os.path.join(REPO_ROOT, f)
            for f in REQUIRED_OUTPUTS
        }


class CustomBuild(build):
    sub_commands = [("build_firmware", lambda self: True)] + build.sub_commands


setup(
    cmdclass={
        "build": CustomBuild,
        "build_firmware": BuildFirmware,
    }
)
