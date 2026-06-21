MSPM0_SDK_INSTALL_DIR ?= d:/TI-MPSM0/ti/mspm0_sdk_2_10_00_04
TICLANG_ARMCOMPILER ?= d:/TI-MPSM0/ti/ccs2050/ccs/tools/compiler/ti-cgt-armllvm_4.0.4.LTS
SYSCONFIG_TOOL ?= d:/TI-MPSM0/ti/ccs2050/ccs/utils/sysconfig_1.27.0/sysconfig_cli.bat

OUT_DIR := Debug
NAME := LED1
APP_DEFINES ?=

CC = "$(TICLANG_ARMCOMPILER)/bin/tiarmclang"
LNK = "$(TICLANG_ARMCOMPILER)/bin/tiarmclang"
SIZE = "$(TICLANG_ARMCOMPILER)/bin/tiarmsize"
HEX = "$(TICLANG_ARMCOMPILER)/bin/tiarmhex"

SYSCFG_CMD_STUB = "$(SYSCONFIG_TOOL)" --compiler ticlang --product "$(MSPM0_SDK_INSTALL_DIR)/.metadata/product.json"
SYSCFG_FILES := $(shell $(SYSCFG_CMD_STUB) --listGeneratedFiles --listReferencedFiles --output $(OUT_DIR) empty.syscfg)
SYSCFG_C_FILES := $(filter %.c,$(SYSCFG_FILES))
SYSCFG_H_FILES := $(filter %.h,$(SYSCFG_FILES))
SYSCFG_OPT_FILES := $(filter %.opt,$(SYSCFG_FILES))

APP_C_FILES := $(wildcard *.c) $(wildcard Hardware/*.c)
OBJECTS := $(patsubst %.c,$(OUT_DIR)/%.obj,$(APP_C_FILES)) $(patsubst %.c,$(OUT_DIR)/%.obj,$(notdir $(SYSCFG_C_FILES)))
DEP_FILES := $(OBJECTS:.obj=.d)

# Enable verbose output with: gmake VERBOSE=1
V := @
ifeq ($(VERBOSE),1)
  V :=
endif

CFLAGS += \
    $(APP_DEFINES) \
    -I. \
    -IHardware \
    -I$(OUT_DIR) \
    $(addprefix @,$(SYSCFG_OPT_FILES)) \
    -O2 \
    "-I$(MSPM0_SDK_INSTALL_DIR)/source/third_party/CMSIS/Core/Include" \
    "-I$(MSPM0_SDK_INSTALL_DIR)/source" \
    -gdwarf-3 \
    -mcpu=cortex-m0plus \
    -march=thumbv6m \
    -mfloat-abi=soft \
    -mthumb \
    -Wall \
    -Wno-missing-braces \
    -Wno-invalid-source-encoding

LFLAGS += \
    -ldevice.cmd.genlibs \
    "-L$(MSPM0_SDK_INSTALL_DIR)/source" \
    -L. \
    "-L$(OUT_DIR)" \
    "$(OUT_DIR)/device_linker.cmd" \
    "-Wl,-m,$(OUT_DIR)/$(NAME).map" \
    "-Wl,--xml_link_info=$(OUT_DIR)/$(NAME)_linkInfo.xml" \
    -Wl,--rom_model \
    -Wl,--warn_sections \
    "-L$(TICLANG_ARMCOMPILER)/lib" \
    -llibc.a

.PHONY: all syscfg clean rebuild size hex task1 normal pan_tilt_debug test

all: $(OUT_DIR)/$(NAME).out hex size

task1 normal:
	$(MAKE) -B all APP_DEFINES=

pan_tilt_debug test:
	$(MAKE) -B all APP_DEFINES="-DAPP_MODE_PAN_TILT_DEBUG=1 -DAPP_MODE_TASK1=0"

$(OUT_DIR):
	@if not exist "$(OUT_DIR)" mkdir "$(OUT_DIR)"

.INTERMEDIATE: syscfg
$(SYSCFG_FILES): syscfg
	@echo generation complete

syscfg: empty.syscfg | $(OUT_DIR)
	@echo Generating SysConfig files...
	$(V) $(SYSCFG_CMD_STUB) --output $(OUT_DIR) $<

define C_RULE
$(OUT_DIR)/$(basename $(notdir $(1))).obj: $(1) $(SYSCFG_H_FILES) | $(OUT_DIR)
	@echo Building $$@
	@if not exist "$$(dir $$@)" mkdir "$$(dir $$@)"
	$(V) $(CC) $(CFLAGS) -MMD -MP -MF "$$(@:.obj=.d)" -c $$< -o $$@
endef

$(foreach c_file,$(SYSCFG_C_FILES),$(eval $(call C_RULE,$(c_file))))

$(OUT_DIR)/%.obj: %.c $(SYSCFG_H_FILES) | $(OUT_DIR)
	@echo Building $@
	@if not exist "$(dir $@)" mkdir "$(dir $@)"
	$(V) $(CC) $(CFLAGS) -MMD -MP -MF "$(@:.obj=.d)" -c $< -o $@

$(OUT_DIR)/$(NAME).out: $(OBJECTS) $(OUT_DIR)/device_linker.cmd $(OUT_DIR)/device.cmd.genlibs
	@echo Linking $@
	$(V) $(LNK) -Wl,-u,_c_int00 $(OBJECTS) $(LFLAGS) -o $@

size: $(OUT_DIR)/$(NAME).out
	$(V) $(SIZE) $<

hex: $(OUT_DIR)/$(NAME).out
	@echo Generating $(OUT_DIR)/$(NAME).hex
	$(V) $(HEX) --memwidth=8 --romwidth=8 --intel -o $(OUT_DIR)/$(NAME).hex $<

clean:
	@echo Cleaning...
	-@if exist "$(OUT_DIR)\*.obj" del /Q "$(OUT_DIR)\*.obj"
	-@if exist "$(OUT_DIR)\*.d" del /Q "$(OUT_DIR)\*.d"
	-@if exist "$(OUT_DIR)\Hardware\*.obj" del /Q "$(OUT_DIR)\Hardware\*.obj"
	-@if exist "$(OUT_DIR)\Hardware\*.d" del /Q "$(OUT_DIR)\Hardware\*.d"
	-@if exist "$(OUT_DIR)\$(NAME).out" del /Q "$(OUT_DIR)\$(NAME).out"
	-@if exist "$(OUT_DIR)\$(NAME).hex" del /Q "$(OUT_DIR)\$(NAME).hex"
	-@if exist "$(OUT_DIR)\$(NAME).map" del /Q "$(OUT_DIR)\$(NAME).map"
	-@if exist "$(OUT_DIR)\$(NAME)_linkInfo.xml" del /Q "$(OUT_DIR)\$(NAME)_linkInfo.xml"

rebuild: clean all

-include $(DEP_FILES)
