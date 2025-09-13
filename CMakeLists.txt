cmake_minimum_required (VERSION 3.5)

project(korkscript C CXX)

include (CheckIncludeFiles)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_definitions(-DUNICODE -D_UNICODE -DINITGUIDE)

include_directories(
	.
	./engine
)

set(KS_NETWORK_SRCS
	./engine/console/telnetConsole.cc
	./engine/console/telnetDebugger.cc

	./engine/platform/platformNetAsync.cc
	./engine/platform/platformNetwork.cc
)
set(KS_SRCS
	./engine/core/idGenerator.cc
	./engine/core/tVector.cc
	./engine/core/findMatch.cc
	./engine/core/hashFunction.cc
	./engine/core/memStream.cc
	./engine/core/stringTable.cc
	./engine/core/dataChunker.cc
	./engine/core/fileStream.cc
	./engine/core/nStream.cc
	./engine/core/unicode.cc
	./engine/core/stringUnit.cc
	./engine/core/escape.cc

	./engine/platform/platformFileIO.cc
	./engine/platform/platform.cc
	./engine/platform/platformAssert.cc
	./engine/platform/platformMemory.cc
	./engine/platform/platformStrings.cc
	./engine/platform/basicPlatform.cc

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

	./engine/console/consoleValue.h
	./engine/console/simpleLexer.h
	./engine/console/simpleParser.h
	./engine/embed/api.h
	./engine/embed/api.cpp
)

set(TORQUESIM_SRCS
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
	./torqueSim/sim/simManager.cc
)

set(KS_WASM_SRCS
	${KS_SRCS}
	${KS_NETWORK_SRCS}
	wasm/wasmModule.cpp
)

set(KS_TEST_SRCS
	${KS_SRCS}
	${KS_NETWORK_SRCS}
	test/main.cpp
)

set(KS_API_TEST_SRCS
	${KS_SRCS}
	${KS_NETWORK_SRCS}
	test/apiTest.cpp
)
set(KS_AST_PRINT_SRCS
	${KS_SRCS}
	${KS_NETWORK_SRCS}
	test/astPrint.cpp
)

set(KS_TORQUE_TEST_SRCS
	${KS_SRCS}
	${KS_NETWORK_SRCS}
	${TORQUESIM_SRCS}
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
add_executable(cstest ${KS_TEST_SRCS})
add_executable(csprintast ${KS_AST_PRINT_SRCS})
add_executable(csapitest ${KS_API_TEST_SRCS})
add_executable(torquetest ${KS_TORQUE_TEST_SRCS})
endif()
