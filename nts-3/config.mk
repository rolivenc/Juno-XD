##############################################################################
# Configuration for Makefile (NTS-3 kaoss pad kit, genericfx)
#

PROJECT := bbd_chorus
PROJECT_TYPE := genericfx

##############################################################################
# Sources
#

# C sources
UCSRC = header.c

# C++ sources
UCXXSRC = unit.cc

# List ASM source files here
UASMSRC =

UASMXSRC =

##############################################################################
# Include Paths
#

# Repository root, for the shared DSP core (bbd_chorus_dsp.h)
UINCDIR = $(SHARED_SRC_PATH)

##############################################################################
# Library Paths
#

ULIBDIR =

##############################################################################
# Libraries
#

ULIBS  = -lm

##############################################################################
# Macros
#

# Add -DBBD_MODE_GLIDE=0 to switch modes instantly (no depth glide)
UDEFS =
