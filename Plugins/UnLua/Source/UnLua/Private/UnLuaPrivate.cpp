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

#include "UnLuaPrivate.h"
#include "lua.hpp"

void UnLuaLogWithTraceback(lua_State* L, ELogVerbosity::Type Verbosity, const FString& LogMsg)
{
    luaL_traceback(L, L, "", 0);
    FString FullMsg = LogMsg + UTF8_TO_TCHAR(lua_tostring(L, -1));
    lua_pop(L, 1);
    
    switch (Verbosity)
    {
    case ELogVerbosity::Log:
        UE_LOG(LogUnLua, Log, TEXT("%s"), *FullMsg);
        break;
    case ELogVerbosity::Warning:
        UE_LOG(LogUnLua, Warning, TEXT("%s"), *FullMsg);
        break;
    case ELogVerbosity::Error:
        UE_LOG(LogUnLua, Error, TEXT("%s"), *FullMsg);
        break;
    default:
        UE_LOG(LogUnLua, Log, TEXT("%s"), *FullMsg);
        break;
    }
}
