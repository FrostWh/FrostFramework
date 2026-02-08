# UnLua 修改记录（基于 UnLua-master 适配 UE5.5+ / Angelscript 引擎分支）

> **源版本**: `UnLua-master` (Tencent/UnLua 官方版本)
> **目标环境**: UE5.5+ Angelscript 引擎分支 (FrostFramework)
> **修改文件总计**: 20 个文件（UnLua 17 个 + UnLuaExtensions 3 个）

---

## 一、修改总览

| # | 修改类别 | 涉及文件数 | 修改原因 |
|---|---------|-----------|---------|
| 1 | Build.cs 编译选项 API 变更 | 5 | UE5.5/5.6 废弃旧 API |
| 2 | Lua TString 命名冲突 | 4 | UE5.5 引入 `using TString = ...` 与 Lua 内部 `TString` 冲突 |
| 3 | AsyncLoading 枚举拆分 | 1 | UE5.5 将 `AsyncLoading` 拆为两个 Phase |
| 4 | UMetaData → FMetaData | 1 | UE5.6 更改元数据 API |
| 5 | Children 字段访问修复 | 1 | 修复原始代码 Bug（指针值拷贝问题） |
| 6 | ObjectReferencer GC API | 1 | UE5.5 更改 GC 引用收集 API |
| 7 | PropertyRegistry 属性构造 | 1 | UE5.5+ Angelscript 分支新增 `AngelscriptPropertyFlags` 字段 |
| 8 | FString::Printf consteval 兼容 | 2 | UE5.5 启用编译期格式字符串校验 |
| 9 | UPROPERTY 类型现代化 | 1 | `UFunction*` → `TObjectPtr<UFunction>` |
| 10 | TChooseClass 兼容层 | 1 | UE5.5 移除 `TChooseClass` 模板 |
| 11 | TIsTriviallyDestructible 替换 | 2 | UE5.5 移除 UE 自定义 trait，改用 `std::is_trivially_destructible` |
| 12 | MetaClass 路径补全 | 1 | UE5.5 要求完整路径 |

---

## 二、详细修改说明

### 2.1 Build.cs 编译选项 API 变更

**涉及文件**:
- `Source/ThirdParty/Lua/Lua.Build.cs`
- `UnLuaExtensions/LuaProtobuf/Source/LuaProtobuf.Build.cs`
- `UnLuaExtensions/LuaRapidjson/Source/LuaRapidjson.Build.cs`
- `UnLuaExtensions/LuaSocket/Source/LuaSocket.Build.cs`

**修改内容**:

1. `bEnableUndefinedIdentifierWarnings = false` → `UndefinedIdentifierWarningLevel = WarningLevel.Off`（UE5.5+）
2. `ShadowVariableWarningLevel = WarningLevel.Off` → `CppCompileWarningSettings.ShadowVariableWarningLevel = WarningLevel.Off`（UE5.6+，仅 Lua.Build.cs）
3. 移除 `VisualStudio2019` 编译器判断（UE5.5+ 不再支持 VS2019）

**修改方式**: 使用 `#if UE_5_5_OR_LATER` / `#if UE_5_6_OR_LATER` 条件编译。

**评估**: ✅ 修改方案合理，使用引擎提供的预处理宏做版本判断，保持向后兼容。

---

### 2.2 Lua TString 命名冲突

**涉及文件**:
- `Source/ThirdParty/Lua/lua-5.4.3/src/luaconf.h`
- `Source/ThirdParty/Lua/lua-5.4.4/src/luaconf.h`
- `Source/UnLua/Public/lua.hpp`
- `Source/UnLua/Private/LuaCore.cpp`
- `Source/UnLua/Private/LuaEnv.cpp`

**修改内容**:

UE5.5 在 `ContainersFwd.h` 中引入 `using TString = ...` 模板别名，与 Lua 内部 `typedef struct TString` 产生命名冲突。

