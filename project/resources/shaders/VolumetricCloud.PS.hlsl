Texture2D<float> gDepthTexture : register(t0);
SamplerState gSampler : register(s0);

struct CloudPassConstants
{
    float4x4 inverseViewProjection;
    float3 cameraPosition;
    float padding0;
    float3 volumeCenter;
    float density;
    float3 volumeHalfExtents;
    float absorption;
    float3 windOffset;
    float noiseScale;
    float3 sunDirection;
    float detailNoiseScale;
    float4 cloudColor;
    float lightAbsorption;
    float detailWeight;
    float edgeFade;
    float ambientLighting;
    float sunIntensity;
    uint viewStepCount;
    uint lightStepCount;
    uint debugViewMode;
    float lodFactorScale;
    uint disableDistanceLod;
    float padding1;
    float padding2;
    float4 renderInfo;
    float4 cloudFlowDirectionSpeed;
    float cloudTime;
    uint enableCloudFlow;
    float padding3;
    float padding4;
    float4 nearCameraFade;
    float4 cloudLayerFade;
    float4 cloudBottomShaping;
    float4 cloudBottomShapingExtra;
};

ConstantBuffer<CloudPassConstants> gCloudPass : register(b0);

struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

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

float FBM(float3 p)
{
    float value = 0.0f;
    float amplitude = 0.5f;
    float frequency = 1.0f;

    [unroll]
    for (uint i = 0; i < 4; ++i)
    {
        value += ValueNoise3D(p * frequency) * amplitude;
        frequency *= 2.0f;
        amplitude *= 0.5f;
    }

    return value;
}

float ComputeDistanceLodFactor(float entryDistance)
{
    float volumeRadius = max(length(gCloudPass.volumeHalfExtents), 0.0001f);
    float lodStart = max(volumeRadius * 0.75f, 8.0f);
    float lodEnd = lodStart + max(volumeRadius * 2.50f, 16.0f);
    float lodFactor = saturate((entryDistance - lodStart) / max(lodEnd - lodStart, 0.0001f));
    return saturate(lodFactor * gCloudPass.lodFactorScale);
}

void ComputeDistanceLodScales(float entryDistance, out float stepScale, out float densityScale)
{
    float lodFactor = (gCloudPass.disableDistanceLod != 0u) ? 0.0f : ComputeDistanceLodFactor(entryDistance);
    stepScale = lerp(1.0f, 0.45f, lodFactor);
    densityScale = lerp(1.0f, 0.65f, lodFactor);
}

float4 ReconstructWorldPosition(float2 uv, float depth)
{
    float2 clipXY = float2(
        uv.x * 2.0f - 1.0f,
        1.0f - uv.y * 2.0f);

    float4 clipPosition = float4(clipXY, depth, 1.0f);
    float4 worldPosition = mul(clipPosition, gCloudPass.inverseViewProjection);
    worldPosition.xyz /= max(worldPosition.w, 0.0001f);
    worldPosition.w = 1.0f;
    return worldPosition;
}

bool RayBoxIntersect(
    float3 rayOrigin,
    float3 rayDirection,
    float3 boxMin,
    float3 boxMax,
    out float tNear,
    out float tFar)
{
    float3 inverseDirection;
    inverseDirection.x = (abs(rayDirection.x) > 0.00001f) ? (1.0f / rayDirection.x) : 1.0e20f;
    inverseDirection.y = (abs(rayDirection.y) > 0.00001f) ? (1.0f / rayDirection.y) : 1.0e20f;
    inverseDirection.z = (abs(rayDirection.z) > 0.00001f) ? (1.0f / rayDirection.z) : 1.0e20f;

    float3 t0 = (boxMin - rayOrigin) * inverseDirection;
    float3 t1 = (boxMax - rayOrigin) * inverseDirection;

    float3 tMin = min(t0, t1);
    float3 tMax = max(t0, t1);

    tNear = max(max(tMin.x, tMin.y), tMin.z);
    tFar = min(min(tMax.x, tMax.y), tMax.z);

    return tFar > max(tNear, 0.0f);
}

