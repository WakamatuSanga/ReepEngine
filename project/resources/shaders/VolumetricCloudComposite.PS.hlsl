Texture2D<float4> gCloudTexture : register(t0);
Texture2D<float> gDepthTexture : register(t1);
SamplerState gSampler : register(s0);

struct CloudCompositeConstants
{
    float2 cloudTextureSize;
    float2 outputTextureSize;
    uint enableDepthAwareUpsample;
    float depthThreshold;
    float2 padding;
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

    if (gComposite.enableDepthAwareUpsample != 0u)
    {
        float2 texel = 1.0f / max(gComposite.outputTextureSize, float2(1.0f, 1.0f));
        float centerDepth = LoadDepth(uv);
        float maxDepthDiff = 0.0f;
        maxDepthDiff = max(maxDepthDiff, abs(LoadDepth(uv + float2(texel.x, 0.0f)) - centerDepth));
        maxDepthDiff = max(maxDepthDiff, abs(LoadDepth(uv - float2(texel.x, 0.0f)) - centerDepth));
        maxDepthDiff = max(maxDepthDiff, abs(LoadDepth(uv + float2(0.0f, texel.y)) - centerDepth));
        maxDepthDiff = max(maxDepthDiff, abs(LoadDepth(uv - float2(0.0f, texel.y)) - centerDepth));

        if (maxDepthDiff > gComposite.depthThreshold)
        {
            cloud = LoadPointCloud(uv);
        }
    }

    return cloud;
}