- 在 `luaconf.h` 中添加 `#define TString Lua_TString`，全局重命名 Lua 内部的 `TString`
- 在 `lua.hpp` 的末尾添加 `#undef TString`，包含完 Lua 头文件后取消宏定义
- 在 `LuaCore.cpp` / `LuaEnv.cpp` 中引入 Lua 内部头文件（如 `lobject.h`、`lstate.h`）前，重新 `#define TString Lua_TString`，用完后 `#undef`

**评估**: ⚠️ **可改进**

当前方案的问题：
- **容易遗漏**：任何新增的 `.cpp` 文件如果引入了 Lua 内部头文件，都需要手动添加 `#define/#undef` 包装
- **分散管理**：`#define` 和 `#undef` 分散在多个文件中，维护成本高

**建议改进方案**：创建一个统一的 Lua 内部头文件包装器：

```cpp
// Source/UnLua/Private/LuaInternalHeaders.h
#pragma once

// 在包含 Lua 内部头文件前重新定义 TString 以避免与 UE5.5+ 冲突
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
// ... 其他需要的 Lua 内部头文件

#ifdef __cplusplus
#if !LUA_COMPILE_AS_CPP
}
#endif
#endif

// 包含完毕后恢复
#ifdef UNLUA_TSTRING_REDEFINED
#undef TString
#undef UNLUA_TSTRING_REDEFINED
#endif
```

这样所有需要 Lua 内部头文件的 `.cpp` 文件只需 `#include "LuaInternalHeaders.h"` 即可。

---

### 2.3 AsyncLoading 枚举拆分

**涉及文件**:
- `Source/UnLua/Private/LuaEnv.cpp`

**修改内容**:

UE5.5 将 `EInternalObjectFlags::AsyncLoading` 拆分为 `AsyncLoadingPhase1` 和 `AsyncLoadingPhase2` 两个独立标志。

```cpp
#if UE_VERSION_NEWER_THAN(5, 4, 0)
    constexpr EInternalObjectFlags AsyncObjectFlags = 
        EInternalObjectFlags::AsyncLoadingPhase1 | 
        EInternalObjectFlags::AsyncLoadingPhase2 | 
        EInternalObjectFlags::Async;
#else
    constexpr EInternalObjectFlags AsyncObjectFlags = 
        EInternalObjectFlags::AsyncLoading | 
        EInternalObjectFlags::Async;
#endif
```

**评估**: ✅ 修改方案正确，使用 `UE_VERSION_NEWER_THAN(5, 4, 0)` 判断。

---

### 2.4 UMetaData → FMetaData

**涉及文件**:
- `Source/UnLua/Private/LuaFunction.cpp`

**修改内容**:

UE5.6+ 将 `UMetaData::CopyMetadata` 更改为 `FMetaData::CopyMetadata`，并新增 `#include "UObject/MetaData.h"` 头文件。

```cpp
#if WITH_METADATA
#if UE_VERSION_NEWER_THAN(5, 5, 0)
    FMetaData::CopyMetadata(Function, this);
#else
    UMetaData::CopyMetadata(Function, this);
#endif
#endif
```

**评估**: ✅ 修改方案合理。

---

### 2.5 LuaOverridesClass Children 字段访问修复

**涉及文件**:
- `Source/UnLua/Private/LuaOverridesClass.cpp`

**修改内容**:

**这是一个 Bug 修复**，原始代码存在问题：

```cpp
// ❌ 原始代码（Bug）
auto ChildrenPtr = Class->Children.Get();  // 值拷贝！
auto Field = &ChildrenPtr;  // 指向的是局部拷贝，修改不影响 Class->Children

// ✅ 修复后
TObjectPtr<UField>* Field = &Class->Children;  // 直接指向成员
```

原始代码中 `Class->Children.Get()` 返回的是一个指针值的**拷贝**，修改 `ChildrenPtr` 并不会影响 `Class->Children` 的实际值。修复后直接取 `Class->Children` 的地址，确保对链表的修改生效。

