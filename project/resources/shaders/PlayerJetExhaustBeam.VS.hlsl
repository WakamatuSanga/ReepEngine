struct BeamConstants
{
    float4x4 viewProjection;
    float4 params;
};

ConstantBuffer<BeamConstants> gBeam : register(b0);

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
    output.position = mul(float4(input.position, 1.0f), gBeam.viewProjection);
    output.uv = input.uv;
    return output;
}
