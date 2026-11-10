#ifndef VOLUME_NOISE_HLSLI
#define VOLUME_NOISE_HLSLI

float Hash31(float3 p)
{
    p = frac(p * 0.1031f);
    p += dot(p, p.yzx + 33.33f);
    return frac((p.x + p.y) * p.z);
}

float ValueNoise3D(float3 p)
{
    float3 cell = floor(p);
    float3 local = frac(p);
    local = local * local * (3.0f - 2.0f * local);

    float n000 = Hash31(cell + float3(0.0f, 0.0f, 0.0f));
    float n100 = Hash31(cell + float3(1.0f, 0.0f, 0.0f));
    float n010 = Hash31(cell + float3(0.0f, 1.0f, 0.0f));
    float n110 = Hash31(cell + float3(1.0f, 1.0f, 0.0f));
    float n001 = Hash31(cell + float3(0.0f, 0.0f, 1.0f));
    float n101 = Hash31(cell + float3(1.0f, 0.0f, 1.0f));
    float n011 = Hash31(cell + float3(0.0f, 1.0f, 1.0f));
    float n111 = Hash31(cell + float3(1.0f, 1.0f, 1.0f));

    float nx00 = lerp(n000, n100, local.x);
    float nx10 = lerp(n010, n110, local.x);
    float nx01 = lerp(n001, n101, local.x);
    float nx11 = lerp(n011, n111, local.x);

    float nxy0 = lerp(nx00, nx10, local.y);
    float nxy1 = lerp(nx01, nx11, local.y);

    return lerp(nxy0, nxy1, local.z);
}

#endif
