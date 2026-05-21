#!/usr/bin/env python3
import re

import cppcheck


INTEGER_BOOL_LITERAL = re.compile(r"^[01]([uUlL]*)$")


def _is_external_token(token):
  if token is None or token.file is None:
    return False
  filename = token.file.replace("\\", "/")
  return "/.venv/" in filename or "/site-packages/" in filename


def _is_bool_token(token):
  if token is None:
    return False
  if token.valueType is not None and token.valueType.type == "bool":
    return True
  return token.variable is not None and token.variable.typeEndToken is not None and token.variable.typeEndToken.str == "bool"


def _is_integer_bool_literal(token):
  return (
    token is not None and
    token.isNumber and
    token.macroName not in ("true", "false") and
    INTEGER_BOOL_LITERAL.match(token.str) is not None
  )


@cppcheck.checker
def integer_literal_bool(cfg, data):
  for token in cfg.tokenlist:
    if token.str != "=":
      continue

    lhs = token.astOperand1
    rhs = token.astOperand2
    if not _is_external_token(rhs) and _is_bool_token(lhs) and _is_integer_bool_literal(rhs):
      cppcheck.reportError(rhs, "style", "use true/false for bool values", "integerLiteralBool")
