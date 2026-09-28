struct BeamConstants
{
    float4x4 viewProjection;
    float4 params;
    float4 qualityParams;
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
    float beamNearFade : TEXCOORD1;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    output.position = mul(float4(input.position, 1.0f), gBeam.viewProjection);
    output.uv = input.uv;
    // Perspective interpolation retains clip Z at the fragment's world position.
    output.beamNearFade = gBeam.qualityParams.w > 0.0f
        ? output.position.z / gBeam.qualityParams.w : 1.0f;
    return output;
}
