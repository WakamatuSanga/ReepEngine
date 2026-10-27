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
    float4 screenNoise;
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

float StaticNoise(float2 pixelPosition)
{
    float tick = floor(gMaterial.screenNoise.y * 30.0f);
    float2 cell = floor(pixelPosition / max(gMaterial.screenNoise.z, 1.0f));
    return frac(sin(dot(cell + float2(tick * 13.37f, tick * 7.19f),
        float2(12.9898f, 78.233f))) * 43758.5453f);
}

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    if (gMaterial.screenNoise.x > 0.5f && gMaterial.screenNoise.x < 1.5f)
    {
        // Reuse the existing Object3d random-noise hash for TV-style static.
        // Screen pixels keep the grain size independent of Game View aspect.
        float tick = floor(gMaterial.screenNoise.y * 30.0f);
        float noise = StaticNoise(input.position.xy);
        float row = floor(input.position.y / 2.0f);
        float lineNoise = frac(sin(dot(float2(row, tick), float2(12.9898f, 78.233f))) * 43758.5453f);
        float interference = step(0.975f, lineNoise);
        float grey = max(step(0.5f, noise), interference);
        output.color = float4(gMaterial.color.rgb * grey,
            saturate(gMaterial.color.a * (0.35f + 0.65f * abs(noise * 2.0f - 1.0f))
                + interference * gMaterial.screenNoise.w * gMaterial.color.a));
        return output;
    }
	
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
	
    if (gMaterial.textureAlphaScale.z > 0.5f)
    {
        output.color.rgb = gMaterial.color.rgb * max(textureColor.r, max(textureColor.g, textureColor.b));
    }
    if (gMaterial.gradientControl.z > 0.0f)
    {
        float edge = gMaterial.gradientControl.z;
        output.color.a *= smoothstep(0.0f, edge, input.localPosition.y)
            * smoothstep(0.0f, edge, 1.0f - input.localPosition.y);
    }
    if (gMaterial.screenNoise.x > 1.5f)
    {
        // Modulate the sampled lettering, preserving its original transparent
        // silhouette and red glitch details. This adds no rectangular overlay.
        output.color.rgb *= 1.0f - saturate(gMaterial.screenNoise.w) * StaticNoise(input.position.xy);
    }
    return output;
}
