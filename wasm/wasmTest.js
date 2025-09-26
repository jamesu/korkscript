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

// Global stores
let GlobalIds = {};
let GlobalNames = {};
let __nextAutoId = 1; // autoincrementing ID source

// Helper iFind lookups
const iFind = {
  byName: (name) => (name != null ? GlobalNames[name] ?? null : null),
  // "path" is effectively the same as name for now
  byPath: (path) => { console.log("LOOK BY PATH:",path); var pathInt = parseInt(path); console.log('path...', path, pathInt); var r = path != null ? GlobalNames[path] ?? GlobalIds[pathInt] ?? null : null; console.log('ret=',r); return r; },
  byId: (id) => {
    // accept number or numeric string
    const n = typeof id === 'string' ? Number(id) : id;
    return Number.isFinite(n) ? (GlobalIds[n] ?? null) : null;
  },
};

// Use your API
const vm = new Module.Vm({
  logFn: (level, msg) => console.log("[vmlog]", msg),
  "iFind": iFind,
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

const playerNs = vm.findNamespace("Player", "");
vm.addNamespaceFunction(
  playerNs, "doThat", "does This", 2, 32, "void",
  (objPtr, vmPtr, argv) => { console.log('Doing That'); objPtr.newField = 123; }
);


const PlayerClass = vm.registerClass({
  name: "Player",

  iCreate: {
    create: (_klass, _vm) => {
      return {}; // basic object data
    },

    destroy: (_klass, _vm, _object) => {
      /* no-op */
    },

    addObject: (_vm, objectData, _placeAtRoot, _groupAddId) => {
      if (objectData.id == null) {
        objectData.id = __nextAutoId++;
      }

      GlobalIds[objectData.id] = objectData;
      console.log('addObject data is:', objectData);

      if (objectData.name != null) {
        GlobalNames[objectData.name] = objectData;
      }

      _vm.setObjectNamespace(objectData, playerNs);

      return true;
    },

    processArgs: (_vm, _objectData, _name, _isDatablock, _internalName, _args) => {
      return true;
    },

    removeObject: (_klass, _vm, objectData) => {
      console.log('removing object');
      if (!objectData) return;

      if (objectData.id != null && GlobalIds[objectData.id]) {
        delete GlobalIds[objectData.id];
      }
      if (objectData.name != null && GlobalNames[objectData.name]) {
        delete GlobalNames[objectData.name];
      }
    },

    getId: (objectData) => {
      return objectData?.id ?? null;
    },
  },
});



console.log('Code test....');
vm.evalCode("$x=2002; echo(test,ing);", "");
console.log('Code end.');
console.log('x =', vm.getGlobal('$x'));

vm.evalCode('function Player::doThis() { echo("Doing this"); }', "");

vm.evalCode('$player = new Player() { position = "1 2 3"; }; echo($player); $player.doThis(); $player.doThat();', "");

console.log(GlobalIds);
console.log(GlobalNames);

vm.delete();

