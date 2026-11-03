#include "LocalVertexFactory.h"

void FLocalVertexFactory::SetData(const FDataType& InData)
{
    if (IsInitialized()) throw std::logic_error("Release vertex factory before changing layout");
    if (InData.PositionComponent.Type != VET_Float3 || InData.TextureCoordinates.size() != 1 ||
        InData.TextureCoordinates[0].Type != VET_Float2)
        throw std::invalid_argument("Local vertex factory requires float3 position and one float2 UV");
    Data = InData; // 复制描述，Buffer 指针仍是借用关系。
}
void FLocalVertexFactory::InitRHI(FRHICommandListBase& RHICmdList)
{
    (void)RHICmdList;
    if (Data.TextureCoordinates.size() != 1) throw std::logic_error("SetData before initializing vertex factory");
    FVertexDeclarationElementList Elements;
    Elements.push_back(AccessStreamComponent(Data.PositionComponent, 0));
    // 对齐 UE LocalVertexFactory：Position=ATTRIBUTE0，首个 UV=ATTRIBUTE4；1/2/3 留给切线和颜色。
    Elements.push_back(AccessStreamComponent(Data.TextureCoordinates[0], 4));
    InitDeclaration(Elements);
}
