cmake_minimum_required (VERSION 2.8.8)

project(korkscript C CXX)

include (CheckIncludeFiles)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_definitions(-DUNICODE -D_UNICODE -DINITGUIDE)

include_directories(
	.
	./engine
	./ext/ghc
	./ext/yamc/include
)

set(KS_NETWORK_SRCS
	./engine/console/telnetConsole.cc
	./engine/console/telnetDebugger.cc

	#./engine/platform/platformNetAsync.cc
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

	./engine/platform/profiler.cc
	./engine/platform/platformFileIO.cc
	./engine/platform/platform.cc
	./engine/platform/platformAssert.cc
	./engine/platform/platformMemory.cc
	./engine/platform/platformStrings.cc
	./engine/platform/basicPlatform.cc

	./engine/console/typeValidators.cc
	./engine/console/consoleFunctions.cc
	./engine/console/compiledEval.cc
	./engine/console/consoleTypes.cc

	./engine/console/codeBlock.cc
	./engine/console/codeBlockWorld.cc
	./engine/console/consoleDoc.cc
	./engine/console/consoleObject.cc
	./engine/console/stringStack.cc
	./engine/console/dynamicTypes.cc
	./engine/console/consoleParser.cc
	./engine/console/console.cc


	./engine/console/consoleInternal.cc
)

set(KS_COMPILER_SRCS
	./engine/console/astNodes.cc
	./engine/console/astAlloc.cc
	./engine/console/compiler.cc
)

set(KS_INTERPRETER_SRCS
	./engine/console/CMDscan.cc
	./engine/console/cmdgram.cc
)

set(SIM_SRCS
	./engine/console/consoleLogger.cc
	./engine/console/simManager.cc
	./engine/console/simDictionary.cc
	./engine/console/simBase.cc
	./engine/console/scriptObject.cc
)

set(KS_TEST_SRCS
	${KS_SRCS}
	${KS_COMPILER_SRCS}
	${KS_INTERPRETER_SRCS}
	${KS_NETWORK_SRCS}
	test/main.cpp
)

add_executable(cstest ${KS_TEST_SRCS})
