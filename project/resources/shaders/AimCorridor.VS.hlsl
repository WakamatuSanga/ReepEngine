struct AimCorridorConstants
{
    float4x4 viewProjection;
    float4 appearance;
    float4 sampling;
    float4 flags;
    float4 coreTint;
    float4 glowTint;
};

ConstantBuffer<AimCorridorConstants> gAimCorridor : register(b0);

struct VertexShaderInput
{
    float3 position : POSITION;
    float2 uv : TEXCOORD0;
};

struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    output.position = mul(float4(input.position, 1.0f), gAimCorridor.viewProjection);
    output.uv = input.uv;
    return output;
}
