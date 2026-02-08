// Tencent is pleased to support the open source community by making UnLua available.
// 
// Copyright (C) 2019 Tencent. All rights reserved.
//
// Licensed under the MIT License (the "License"); 
// you may not use this file except in compliance with the License. You may obtain a copy of the License at
//
// http://opensource.org/licenses/MIT
//
// Unless required by applicable law or agreed to in writing, 
// software distributed under the License is distributed on an "AS IS" BASIS, 
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
// See the License for the specific language governing permissions and limitations under the License.

#pragma once

/**
 * Unified wrapper for including Lua internal headers (lobject.h, lstate.h, lfunc.h, etc.)
 *
 * UE5.5+ introduces 'using TString = ...' in ContainersFwd.h which conflicts with
 * Lua's internal 'typedef struct TString'. This header centralizes the TString
 * define/undef logic so that individual .cpp files don't need to manage it themselves.
 *
 * Usage:
 *   #include "LuaInternalHeaders.h"
 *
 * Note: The public Lua API headers (lua.h, lualib.h, lauxlib.h) are handled by lua.hpp,
 * which already includes the TString undef. This header is only needed when you require
 * access to Lua's internal structures (e.g., lobject.h, lstate.h, lfunc.h).
 */

// Re-define TString before including Lua internal headers to avoid conflict with UE5.5+ TString
#ifndef TString
#define TString Lua_TString
#define UNLUA_TSTRING_REDEFINED
#endif

#ifdef __cplusplus
#if !LUA_COMPILE_AS_CPP
extern "C" {
#endif
#endif

#include "lobject.h"
#include "lstate.h"
#include "lfunc.h"

#ifdef __cplusplus
#if !LUA_COMPILE_AS_CPP
}
#endif
#endif

// Restore TString after including Lua internal headers
#ifdef UNLUA_TSTRING_REDEFINED
#undef TString
#undef UNLUA_TSTRING_REDEFINED
#endif
