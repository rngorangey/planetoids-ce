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

# reverse the comment when displaying floats in dbg_printf is needed
HAS_PRINTF ?= NO
#USE_FLASH_FUNCTIONS ?= NO	

# ----------------------------

ifndef CEDEV
$(error CEDEV environment path variable is not set)
endif

include $(CEDEV)/meta/makefile.mk
