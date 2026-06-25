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
    float4 cloudBottomUndulation;
    float4 volumeEdgeFade;
    float4 farCloudLayer;
    float4 farCloudLayerExtra;
    float4 farCloudColor;
    float4 cloudSeaLayer;
    float4 cloudSeaShape;
    float4 cloudSeaFlow;
    float4 cloudSeaColor;
    float4 influenceCentersAndRadius[16];
    float4 influenceParams[16];
    float4 influenceSettings; // x:count y:enableCloudClear z:enableTunnel w:tunnelClearStrength
    float4 cameraTunnelStartLength; // xyz:start w:length
    float4 cameraTunnelDirectionRadius; // xyz:direction w:radius
};

ConstantBuffer<CloudPassConstants> gCloudPass : register(b0);

struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

static const uint kInfluenceFieldFlagAffectCloud = 1u << 2;

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

float ComputeInfluenceClearFactor(float3 worldPosition)
{
    float clearFactor = 0.0f;
    if (gCloudPass.influenceSettings.y > 0.5f)
    {
        uint fieldCount = min((uint)(gCloudPass.influenceSettings.x + 0.5f), 16u);
        [loop]
        for (uint index = 0u; index < fieldCount; ++index)
        {
            float4 centerAndRadius = gCloudPass.influenceCentersAndRadius[index];
            float4 params = gCloudPass.influenceParams[index];
            uint flags = (uint)(params.w + 0.5f);
            if ((flags & kInfluenceFieldFlagAffectCloud) == 0u)
            {
                continue;
            }

            float radius = max(centerAndRadius.w, 0.0001f);
            float distanceValue = distance(worldPosition, centerAndRadius.xyz);
            if (distanceValue >= radius)
            {
                continue;
            }

            float falloff = pow(saturate(1.0f - distanceValue / radius), max(params.z, 0.01f));
            float edgeNoise = lerp(0.90f, 1.08f, ValueNoise3D(worldPosition * 0.09f + index * 11.37f));
            clearFactor = max(clearFactor, falloff * saturate(params.y) * edgeNoise);
        }
    }

    if (gCloudPass.influenceSettings.z > 0.5f)
    {
        float tunnelLength = max(gCloudPass.cameraTunnelStartLength.w, 0.0f);
        float tunnelRadius = max(gCloudPass.cameraTunnelDirectionRadius.w, 0.0001f);
        float3 tunnelStart = gCloudPass.cameraTunnelStartLength.xyz;
        float3 tunnelDirection = normalize(gCloudPass.cameraTunnelDirectionRadius.xyz);
        float along = clamp(dot(worldPosition - tunnelStart, tunnelDirection), 0.0f, tunnelLength);
        float3 closestPoint = tunnelStart + tunnelDirection * along;
        float distanceToTunnel = distance(worldPosition, closestPoint);
        if (distanceToTunnel < tunnelRadius)
        {
            float radial = pow(saturate(1.0f - distanceToTunnel / tunnelRadius), 2.0f);
            float lengthFade = smoothstep(0.0f, min(8.0f, max(tunnelLength, 0.0001f)), along) * (1.0f - smoothstep(max(tunnelLength - 8.0f, 0.0f), tunnelLength, along));
            clearFactor = max(clearFactor, radial * lengthFade * saturate(gCloudPass.influenceSettings.w));
        }
    }

    return saturate(clearFactor);
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
    if (gCloudPass.volumeEdgeFade.x > 0.5f)
    {
        float3 boxMin = gCloudPass.volumeCenter - gCloudPass.volumeHalfExtents;
        float3 boxMax = gCloudPass.volumeCenter + gCloudPass.volumeHalfExtents;
        float3 toMin = worldPosition - boxMin;
        float3 toMax = boxMax - worldPosition;
        float edgeWorldDistance = min(min(min(toMin.x, toMin.y), toMin.z), min(min(toMax.x, toMax.y), toMax.z));
        edgeMask *= smoothstep(0.0f, max(gCloudPass.volumeEdgeFade.y, 0.0001f), edgeWorldDistance);
    }

    float layerBottomY = gCloudPass.volumeCenter.y - gCloudPass.volumeHalfExtents.y;
    float layerTopY = gCloudPass.volumeCenter.y + gCloudPass.volumeHalfExtents.y;
    if (gCloudPass.cloudBottomShaping.x > 0.5f)
    {
        float bottomNoiseScale = max(abs(gCloudPass.cloudBottomUndulation.y), 0.00001f);
        float bottomNoise = ValueNoise3D(float3(worldPosition.xz * bottomNoiseScale, 2.17f));
        layerBottomY += (bottomNoise - 0.5f) * gCloudPass.cloudBottomUndulation.x;
    }
    float bottomFade = max(gCloudPass.cloudLayerFade.x * lerp(1.0f, 1.35f, saturate(gCloudPass.cloudBottomUndulation.w)), 0.0001f);
    float bottomMask = smoothstep(0.0f, bottomFade, worldPosition.y - layerBottomY);
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
        float bottomDetailTarget = lerp(saturate(gCloudPass.cloudBottomUndulation.z), saturate(1.0f - gCloudPass.cloudBottomShaping.w), 0.35f);
        bottomDetailMultiplier = lerp(bottomDetailTarget, 1.0f, saturate(shapedBottomMask));
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
    density *= 1.0f - ComputeInfluenceClearFactor(worldPosition);
    return max(density, 0.0f);
}