float ComputeCloudDensity(float3 worldPosition)
{
    float3 safeHalfExtents = max(gCloudPass.volumeHalfExtents, float3(0.0001f, 0.0001f, 0.0001f));
    float3 local = (worldPosition - gCloudPass.volumeCenter) / safeHalfExtents;
    float3 absLocal = abs(local);
    if (any(absLocal > 1.0f))
    {
        return 0.0f;
    }

    float edgeDistance = 1.0f - max(absLocal.x, max(absLocal.y, absLocal.z));
    float edgeMask = saturate(edgeDistance / max(gCloudPass.edgeFade, 0.0001f));

    float layerBottomY = gCloudPass.volumeCenter.y - gCloudPass.volumeHalfExtents.y;
    float layerTopY = gCloudPass.volumeCenter.y + gCloudPass.volumeHalfExtents.y;
    float bottomMask = smoothstep(0.0f, max(gCloudPass.cloudLayerFade.x, 0.0001f), worldPosition.y - layerBottomY);
    float topMask = smoothstep(0.0f, max(gCloudPass.cloudLayerFade.y, 0.0001f), layerTopY - worldPosition.y);
    float shapedBottomMask = bottomMask;
    if (gCloudPass.cloudBottomShaping.x > 0.5f)
    {
        float bottomSmoothPower = lerp(1.0f, 2.5f, saturate(gCloudPass.cloudBottomShaping.z));
        float flattenedBottomMask = pow(saturate(bottomMask), bottomSmoothPower);
        shapedBottomMask = lerp(bottomMask, flattenedBottomMask, saturate(gCloudPass.cloudBottomShaping.y));
    }
    float heightMask = saturate(shapedBottomMask * topMask);

    float3 flowOffset = 0.0f;
    if (gCloudPass.enableCloudFlow != 0u)
    {
        flowOffset = gCloudPass.cloudFlowDirectionSpeed.xyz * gCloudPass.cloudTime * gCloudPass.cloudFlowDirectionSpeed.w;
    }

    float3 baseNoisePosition = (worldPosition + gCloudPass.windOffset - flowOffset) * gCloudPass.noiseScale;
    float3 detailNoisePosition = (worldPosition + gCloudPass.windOffset * 1.7f - flowOffset * 1.7f + 19.31f) * gCloudPass.detailNoiseScale;

    float baseNoise = FBM(baseNoisePosition);
    float detailNoise = FBM(detailNoisePosition);
    float bottomDetailMultiplier = 1.0f;
    if (gCloudPass.cloudBottomShaping.x > 0.5f)
    {
        bottomDetailMultiplier = lerp(
            saturate(1.0f - gCloudPass.cloudBottomShaping.w),
            1.0f,
            saturate(shapedBottomMask));
    }
    float shape = baseNoise - detailNoise * bottomDetailMultiplier * gCloudPass.detailWeight - 0.40f;
    float noiseMask = saturate(shape * 2.5f);

    float density = noiseMask * edgeMask * heightMask * gCloudPass.density * gCloudPass.cloudColor.a;
    if (gCloudPass.cloudBottomShaping.x > 0.5f)
    {
        float bottomDensityScale = lerp(
            saturate(gCloudPass.cloudBottomShapingExtra.x),
            1.0f,
            saturate(shapedBottomMask));
        density *= bottomDensityScale;
    }
    if (gCloudPass.nearCameraFade.w > 0.5f)
    {
        float distanceFromCamera = length(worldPosition - gCloudPass.cameraPosition);
        float fade = smoothstep(gCloudPass.nearCameraFade.x, gCloudPass.nearCameraFade.y, distanceFromCamera);
        density *= lerp(gCloudPass.nearCameraFade.z, 1.0f, fade);
    }
    return max(density, 0.0f);
}

float ComputeLightTransmittance(
    float3 samplePosition,
    float3 lightDirection,
    float3 boxMin,
    float3 boxMax,
    float densityScale,
    float stepScale)
{
    float lightNear = 0.0f;
    float lightFar = 0.0f;
    if (!RayBoxIntersect(samplePosition, lightDirection, boxMin, boxMax, lightNear, lightFar))
    {
        return 1.0f;
    }

    float marchEnd = max(lightFar, 0.0f);
    uint baseLightSteps = max(gCloudPass.lightStepCount, 1u);
    uint lightSteps = max(1u, (uint)(float(baseLightSteps) * stepScale + 0.5f));
    float stepSize = marchEnd / float(lightSteps);
    if (stepSize <= 0.0f)
    {
        return 1.0f;
    }

    float opticalDepth = 0.0f;

    [loop]
    for (uint i = 0u; i < lightSteps; ++i)
    {
        float distanceAlongRay = stepSize * (float(i) + 0.5f);
        float3 lightSamplePosition = samplePosition + lightDirection * distanceAlongRay;
        opticalDepth += ComputeCloudDensity(lightSamplePosition) * densityScale * stepSize;

        if (opticalDepth * gCloudPass.lightAbsorption > 8.0f)
        {
            break;
        }
    }

    return exp(-opticalDepth * gCloudPass.lightAbsorption);
}

