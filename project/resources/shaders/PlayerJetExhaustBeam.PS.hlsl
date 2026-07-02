struct BeamConstants
{
    float4x4 viewProjection;
    float4 params;        // x: brightness, y: flicker strength, z: time, w: mode
    float4 qualityParams; // x: alpha scale, y: edge softness, z: tip fade power, w: unused
};

ConstantBuffer<BeamConstants> gBeam : register(b0);

struct PixelShaderInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

float4 MakeOutput(float4 color) : SV_TARGET0
{
    return color;
}

float4 main(PixelShaderInput input) : SV_TARGET0
{
    float brightness = max(gBeam.params.x, 0.0f);
    float flickerStrength = saturate(gBeam.params.y);
    float time = gBeam.params.z;
    uint mode = uint(gBeam.params.w + 0.5f);
    float alphaScale = saturate(gBeam.qualityParams.x);
    float edgeSoftness = max(gBeam.qualityParams.y, 0.5f);
    float tipFadePower = max(gBeam.qualityParams.z, 0.2f);

    if (mode == 1u)
    {
        float2 centered = input.uv * 2.0f - 1.0f;
        float radial = saturate(1.0f - dot(centered, centered));
        float alpha = pow(radial, 2.8f) * alphaScale;
        if (alpha <= 0.01f)
        {
            discard;
        }
        float flicker = 1.0f + sin(time * 43.0f) * flickerStrength * 0.04f;
        return MakeOutput(float4(float3(1.0f, 0.94f, 0.58f) * brightness * flicker, alpha * 0.62f));
    }

    if (mode == 2u)
    {
        float u = saturate(input.uv.x);
        float edge = abs(input.uv.y * 2.0f - 1.0f);
        float band = pow(saturate(1.0f - edge), 1.45f);
        float angularFlicker = 1.0f + sin(u * 37.0f + time * 18.0f) * flickerStrength * 0.08f;
        float alpha = band * alphaScale;
        if (alpha <= 0.01f)
        {
            discard;
        }
        float3 rimColor = lerp(float3(0.45f, 0.82f, 1.0f), float3(1.0f, 1.0f, 1.0f), band);
        return MakeOutput(float4(rimColor * brightness * angularFlicker, alpha));
    }

    if (mode == 3u)
    {
        float u = saturate(input.uv.x);
        float edge = abs(input.uv.y * 2.0f - 1.0f);
        float band = pow(saturate(1.0f - edge), edgeSoftness);
        float pulse = 1.0f + sin(time * 58.0f + u * 11.0f) * flickerStrength * 0.08f;
        float alpha = band * alphaScale;
        if (alpha <= 0.01f)
        {
            discard;
        }
        float3 laserColor = lerp(float3(1.0f, 0.08f, 0.05f), float3(1.0f, 0.92f, 0.88f), pow(band, 2.2f));
        return MakeOutput(float4(laserColor * brightness * pulse, alpha));
    }
    float u = saturate(input.uv.x);
    float v = abs(input.uv.y * 2.0f - 1.0f);
    float center = saturate(1.0f - v);
    float core = pow(center, edgeSoftness);
    float edgeHeat = pow(center, 1.2f);
    float tipFade = pow(saturate(1.0f - smoothstep(0.0f, 1.0f, u)), tipFadePower);

    float3 c0 = float3(1.0f, 0.95f, 0.65f);
    float3 c1 = float3(1.0f, 0.45f, 0.05f);
    float3 c2 = float3(0.65f, 0.06f, 0.02f);
    float3 color = lerp(c0, c1, smoothstep(0.0f, 0.45f, u));
    color = lerp(color, c2, smoothstep(0.45f, 1.0f, u));

    float flicker = 1.0f + sin(time * 37.0f + u * 9.0f) * flickerStrength * 0.06f;
    float alpha = saturate(lerp(edgeHeat * 0.22f, core, 0.80f) * tipFade * alphaScale);
    if (alpha <= 0.01f)
    {
        discard;
    }

    return MakeOutput(float4(color * brightness * flicker, alpha));
}
