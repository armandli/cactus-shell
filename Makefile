BUILD_DIR ?= build
CACTUS_ROOT ?=
JOBS ?= $(shell sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)

.PHONY: all configure build test clean rebuild run

all: build

configure: $(BUILD_DIR)/CMakeCache.txt

$(BUILD_DIR)/CMakeCache.txt: CMakeLists.txt
	@if [ -z "$(CACTUS_ROOT)" ]; then \
		echo "CACTUS_ROOT is not set. Usage: make configure CACTUS_ROOT=/path/to/cactus" >&2; \
		exit 1; \
	fi
	cmake -S . -B $(BUILD_DIR) -DCACTUS_ROOT=$(CACTUS_ROOT)

build: configure
	cmake --build $(BUILD_DIR) -j$(JOBS)

test: build
	ctest --test-dir $(BUILD_DIR) --output-on-failure

clean:
	rm -rf $(BUILD_DIR)

rebuild: clean build

run: build
	./$(BUILD_DIR)/src/cactus $(WEIGHTS)
