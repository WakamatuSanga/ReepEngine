#include "CloudSeaBackground.hlsli"
struct TransformationMatrix {
    float4x4 WVP;
    float4 styleControl,styleZenith,styleHorizon,styleCloudShadow,styleCloudHighlight,styleSun,styleSunColor,styleCumulus,styleCamera,styleCumulusExtra,styleCumulusDistribution,styleCloudRelief,styleCloudLayout;
    float4 blueFraming;
};
ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);
TextureCube<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float3 direction : TEXCOORD0;
    float3 clipXYW : TEXCOORD1;
    nointerpolation float blueFramingStrength : TEXCOORD2;
};

float4 main(VertexShaderOutput input) : SV_TARGET
{
    float4 color = gTexture.Sample(gSampler, normalize(input.direction));
    if (input.blueFramingStrength > 0.0f)
    {
        float2 ndc = input.clipXYW.xy / input.clipXYW.z;
        float sides = smoothstep(0.15f, 1.0f, abs(ndc.x));
        float top = smoothstep(0.0f, 1.0f, ndc.y);
        float edge = 1.0f - (1.0f - sides) * (1.0f - top);
        // Color only the blue sky, preserving the cube texture's white clouds.
        float skyMask = smoothstep(0.02f, 0.18f, color.b - color.r);
        float amount = saturate(input.blueFramingStrength) * skyMask;
        float3 blue = lerp(float3(0.24f, 0.55f, 0.82f),
                           float3(0.025f, 0.18f, 0.48f), edge);
        color.rgb = lerp(color.rgb, blue, amount);
    }
    if(gTransformationMatrix.styleControl.x>0) {
        float3 ray=normalize(input.direction);
        float skyMask=smoothstep(.02,.18,color.b-color.r);
        float altitude=smoothstep(-.05,.65,ray.y);
        if(gTransformationMatrix.styleSun.w>0)altitude=smoothstep(-.40,.20,ray.y);
        float3 tint=lerp(gTransformationMatrix.styleHorizon.rgb,gTransformationMatrix.styleZenith.rgb,altitude);
        color.rgb=lerp(color.rgb,tint,(gTransformationMatrix.styleCumulusDistribution.w>0?1:skyMask)*gTransformationMatrix.styleControl.x);
    }
    if(gTransformationMatrix.styleCumulus.x>0) {
        float4 cloud=DistantCumulus(gTransformationMatrix.styleCamera.xyz,normalize(input.direction),gTransformationMatrix.styleCumulus,gTransformationMatrix.styleSun.xyz,gTransformationMatrix.styleSunColor.rgb,color.rgb,gTransformationMatrix.styleCumulusExtra,gTransformationMatrix.styleCumulusDistribution.x,gTransformationMatrix.styleCloudShadow.rgb,gTransformationMatrix.styleCloudHighlight.rgb);
        color.rgb=lerp(color.rgb,cloud.rgb,cloud.a);
    }
    return color;
}
