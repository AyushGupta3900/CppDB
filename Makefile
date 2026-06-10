# CppDB Makefile
# Compiles every .cpp under src/ + main.cpp into ./build/cppdb
# and every tests/*.cpp into its own ./build/test_<name> binary.

CXX      := clang++
CXXFLAGS := -std=c++23 -Wall -Wextra -Wpedantic -g -Iinclude
LDFLAGS  := -pthread

SRC      := $(shell find src -name '*.cpp' 2>/dev/null)
OBJ      := $(SRC:.cpp=.o)
BIN      := build/cppdb

# Each test file becomes its own binary: tests/foo.cpp -> build/test_foo
TEST_SRC := $(wildcard tests/*.cpp)
TEST_BIN := $(patsubst tests/%.cpp,build/test_%,$(TEST_SRC))

.PHONY: all clean tests run

all: $(BIN)

# Main app: links main.cpp with everything in src/
$(BIN): main.cpp $(SRC)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

# Build & run a single test:  make build/test_schema && ./build/test_schema
# A test is linked against all src/ so it can use your classes.
build/test_%: tests/%.cpp $(SRC)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

# Build all tests
tests: $(TEST_BIN)

run: $(BIN)
	./$(BIN)

clean:
	rm -rf build/*.o build/cppdb build/test_* $(OBJ)

# Usage:
#   make            -> builds main app (once you have a main.cpp)
#   make tests      -> builds all test binaries
#   make build/test_schema && ./build/test_schema
