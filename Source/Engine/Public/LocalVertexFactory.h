#pragma once
#include "VertexFactory.h"

// UE 的 LocalVertexFactory 归属 Engine。当前只实现本地静态网格的 Position + 一个 UV 通道。
// 尚无切线/颜色、manual fetch、VertexFactoryType 注册、Shader include 和材质排列。
class FLocalVertexFactory : public FVertexFactory
{
public:
    struct FDataType
    {
        FVertexStreamComponent PositionComponent;
        std::vector<FVertexStreamComponent> TextureCoordinates;
    };
    // 仅在未初始化时修改布局；更新已创建布局须先释放并重建相应 PSO。
    void SetData(const FDataType& InData);
protected:
    void InitRHI(FRHICommandListBase& RHICmdList) override;
private:
    FDataType Data;
};
