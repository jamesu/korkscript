cmake_minimum_required (VERSION 3.5)

project(korkscript C CXX)

include (CheckIncludeFiles)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#set(CMAKE_C_FLAGS "-g -fsanitize=address,undefined")
#set(CMAKE_CXX_FLAGS "-g -fsanitize=address,undefined")

add_definitions(-DUNICODE -D_UNICODE -DINITGUIDE)

include_directories(
	.
	./engine
)

set(KS_SRCS
	./engine/core/findMatch.cc
	./engine/core/hashFunction.cc
	./engine/core/memStream.cc
	./engine/core/nStream.cc
	./engine/core/escape.cc

	./engine/platform/platform.cc
	./engine/platform/platformAssert.cc

	./engine/console/typeValidators.cc
	./engine/console/compiledEval.cc
	./engine/console/codeBlock.cc
	./engine/console/consoleDoc.cc
	./engine/console/stringStack.cc
	./engine/console/consoleInternal.cc
	./engine/console/consoleNamespace.cc
	./engine/console/astNodes.cc
	./engine/console/astAlloc.cc
	./engine/console/compiler.cc

	./engine/console/telnetConsole.cc
	./engine/console/telnetDebugger.cc

	./engine/console/consoleValue.h
	./engine/console/simpleLexer.h
	./engine/console/simpleParser.h
	./engine/embed/api.h
	./engine/embed/api.cpp
)

set(TORQUESIM_SRCS
	./torqueSim/core/idGenerator.cc
	./torqueSim/core/fileStream.cc
	./torqueSim/core/stringUnit.cc
	./torqueSim/core/stringTable.cc
	./torqueSim/core/unicode.cc
	./torqueSim/core/unicode.h
	./torqueSim/console/console.cc
	./torqueSim/console/console.h
	./torqueSim/console/consoleFunctions.cc
	./torqueSim/console/consoleLogger.cc
	./torqueSim/console/consoleLogger.h
	./torqueSim/console/consoleObject.cc
	./torqueSim/console/consoleObject.h
	./torqueSim/console/consoleTypes.cc
	./torqueSim/console/consoleTypes.h
	./torqueSim/sim/dynamicTypes.cc
	./torqueSim/sim/scriptObject.cc
	./torqueSim/sim/simBase.cc
	./torqueSim/sim/simBase.h
	./torqueSim/sim/simDictionary.cc
	./torqueSim/sim/simDictionary.h
	./torqueSim/sim/simFiberManager.cc
	./torqueSim/sim/simFiberManager.h
	./torqueSim/sim/simManager.cc
	./torqueSim/platform/basicPlatformProcess.cc
	./torqueSim/platform/platformFileIO.cc
	./torqueSim/platform/platformStrings.cc
	./torqueSim/platform/platformThread.cc
	./torqueSim/platform/platformNetAsync.cc
	./torqueSim/platform/platformNetwork.cc
)

set(KS_WASM_SRCS
	${KS_SRCS}
	wasm/wasmModule.cpp
)

set(KS_TEST_SRCS
	${KS_SRCS}
	test/main.cpp
)

set(KS_API_TEST_SRCS
	${KS_SRCS}
	test/apiTest.cpp
)
set(KS_AST_PRINT_SRCS
	${KS_SRCS}
	test/astPrint.cpp
)
set(TEST_RUNNER_SRCS
	${KS_SRCS}
	${TORQUESIM_SRCS}
	test/testRunner.cpp
)


set(TORQUESIM_BRIDGE_SRCS )
set(ENABLE_WASM3_BRIDGE OFF)
set(ENABLE_WASMTIME_BRIDGE OFF)
set(WASM3_DIR "../wasm3")

if (ENABLE_WASM3_BRIDGE)
	set(TORQUESIM_BRIDGE_SRCS 
		${TORQUESIM_BRIDGE_SRCS} 
		./torqueSim/bridges/wasm/wasm3_embed.cpp
	)

	file(GLOB WASM3_SOURCES
	     "${WASM3_DIR}/source/*.c")
	add_library(wasm3 STATIC ${WASM3_SOURCES})
	target_include_directories(wasm3 PUBLIC "${WASM3_DIR}/source")
	target_compile_definitions(wasm3 PUBLIC WASM3_NO_FILESYSTEM)
endif()


if (ENABLE_WASMTIME_BRIDGE)
	if (NOT WASMTIME_C_API_DIR)
	  message(FATAL_ERROR "Please set WASMTIME_C_API_DIR to your wasmtime folder")
	endif()

	set(_WT_INC "${WASMTIME_C_API_DIR}/include")
	set(_WT_LIB_DIR "${WASMTIME_C_API_DIR}/lib")
	set(_WT_LIB "${_WT_LIB_DIR}/libwasmtime.a")

	add_library(Wasmtime::wasmtime INTERFACE IMPORTED)
	set_target_properties(Wasmtime::wasmtime PROPERTIES
	  INTERFACE_INCLUDE_DIRECTORIES "${_WT_INC}"
	  INTERFACE_LINK_LIBRARIES      "${_WT_LIB}"
	)

	set(TORQUESIM_BRIDGE_SRCS 
		${TORQUESIM_BRIDGE_SRCS} 
		./torqueSim/bridges/wasm/wasmtime_embed.cpp
	)
endif()

set(KS_TORQUE_TEST_SRCS
	${KS_SRCS}
	${TORQUESIM_SRCS}
	${TORQUESIM_BRIDGE_SRCS}
	test/torqueTest.cpp
)



if (EMSCRIPTEN)

add_definitions(-DTORQUE_NO_SOCKETS)
add_executable(korkscript ${KS_WASM_SRCS})

target_link_options(korkscript PRIVATE
    "-g"
    "-sENVIRONMENT=node,web"
    "-sALLOW_MEMORY_GROWTH"
    "-sDEMANGLE_SUPPORT=1"
    "-sMODULARIZE=1"
    "-sEXPORT_ES6=1"
    "-sSTACK_SIZE=1048576"
    "-sASSERTIONS=0"
    "-Os"
    "-lembind"
    "-sERROR_ON_UNDEFINED_SYMBOLS=1"
  )

else()
#add_executable(cstest ${KS_TEST_SRCS})
add_executable(csprintast ${KS_AST_PRINT_SRCS})
add_executable(csapitest ${KS_API_TEST_SRCS})
add_executable(torquetest ${KS_TORQUE_TEST_SRCS})
add_executable(testrunner ${TEST_RUNNER_SRCS})

if (ENABLE_WASM3_BRIDGE)
	target_link_libraries(torquetest wasm3)
endif()

if (ENABLE_WASMTIME_BRIDGE)
	target_link_libraries(torquetest Wasmtime::wasmtime)
endif()

target_include_directories(torquetest PRIVATE ./torqueSim)
target_include_directories(testrunner PRIVATE ./torqueSim)
endif()