float2 SafeNormalize2(float2 value, float2 fallbackValue)
{
    float lengthSquared = dot(value, value);
    return (lengthSquared > 0.000001f) ? value * rsqrt(lengthSquared) : fallbackValue;
}

float4 CompositePremultiplied(float4 underLayer, float4 overLayer)
{
    return float4(underLayer.rgb + overLayer.rgb * (1.0f - underLayer.a), saturate(underLayer.a + overLayer.a * (1.0f - underLayer.a)));
}

float4 ComputeFarCloudLayer(float2 uv, float3 rayDirection, float depth)
{
    if (gCloudPass.farCloudLayer.x <= 0.5f || gCloudPass.farCloudLayerExtra.x <= 0.0001f || depth < 0.99999f || rayDirection.y <= 0.002f)
    {
        return float4(0.0f, 0.0f, 0.0f, 0.0f);
    }

    float planeY = gCloudPass.cameraPosition.y + gCloudPass.farCloudLayer.z;
    float distanceToPlane = (planeY - gCloudPass.cameraPosition.y) / rayDirection.y;
    if (distanceToPlane <= 0.0f)
    {
        return float4(0.0f, 0.0f, 0.0f, 0.0f);
    }

    float3 farPosition = gCloudPass.cameraPosition + rayDirection * distanceToPlane;
    float2 flowDirection = SafeNormalize2(gCloudPass.cloudFlowDirectionSpeed.xz, float2(0.0f, -1.0f));
    float scale = max(gCloudPass.farCloudLayer.w, 0.00001f);
    float2 cloudUv = farPosition.xz * scale - flowDirection * gCloudPass.cloudTime * gCloudPass.farCloudLayerExtra.y * scale;
    float noiseA = FBM(float3(cloudUv, 6.37f));
    float noiseB = FBM(float3(cloudUv * 2.73f + 13.17f, 3.19f));
    float noiseValue = noiseA * 0.62f + noiseB * 0.38f;
    if (gCloudPass.farCloudLayerExtra.z <= 0.5f)
    {
        noiseValue = 0.5f + 0.18f * sin(cloudUv.x * 5.1f + cloudUv.y * 1.7f) + 0.16f * sin(cloudUv.y * 3.8f + 1.3f);
    }

    float targetDistance = max(gCloudPass.farCloudLayer.y, 1.0f);
    float distanceMask = smoothstep(targetDistance * 0.25f, targetDistance, distanceToPlane) * (1.0f - smoothstep(targetDistance * 2.15f, targetDistance * 2.85f, distanceToPlane));
    float topScreenMask = 1.0f - smoothstep(0.35f, 0.98f, uv.y);
    float cloudMask = smoothstep(0.42f, 0.72f, noiseValue);
    float alpha = saturate(cloudMask * topScreenMask * distanceMask * gCloudPass.farCloudLayerExtra.x);
    return float4(gCloudPass.farCloudColor.rgb * alpha, alpha);
}

