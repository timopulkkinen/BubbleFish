# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

#
# GNU Make based build file.  For details on GNU Make see:
#   http://www.gnu.org/software/make/manual/make.html
#
#


#
# Macros for TOOLS
#
# We use the C++ compiler for everything and then use the -Wl,-as-needed flag
# in the linker to drop libc++ unless it's actually needed.
#
HOST_CC?=gcc
HOST_CXX?=g++
HOST_LINK?=g++
HOST_LIB?=ar r

ifeq (,$(findstring gcc,$(shell $(WHICH) gcc)))
$(warning To skip the host build use:)
$(warning "make all_versions NO_HOST_BUILDS=1")
$(error Unable to find gcc in PATH while building Host build)
endif


LINUX_WARNINGS?=-Wno-long-long
LINUX_CCFLAGS=-fPIC -pthread $(LINUX_WARNINGS) -I$(NACL_SDK_ROOT)/include -I$(NACL_SDK_ROOT)/include/linux


#
# Individual Macros
#
# $1 = Source Name
# $2 = Compile Flags
#
define C_COMPILER_RULE
-include $(call SRC_TO_DEP,$(1))
$(call SRC_TO_OBJ,$(1)): $(1) $(TOP_MAKE) | $(dir $(call SRC_TO_OBJ,$(1)))dir.stamp
	$(call LOG,CC,$$@,$(HOST_CC) -o $$@ -c $$< -fPIC $(POSIX_FLAGS) $(2) $(LINUX_FLAGS))
endef

define CXX_COMPILER_RULE
-include $(call SRC_TO_DEP,$(1))
$(call SRC_TO_OBJ,$(1)): $(1) $(TOP_MAKE) | $(dir $(call SRC_TO_OBJ,$(1)))dir.stamp
	$(call LOG,CXX,$$@,$(HOST_CXX) -o $$@ -c $$< -fPIC $(POSIX_FLAGS) $(2) $(LINUX_FLAGS))
endef


# $1 = Source Name
# $2 = POSIX Compile Flags
# $3 = VC Flags (unused)
#
define COMPILE_RULE
ifeq ($(suffix $(1)),.c)
$(call C_COMPILER_RULE,$(1),$(2) $(foreach inc,$(INC_PATHS),-I$(inc)))
else
$(call CXX_COMPILER_RULE,$(1),$(2) $(foreach inc,$(INC_PATHS),-I$(inc)))
endif
endef


#
# SO Macro
#
# $1 = Target Name
# $2 = List of Sources
#
#
define SO_RULE
$(error 'Shared libraries not supported by Host')
endef


#
# LIB Macro
#
# $1 = Target Name
# $2 = List of Sources
#
#
define LIB_RULE
$(STAMPDIR)/$(1).stamp: $(NACL_SDK_ROOT)/lib/$(OSNAME)_host/$(CONFIG)/lib$(1).a
	@echo "TOUCHED $$@" > $(STAMPDIR)/$(1).stamp

all: $(NACL_SDK_ROOT)/lib/$(OSNAME)_host/$(CONFIG)/lib$(1).a
$(NACL_SDK_ROOT)/lib/$(OSNAME)_host/$(CONFIG)/lib$(1).a : $(foreach src,$(2),$(call SRC_TO_OBJ,$(src)))
	$(MKDIR) -p $$(dir $$@)
	$(call LOG,LIB,$$@,$(HOST_LIB) $$@ $$^)
endef


#
# Link Macro
#
# $1 = Target Name
# $2 = List of inputs
# $3 = List of libs
# $4 = List of deps
# $5 = List of lib dirs
# $6 = Other Linker Args
#
define LINKER_RULE
all: $(1)
$(1): $(2) $(foreach dep,$(4),$(STAMPDIR)/$(dep).stamp)
	$(call LOG,LINK,$$@,$(HOST_LINK) -shared -o $(1) $(2) $(NACL_LDFLAGS) $(foreach path,$(5),-L$(path)/$(OSNAME)_host)/$(CONFIG) $(foreach lib,$(3),-l$(lib)) $(6))
endef


#
# Link Macro
#
# $1 = Target Name
# $2 = List of Sources
# $3 = List of LIBS
# $4 = List of DEPS
# $5 = POSIX Linker Switches
# $6 = VC Linker Switches
#
define LINK_RULE
$(call LINKER_RULE,$(OUTDIR)/$(1)$(HOST_EXT),$(foreach src,$(2),$(call SRC_TO_OBJ,$(src))),$(filter-out pthread,$(3)),$(4),$(LIB_PATHS),$(5))
endef

all : $(LIB_LIST) $(DEPS_LIST)
