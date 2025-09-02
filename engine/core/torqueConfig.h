//-----------------------------------------------------------------------------
// Torque Game Engine
// Copyright (C) GarageGames.com, Inc.
//-----------------------------------------------------------------------------

#ifndef _TORQUECONFIG_H_
#define _TORQUECONFIG_H_

//Hi, and welcome to the Torque Config file.
//
//This file is a central reference for the various configuration flags that
//you'll be using when controlling what sort of a Torque build you have. In
//general, the information here is global for your entire codebase, applying
//not only to your game proper, but also to all of your tools.
//
//This file also contains information which is used for various other needs,
//for instance, defines indicating what engine we're building, or what version
//we're at.

/// What engine are we running? The presence and value of this define are
/// used to determine what engine (TGE, T2D, etc.) and version thereof we're
/// running - useful if you're a content pack or other 3rd party code
/// snippet!
///
/// Version number is major * 1000 + minor * 100 + revision * 10.
#define TORQUE_GAME_ENGINE          1420

/// What's the name of your game? Used in a variety of places.
#define TORQUE_GAME_NAME            "Torque Demo"

/// Human readable version string.
#define TORQUE_GAME_VERSION_STRING  "Torque Demo 1.4.2 (TGE 1.4.2)"

/// Define me if you want to enable multithreading support.
#ifndef TORQUE_MULTITHREAD
//#define TORQUE_MULTITHREAD
#endif

/// Define me to enable unicode support.
#ifndef TORQUE_UNICODE
#define TORQUE_UNICODE
#endif

/// Define me to enable debug mode; enables a great number of additional
/// sanity checks, as well as making AssertFatal and AssertWarn do something.
/// This is usually defined by the build target.
//#define TORQUE_DEBUG

/// Define me if this is a shipping build; if defined I will instruct Torque
/// to batten down some hatches and generally be more "final game" oriented.
/// Notably this disables a liberal resource manager file searching, and
/// console help strings.
//#define TORQUE_SHIPPING

/// Define me to enable a variety of network debugging aids.
//#define TORQUE_DEBUG_NET

/// Modify me to enable metric gathering code in the renderers.
///
/// 0 does nothing; higher numbers enable higher levels of metric gathering.
//#define TORQUE_GATHER_METRICS 0

/// Define me if you want to enable debug guards in the memory manager.
///
/// Debug guards are known values placed before and after every block of
/// allocated memory. They are checked periodically by Memory::validate(),
/// and if they are modified (indicating an access to memory the app doesn't
/// "own"), an error is flagged (ie, you'll see a crash in the memory
/// manager's validate code). Using this and a debugger, you can track down
/// memory corruption issues quickly.
//#define TORQUE_DEBUG_GUARD

/// Define me if you want to enable debug guards on the FrameAllocator.
/// 
/// This is similar to the above memory manager guards, but applies only to the
/// fast FrameAllocator temporary pool memory allocations. The guards are only
/// checked when the FrameAllocator frees memory (when it's water mark changes).
/// This is most useful for detecting buffer overruns when using FrameTemp<> .
/// A buffer overrun in the FrameAllocator is unlikely to cause a crash, but may
/// still result in unexpected behavior, if other FrameTemp's are stomped.
///#define FRAMEALLOCATOR_DEBUG_GUARD

/// Define to disable Ogg Vorbis audio support. Libs are compiled without this by
/// default.
//#define TORQUE_NO_OGGVORBIS

// Finally, we define some dependent #defines. This enables some subsidiary
// functionality to get automatically turned on in certain configurations.

#ifdef TORQUE_DEBUG
#  define TORQUE_GATHER_METRICS 0
#endif

#ifdef TORQUE_RELEASE
  // If it's not DEBUG, it's a RELEASE build, put appropriate things here.
#endif

#ifdef TORQUE_SHIPPING
 // TORQUE_SHIPPING flags here.
#else
#endif

#ifdef TORQUE_LIB
   #ifndef TORQUE_NO_OGGVORBIS
   #define TORQUE_NO_OGGVORBIS
   #endif
#endif

#define TORQUE_UNUSED( arg )

// Someday, it might make sense to do some pragma magic here so we error
// on inconsistent flags.

// Disable engine memory manager
#define TORQUE_DISABLE_MEMORY_MANAGER

#endif

