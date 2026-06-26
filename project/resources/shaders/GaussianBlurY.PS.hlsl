#include "CopyImage.hlsli"

Texture2D<float4> gTexture : register(t0);
Texture2D<float> gDepthTexture : register(t1);
Texture2D<float4> gNormalTexture : register(t2);
Texture2D<float4> gDissolveNoiseTexture : register(t3);
SamplerState gSampler : register(s0);
ConstantBuffer<PostEffectParameters> gPostEffectParameters : register(b0);

float3 ApplyGaussianBlurVertical(float2 texcoord)
{
    uint width, height;
    gTexture.GetDimensions(width, height);

    float radius = gPostEffectParameters.gaussianIntensity;
    float2 texelOffset = float2(0.0f, 1.0f / height) * radius;

    float3 color = gTexture.Sample(gSampler, texcoord).rgb * 0.375f;
    color += gTexture.Sample(gSampler, texcoord + texelOffset).rgb * 0.25f;
    color += gTexture.Sample(gSampler, texcoord - texelOffset).rgb * 0.25f;
    color += gTexture.Sample(gSampler, texcoord + texelOffset * 2.0f).rgb * 0.0625f;
    color += gTexture.Sample(gSampler, texcoord - texelOffset * 2.0f).rgb * 0.0625f;

    return color;
}

float3 SampleRadialBlurSource(float2 texcoord)
{
    return ApplyGaussianBlurVertical(texcoord);
}

float3 ApplyRadialBlur(float2 texcoord, float3 baseColor)
{
    if (gPostEffectParameters.radialBlurEnabled == 0 || gPostEffectParameters.radialBlurStrength <= 0.0001f) {
        return baseColor;
    }

    uint sampleCount = clamp(gPostEffectParameters.radialBlurSampleCount, 1u, 32u);
    if (sampleCount <= 1u) {
        return baseColor;
    }

    float2 direction = texcoord - gPostEffectParameters.radialBlurCenter;
    float centerClearRadius = max(gPostEffectParameters.radialBlurCenterClearRadius, 0.0f);
    float outerEffectRadius = max(gPostEffectParameters.radialBlurOuterEffectRadius, centerClearRadius + 0.0001f);
    float centerMask = smoothstep(centerClearRadius, outerEffectRadius, length(direction));
    float effectiveStrength = gPostEffectParameters.radialBlurStrength * centerMask;
    if (effectiveStrength <= 0.0001f) {
        return baseColor;
    }

    float3 accumulatedColor = baseColor;

    [loop]
    for (uint i = 1; i < sampleCount; ++i) {
        float t = (float)i / (float)(sampleCount - 1u);
        float2 sampleUV = texcoord - direction * (effectiveStrength * t);
        accumulatedColor += SampleRadialBlurSource(sampleUV);
    }

    return accumulatedColor / float(sampleCount);
}

float3 ApplySmoothing(float2 texcoord, float3 baseColor, float intensity)
{
    uint width, height;
    gTexture.GetDimensions(width, height);
    float2 texelSize = 1.0f / float2(width, height);

    float3 accumulatedColor = 0.0f;
    [unroll]
    for (int y = -1; y <= 1; ++y) {
        [unroll]
        for (int x = -1; x <= 1; ++x) {
            float2 offset = float2(x, y) * texelSize;
            accumulatedColor += gTexture.Sample(gSampler, texcoord + offset).rgb;
        }
    }

    float3 smoothedColor = accumulatedColor / 9.0f;
    return lerp(baseColor, smoothedColor, saturate(intensity));
}

float3 ApplyGrayscale(float3 color, float intensity)
{
    float luminance = dot(color, float3(0.2125f, 0.7154f, 0.0721f));
    return lerp(color, luminance.xxx, saturate(intensity));
}

float3 ApplySepia(float3 color, float intensity)
{
    float3 sepiaColor;
    sepiaColor.r = dot(color, float3(0.393f, 0.769f, 0.189f));
    sepiaColor.g = dot(color, float3(0.349f, 0.686f, 0.168f));
    sepiaColor.b = dot(color, float3(0.272f, 0.534f, 0.131f));
    return lerp(color, saturate(sepiaColor), saturate(intensity));
}

