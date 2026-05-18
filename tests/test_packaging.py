import tomllib
from pathlib import Path


def test_firmware_artifacts_are_packaged():
  pyproject = tomllib.loads(Path("pyproject.toml").read_text())
  package_data = pyproject["tool"]["setuptools"]["package-data"]

  board_data = package_data["panda.board"]
  assert "obj/*.bin" in board_data
  assert "obj/*.bin.signed" in board_data
  assert "obj/version" in board_data


def test_firmware_artifacts_are_in_sdist_manifest():
  manifest = Path("MANIFEST.in").read_text()

  assert "recursive-include board/obj *.bin *.bin.signed version" in manifest
