#ifndef NUM_DIR_LIGHTS
#define NUM_DIR_LIGHTS 1
#endif
#ifndef NUM_POINT_LIGHTS
#define NUM_POINT_LIGHTS 0
#endif

#ifndef NUM_SPOT_LIGHTS
#define NUM_SPOT_LIGHTS 0
#endif

#include "LightingUtil.hlsl"

cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
};

cbuffer cbMaterial : register(b1)
{
    float4 gDiffuseAlbedo;
    float3 gFresnelR0;
    float gRoughness;
    float4x4 gMatTransform;
};

cbuffer cbPass : register(b2)
{
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
    float4x4 gInvViewProj;
    float3 gEyePosW;
    float cbPerObjectPad1;
    float2 gRenderTargetSize;
    float2 gInvRenderTargetSize;
    float gNearZ;
    float gFarZ;
    float gTotalTime;
    float gDeltaTime;

    float4 gAmbientLight;
    Light gLights[MaxLights];
};

struct VertexIn
{
    float3 PosL : POSITION;
    float3 Normal : NORMAL;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 PosW : POSITION;
    float3 NormalW : NORMAL;
};


VertexOut VS(VertexIn vin)
{
    VertexOut vout = (VertexOut) 0.0f;
    float4 posw = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosW = posw.xyz;

    // 这里能成立的前提是假设没有非均匀缩放，不然得用逆转置
    vout.NormalW = mul(vin.Normal, (float3x3) gWorld);
    vout.PosH = mul(posw, gViewProj);
    return vout;

}

float4 PS(VertexOut pin):SV_Target
{
    pin.NormalW = normalize(pin.NormalW);
    float3 toEyeW = normalize(gEyePosW - pin.PosW);
    float4 ambient = gAmbientLight * gDiffuseAlbedo;
    const float shininess = 1.0f = gRoughness;
	Material mat = {gDiffuseAlbedo, gFresnelR0, shininess};
    float3 shadoeFactor = 1.0f;
    float4 directLight = ComputeLighting(gLights, mat, pin.PosW, pin.NormalW, toEyeW, shadoeFactor);
    float4 litColor = ambient + directLight;
    litColor.a = gDiffuseAlbedo.a;
    return litColor;
}