float3 ApplyInvert(float3 color, float intensity)
{
    return lerp(color, 1.0f - color, saturate(intensity));
}

float ApplyVignette(float2 texcoord, float intensity)
{
    float2 centered = texcoord - 0.5f;
    float distanceFromCenter = length(centered) * 1.41421356f;
    float vignette = saturate(1.0f - distanceFromCenter * distanceFromCenter);
    vignette = pow(vignette, 1.5f);
    return lerp(1.0f, vignette, saturate(intensity));
}

float3 SampleOutlineSource(float2 texcoord)
{
    return gTexture.Sample(gSampler, texcoord).rgb;
}

float ApplyOutlineResponse(float edge)
{
    float threshold = gPostEffectParameters.outlineThreshold;
    float softness = max(gPostEffectParameters.outlineSoftness, 0.0001f);
    edge = smoothstep(threshold, threshold + softness, edge);
    return saturate(edge * gPostEffectParameters.outlineIntensity);
}

float ApplyDepthOutlineResponse(float edge)
{
    float threshold = gPostEffectParameters.outlineDepthThreshold;
    float softness = max(gPostEffectParameters.outlineSoftness, 0.0001f);
    edge = smoothstep(threshold, threshold + softness, edge);
    edge *= gPostEffectParameters.outlineDepthStrength;
    return saturate(edge * gPostEffectParameters.outlineIntensity);
}

float ApplyNormalOutlineResponse(float edge)
{
    float threshold = gPostEffectParameters.outlineNormalThreshold;
    float softness = max(gPostEffectParameters.outlineSoftness, 0.0001f);
    edge = smoothstep(threshold, threshold + softness, edge);
    edge *= gPostEffectParameters.outlineNormalStrength;
    return saturate(edge * gPostEffectParameters.outlineIntensity);
}

float ComputeColorDiff8Edge(float2 texcoord, float2 thickness)
{
    float3 center = SampleOutlineSource(texcoord);
    float3 left = SampleOutlineSource(texcoord + float2(-thickness.x, 0.0f));
    float3 right = SampleOutlineSource(texcoord + float2(thickness.x, 0.0f));
    float3 up = SampleOutlineSource(texcoord + float2(0.0f, -thickness.y));
    float3 down = SampleOutlineSource(texcoord + float2(0.0f, thickness.y));
    float3 upLeft = SampleOutlineSource(texcoord + float2(-thickness.x, -thickness.y));
    float3 upRight = SampleOutlineSource(texcoord + float2(thickness.x, -thickness.y));
    float3 downLeft = SampleOutlineSource(texcoord + float2(-thickness.x, thickness.y));
    float3 downRight = SampleOutlineSource(texcoord + float2(thickness.x, thickness.y));

    float edge = 0.0f;
    edge = max(edge, length(center - left));
    edge = max(edge, length(center - right));
    edge = max(edge, length(center - up));
    edge = max(edge, length(center - down));
    edge = max(edge, length(center - upLeft));
    edge = max(edge, length(center - upRight));
    edge = max(edge, length(center - downLeft));
    edge = max(edge, length(center - downRight));

    return edge;
}

float SampleLuminance(float2 texcoord)
{
    return dot(SampleOutlineSource(texcoord), float3(0.2125f, 0.7154f, 0.0721f));
}

float ComputeSobelEdge(float2 texcoord, float2 thickness)
{
    float topLeft = SampleLuminance(texcoord + float2(-thickness.x, -thickness.y));
    float top = SampleLuminance(texcoord + float2(0.0f, -thickness.y));
    float topRight = SampleLuminance(texcoord + float2(thickness.x, -thickness.y));
    float left = SampleLuminance(texcoord + float2(-thickness.x, 0.0f));
    float right = SampleLuminance(texcoord + float2(thickness.x, 0.0f));
    float bottomLeft = SampleLuminance(texcoord + float2(-thickness.x, thickness.y));
    float bottom = SampleLuminance(texcoord + float2(0.0f, thickness.y));
    float bottomRight = SampleLuminance(texcoord + float2(thickness.x, thickness.y));

    float gx = (-topLeft) + topRight
        + (-2.0f * left) + (2.0f * right)
        + (-bottomLeft) + bottomRight;

    float gy = (-topLeft) + (-2.0f * top) + (-topRight)
        + bottomLeft + (2.0f * bottom) + bottomRight;

    return length(float2(gx, gy));
}

