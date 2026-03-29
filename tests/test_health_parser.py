import tempfile

import pytest

from panda.python import _parse_c_struct


def test_parse_c_struct_rejects_unknown_field_types():
  with tempfile.NamedTemporaryFile(mode="w", suffix=".h") as f:
    f.write("struct __attribute__((packed)) health_t {\n  uint32_t ok;\n  bool nope;\n};\n")
    f.flush()
    with pytest.raises(ValueError, match="unsupported health_t layout"):
      _parse_c_struct(f.name, "health_t")
