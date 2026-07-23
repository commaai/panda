#!/usr/bin/env python3
"""Wrapper around cppcheck's misra.py that exits on first violation.

Used by mutation tests to fail fast — we only need to know if ANY
violation exists, not enumerate them all. This saves ~10s per test
by killing the 54MB XML dump parsing early.
"""
import sys

# cppcheck prepends its addons/ dir to sys.path, so these are directly importable
import cppcheckdata
import misra

_original_reportError = misra.MisraChecker.reportError

def _failfast_reportError(self, location, num1, num2):
  ruleNum = num1 * 100 + num2

  # misra's own suppression checks
  if self.isRuleGloballySuppressed(ruleNum):
    return
  if self.settings.verify:
    return _original_reportError(self, location, num1, num2)
  if self.isRuleSuppressed(location.file, location.linenr, ruleNum):
    return _original_reportError(self, location, num1, num2)

  errorId = 'misra-c2012-%d.%d' % (num1, num2)

  # cppcheck's own suppression checks (file-pattern, line, block).
  # with --cli these are normally applied AFTER the addon, so we check here.
  if cppcheckdata.is_suppressed(location, '', errorId):
    return _original_reportError(self, location, num1, num2)

  # cppcheck-suppress-macro: suppresses a rule for all expansions of a macro.
  # is_suppressed doesn't handle type="macro", so check manually.
  if getattr(location, 'macroName', None):
    for s in cppcheckdata.current_dumpfile_suppressions:
      if s.suppressionType == 'macro' and s.errorId == errorId:
        return _original_reportError(self, location, num1, num2)

  # real violation — report it then bail out
  _original_reportError(self, location, num1, num2)
  sys.exit(1)

misra.MisraChecker.reportError = _failfast_reportError
misra.main()
