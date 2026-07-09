#include "CopyImage.hlsli"

Texture2D<float4> gSceneTexture : register(t0);
SamplerState gSampler : register(s0);

struct ImpactInstance
{
    float4 data0; // center uv.xy, radius, normalized age
    float4 data1; // distortion, thickness, ring, chromatic
    float4 data2; // flash, type, unused, unused
};

cbuffer ImpactDistortionConstants : register(b0)
{
    float4 gScreenSizeAndOptions; // width, height, enabled, show marker
    float4 gCommonParams;         // count, force strong, disable flags, quality
    ImpactInstance gInstances[32];
};

float RingMask(float distanceToCenter, float radius, float thickness)
{
    float d = abs(distanceToCenter - radius);
    float inner = max(thickness, 0.0001f);
    return 1.0f - smoothstep(inner, inner * 2.4f, d);
}

float4 main(VertexShaderOutput input) : SV_TARGET
{
    float2 uv = input.texcoord;
    if (gScreenSizeAndOptions.z < 0.5f || gCommonParams.x < 0.5f) {
        return gSceneTexture.Sample(gSampler, uv);
    }

    float2 screenSize = max(gScreenSizeAndOptions.xy, float2(1.0f, 1.0f));
    float aspect = screenSize.x / screenSize.y;
    uint instanceCount = min((uint)gCommonParams.x, 32u);
    float forceStrong = gCommonParams.y > 0.5f ? 1.55f : 1.0f;
    bool disableChromatic = fmod(gCommonParams.z, 2.0f) >= 1.0f;
    bool disableRing = fmod(floor(gCommonParams.z / 2.0f), 2.0f) >= 1.0f;
    bool disableFlash = fmod(floor(gCommonParams.z / 4.0f), 2.0f) >= 1.0f;
    bool visualQuality = gCommonParams.w > 0.5f;

    float2 totalOffset = 0.0f;
    float2 chromaticOffsetAccum = 0.0f;
    float3 ringColor = 0.0f;
    float flash = 0.0f;

    [loop]
    for (uint i = 0u; i < instanceCount; ++i) {
        ImpactInstance instance = gInstances[i];
        float2 center = instance.data0.xy;
        float radius = instance.data0.z;
        float age = saturate(instance.data0.w);
        float fade = saturate(1.0f - age);
        fade *= fade;

        float2 delta = uv - center;
        float2 aspectDelta = float2(delta.x * aspect, delta.y);
        float distanceToCenter = length(aspectDelta);
        float2 direction = distanceToCenter > 0.00001f ? delta / max(length(delta), 0.00001f) : float2(0.0f, 1.0f);
        float mask = RingMask(distanceToCenter, radius, instance.data1.y);
        float wakeMask = 1.0f - smoothstep(radius, radius + instance.data1.y * 4.0f, distanceToCenter);
        wakeMask *= smoothstep(radius * 0.25f, radius, distanceToCenter);

        float distortion = instance.data1.x * forceStrong * fade;
        if (visualQuality) {
            distortion *= 1.18f;
        }
        float ripple = (mask * 0.85f + wakeMask * 0.15f);
        totalOffset += direction * distortion * ripple;
        chromaticOffsetAccum += direction * mask * instance.data1.w * fade * forceStrong;

        float type = instance.data2.y;
        float3 tint = lerp(float3(0.35f, 0.85f, 1.0f), float3(1.0f, 0.48f, 0.16f), type);
        if (!disableRing) {
            ringColor += tint * mask * instance.data1.z * fade * forceStrong;
        }
        if (!disableFlash) {
            flash += mask * instance.data2.x * fade * forceStrong;
        }
        if (gScreenSizeAndOptions.w > 0.5f) {
            float marker = 1.0f - smoothstep(0.0025f, 0.0065f, length(aspectDelta));
            ringColor += float3(0.1f, 1.0f, 0.35f) * marker;
        }
    }

    float2 distortedUv = saturate(uv + totalOffset);
    float4 baseColor = gSceneTexture.Sample(gSampler, distortedUv);
    float3 color = baseColor.rgb;

    if (!disableChromatic) {
        float chromaticStrength = visualQuality ? 1.25f : 1.0f;
        float2 chromaticOffset = chromaticOffsetAccum * chromaticStrength;
        float red = gSceneTexture.Sample(gSampler, saturate(distortedUv + chromaticOffset)).r;
        float blue = gSceneTexture.Sample(gSampler, saturate(distortedUv - chromaticOffset)).b;
        color.r = red;
        color.b = blue;
    }

    color += ringColor;
    color = lerp(color, float3(1.0f, 1.0f, 1.0f), saturate(flash));
    return float4(saturate(color), baseColor.a);
}
