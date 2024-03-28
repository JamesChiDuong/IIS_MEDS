SHELL = sh -xv

ifdef SRCDIR

VPATH = $(SRCDIR)

# Add your targets here
TARGETS = test.hex

all: $(TARGETS)

include config.mk

# For each target define a TARGETNAME_SRC, TARGETNAME_OBJ and define any
# additional dependencies for your the target TARGETNAME.elf file (just
# define the dependencies, a generic rule for .elf target exists in
# config.mk).
TEST_SRC = ref/test.c ref/meds.c ref/matrixmod.c ref/seed.c ref/util.c ref/bitstream.c ref/randombytes.c ref/osfreq.c ref/fips202.c
ifeq ($(TARGET),stm32f4)
  TEST_SRC += 
endif
TEST_OBJ = $(call objs,$(TEST_SRC))
test.elf: $(TEST_OBJ) libhal.a

CFLAGS += -DMEDS9923

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

.PHONY: $(OBJDIR)
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
