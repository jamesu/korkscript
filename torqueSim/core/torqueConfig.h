#pragma once

//-----------------------------------------------------------------------------
// Copyright (c) 2024 tgemit contributors.
// See AUTHORS file and git repository for contributor information.
//
// SPDX-License-Identifier: MIT
//-----------------------------------------------------------------------------

#define TORQUE_GAME_ENGINE 1430
#define TORQUE_GAME_NAME "TGEMit Demo"
#define TORQUE_GAME_VERSION_STRING "TGEMIT DEMO 1.4.3 (Based on TGE 1.4.2)"

#ifndef TORQUE_UNICODE
#define TORQUE_UNICODE
#endif

#ifdef TORQUE_DEBUG
#define TORQUE_GATHER_METRICS 0
#endif

#ifdef TORQUE_LIB
#ifndef TORQUE_NO_OGGVORBIS
#define TORQUE_NO_OGGVORBIS
#endif
#endif

#ifndef TORQUE_SHIPPING
#define TORQUE_ENABLE_PROFILER
#endif

#ifndef TORQUE_DISABLE_MEMORY_MANAGER
#define TORQUE_DISABLE_MEMORY_MANAGER
#endif

#define TGEMIT_INCLUDE_NON_MIT

