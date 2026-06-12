# CppDB Makefile
# Compiles every .cpp under src/ + main.cpp into ./build/cppdb
# and every tests/*.cpp into its own ./build/test_<name> binary.

CXX      := clang++
CXXFLAGS := -std=c++23 -Wall -Wextra -Wpedantic -g -Iinclude
LDFLAGS  := -pthread

# `make WERROR=1 ...` promotes warnings to errors (used by CI)
ifdef WERROR
CXXFLAGS += -Werror
endif

SRC      := $(shell find src -name '*.cpp' 2>/dev/null)
HDR      := $(shell find include tests -name '*.hpp' 2>/dev/null)
OBJ      := $(SRC:.cpp=.o)
BIN      := build/cppdb

# Each test file becomes its own binary: tests/foo.cpp -> build/test_foo
TEST_SRC := $(wildcard tests/*.cpp)
TEST_BIN := $(patsubst tests/%.cpp,build/test_%,$(TEST_SRC))
TSAN_BIN := $(patsubst tests/%.cpp,build/tsan_%,$(TEST_SRC))
ASAN_BIN := $(patsubst tests/%.cpp,build/asan_%,$(TEST_SRC))

.PHONY: all clean tests run check tsan asan

all: $(BIN)

# Main app: links main.cpp with everything in src/
# Headers are prerequisites too, so editing a .hpp triggers a rebuild,
# but only the .cpp files are passed to the compiler.
$(BIN): main.cpp $(SRC) $(HDR)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(filter %.cpp,$^) -o $@ $(LDFLAGS)

# Build & run a single test:  make build/test_schema && ./build/test_schema
# A test is linked against all src/ so it can use your classes.
build/test_%: tests/%.cpp $(SRC) $(HDR)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(filter %.cpp,$^) -o $@ $(LDFLAGS)

# Build all tests
tests: $(TEST_BIN)

# Build and run every test; fails on the first failing test binary
check: $(TEST_BIN)
	@for t in $(TEST_BIN); do echo "== $$t"; ./$$t || exit 1; done

# Same suite under ThreadSanitizer (slower; catches data races and deadlocks)
build/tsan_%: tests/%.cpp $(SRC) $(HDR)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -fsanitize=thread -O1 $(filter %.cpp,$^) -o $@ $(LDFLAGS)

tsan: $(TSAN_BIN)
	@for t in $(TSAN_BIN); do echo "== $$t"; ./$$t || exit 1; done

# Address + UndefinedBehavior sanitizers (memory errors, leaks, UB)
build/asan_%: tests/%.cpp $(SRC) $(HDR)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -fsanitize=address,undefined -fno-omit-frame-pointer -O1 $(filter %.cpp,$^) -o $@ $(LDFLAGS)

asan: $(ASAN_BIN)
	@for t in $(ASAN_BIN); do echo "== $$t"; ./$$t || exit 1; done

# Benchmarks: optimized build, otherwise the numbers are meaningless
build/bench: bench/bench.cpp $(SRC) $(HDR)
	@mkdir -p build
	$(CXX) -std=c++23 -Wall -Wextra -Wpedantic -O2 -DNDEBUG -Iinclude $(filter %.cpp,$^) -o $@ $(LDFLAGS)

bench: build/bench
	./build/bench

run: $(BIN)
	./$(BIN)

clean:
	rm -rf build/*.o build/cppdb build/test_* $(OBJ)

# Usage:
#   make            -> builds main app (once you have a main.cpp)
#   make tests      -> builds all test binaries
#   make build/test_schema && ./build/test_schema