在 `AddToOwner()` 和 `RemoveFromOwner()` 两个函数中都做了同样的修复。

**评估**: ✅ **修复正确且必要**。这是原始 UnLua 的 Bug，建议向上游提交 PR。

---

### 2.6 ObjectReferencer GC API 变更

**涉及文件**:
- `Source/UnLua/Private/ObjectReferencer.h`

**修改内容**:

UE5.5+ 中 `FReferenceCollector::AddReferencedObjects(TSet<UObject*>&)` 重载被移除，改为逐个添加引用。同时 `TSet<UObject*>` 更改为 `TSet<TObjectPtr<UObject>>`。

```cpp
#if UE_VERSION_NEWER_THAN(5, 4, 0)
    TSet<TObjectPtr<UObject>> ReferencedObjects;
    // 使用循环逐个添加引用
    for (TObjectPtr<UObject>& Object : ReferencedObjects)
    {
        Collector.AddReferencedObject(Object);
    }
#else
    TSet<UObject*> ReferencedObjects;
    Collector.AddReferencedObjects(ReferencedObjects);
#endif
```

**评估**: ✅ 修改方案合理，符合 UE5.5+ 的 GC API 变更。

---

### 2.7 PropertyRegistry 属性构造（Angelscript 特有）

**涉及文件**:
- `Source/UnLua/Private/Registries/PropertyRegistry.cpp`

**修改内容**:

这是**最大的修改**（+157 行），为 UE5.5+ Angelscript 引擎分支的属性构造系统做适配。

Angelscript 分支的 `UECodeGen_Private::*PropertyParams` 结构体新增了 `AngelscriptPropertyFlags` 字段，导致结构体布局发生变化，原有的聚合初始化方式不再适用。

修改涉及以下属性类型的构造：
- `FBoolProperty`
- `FIntProperty`
- `FFloatProperty`
- `FStrProperty`
- `FNameProperty`
- `FTextProperty`
- `FObjectProperty`
- `FStructProperty`
- `FEnumProperty`

每种属性都新增了 `#elif UE_VERSION_NEWER_THAN(5, 4, 0)` 分支，使用逐字段赋值代替聚合初始化。

**评估**: ⚠️ **需要注意**

1. **条件编译不够精确**：使用 `UE_VERSION_NEWER_THAN(5, 4, 0)` 无法区分是 Angelscript 分支还是原版 UE5.5+。如果将来需要支持原版 UE5.5+，这些修改可能会导致编译错误。建议增加 Angelscript 特有的宏判断，例如：

```cpp
#if defined(WITH_ANGELSCRIPT) && UE_VERSION_NEWER_THAN(5, 4, 0)
    // Angelscript 分支专用路径
#elif UE_VERSION_NEWER_THAN(5, 4, 0)
    // 原版 UE5.5+ 路径
#else
    // 旧版本路径
#endif
```

2. **代码重复度高**：9 种属性类型的构造代码结构几乎一致，可以考虑使用模板或辅助函数来减少代码重复。

3. **FTextProperty 简化**：`#elif UE_VERSION_NEWER_THAN(5, 2, 1)` 直接用 `new FTextProperty(PropertyCollector, "", RF_Transient)` 是正确的简化，同时清理了原始代码中混乱的条件编译嵌套。

---

### 2.8 FString::Printf consteval 格式字符串兼容

**涉及文件**:
- `Source/UnLua/Private/UnLuaConsoleCommands.cpp`
- `Source/UnLua/Private/UnLuaPrivate.h`

**修改内容**:

UE5.5 对 `FString::Printf` 启用了编译期格式字符串校验（`consteval`），不再接受运行时动态生成的格式字符串。

#### UnLuaConsoleCommands.cpp
将格式字符串从变量改为直接传入 `FString::Printf`：
```cpp
// ❌ 原始代码（UE5.5 编译失败）
const auto& Format = TEXT(R"(...)");
const auto Chunk = FString::Printf(Format, *Args[0]);

// ✅ 修复后
const auto Chunk = FString::Printf(TEXT(R"(...)"), *Args[0]);
```

