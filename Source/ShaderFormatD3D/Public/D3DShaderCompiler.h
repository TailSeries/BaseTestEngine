#pragma once
#include "ShaderCompilerCore.h"

// 平台编译器入口。当前使用 FXC/SM5 + D3DReflect，不能据此宣称已实现 UE 的 DXC/SM6 链路。
FShaderCompilerOutput CompileD3DShader(const FShaderCompilerInput& Input);
