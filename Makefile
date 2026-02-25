.PHONY: all clean install

PREFIX ?= /usr/local

all:
	cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$(PREFIX) -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
	cmake --build build
	cp build/compile_commands.json compile_commands.json

install: all
	cmake --install build

clean:
	rm -rf build
