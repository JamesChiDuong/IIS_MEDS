ifndef _HAL
_HAL :=

export DEVICE ?=
export ROOT_PATH
ifeq ($(DEVICE),)
$(error No DEVICE specified for linker script generator)
endif

RETAINED_VARS += DEVICE

# include target.mk
###############
# HAL library #
###############

LIBHAL_SRC := \
	ref/hal/hal-opencm3.c \
	ref/include/randombytes.c \
	ref/include/bitstream.c \
	ref/include/matrixmod.c \
	ref/include/fips202.c \
	ref/include/osfreq.c \
	ref/include/seed.c \
	ref/include/util.c \
	ref/include/meds.c

LIBHAL_SRC_BUILD := \
	$(CURDIR)/ref/include/meds.c \
	$(CURDIR)/ref/include/util.c \
	$(CURDIR)/ref/include/seed.c \
	$(CURDIR)/ref/include/osfreq.c \
	$(CURDIR)/ref/include/fips202.c \
	$(CURDIR)/ref/include/matrixmod.c \
	$(CURDIR)/ref/include/bitstream.c \
	$(CURDIR)/ref/include/randombytes.c
LIBHAL_OBJ := $(call objs,$(LIBHAL_SRC))
LIBHAL_SRC_BUILD_OBJ := $(call objs,$(LIBHAL_SRC_BUILD))
libhal.a: $(LIBHAL_OBJ) 

LIBDEPS += libhal.a
LDLIBS += -lhal
######################
# libopencm3 Library #
######################

export OPENCM3_DIR := $(CURDIR)/libopencm3-master
libopencm3-stamp: ref/hal/libopencm3-master.tar.gz
	$(Q)tar xf $<
	@echo "  BUILD   libopencm3"
	$(Q)$(MAKE) -C $(OPENCM3_DIR) TARGETS=$(OPENCM3_TARGETS)
	$(Q)touch $@


$(OPENCM3_DIR)/mk/genlink-config.mk $(OPENCM3_DIR)/mk/genlink-rules.mk: libopencm3-stamp

include $(OPENCM3_DIR)/mk/genlink-config.mk
include $(OPENCM3_DIR)/mk/genlink-rules.mk

CROSS_PREFIX ?= arm-none-eabi
CC := $(CROSS_PREFIX)-gcc
CPP := $(CROSS_PREFIX)-cpp
AR := $(CROSS_PREFIX)-ar
LD := $(CC)
OBJCOPY := $(CROSS_PREFIX)-objcopy
SIZE := $(CROSS_PREFIX)-size

CFLAGS += \
	$(ARCH_FLAGS) \
	--specs=nano.specs \
	--specs=nosys.specs

LDFLAGS += \
	-Wl,--wrap=_read \
	-Wl,--wrap=_write \
	-L. \
	--specs=nano.specs \
	--specs=nosys.specs \
	-nostartfiles \
	-ffreestanding \
	-T$(LDSCRIPT) \
	$(ARCH_FLAGS)

LINKDEPS += $(LDSCRIPT) $(LIBDEPS)

OBJ += $(LIBHAL_OBJ)

endif
