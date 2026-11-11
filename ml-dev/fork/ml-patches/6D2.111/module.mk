# The film OS (src/filmroll.c, src/fos_*.c) as ML/modules/filmos.mo, for the 6D Mark II 1.1.1 only:
# the sources stay in src/, compiled here with the camera's definitions. The maths (powf, exp2f ...)
# and libgcc's helpers are autoexec.bin's, called through its symbol file (6D2_111.sym), not linked in.
MODULE_NAME = filmos
FILMOS_SRC = filmroll fos_gfx fos_film fos_ui fos_shelf fos_pages fos_hud fos_print fos_dark fos_dev fos_lab fos_looks fos_jpeg
MODULE_OBJS = $(addprefix $(BUILD_DIR)/,$(addsuffix .o,$(FILMOS_SRC)))
TOP_DIR = ../..
PLAT = $(TOP_DIR)/platform/6D2.111

include $(TOP_DIR)/modules/Makefile
.DEFAULT_GOAL := $(BUILD_DIR)/module_complete

vpath %.c $(TOP_DIR)/src
CFLAGS += -I$(PLAT) -I$(PLAT)/include -I$(TOP_DIR)/src \
    -DCONFIG_MAGICLANTERN=1 -DCONFIG_6D2=1 -DCONFIG_6D2_111=1 -DCONFIG_FW_VERSION=111 -DCAMERA_MODEL=\"6D2\" \
    -DCONFIG_MODULES -DCONFIG_CONSOLE -DCONFIG_TCC \
    -fno-unwind-tables -fno-asynchronous-unwind-tables

# the 6D Mark II's Cortex-A9, not the ARMv5 of ML's other modules: ARM mode (the loader's relocations),
# addresses from literal pools rather than movw/movt (ML's TCC doesn't take those relocations)
CFLAGS += -march=armv7-a -mtune=cortex-a9 -marm -mword-relocations
