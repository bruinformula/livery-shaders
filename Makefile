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

clean:
	@echo "Cleaning all compiled shaders..."
	@find . -type f -name "*.oso" -delete

# Declare non-file targets
.PHONY: all clean