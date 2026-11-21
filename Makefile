all: build

mkdir_build:
	rm -rf build
	mkdir -p build

build: mkdir_build
	cd build && cmake ..
	$(MAKE) -C build

build_ninja: mkdir_build
	cd build; cmake -GNinja ..
	ninja -C build

debug: mkdir_build
	cd build && cmake -DCMAKE_BUILD_TYPE=Debug ..
	$(MAKE) -C build

debug_ninja: mkdir_build
	cd build; cmake -GNinja -DCMAKE_BUILD_TYPE=Debug ..
	ninja -C build

run:
	cd build && ./uContra

assets:
	python3 tools/prepare_assets.py
	@if test -d build; then mkdir -p build/files/stages && cp source/files/stages/* build/files/stages/; fi

.PHONY: all mkdir_build build build_ninja debug debug_ninja run assets clean

clean:
	rm -rf build
