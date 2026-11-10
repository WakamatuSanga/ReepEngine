#include "CopyImage.hlsli"
#include "LockedWingSmoke.hlsli"
#include "VolumeNoise.hlsli"

struct SmokePuff
{
    float4 centerAndRadius;
    float2 opacityAndSeed;
};

StructuredBuffer<SmokePuff> gPuffs : register(t2);
StructuredBuffer<uint> gTiles : register(t3);

float4 main(VertexShaderOutput input) : SV_TARGET0
{
    uint2 pixel = uint2(input.position.xy);
    uint tile = (pixel.y / 16 * gSmoke.tileColumns + pixel.x / 16) * 49;
    uint count = min(48u, gTiles[tile]);
    if (count == 0) return 0.0;

    float2 uv = input.position.xy / gSmoke.smokeSize;
    float3 ray = float3((uv * float2(2, -2) + float2(-1, 1)) / gSmoke.projection.xy, 1.0);
    float rayLengthSq = dot(ray, ray);
    float rayLength = sqrt(rayLengthSq);
    float sceneZ = LoadSceneZ(uint2(uv * gSmoke.outputSize));
    float opticalDepth = 0.0;
    float weightedLight = 0.0;
    float nearZ = 60000.0;
    float farZ = 0.0;
    const float3 sun = float3(-0.46, 0.78, -0.42);

    [loop]
    for (uint index = 0; index < count; ++index)
    {
        SmokePuff puff = gPuffs[gTiles[tile + index + 1]];
        float3 center = puff.centerAndRadius.xyz;
        float radius = puff.centerAndRadius.w;
        float alongRay = dot(center, ray);
        float discriminant = alongRay * alongRay - rayLengthSq * (dot(center, center) - radius * radius);
        if (discriminant <= 0.0) continue;
        float extent = sqrt(discriminant);
        float entry = max(gSmoke.nearPlane, (alongRay - extent) / rayLengthSq);
        float leave = min(sceneZ, (alongRay + extent) / rayLengthSq);
        if (leave <= entry) continue;

        float stepZ = (leave - entry) / 6.0;
        float extinction = stepZ * rayLength * gSmoke.density * puff.opacityAndSeed.x * 2.45;
        [unroll]
        for (uint step = 0; step < 6; ++step)
        {
            float z = entry + (float(step) + 0.5) * stepZ;
            float3 local = (ray * z - center) / radius;
            float3 worldOffset = mul(float4(local, 0), gSmoke.viewToWorld).xyz;
            float3 noisePoint = worldOffset * 3.4 + puff.opacityAndSeed.y;
            float lobes = ValueNoise3D(noisePoint);
            float detail = ValueNoise3D(noisePoint * 2.3 + 5.1);
            float noise = lobes * 0.76 + detail * 0.24;
            float radial = length(local);
            float shape = saturate((1.0 - radial + (noise - 0.55) * 0.44) * 2.8);
            float weight = extinction * shape * smoothstep(0.24, 0.74, noise);
            if (weight < 0.0005) continue;
            float lit = smoothstep(-0.67, 0.75, dot(worldOffset, sun) + (lobes - 0.5) * 0.32);
            float light = lerp(0.34, 0.76, lit) * (0.85 + 0.20 * detail);
            weightedLight += weight * light;
            opticalDepth += weight;
            nearZ = min(nearZ, z - stepZ * 0.5);
            farZ = max(farZ, z + stepZ * 0.5);
        }
    }

    if (opticalDepth <= 0.001) return 0.0;
    // Overlapping puffs share one optical-depth limit so a salvo stays readable.
    float opacity = min(0.62, 1.0 - exp(-opticalDepth));
    return float4(weightedLight / opticalDepth, opacity, nearZ, farZ);
}
