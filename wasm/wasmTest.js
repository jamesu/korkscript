import createKork from './korkscript.js';
import { fileURLToPath } from 'url';
import path from 'path';

const __filename = fileURLToPath(import.meta.url);
const __dirname  = path.dirname(__filename);

const Module = await createKork({
  locateFile: (f) => path.join(__dirname, f),
  // optional logging hooks
  print:    (...a) => console.log('[kork]', ...a),
  printErr: (...a) => console.error('[kork]', ...a),
});

// Use your API
const vm = new Module.Vm(
  {
  "logFn": (level, msg) => console.log("[vmlog]", msg),
  "iFind": {
    "byName": (name) => null,
    "byPath": (name) => null,
    "byId": (name) => null,
  },

});


console.log('Global test....');
vm.setGlobal('x', 123);
console.log('x =', vm.getGlobal('$x'));

const ns = vm.getGlobalNamespace();
console.log(ns);

console.log('calling addNamespaceFunction');

vm.addNamespaceFunction(
  ns, "echo", "echo back", 1, 32, "void",
  (objPtr, vmPtr, argv) => console.log('[echo]', argv.slice(1).join(''))
);


console.log('Code test....');
vm.evalCode("$x=2002; echo(test,ing);", "");
console.log('Code end.');
console.log('x =', vm.getGlobal('$x'));

vm.delete();