float4 main(VertexShaderOutput input) : SV_TARGET0
{
    float2 depthTextureSize = max(gCloudPass.renderInfo.xy, float2(1.0f, 1.0f));
    uint2 pixelCoord = min(uint2(input.texcoord * depthTextureSize), uint2(depthTextureSize - 1.0f));
    float depth = gDepthTexture.Load(int3(pixelCoord, 0));

    float4 farWorld = ReconstructWorldPosition(input.texcoord, 1.0f);
    float3 rayDirection = normalize(farWorld.xyz - gCloudPass.cameraPosition);

    float3 boxMin = gCloudPass.volumeCenter - gCloudPass.volumeHalfExtents;
    float3 boxMax = gCloudPass.volumeCenter + gCloudPass.volumeHalfExtents;

    float boxNear = 0.0f;
    float boxFar = 0.0f;
    if (!RayBoxIntersect(gCloudPass.cameraPosition, rayDirection, boxMin, boxMax, boxNear, boxFar))
    {
        return float4(0.0f, 0.0f, 0.0f, 0.0f);
    }

    float marchStart = max(boxNear, 0.0f);
    float marchEnd = boxFar;

    if (depth < 0.99999f)
    {
        float4 opaqueWorld = ReconstructWorldPosition(input.texcoord, depth);
        float opaqueDistance = dot(opaqueWorld.xyz - gCloudPass.cameraPosition, rayDirection);
        marchEnd = min(marchEnd, opaqueDistance);
    }

    if (marchEnd <= marchStart)
    {
        return float4(0.0f, 0.0f, 0.0f, 0.0f);
    }

    float stepScale = 1.0f;
    float densityScale = 1.0f;
    ComputeDistanceLodScales(marchStart, stepScale, densityScale);

    uint baseViewSteps = max(gCloudPass.viewStepCount, 1u);
    uint viewSteps = max(1u, (uint)(float(baseViewSteps) * stepScale + 0.5f));
    float stepSize = (marchEnd - marchStart) / float(viewSteps);
    if (stepSize <= 0.0f)
    {
        return float4(0.0f, 0.0f, 0.0f, 0.0f);
    }

    float3 lightDirection = normalize(-gCloudPass.sunDirection);
    float3 accumulatedColor = 0.0f;
    float transmittance = 1.0f;
    float maxDensity = 0.0f;
    float weightedLight = 0.0f;
    float accumulatedDensity = 0.0f;

    [loop]
    for (uint i = 0u; i < viewSteps; ++i)
    {
        float distanceAlongRay = marchStart + stepSize * (float(i) + 0.5f);
        float3 samplePosition = gCloudPass.cameraPosition + rayDirection * distanceAlongRay;

        float density = ComputeCloudDensity(samplePosition) * densityScale;
        if (density <= 0.0001f)
        {
            continue;
        }

        float lightTransmittance = ComputeLightTransmittance(samplePosition, lightDirection, boxMin, boxMax, densityScale, stepScale);
        float lighting = saturate(gCloudPass.ambientLighting + lightTransmittance * gCloudPass.sunIntensity);
        maxDensity = max(maxDensity, density);
        weightedLight += density * lighting;
        accumulatedDensity += density;

        float alpha = 1.0f - exp(-density * gCloudPass.absorption * stepSize);
        float3 sampleColor = gCloudPass.cloudColor.rgb * lighting;

        accumulatedColor += transmittance * alpha * sampleColor;
        transmittance *= (1.0f - alpha);

        if (transmittance <= 0.015f)
        {
            break;
        }
    }

    float finalAlpha = saturate(1.0f - transmittance);
    float densityView = saturate(maxDensity / max(gCloudPass.density, 0.0001f));
    float lightView = (accumulatedDensity > 0.0001f) ? saturate(weightedLight / accumulatedDensity) : 0.0f;

    if (gCloudPass.debugViewMode == 1u)
    {
        return float4(finalAlpha.xxx, 1.0f);
    }
    if (gCloudPass.debugViewMode == 2u)
    {
        return float4(densityView.xxx, 1.0f);
    }
    if (gCloudPass.debugViewMode == 3u)
    {
        return float4(lightView.xxx, 1.0f);
    }

    return float4(accumulatedColor, finalAlpha);
}
