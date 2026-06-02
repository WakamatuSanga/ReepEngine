#include "GpuParticle.hlsli"

ConstantBuffer<PerView> gPerView : register(b0);
StructuredBuffer<Particle> gParticles : register(t0);
StructuredBuffer<ParticleType> gParticleTypes : register(t1);

static const float2 kPositions[6] =
{
    float2(-0.5f, -0.5f),
    float2(-0.5f, 0.5f),
    float2(0.5f, -0.5f),
    float2(0.5f, -0.5f),
    float2(-0.5f, 0.5f),
    float2(0.5f, 0.5f),
};

static const float2 kTexcoords[6] =
{
    float2(0.0f, 1.0f),
    float2(0.0f, 0.0f),
    float2(1.0f, 1.0f),
    float2(1.0f, 1.0f),
    float2(0.0f, 0.0f),
    float2(1.0f, 0.0f),
};

float2 ApplyAtlasTexcoord(float2 baseTexcoord, Particle particle, ParticleType particleType)
{
    uint atlasRows = max(particleType.atlasRows, 1u);
    uint atlasColumns = max(particleType.atlasColumns, 1u);
    uint atlasCapacity = atlasRows * atlasColumns;
    uint frameCount = clamp(particleType.frameCount, 1u, atlasCapacity);
    float normalizedAge = saturate(particle.currentTime / max(particle.lifeTime, 0.0001f));
    float frameValue = particleType.frameSpeed > 0.0f
        ? particle.currentTime * particleType.frameSpeed
        : normalizedAge * float(frameCount);
    uint frameIndex = uint(floor(max(frameValue, 0.0f)));
    frameIndex = particleType.loopAtlas != 0u
        ? frameIndex % frameCount
        : min(frameIndex, frameCount - 1u);

    uint row = frameIndex / atlasColumns;
    uint column = frameIndex % atlasColumns;
    float2 uvScale = 1.0f / float2(float(atlasColumns), float(atlasRows));
    float2 uvOffset = float2(float(column), float(row)) * uvScale;
    return baseTexcoord * uvScale + uvOffset;
}

VertexShaderOutput main(uint vertexId : SV_VertexID, uint instanceId : SV_InstanceID)
{
    Particle particle = gParticles[instanceId];
    ParticleType particleType = gParticleTypes[particle.type];
    float alive = particle.alive != 0u ? 1.0f : 0.0f;
    float lifeRate = saturate(particle.currentTime / max(particle.lifeTime, 0.0001f));
    float particleScale = lerp(particleType.startScale, particleType.endScale, lifeRate);
    float4 particleColor = lerp(particleType.startColor, particleType.endColor, lifeRate);

    float3 localPosition = float3(kPositions[vertexId] * particleScale, 0.0f);
    float3 billboardOffset = mul(float4(localPosition, 0.0f), gPerView.billboardMatrix).xyz;
    float4 worldPosition = float4(particle.translate + billboardOffset, 1.0f);

    VertexShaderOutput output;
    float2 uv = kTexcoords[vertexId];
    if (particleType.useAtlas != 0u)
    {
        uv = ApplyAtlasTexcoord(uv, particle, particleType);
    }

    output.position = mul(worldPosition, gPerView.viewProjection);
    output.texcoord = uv;
    output.alive = alive;
    output.color = particleColor;
    output.typeColor = particleType.baseColor;
    output.textureIndex = particleType.textureIndex;
    return output;
}
