#include "Sprite.hlsli"

struct Material
{
    float32_t4 color;
    int32_t enableLighting;
    float32_t3 padding;
    float32_t4x4 uvTransform;
    float4 gradientStartColor;
    float4 gradientEndColor;
    float4 gradientPoints;
    float4 whiteSelection;
    float4 gradientControl;
    float4 textureAlphaScale;
};
// b0: マテリアル
ConstantBuffer<Material> gMaterial : register(b0);

// t0: テクスチャ
Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);
SamplerState gClampSampler : register(s1);

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
	
	// UV変換
    float32_t4 transformedUV = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
	
	// テクスチャサンプリング
    float32_t4 textureColor = gMaterial.gradientControl.w > 0.5f
        ? gTexture.Sample(gClampSampler, transformedUV.xy)
        : gTexture.Sample(gSampler, transformedUV.xy);

    if (gMaterial.gradientControl.x > 0.5f)
    {
        // SRGB texture sampling already returns linear RGB. Select before tinting.
        float brightest = max(textureColor.r, max(textureColor.g, textureColor.b));
        float darkest = min(textureColor.r, min(textureColor.g, textureColor.b));
        float saturation = (brightest - darkest) / max(brightest, 0.0001f);
        float brightness = dot(textureColor.rgb, float3(0.2126f, 0.7152f, 0.0722f));
        float4 selection = gMaterial.whiteSelection;
        float whiteMask = smoothstep(selection.x - selection.z, selection.x + selection.z, brightness)
            * (1.0f - smoothstep(selection.y - selection.w, selection.y + selection.w, saturation));
        float2 direction = gMaterial.gradientPoints.zw - gMaterial.gradientPoints.xy;
        // Coincident endpoints select the start color; no division by zero.
        float t = saturate(dot(input.localPosition - gMaterial.gradientPoints.xy, direction)
            / max(dot(direction, direction), 0.00000001f));
        float3 gradient = lerp(gMaterial.gradientStartColor.rgb, gMaterial.gradientEndColor.rgb, t);
        textureColor.rgb = lerp(textureColor.rgb, gradient, whiteMask * gMaterial.gradientControl.y);
    }
	
	// 色合成
    output.color = gMaterial.color * textureColor;
    if (gMaterial.textureAlphaScale.w > 0.5f)
    {
        // Straight-alpha HUD PNG: green detail and dark blue panels retain
        // their original per-pixel opacity; default multipliers are both 1.
        output.color.a *= textureColor.g > 0.2f ? gMaterial.textureAlphaScale.y : gMaterial.textureAlphaScale.x;
    }
	
    if (gMaterial.gradientControl.z > 0.0f)
    {
        float edge = gMaterial.gradientControl.z;
        output.color.a *= smoothstep(0.0f, edge, input.localPosition.y)
            * smoothstep(0.0f, edge, 1.0f - input.localPosition.y);
    }
    return output;
}
