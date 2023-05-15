CXX      = g++-12
CXXFLAGS = -Wall -Wextra -Wpedantic -std=c++23 -g
LDFLAGS  = -ltbb -lwstm -lboost_thread -lzmq

INC_DIR = include
LIB_DIR = libraries
SRC_DIR = source

# from https://stackoverflow.com/questions/2483182/recursive-wildcards-in-gnu-make/18258352#18258352
rwildcard=$(foreach d,$(wildcard $(1:=/*)),$(call rwildcard,$d,$2) $(filter $(subst *,%,$2),$d))
SOURCES := $(call rwildcard,$(SRC_DIR),*.cpp)

.PHONY: all
all:
	${CXX} ${CXXFLAGS} -I${INC_DIR} ${SOURCES} -L${LIB_DIR} ${LDFLAGS}

.PHONY: clean
clean:
	rm a.out 