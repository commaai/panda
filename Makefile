# Bootstrap once, then parse the build rules with the project environment active.
.DEFAULT_GOAL := all
GOALS := $(if $(MAKECMDGOALS),$(MAKECMDGOALS),all)
NEEDS_SETUP := $(if $(filter-out clean help,$(GOALS)),$(if $(filter 1,$(PANDA_ENV_READY)),,1))

ifeq ($(NEEDS_SETUP),1)
.PHONY: $(GOALS) setup
$(GOALS): setup

setup:
	@bash -c 'source ./setup.sh; exec "$$@"' -- $(MAKE) --no-print-directory PANDA_ENV_READY=1 $(GOALS)
else
include board/build.mk
endif
