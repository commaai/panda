# alfred sources this at the top of every CI step, on the UP and on the DUT
unset PYTHONPATH VIRTUAL_ENV
export UV_PROJECT_ENVIRONMENT="$WORKSPACE.venv"
if [ -f "$UV_PROJECT_ENVIRONMENT/bin/activate" ]; then source "$UV_PROJECT_ENVIRONMENT/bin/activate"; fi