float SampleDepth(float2 texcoord)
{
    return gDepthTexture.Sample(gSampler, texcoord);
}

float3 SampleNormal(float2 texcoord)
{
    float3 normal = gNormalTexture.Sample(gSampler, texcoord).xyz * 2.0f - 1.0f;
    float lengthSquared = dot(normal, normal);
    if (lengthSquared > 0.0001f) {
        normal *= rsqrt(lengthSquared);
    } else {
        normal = 0.0f;
    }
    return normal;
}

float LinearizeDepth(float depth)
{
    float nearZ = gPostEffectParameters.depthNear;
    float farZ = gPostEffectParameters.depthFar;
    float linearDepth = (nearZ * farZ) / max(farZ - depth * (farZ - nearZ), 0.0001f);
    return saturate((linearDepth - nearZ) / max(farZ - nearZ, 0.0001f));
}

float SampleLinearDepth(float2 texcoord)
{
    return LinearizeDepth(SampleDepth(texcoord));
}

float ComputeDepthEdge(float2 texcoord, float2 thickness)
{
    float centerDepth = SampleLinearDepth(texcoord);
    float leftDepth = SampleLinearDepth(texcoord + float2(-thickness.x, 0.0f));
    float rightDepth = SampleLinearDepth(texcoord + float2(thickness.x, 0.0f));
    float upDepth = SampleLinearDepth(texcoord + float2(0.0f, -thickness.y));
    float downDepth = SampleLinearDepth(texcoord + float2(0.0f, thickness.y));

    float edge = 0.0f;
    edge = max(edge, abs(centerDepth - leftDepth));
    edge = max(edge, abs(centerDepth - rightDepth));
    edge = max(edge, abs(centerDepth - upDepth));
    edge = max(edge, abs(centerDepth - downDepth));

    return edge;
}

float ComputeNormalEdge(float2 texcoord, float2 thickness)
{
    float3 centerNormal = SampleNormal(texcoord);
    float3 leftNormal = SampleNormal(texcoord + float2(-thickness.x, 0.0f));
    float3 rightNormal = SampleNormal(texcoord + float2(thickness.x, 0.0f));
    float3 upNormal = SampleNormal(texcoord + float2(0.0f, -thickness.y));
    float3 downNormal = SampleNormal(texcoord + float2(0.0f, thickness.y));

    float edge = 0.0f;
    edge = max(edge, length(centerNormal - leftNormal));
    edge = max(edge, length(centerNormal - rightNormal));
    edge = max(edge, length(centerNormal - upNormal));
    edge = max(edge, length(centerNormal - downNormal));
    return edge;
}

float ComputeHybridEdge(float2 texcoord, float2 thickness)
{
    float edgeColor = 0.0f;
    if (gPostEffectParameters.hybridColorSource == 1) {
        edgeColor = ComputeColorDiff8Edge(texcoord, thickness);
    } else {
        edgeColor = ComputeSobelEdge(texcoord, thickness);
    }

    float edgeDepth = ComputeDepthEdge(texcoord, thickness);

    edgeColor = ApplyOutlineResponse(edgeColor);
    edgeDepth = ApplyDepthOutlineResponse(edgeDepth);

    return saturate(
        edgeColor * gPostEffectParameters.hybridColorWeight +
        edgeDepth * gPostEffectParameters.hybridDepthWeight);
}

