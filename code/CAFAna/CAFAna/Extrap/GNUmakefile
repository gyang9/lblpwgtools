include SoftRelTools/arch_spec_root.mk

LIB_TYPE    := shared
LIB         := lib$(PACKAGE)Extrap
LIBCXXFILES := $(wildcard *.cxx)

LIBLINK    := -L$(SRT_PRIVATE_CONTEXT)/lib/$(SRT_SUBDIR) -L$(SRT_PUBLIC_CONTEXT)/lib/$(SRT_SUBDIR) -l$(PACKAGE) -l$(PACKAGE)Extrap

include SoftRelTools/standard.mk

override LIBLIBS += $(LOADLIBES)  -L$(SRT_PRIVATE_CONTEXT)/lib/$(SRT_SUBDIR) -L$(SRT_PUBLIC_CONTEXT)/lib/$(SRT_SUBDIR)