float4 ComputeCloudSeaLayer(float2 uv, float3 rayDirection, float depth)
{
    if (gCloudPass.cloudSeaLayer.x <= 0.5f || gCloudPass.cloudSeaLayer.w <= 0.0001f || depth < 0.99999f || rayDirection.y <= 0.002f)
    {
        return float4(0.0f, 0.0f, 0.0f, 0.0f);
    }

    float planeY = gCloudPass.cameraPosition.y + gCloudPass.cloudSeaLayer.z;
    float distanceToPlane = (planeY - gCloudPass.cameraPosition.y) / rayDirection.y;
    if (distanceToPlane <= 0.0f)
    {
        return float4(0.0f, 0.0f, 0.0f, 0.0f);
    }

    float3 seaPosition = gCloudPass.cameraPosition + rayDirection * distanceToPlane;
    float2 flowDirection = SafeNormalize2(gCloudPass.cloudFlowDirectionSpeed.xz, float2(0.0f, -1.0f));
    float2 forward2 = -flowDirection;
    float2 right2 = float2(forward2.y, -forward2.x);
    float2 local = seaPosition.xz - gCloudPass.cameraPosition.xz;
    float forwardDistance = dot(local, forward2);
    float rightDistance = dot(local, right2);

    float seaDistance = max(gCloudPass.cloudSeaLayer.y, 1.0f);
    float seaWidth = max(gCloudPass.cloudSeaShape.x, 1.0f);
    float seaDepth = max(gCloudPass.cloudSeaShape.y, 1.0f);
    float depthMask = smoothstep(seaDistance - seaDepth * 0.55f, seaDistance, forwardDistance) * (1.0f - smoothstep(seaDistance + seaDepth * 0.45f, seaDistance + seaDepth * 0.75f, forwardDistance));
    float widthMask = 1.0f - smoothstep(seaWidth * 0.42f, seaWidth * 0.50f, abs(rightDistance));
    float2 cameraRelativeUv = float2(rightDistance, forwardDistance);
    float2 noiseSource = lerp(seaPosition.xz, cameraRelativeUv, saturate(gCloudPass.cloudSeaFlow.y));
    float noiseScale = max(gCloudPass.cloudSeaShape.z, 0.0001f);
    float2 seaUv = noiseSource * noiseScale - flowDirection * gCloudPass.cloudTime * gCloudPass.cloudSeaFlow.x * noiseScale;
    float noiseA = FBM(float3(seaUv, 11.23f));
    float noiseB = FBM(float3(seaUv * 2.41f + 23.7f, 4.91f));
    float noiseValue = noiseA * 0.68f + noiseB * 0.32f;
    float softness = lerp(0.06f, 0.28f, saturate(gCloudPass.cloudSeaShape.w));
    float cloudMask = smoothstep(0.50f - softness, 0.50f + softness, noiseValue);
    float horizonMask = 1.0f - smoothstep(0.60f, 1.0f, uv.y);
    float alpha = saturate(cloudMask * depthMask * widthMask * horizonMask * gCloudPass.cloudSeaLayer.w);
    return float4(gCloudPass.cloudSeaColor.rgb * alpha, alpha);
}

float4 ComputeCloudNoiseDebug(float2 uv, float3 rayDirection, float depth)
{
    float4 sea = ComputeCloudSeaLayer(uv, rayDirection, depth);
    float4 farCloud = ComputeFarCloudLayer(uv, rayDirection, depth);
    return float4(farCloud.a, sea.a, saturate(farCloud.a + sea.a), 1.0f);
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
    float4 farCloud = ComputeFarCloudLayer(input.texcoord, rayDirection, depth);
    float4 cloudSea = ComputeCloudSeaLayer(input.texcoord, rayDirection, depth);
    float4 distantCloud = CompositePremultiplied(cloudSea, farCloud);
    if (gCloudPass.debugViewMode == 4u) { return farCloud; }
    if (gCloudPass.debugViewMode == 7u) { return cloudSea; }
    if (gCloudPass.debugViewMode == 6u) { return ComputeCloudNoiseDebug(input.texcoord, rayDirection, depth); }

    float3 boxMin = gCloudPass.volumeCenter - gCloudPass.volumeHalfExtents;
    float3 boxMax = gCloudPass.volumeCenter + gCloudPass.volumeHalfExtents;

    float boxNear = 0.0f;
    float boxFar = 0.0f;
    if (!RayBoxIntersect(gCloudPass.cameraPosition, rayDirection, boxMin, boxMax, boxNear, boxFar))
    {
        return (gCloudPass.debugViewMode == 5u) ? float4(0.0f, 0.0f, 0.0f, 0.0f) : distantCloud;
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
        return (gCloudPass.debugViewMode == 5u) ? float4(0.0f, 0.0f, 0.0f, 0.0f) : distantCloud;
    }

    float stepScale = 1.0f;
    float densityScale = 1.0f;
    ComputeDistanceLodScales(marchStart, stepScale, densityScale);

    uint baseViewSteps = max(gCloudPass.viewStepCount, 1u);
    uint viewSteps = max(1u, (uint)(float(baseViewSteps) * stepScale + 0.5f));
    float stepSize = (marchEnd - marchStart) / float(viewSteps);
    if (stepSize <= 0.0f)
    {
        return (gCloudPass.debugViewMode == 5u) ? float4(0.0f, 0.0f, 0.0f, 0.0f) : distantCloud;
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

    if (gCloudPass.debugViewMode == 5u)
    {
        return float4(accumulatedColor, finalAlpha);
    }
    float distantAlpha = distantCloud.a * (1.0f - finalAlpha);
    return float4(accumulatedColor + distantCloud.rgb * (1.0f - finalAlpha), saturate(finalAlpha + distantAlpha));
}
