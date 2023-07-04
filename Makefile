# Based on https://spin.atomicobject.com/2016/08/26/makefile-c-projects/

CXX      = g++-12
TARGETS  = $(basename $(notdir $(wildcard apps/*.cpp)))
BINARIES = $(TARGETS:%=$(BUILD_DIR)/bin/%)

BUILD_DIR ?= ./build
LIB_DIR   ?= ./libraries
SRC_DIRS  ?= ./source
EXE_DIRS  ?= ./apps

INC_DIRS  := ./include
INC_FLAGS := $(addprefix -I,$(INC_DIRS))

# These 3 variables are only related to the library code (source directory).
SRCS := $(shell find $(SRC_DIRS) -name *.cpp)
OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

CPPFLAGS ?= $(INC_FLAGS) -MMD -MP -std=c++17 -Wall -Wextra -Wpedantic #-DSPLITTABLE_DEBUG
LDFLAGS  := $(LD_FLAGS) -L$(LIB_DIR) -lstdc++ -lm -lpthread -lboost_thread -lwstm 

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