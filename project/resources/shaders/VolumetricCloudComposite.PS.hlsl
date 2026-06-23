Texture2D<float4> gCloudTexture : register(t0);
Texture2D<float> gDepthTexture : register(t1);
SamplerState gSampler : register(s0);

struct CloudCompositeConstants
{
    float2 cloudTextureSize;
    float2 outputTextureSize;
    uint enableDepthAwareUpsample;
    uint enableGameplayObjectPreserve;
    uint enableCloudDepthTest;
    uint enableGameplayObjectMask;
    float depthThreshold;
    float cloudOverGameplayObjectStrength;
    float foregroundCloudAlphaReduction;
    uint compositeDebugMode;
};

ConstantBuffer<CloudCompositeConstants> gComposite : register(b0);

struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

float LoadDepth(float2 uv)
{
    float2 textureSize = max(gComposite.outputTextureSize, float2(1.0f, 1.0f));
    uint2 coord = min(uint2(saturate(uv) * textureSize), uint2(textureSize - 1.0f));
    return gDepthTexture.Load(int3(coord, 0));
}

float4 LoadPointCloud(float2 uv)
{
    float2 textureSize = max(gComposite.cloudTextureSize, float2(1.0f, 1.0f));
    uint2 coord = min(uint2(saturate(uv) * textureSize), uint2(textureSize - 1.0f));
    return gCloudTexture.Load(int3(coord, 0));
}

float4 main(VertexShaderOutput input) : SV_TARGET0
{
    float2 uv = saturate(input.texcoord);
    float4 cloud = gCloudTexture.Sample(gSampler, uv);
    float sceneDepth = LoadDepth(uv);
    float preserveMask = 0.0f;

    if (gComposite.enableDepthAwareUpsample != 0u)
    {
        float2 texel = 1.0f / max(gComposite.outputTextureSize, float2(1.0f, 1.0f));
        float maxDepthDiff = 0.0f;
        maxDepthDiff = max(maxDepthDiff, abs(LoadDepth(uv + float2(texel.x, 0.0f)) - sceneDepth));
        maxDepthDiff = max(maxDepthDiff, abs(LoadDepth(uv - float2(texel.x, 0.0f)) - sceneDepth));
        maxDepthDiff = max(maxDepthDiff, abs(LoadDepth(uv + float2(0.0f, texel.y)) - sceneDepth));
        maxDepthDiff = max(maxDepthDiff, abs(LoadDepth(uv - float2(0.0f, texel.y)) - sceneDepth));

        if (maxDepthDiff > gComposite.depthThreshold)
        {
            cloud = LoadPointCloud(uv);
        }
    }

    if (gComposite.enableGameplayObjectPreserve != 0u && gComposite.enableCloudDepthTest != 0u)
    {
        preserveMask = (sceneDepth < 0.99999f) ? 1.0f : 0.0f;
    }

    float4 preserveResult = cloud;
    if (preserveMask > 0.0f)
    {
        float preserveStrength = saturate(gComposite.cloudOverGameplayObjectStrength);
        float reductionStrength = saturate(1.0f - gComposite.foregroundCloudAlphaReduction);
        float cloudScale = min(preserveStrength, reductionStrength);
        preserveResult.rgb *= cloudScale;
        preserveResult.a *= cloudScale;
    }

    if (gComposite.compositeDebugMode == 1u)
    {
        return float4(cloud.a.xxx, 1.0f);
    }
    if (gComposite.compositeDebugMode == 2u)
    {
        return float4(preserveMask.xxx, 1.0f);
    }
    if (gComposite.compositeDebugMode == 3u)
    {
        return float4(preserveResult.rgb + preserveMask.xxx * 0.25f, saturate(preserveResult.a + preserveMask * 0.25f));
    }

    return preserveResult;
}