#### UnLuaPrivate.h
重构了 `UNLUA_LOG` / `UNLUA_LOGWARNING` / `UNLUA_LOGERROR` 宏：
- 新增 `UnLuaUnsafePrintf` 辅助函数，使用 `FCString::GetVarArgs` 绕过 consteval 检查
- 新增 `UnLuaLogWithTraceback` 函数声明，将日志逻辑从宏中移出

**评估**: ✅ 修改方案合理。

- `UnLuaLogWithTraceback` 实现在 `Source/UnLua/Private/UnLuaPrivate.cpp` 中，通过 `#include "lua.hpp"` 引入 Lua API，结构清晰
- 将日志宏从内联展开改为函数调用，避免了宏内直接使用 Lua 头文件（TString 冲突）和 `FString::Printf`（consteval 冲突）
- `UnLuaUnsafePrintf` 使用固定 4096 字符缓冲区，超长日志会被截断（低优先级改进项）

---

### 2.9 UPROPERTY 类型现代化

**涉及文件**:
- `Source/UnLua/Public/LuaFunction.h`

**修改内容**:

```cpp
// 原始
UPROPERTY()
UFunction* Overridden;

// 修改后
UPROPERTY()
TObjectPtr<UFunction> Overridden;
```

**评估**: ✅ 符合 UE5 的 `TObjectPtr` 现代化规范。UE5 推荐 UPROPERTY 使用 `TObjectPtr` 代替裸指针。

---

### 2.10 TChooseClass 兼容层

**涉及文件**:
- `Source/UnLua/Public/UnLuaTemplate.h`

**修改内容**:

UE5.5 移除了 `TChooseClass` 模板，在 `UnLuaTemplate.h` 中添加兼容实现：

```cpp
template<bool bCondition, typename TrueType, typename FalseType>
struct TChooseClass
{
    using Result = typename std::conditional<bCondition, TrueType, FalseType>::type;
};
```

**评估**: ⚠️ **可改进**

当前实现放在全局命名空间，在 UE5.4 及以下版本中可能与引擎自带的 `TChooseClass` 冲突。建议添加版本保护：

```cpp
#if UE_VERSION_NEWER_THAN(5, 4, 0)
template<bool bCondition, typename TrueType, typename FalseType>
struct TChooseClass
{
    using Result = typename std::conditional<bCondition, TrueType, FalseType>::type;
};
#endif
```

---

### 2.11 TIsTriviallyDestructible 替换

**涉及文件**:
- `Source/UnLua/Public/UnLuaEx.inl`
- `Source/UnLua/Public/UnLuaLegacy.h`

**修改内容**:

UE5.5 移除了 `TIsTriviallyDestructible<T>` 和 `TNot<>`，改用标准库：

```cpp
// ❌ 原始
TIsTriviallyDestructible<T>::Value
TNot<TIsTriviallyDestructible<ClassType>>

// ✅ 修改后
std::is_trivially_destructible<T>::value
!std::is_trivially_destructible<ClassType>::value
```

同时在 `UnLuaEx.inl` 中修复了 `TType<ClassType>::GetName()` 返回类型变化的问题：
```cpp
// 原始：GetName() 返回 TCHAR*
UE_LOG(LogUnLua, Warning, TEXT("...%s..."), TType<ClassType>::GetName(), ...);

// 修改后：GetName() 返回 const char*
UE_LOG(LogUnLua, Warning, TEXT("...%s..."), ANSI_TO_TCHAR(TType<ClassType>::GetName()), ...);
```

**评估**: ✅ 修改正确。`std::is_trivially_destructible` 是 C++17 标准，UE5 已全面支持。

---

### 2.12 MetaClass 路径补全

**涉及文件**:
- `Source/UnLua/Public/UnLuaSettings.h`

**修改内容**:

