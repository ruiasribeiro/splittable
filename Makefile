# Based on https://spin.atomicobject.com/2016/08/26/makefile-c-projects/

# NOTE: It does not trigger a rebuild when a source file is deleted, so `make
# clean` needs to be executed in that case.

CXX      = g++-12
TARGETS  = $(basename $(notdir $(wildcard apps/*.cpp)))
BINARIES = $(TARGETS:%=$(BUILD_DIR)/bin/%)

BUILD_DIR ?= ./build
LIB_DIR   ?= ./libraries
SRC_DIRS  ?= ./source
EXE_DIRS  ?= ./apps

# External dependencies are added as `-isystem` in order to avoid getting 
# their warnings during compilation. 
INC_FLAGS := -isystem ./include -I./headers/ 
# Alternate version to use on the ARM machine.
# INC_FLAGS := -isystem /usr/include/boost1.78/ -isystem ./include -I./headers/ 

# These 3 variables are only related to the library code (source directory).
SRCS := $(shell find $(SRC_DIRS) -name *.cpp)
OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

CPPFLAGS := $(INC_FLAGS)
# The `-Wno-array-bounds` is needed to surpress warnings from the immer library.
CPPFLAGS += -Wall -Wextra -Wpedantic -Wno-array-bounds -Wno-interference-size
CPPFLAGS += -MMD -MP -std=c++20 -march=native -O3 #-Og -g
CPPFLAGS += #-DSPLITTABLE_DEBUG
LDFLAGS  := $(LD_FLAGS) -L$(LIB_DIR) -Wl,--start-group -lstdc++ -lm -lboost_system -lpthread -lboost_thread -lwstm -lboost_program_options -ltbb -Wl,--end-group

.PHONY: all
all: $(BINARIES)

# Build the executables.
$(BUILD_DIR)/bin/%: $(BUILD_DIR)/apps/%.cpp.o $(OBJS) 
	$(MKDIR_P) $(dir $@)
	$(CXX) $(OBJS) $< -o $@ $(LDFLAGS)

# Compile all object files.
$(BUILD_DIR)/%.cpp.o: %.cpp
	$(MKDIR_P) $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

.PHONY: clean
clean:
	$(RM) -r $(BUILD_DIR)

-include $(DEPS)

MKDIR_P ?= mkdir -p