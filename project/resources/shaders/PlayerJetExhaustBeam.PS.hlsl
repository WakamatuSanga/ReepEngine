struct BeamConstants
{
    float4x4 viewProjection;
    float4 params; // x: brightness, y: flicker strength, z: time, w: mode
};

ConstantBuffer<BeamConstants> gBeam : register(b0);

struct PixelShaderInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
    float4 normal : SV_TARGET1;
};

PixelShaderOutput MakeOutput(float4 color)
{
    PixelShaderOutput output;
    output.color = color;
    output.normal = float4(0.5f, 0.5f, 1.0f, 1.0f);
    return output;
}

PixelShaderOutput main(PixelShaderInput input)
{
    float brightness = max(gBeam.params.x, 0.0f);
    float flickerStrength = saturate(gBeam.params.y);
    float time = gBeam.params.z;
    uint mode = uint(gBeam.params.w + 0.5f);

    if (mode == 1u)
    {
        float2 centered = input.uv * 2.0f - 1.0f;
        float radial = saturate(1.0f - dot(centered, centered));
        float alpha = pow(radial, 2.2f);
        if (alpha <= 0.01f)
        {
            discard;
        }
        float flicker = 1.0f + sin(time * 53.0f) * flickerStrength * 0.12f;
        return MakeOutput(float4(float3(1.0f, 0.94f, 0.58f) * brightness * flicker, alpha * 0.75f));
    }

    float u = saturate(input.uv.x);
    float v = abs(input.uv.y * 2.0f - 1.0f);
    float core = pow(saturate(1.0f - v), 2.5f);
    float lengthFade = pow(saturate(1.0f - u), 1.2f);
    float edgeHeat = pow(saturate(1.0f - v), 0.8f);

    float3 c0 = float3(1.0f, 0.95f, 0.65f);
    float3 c1 = float3(1.0f, 0.45f, 0.05f);
    float3 c2 = float3(0.65f, 0.06f, 0.02f);
    float3 color = lerp(c0, c1, smoothstep(0.0f, 0.45f, u));
    color = lerp(color, c2, smoothstep(0.45f, 1.0f, u));

    float flicker = 1.0f + sin(time * 47.0f + u * 13.0f) * flickerStrength * 0.10f;
    float alpha = saturate(lerp(edgeHeat * 0.35f, core, 0.72f) * lengthFade);
    if (alpha <= 0.01f)
    {
        discard;
    }

    return MakeOutput(float4(color * brightness * flicker, alpha));
}
