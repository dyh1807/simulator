# Makefile with multi-threaded compilation support

# Enable SimDDR by default; override with USE_SIM_DDR=0 if needed.
USE_SIM_DDR ?= 1
# Default backend is AXI3 (256-bit). Define USE_SIM_DDR_AXI4=1 to select AXI4.
USE_SIM_DDR_AXI3 ?= 0
USE_SIM_DDR_AXI4 ?= 0

CXX := g++
# Flags for the default target
CXXFLAGS := -O3 -march=native -funroll-loops -mtune=native --std=c++2a
LDFLAGS := -lz -lstdc++fs

# Include directories
CXXINCLUDE := -I./include/ \
              -I./back-end/include/ \
              -I./back-end/EXU/include/ \
              -I./back-end/tools/include/ \
              -I./diff/include/ \
              -I./front-end/ \
              -I./mmu/include/ \
              -I./memory/include/

ifeq ($(USE_SIM_DDR),1)
CXXFLAGS += -DUSE_SIM_DDR
CXXINCLUDE += -I./sim_ddr/include/ -I./axi_interconnect/include/
ifeq ($(USE_SIM_DDR_AXI3),1)
ifeq ($(USE_SIM_DDR_AXI4),1)
$(error USE_SIM_DDR_AXI3 and USE_SIM_DDR_AXI4 are mutually exclusive)
endif
endif
ifeq ($(USE_SIM_DDR_AXI3),1)
CXXFLAGS += -DUSE_SIM_DDR_AXI3
endif
ifeq ($(USE_SIM_DDR_AXI4),1)
CXXFLAGS += -DUSE_SIM_DDR_AXI4
endif
endif

# Source files (dynamically found)
SRCS := $(shell find ./back-end/ -name "*.cpp")
SRCS += $(shell find ./front-end/ -name "*.cpp")
SRCS += $(shell find ./diff/ -name "*.cpp")
SRCS += $(shell find ./mmu/ -name "*.cpp")
SRCS += $(shell find ./memory/ -name "*.cpp")
SRCS += ./main.cpp
SRCS += ./rv_simu_mmu_v2.cpp

ifeq ($(USE_SIM_DDR),1)
SRCS += $(shell find ./sim_ddr/ -name "*.cpp" ! -name "*_test.cpp")
SRCS += $(shell find ./axi_interconnect/ -name "*.cpp" ! -name "*_test.cpp")
endif

# Output binary
TARGET := a.out
# Build directory for object files
BUILD_DIR := build

# Object files (mapped from source files)
OBJS := $(SRCS:%.cpp=$(BUILD_DIR)/%.o)
# Dependency files
DEPS := $(OBJS:.o=.d)

# Libraries
LIBS := ./softfloat.a

# Default target
default: $(TARGET)

# Link object files to create the executable
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) $(LIBS) $(LDFLAGS) -o $@

# Compile source files to object files
# -MMD -MP generates dependency info
$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(CXXINCLUDE) -MMD -MP -c $< -o $@

# Include generated dependencies
-include $(DEPS)

# --- Other Targets (maintained from original) ---
# Note: These targets are kept monolithic as per the original Makefile 
# to ensure specific flags are applied correctly without complex conditional logic.

MEM_DIR=./baremetal
IMG=./baremetal/linux.bin

cov: 
	$(CXX) $(CXXINCLUDE) $(SRCS) $(LIBS) -O0 --coverage -o $(TARGET)

run: 
	./$(TARGET) $(IMG)

clean:
	rm -f $(TARGET)
	rm -rf $(BUILD_DIR)
	rm -rf ./baremetal/memory
	rm -rf ./baremetal/test.code

gdb:
	$(CXX) $(CXXINCLUDE) $(SRCS) $(LIBS) -O0 -g -march=native -lz -o $(TARGET)
	gdb --args ./$(TARGET) $(IMG)

gdb_linux:
	$(CXX) $(CXXINCLUDE) $(SRCS) $(LIBS) -g -o $(TARGET)
	gdb --args ./$(TARGET) ./baremetal/linux.bin

.PHONY: default all clean mem run linux gdb gdb_linux cov
