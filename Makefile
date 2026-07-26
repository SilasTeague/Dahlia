# Thin convenience wrapper around CMake (see docs/adr/0002-cmake-migration.md).
# `cmake --preset <name>` is the source of truth; this just saves typing.

PRESET ?= debug

all: build

configure:
	cmake --preset $(PRESET)

build: configure
	cmake --build --preset $(PRESET)

test: build
	ctest --preset $(PRESET)

run: build
	./build/$(PRESET)/dahlia

clean:
	rm -rf build

.PHONY: all configure build test run clean
