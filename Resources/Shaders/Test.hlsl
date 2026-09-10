struct VSInput { float3 Pos : POSITION; float4 Color : COLOR; };
struct PSInput { float4 Pos : SV_POSITION; float4 Color : COLOR; };

PSInput VSMain(VSInput v)
{
    PSInput o;
    o.Pos   = float4(v.Pos, 1.0);
    o.Color = v.Color;
    return o;
}

float4 PSMain(PSInput i) : SV_TARGET
{
    return i.Color;
}