#include "platform/platform.h"
#include "console/simpleLexer.h"
#include "console/ast.h"
#include "console/compiler.h"
#include "console/simpleParser.h"
#include "core/fileStream.h"
#include <stdio.h>
#include "embed/api.h"
#include <emscripten/bind.h>

using namespace KorkApi;

EMSCRIPTEN_BINDINGS(vm_module) {
  emscripten::class_<Vm>("Vm")
  ;
}
