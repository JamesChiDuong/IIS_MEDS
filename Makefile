SHELL = sh -xv

ifdef SRCDIR

FILE = KAT_test_Serial_IO
VPATH = $(SRCDIR)

ROOT_PATH := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
# Add your targets here
TARGETS = $(FILE).hex

PARAM_OBJ = toy

all: params.h $(TARGETS) run
include config.mk

params.h:
	python $(ROOT_PATH)/include/params.py -p > $(ROOT_PATH)/include/params.h
	python $(ROOT_PATH)/include/params.py -a $(PARAM_OBJ) > $(ROOT_PATH)/include/api.h

run:
	@echo "Run the program"
	+@./$(FILE).elf
# For each target define a TARGETNAME_SRC, TARGETNAME_OBJ and define any
# additional dependencies for your the target TARGETNAME.elf file (just
# define the dependencies, a generic rule for .elf target exists in
# config.mk).
TEST_SRC = ref/$(FILE).c ref/meds.c ref/util.c ref/seed.c ref/osfreq.c ref/fips202.c ref/matrixmod.c ref/bitstream.c ref/randombytes.c  
ifeq ($(TARGET),stm32f4)
  TEST_SRC += 
endif
TEST_OBJ = $(call objs,$(TEST_SRC))
$(FILE).elf: $(TEST_OBJ) libhal.a

#CFLAGS += -DMEDS9923

CFLAGS += -D$(PARAM_OBJ)
LDLIBS += -lssl \
          -lcrypto

CPPFLAGS += -I$(SRCDIR)/include/NIST


# Don't forget to add all objects to the OBJ variable
OBJ += $(TEST_OBJ)

# Include generated dependencies
-include $(filter %.d,$(OBJ:.o=.d))

else
#######################################
# Out-of-tree build mechanism.        #
# You shouldn't touch anything below. #
#######################################
.SUFFIXES:

OBJDIR := build
$(OBJDIR): %:
	+@[ -d $@ ] || mkdir -p $@
	+@$(MAKE) --no-print-directory -r -I$(CURDIR) -C $@ -f $(CURDIR)/Makefile SRCDIR=$(CURDIR) $(MAKECMDGOALS)

Makefile : ;
%.mk :: ;
% :: $(OBJDIR) ;

.PHONY: clean
clean:
	rm -rf $(OBJDIR)

endif
