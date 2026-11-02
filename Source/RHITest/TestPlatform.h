#pragma once
// 阶段 B 后端边界验证；普通绘制仍只调用 RHI。
class FRHIViewport;
class FRHITexture;
void ValidateStageB(FRHIViewport* Viewport, FRHITexture* Depth);
#include "DynamicRHI.h"
#include <vector>
#include <string>
// 平台启动适配器；Shader 编译已迁移到 ShaderFormatD3D。
std::unique_ptr<FDynamicRHI> CreateTestRHI(bool EnableValidation);
void ValidateBackend(FRHIViewport* Viewport, FRHITexture* Depth, FRHIGraphicsPipelineState* PSO, FRHIBuffer* VB, FRHIBuffer* IB);
void ValidateRenderedImage(FRHITexture* Texture, const char* OutputPath);
void CheckDebugMessages();

// 后端诊断：等待同队列回读并恢复原状态，输出紧密 RGBA8 数据。
std::vector<uint8> ReadbackTextureRGBA(FRHITexture* Texture);

// 阶段 C：编译/反射元数据及 Shader 参数对象验证（调用时 RHI 已初始化）。
void ValidateStageCCompiler();
