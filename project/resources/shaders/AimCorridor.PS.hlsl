struct AimCorridorConstants
{
    float4x4 viewProjection;
    float4 appearance; // x: base alpha, y: core intensity, z: glow intensity, w: glow alpha
    float4 sampling;   // xy: inverse texture size, z: glow radius in texels, w: pulse scale
    float4 flags;      // x: disable glow, y: show core only
    float4 coreTint;   // rgb: tint color, a: tint blend amount
    float4 glowTint;   // rgb: glow tint color
};

ConstantBuffer<AimCorridorConstants> gAimCorridor : register(b0);
Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

float4 main(PixelShaderInput input) : SV_TARGET0
{
    float4 center = gTexture.Sample(gSampler, input.uv);
    float baseAlpha = saturate(gAimCorridor.appearance.x);
    float coreIntensity = max(gAimCorridor.appearance.y, 0.0f);
    float pulseScale = max(gAimCorridor.sampling.w, 0.0f);
    float coreMask = center.a;
    float glowMask = 0.0f;
    float3 glowSource = center.rgb;

    bool glowEnabled = gAimCorridor.flags.x < 0.5f && gAimCorridor.flags.y < 0.5f;
    if (glowEnabled)
    {
        float2 texelOffset = gAimCorridor.sampling.xy * max(gAimCorridor.sampling.z, 0.0f);
        static const int2 offsets[8] =
        {
            int2(-1, 0), int2(1, 0), int2(0, -1), int2(0, 1),
            int2(-1, -1), int2(1, -1), int2(-1, 1), int2(1, 1)
        };

        float maxNeighborAlpha = 0.0f;
        [unroll]
        for (uint i = 0; i < 8; ++i)
        {
            float4 neighbor = gTexture.Sample(gSampler, input.uv + float2(offsets[i]) * texelOffset);
            if (neighbor.a > maxNeighborAlpha)
            {
                maxNeighborAlpha = neighbor.a;
                glowSource = neighbor.rgb;
            }
        }
        glowMask = saturate(maxNeighborAlpha - coreMask * 0.5f);
    }

    float glowIntensity = max(gAimCorridor.appearance.z, 0.0f);
    float glowAlpha = saturate(gAimCorridor.appearance.w);
    float tintAmount = saturate(gAimCorridor.coreTint.a);
    float3 coreSource = lerp(center.rgb, gAimCorridor.coreTint.rgb * coreMask, tintAmount);
    float3 tintedGlowSource = lerp(glowSource, gAimCorridor.glowTint.rgb, tintAmount);
    float3 coreColor = coreSource * coreIntensity;
    float3 glowColor = tintedGlowSource * glowMask * glowIntensity;
    float3 outputColor = saturate((coreColor + glowColor) * pulseScale);
    float outputAlpha = saturate((coreMask + glowMask * glowAlpha) * baseAlpha);
    if (outputAlpha <= 0.001f)
    {
        discard;
    }
    return float4(outputColor, outputAlpha);
}
