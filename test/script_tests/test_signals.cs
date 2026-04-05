//-----------------------------------------------------------------------------
// Copyright (c) 2025-2026 korkscript contributors.
// See AUTHORS file and git repository for contributor information.
//
// SPDX-License-Identifier: MIT
//-----------------------------------------------------------------------------

signal SignalEmitter::ping(%amount);
signal SignalEmitter::typedPing(%point : TypeMyPoint3F);

function SignalListener::ping(%this, %amount)
{
   $signalSum += %amount;
   $signalTrace = $signalTrace @ %this.getName() @ ":" @ %amount @ ";";
}

function SignalListener::typedPing(%this, %point)
{
   $signalTyped = %point;
}

function SignalListener::onPing(%this, %amount)
{
   $signalTrace = $signalTrace @ "alias-" @ %this.getName() @ ":" @ %amount @ ";";
}

function SignalEmitter::onRemove(%this)
{
}

function SignalListener::onRemove(%this)
{
}

function testSignals()
{
   $signalSum = 0;
   $signalTrace = "";
   $signalTyped = "";

   %emitter = new ScriptObject(emitterOne)
   {
      class = SignalEmitter;
   };

   %listenerA = new ScriptObject(listenerA)
   {
      class = SignalListener;
   };

   %listenerB = new ScriptObject(listenerB)
   {
      class = SignalListener;
   };

   testInt("signal.addListenerA", %emitter.addListener("ping", %listenerA), 1);
   testInt("signal.addListenerB", %emitter.addListener("ping", %listenerB, "onPing"), 1);
   testInt("signal.addTypedListener", %emitter.addListener("typedPing", %listenerA), 1);

   %emitter.ping(3);
   testInt("signal.sum.1", $signalSum, 3);
   testString("signal.trace.1", $signalTrace, "listenerA:3;alias-listenerB:3;");

   testInt("signal.removeListenerA", %emitter.removeListener("ping", %listenerA), 1);
   %emitter.ping(4);
   testInt("signal.sum.2", $signalSum, 3);
   testString("signal.trace.2", $signalTrace, "listenerA:3;alias-listenerB:3;alias-listenerB:4;");

   %emitter.typedPing("1 2 3");
   testString("signal.typed", $signalTyped, "1 2 3");

   testInt("signal.removeListenerAlias", %emitter.removeListener("ping", %listenerB, "onPing"), 1);
   %emitter.ping(6);
   testInt("signal.sum.3", $signalSum, 3);
   testString("signal.trace.3", $signalTrace, "listenerA:3;alias-listenerB:3;alias-listenerB:4;");

   %listenerB.delete();
   %emitter.ping(5);
   testInt("signal.sum.4", $signalSum, 3);
   testString("signal.trace.4", $signalTrace, "listenerA:3;alias-listenerB:3;alias-listenerB:4;");

   %listenerA.delete();
   %emitter.delete();
}

testSignals();
