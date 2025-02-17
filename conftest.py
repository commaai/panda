import unittest


def pytest_pycollect_makeitem(collector, name, obj):
  if isinstance(obj, type) and issubclass(obj, unittest.TestCase):
    abstract_methods = getattr(obj, '__abstractmethods__', None)
    if abstract_methods:
      raise TypeError(f"Test class {name} is abstract and missing implementations for: {abstract_methods}")