```cpp
// 原始
meta = (MetaClass="Object", ...)

// 修改后
meta = (MetaClass="/Script/CoreUObject.Object", ...)
```

UE5.5 要求 `MetaClass` 使用完整的脚本路径而非短名。

**评估**: ✅ 修改正确。

---

## 三、问题汇总与优化记录

### ✅ 已完成的优化（2026-02-08）

| 优化项 | 操作 | 涉及文件 |
|--------|------|---------|
| TString define/undef 分散管理 | 创建统一的 `LuaInternalHeaders.h`，`LuaCore.cpp` 和 `LuaEnv.cpp` 改为一行 `#include` | `LuaInternalHeaders.h`（新增）、`LuaCore.cpp`、`LuaEnv.cpp` |
| TChooseClass 无版本保护 | 添加 `#if UE_VERSION_NEWER_THAN(5, 4, 0)` 包裹，避免与旧版 UE 冲突 | `UnLuaTemplate.h` |
| PropertyRegistry Angelscript 分支检测 | `UnLua.Build.cs` 检测 Angelscript 插件并定义 `UNLUA_WITH_ANGELSCRIPT` 宏，条件编译从 `UE_VERSION_NEWER_THAN(5, 4, 0)` 改为 `UNLUA_WITH_ANGELSCRIPT && UE_VERSION_NEWER_THAN(5, 4, 0)` | `UnLua.Build.cs`、`PropertyRegistry.cpp` |
| PropertyRegistry 代码重复 | 抽取 `InitCommonPropertyParams<T>()` 模板辅助函数，统一初始化 PropertyParams 公共字段，将 9 种属性类型的重复代码从约 15 行/类型缩减为 3-5 行/类型 | `PropertyRegistry.cpp` |

#### 优化详情

**1. LuaInternalHeaders.h 统一头文件**

新增 `Source/UnLua/Private/LuaInternalHeaders.h`，集中处理：
- `#define TString Lua_TString`（避免与 UE5.5+ TString 冲突）
- `extern "C"` 块（根据 `LUA_COMPILE_AS_CPP` 控制）
- 包含 Lua 内部头文件（`lobject.h`、`lstate.h`、`lfunc.h`）
- `#undef TString`（恢复 UE 的 TString）

此前 `LuaCore.cpp`（18 行 TString 处理代码）和 `LuaEnv.cpp`（13 行）各自重复这段逻辑，现在只需：
```cpp
#include "LuaInternalHeaders.h"
```
未来新增 `.cpp` 如需 Lua 内部头文件，也无需再手动处理 TString 冲突。

**2. TChooseClass 版本保护**

```cpp
#if UE_VERSION_NEWER_THAN(5, 4, 0)
template<bool bCondition, typename TrueType, typename FalseType>
struct TChooseClass { ... };
#endif
```
避免在 UE5.4 及以下版本中与引擎自带的 `TChooseClass` 产生重定义错误。

**3. UNLUA_WITH_ANGELSCRIPT 宏检测**

在 `UnLua.Build.cs` 中检测 Angelscript 插件是否存在：
```csharp
if (IsPluginEnabled("Angelscript"))
    PublicDefinitions.Add("UNLUA_WITH_ANGELSCRIPT=1");
else
    PublicDefinitions.Add("UNLUA_WITH_ANGELSCRIPT=0");
```
PropertyRegistry 的条件编译从 `#elif UE_VERSION_NEWER_THAN(5, 4, 0)` 变为 `#elif UNLUA_WITH_ANGELSCRIPT && UE_VERSION_NEWER_THAN(5, 4, 0)`，确保：
- 在 Angelscript 分支使用 field-by-field 赋值方式（因 `AngelscriptPropertyFlags` 改变了结构体布局）
- 在原版 UE5.5+ 使用标准的聚合初始化方式（fallthrough 到 `#else` 分支）

**4. InitCommonPropertyParams 辅助函数**

