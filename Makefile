OSLC = oslc
OSLC_FLAGS = -O2 -Ishaders/Utility

# Recursively find all .osl files in the current directory and all subdirectories
SRCS := $(shell find . -type f -name "*.osl")

# Substitute .osl extension with .oso for the output files
OBJS := $(SRCS:.osl=.oso)

all: $(OBJS)

%.oso: %.osl
	@echo "Compiling: $<"
	@$(OSLC) $(OSLC_FLAGS) $< -o $@

install-mk12:
	@echo "Installing shaders to $(MK12_ROOT)/_env/shaders..."
	@mkdir -p $(MK12_ROOT)/_env/shaders
	@cp -r shaders/* $(MK12_ROOT)/_env/shaders

houdini-mk12: $(OBJS)
	@echo "Building RenderMan for Houdini nodes into $(MK12_ROOT)/_env/houdini/otls..."
	@hython scripts/build_houdini_hda.py $(MK12_ROOT)/_env/houdini/otls/livery-shaders.hda $(sort $(dir $(OBJS)))

clean:
	@echo "Cleaning all compiled shaders..."
	@find . -type f -name "*.oso" -delete

# Declare non-file targets
.PHONY: all clean install-mk12 houdini-mk12