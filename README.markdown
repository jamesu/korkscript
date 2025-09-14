# korkscript

korkscript is a fork of tgemit, which attempts to strip out the TorqueScript interpreter.

The ultimate goal is to allow for easy embedding into modern C++ libraries.

This project is still WIP, so expect the api and internal code to change as things are better fleshed out.

## Compiling

Project files are generated with cmake.

Simple test app build:

	mkdir build
	cd build
	cmake ..
	make

WASM module build:

	mkdir wasmbuild
	cd build
	emcmake ..
	make

Currently there is no static library target provided, so if you want to use this in a project you will need to incorporate all the relevant files into your build.


## Embedding

Instead of going for a "all the bells and whistles included" system, korkscript relies heavily on being provided information through simple interfaces.
This means you need to provide hooks for:

- Memory management
- Logging
- Finding objects
- Creating objects
- Manipulating dynamic object fields
- Manipulating typed variable data

Examples of embedding are provided in the torqueTest executable as well as the apiTest executable.


## Differences with Torque

There are a few differences, namely:

- Script VM is abstracted through a new API
- Lexer and parser have been rewritten in C++
- Value management via `ConsoleValue` uses a different API
- Sim layer entirely ripped out in favor or writing your own; though a port of the old sim layer is provided in torqueSim.


## License

All code should be considered licensed under the MIT license by their respective authors.