float ComputeFinalHybridEdge(float2 texcoord, float2 thickness)
{
    float edgeColor = 0.0f;
    if (gPostEffectParameters.hybridColorSource == 1) {
        edgeColor = ComputeColorDiff8Edge(texcoord, thickness);
    } else {
        edgeColor = ComputeSobelEdge(texcoord, thickness);
    }

    float edgeDepth = ComputeDepthEdge(texcoord, thickness);
    float edgeNormal = ComputeNormalEdge(texcoord, thickness);

    edgeColor = ApplyOutlineResponse(edgeColor);
    edgeDepth = ApplyDepthOutlineResponse(edgeDepth);
    edgeNormal = ApplyNormalOutlineResponse(edgeNormal);

    return saturate(
        edgeColor * gPostEffectParameters.hybridColorWeight +
        edgeDepth * gPostEffectParameters.hybridDepthWeight +
        edgeNormal * gPostEffectParameters.hybridNormalWeight);
}

float3 ApplyOutline(float2 texcoord, float3 baseColor)
{
    uint width, height;
    gTexture.GetDimensions(width, height);
    float2 texelSize = 1.0f / float2(width, height);
    float2 thickness = texelSize * max(gPostEffectParameters.outlineThickness, 0.5f);

    float edge = 0.0f;
    if (gPostEffectParameters.outlineMode == 1) {
        edge = ComputeColorDiff8Edge(texcoord, thickness);
        edge = ApplyOutlineResponse(edge);
    } else if (gPostEffectParameters.outlineMode == 2) {
        edge = ComputeSobelEdge(texcoord, thickness);
        edge = ApplyOutlineResponse(edge);
    } else if (gPostEffectParameters.outlineMode == 3) {
        edge = ComputeDepthEdge(texcoord, thickness);
        edge = ApplyDepthOutlineResponse(edge);
    } else if (gPostEffectParameters.outlineMode == 4) {
        edge = ComputeHybridEdge(texcoord, thickness);
    } else if (gPostEffectParameters.outlineMode == 5) {
        edge = ComputeNormalEdge(texcoord, thickness);
        edge = ApplyNormalOutlineResponse(edge);
    } else if (gPostEffectParameters.outlineMode == 6) {
        edge = ComputeFinalHybridEdge(texcoord, thickness);
    }

    return lerp(baseColor, gPostEffectParameters.outlineColor.rgb, edge);
}

float3 ApplyDissolve(float2 texcoord, float3 baseColor)
{
    if (gPostEffectParameters.dissolveEnabled == 0) {
        return baseColor;
    }

    float noise = gDissolveNoiseTexture.Sample(gSampler, texcoord).r;
    float threshold = saturate(gPostEffectParameters.dissolveThreshold);
    float edgeWidth = max(gPostEffectParameters.dissolveEdgeWidth, 0.0001f);

    if (noise < threshold) {
        discard;
    }

    float edgeMask = 1.0f - smoothstep(0.0f, edgeWidth, noise - threshold);
    return lerp(baseColor, gPostEffectParameters.dissolveEdgeColor.rgb, saturate(edgeMask));
}

float4 main(VertexShaderOutput input) : SV_TARGET
{
    float4 sampledColor = gTexture.Sample(gSampler, input.texcoord);
    float3 color = ApplyGaussianBlurVertical(input.texcoord);

    float smoothingIntensity = gPostEffectParameters.smoothingIntensity * gPostEffectParameters.smoothingEnabled;
    float grayscaleIntensity = gPostEffectParameters.grayscaleIntensity * gPostEffectParameters.grayscaleEnabled;
    float sepiaIntensity = gPostEffectParameters.sepiaIntensity * gPostEffectParameters.sepiaEnabled;
    float invertIntensity = gPostEffectParameters.invertIntensity * gPostEffectParameters.invertEnabled;
    float vignetteIntensity = gPostEffectParameters.vignetteIntensity * gPostEffectParameters.vignetteEnabled;

    color = ApplyRadialBlur(input.texcoord, color);
    color = ApplySmoothing(input.texcoord, color, smoothingIntensity);
    color = ApplyGrayscale(color, grayscaleIntensity);
    color = ApplySepia(color, sepiaIntensity);
    color = ApplyInvert(color, invertIntensity);
    color *= ApplyVignette(input.texcoord, vignetteIntensity);
    if (gPostEffectParameters.outlineMode != 0) {
        color = ApplyOutline(input.texcoord, color);
    }
    color = ApplyDissolve(input.texcoord, color);

    return float4(color, sampledColor.a);
}
