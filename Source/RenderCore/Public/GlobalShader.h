#pragma once
#include "Shader.h"

// 不依赖材质/VertexFactory 排列的 Shader 基类，例如全屏合成 Shader。
// Global 不代表单例；当前尚无自动注册、GlobalShaderMap 和 permutation 编译。
class FGlobalShader : public FShader
{
public:
    explicit FGlobalShader(const CompiledShaderInitializerType& Initializer) : FShader(Initializer) {}
};
