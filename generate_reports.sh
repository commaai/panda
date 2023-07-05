python3 -m pip install lxml
mypy --html-report ./mypy_after/html-report --any-exprs-report ./mypy_after --linecount-report ./mypy_after --lineprecision-report ./mypy_after --txt-report ./mypy_after/txt-report
