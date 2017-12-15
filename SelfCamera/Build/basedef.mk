# Add inputs and outputs from these tool invocations to the build variables

C_DEPS +=

USES_EFL = yes
USES_USR_INC = yes

SYSROOT = $(SBI_SYSROOT)

USR_INCS := $(addprefix -I $(SYSROOT),$(PLATFORM_INCS_EX))
EFL_INCS = 

ifeq ($(strip $(PLATFORM_LIB_PATHS)),)
RS_LIB_PATHS := "$(SYSROOT)/usr/lib"
else
RS_LIB_PATHS := $(addprefix -L$(SYSROOT),$(PLATFORM_LIB_PATHS))
endif

RS_LIBRARIES := $(addprefix -l,$(RS_LIBRARIES_EX))

PLATFORM_INCS = $(USR_INCS) $(EFL_INCS) \
     -I"$(SDK_PATH)/library"

OS_NAME := $(shell $(UNAME))
