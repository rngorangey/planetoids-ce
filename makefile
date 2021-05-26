# ----------------------------
# Makefile Options
# ----------------------------

NAME ?= PLANET
ICON ?=
DESCRIPTION ?= 
COMPRESSED ?= NO
ARCHIVED ?= NO

CFLAGS ?= -Wall -Wextra -Oz
CXXFLAGS ?= -Wall -Wextra -Oz

HAS_PRINTF := NO

# ----------------------------

ifndef CEDEV
$(error CEDEV environment path variable is not set)
endif

include $(CEDEV)/meta/makefile.mk
