#include "CopyImage.hlsli"
#include "LockedWingSmoke.hlsli"

Texture2D<float4> gLowSmoke : register(t1);

float4 main(VertexShaderOutput input) : SV_TARGET0
{
    float sceneZ = LoadSceneZ(uint2(input.position.xy));
    float2 lowCoord = input.position.xy * gSmoke.smokeSize / gSmoke.outputSize - 0.5;
    int2 base = int2(floor(lowCoord));
    float2 weight = frac(lowCoord);
    float4 result = 0.0;
    [unroll]
    for (int y = 0; y < 2; ++y)
    {
        [unroll]
        for (int x = 0; x < 2; ++x)
        {
            int2 coord = clamp(base + int2(x, y), int2(0, 0), int2(gSmoke.smokeSize) - 1);
            float4 smoke = gLowSmoke.Load(int3(coord, 0));
            if (smoke.g <= 0.0 || sceneZ <= smoke.b) continue;
            float visible = saturate((sceneZ - smoke.b) / max(0.015, smoke.a - smoke.b));
            float bilinear = (x == 0 ? 1.0 - weight.x : weight.x) * (y == 0 ? 1.0 - weight.y : weight.y);
            float alpha = smoke.g * visible * bilinear;
            result += float4(float3(0.91, 0.95, 1.0) * smoke.r * alpha, alpha);
        }
    }
    return result;
}