```cpp
template<typename ParamsType>
void InitCommonPropertyParams(ParamsType& Params, EPropertyFlags PropertyFlags, 
                               UECodeGen_Private::EPropertyGenFlags GenFlags);
```
统一初始化所有 PropertyParams 类型的公共字段（NameUTF8、RepNotifyFuncUTF8、PropertyFlags、Flags、ObjectFlags、SetterFunc、GetterFunc、ArrayDim、AngelscriptPropertyFlags、Metadata 等），每种属性类型只需额外设置其特有字段。

### ⚠️ 剩余可改进项

| 问题 | 建议 | 优先级 |
|------|------|--------|
| UnLuaUnsafePrintf 缓冲区限制 | 使用动态分配替代固定 4096 字节缓冲区 | 低 |

### ✅ 质量良好的修改

| 修改 | 说明 |
|------|------|
| LuaOverridesClass Bug 修复 | 修复了指针值拷贝导致链表操作无效的 Bug |
| Build.cs 版本兼容 | 干净的条件编译，保持向后兼容 |
| std::is_trivially_destructible 替换 | 从 UE 专有 trait 迁移到 C++ 标准 |
| TObjectPtr 现代化 | 符合 UE5 最佳实践 |
| UnLuaLogWithTraceback 抽取 | 将日志+traceback 逻辑从宏抽取为独立函数，避免 Lua 头文件依赖和 consteval 问题 |

---

## 四、修改文件清单

### UnLua 插件 (17+3 个文件)

| 文件路径 | 修改类型 | 行数变化 |
|---------|---------|---------|
| `Source/ThirdParty/Lua/Lua.Build.cs` | 修改 | +10 |
| `Source/ThirdParty/Lua/lua-5.4.3/src/luaconf.h` | 修改 | +7 |
| `Source/ThirdParty/Lua/lua-5.4.4/src/luaconf.h` | 修改 | +7 |
| `Source/UnLua/Private/LuaCore.cpp` | 修改 | +1/-18（优化后） |
| `Source/UnLua/Private/LuaEnv.cpp` | 修改 | +7/-13（优化后） |
| `Source/UnLua/Private/LuaFunction.cpp` | 修改 | +7 |
| `Source/UnLua/Private/LuaInternalHeaders.h` | **新增** | +58（优化新增） |
| `Source/UnLua/Private/LuaOverridesClass.cpp` | 修改 | +4/-8 |
| `Source/UnLua/Private/ObjectReferencer.h` | 修改 | +12 |
| `Source/UnLua/Private/Registries/PropertyRegistry.cpp` | 修改 | 优化后更精简 |
| `Source/UnLua/Private/UnLuaConsoleCommands.cpp` | 修改 | +2/-3 |
| `Source/UnLua/Private/UnLuaPrivate.h` | 修改 | +20/-18 |
| `Source/UnLua/Private/UnLuaPrivate.cpp` | **新增** | +39 |
| `Source/UnLua/Public/LuaFunction.h` | 修改 | +1/-1 |
| `Source/UnLua/Public/UnLuaEx.inl` | 修改 | +2/-2 |
| `Source/UnLua/Public/UnLuaLegacy.h` | 修改 | +2/-2 |
| `Source/UnLua/Public/UnLuaSettings.h` | 修改 | +1/-1 |
| `Source/UnLua/Public/UnLuaTemplate.h` | 修改 | +10（优化后含版本保护） |
| `Source/UnLua/Public/lua.hpp` | 修改 | +8 |
| `Source/UnLua/UnLua.Build.cs` | 修改 | +6（优化新增 Angelscript 检测） |

### UnLuaExtensions 插件 (3 个文件)

| 文件路径 | 修改类型 | 行数变化 |
|---------|---------|---------|
| `LuaProtobuf/Source/LuaProtobuf.Build.cs` | 修改 | +4 |
| `LuaRapidjson/Source/LuaRapidjson.Build.cs` | 修改 | +4 |
| `LuaSocket/Source/LuaSocket.Build.cs` | 修改 | +4 |
