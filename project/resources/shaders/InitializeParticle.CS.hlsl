struct Particle
{
    float3 translate;
    float scale;
    float3 velocity;
    float currentTime;
    float4 color;
    float lifeTime;
    uint alive;
    uint type;
    float padding;
};

struct ParticleType
{
    float4 baseColor;
    float4 startColor;
    float4 endColor;
    float startScale;
    float endScale;
    float lifeTimeMin;
    float lifeTimeMax;
    float speedMin;
    float speedMax;
    float gravity;
    float drag;
    uint useAtlas;
    uint atlasRows;
    uint atlasColumns;
    uint frameCount;
    float frameSpeed;
    uint loopAtlas;
    uint textureIndex;
    uint padding1;
    float4 materialPadding;
};

struct InitializeInfo
{
    uint particleCount;
    uint randomEnabled;
    uint randomSeed;
    uint emitterEnabled;
    float3 emitterPosition;
    float emitterRadius;
    float3 emitterBoxSize;
    float emitterConeHeight;
    uint emitCount;
    uint particleTypeIndex;
    uint emitterShape;
    float padding;
};

RWStructuredBuffer<Particle> gParticles : register(u0);
StructuredBuffer<ParticleType> gParticleTypes : register(t0);
ConstantBuffer<InitializeInfo> gInitializeInfo : register(b0);

uint Hash(uint value)
{
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return value;
}

float Random01(uint seed)
{
    return float(Hash(seed) & 0x00ffffffu) / 16777215.0f;
}

float RandomRange(uint seed, float minValue, float maxValue)
{
    return lerp(minValue, maxValue, Random01(seed));
}

float3 RandomUnitVector(uint seed)
{
    float3 value = float3(
        RandomRange(seed ^ 0x6c8e9cf5u, -1.0f, 1.0f),
        RandomRange(seed ^ 0xb5297a4du, -1.0f, 1.0f),
        RandomRange(seed ^ 0x9e3779b9u, -1.0f, 1.0f));
    return normalize(value + float3(0.001f, 0.001f, 0.001f));
}

float3 MakeSphereEmitterOffset(uint baseSeed, bool useRandom, uint particleIndex, uint emitCount, float radius)
{
    float angle = float(particleIndex) / max(float(emitCount), 1.0f) * 6.28318530718f;
    float3 deterministicDirection = normalize(float3(cos(angle), sin(angle), 0.25f) + float3(0.001f, 0.001f, 0.001f));
    float3 emitterDirection = useRandom ? RandomUnitVector(baseSeed ^ 0x7f4a7c15u) : deterministicDirection;
    float distanceRate = useRandom ? pow(Random01(baseSeed ^ 0x94d049bbu), 1.0f / 3.0f) : float(particleIndex % 16u) / 15.0f;
    return emitterDirection * distanceRate * max(radius, 0.0f);
}

float3 MakeBoxEmitterOffset(uint baseSeed, bool useRandom, uint particleIndex, float3 boxSize)
{
    float3 rate = useRandom
        ? float3(
            Random01(baseSeed ^ 0x68bc21ebu),
            Random01(baseSeed ^ 0x02e5be93u),
            Random01(baseSeed ^ 0x9e3779b9u))
        : frac(float3(float(particleIndex) * 0.754877666f, float(particleIndex) * 0.569840291f, float(particleIndex) * 0.438235197f) + float3(0.13f, 0.37f, 0.61f));
    return (rate - 0.5f) * max(boxSize, 0.0f);
}

float3 MakeConeEmitterOffset(uint baseSeed, bool useRandom, uint particleIndex, uint emitCount, float radius, float height)
{
    float angle = useRandom
        ? RandomRange(baseSeed ^ 0xb5297a4du, 0.0f, 6.28318530718f)
        : float(particleIndex) / max(float(emitCount), 1.0f) * 6.28318530718f;
    float heightRate = useRandom ? Random01(baseSeed ^ 0x85ebca6bu) : frac(float(particleIndex) * 0.381966011f);
    float radialRate = useRandom ? sqrt(Random01(baseSeed ^ 0x27d4eb2fu)) : frac(float(particleIndex) * 0.754877666f);
    float coneRadius = max(radius, 0.0f) * (1.0f - heightRate);
    float radialDistance = radialRate * coneRadius;
    return float3(cos(angle) * radialDistance, heightRate * max(height, 0.001f), sin(angle) * radialDistance);
}

float3 MakeEmitterOffset(uint baseSeed, bool useRandom, uint particleIndex, uint emitCount)
{
    if (gInitializeInfo.emitterShape == 1u)
    {
        return MakeBoxEmitterOffset(baseSeed, useRandom, particleIndex, gInitializeInfo.emitterBoxSize);
    }
    if (gInitializeInfo.emitterShape == 2u)
    {
        return MakeConeEmitterOffset(baseSeed, useRandom, particleIndex, emitCount, gInitializeInfo.emitterRadius, gInitializeInfo.emitterConeHeight);
    }
    return MakeSphereEmitterOffset(baseSeed, useRandom, particleIndex, emitCount, gInitializeInfo.emitterRadius);
}

[numthreads(1024, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint particleIndex = dispatchThreadId.x;
    if (particleIndex >= gInitializeInfo.particleCount)
    {
        return;
    }

    const uint kColumns = 32;
    uint x = particleIndex % kColumns;
    uint y = particleIndex / kColumns;

    bool useRandom = gInitializeInfo.randomEnabled != 0u;
    bool useEmitter = gInitializeInfo.emitterEnabled != 0u;
    uint emitCount = min(gInitializeInfo.emitCount, gInitializeInfo.particleCount);
    uint baseSeed = particleIndex ^ (gInitializeInfo.randomSeed * 747796405u) ^ 2891336453u;
    ParticleType particleType = gParticleTypes[gInitializeInfo.particleTypeIndex];
    if (useEmitter && particleIndex >= emitCount)
    {
        Particle deadParticle;
        deadParticle.translate = gInitializeInfo.emitterPosition;
        deadParticle.scale = 0.0f;
        deadParticle.velocity = float3(0.0f, 0.0f, 0.0f);
        deadParticle.currentTime = 1.0f;
        deadParticle.color = float4(0.0f, 0.0f, 0.0f, 0.0f);
        deadParticle.lifeTime = 0.0f;
        deadParticle.alive = 0u;
        deadParticle.type = gInitializeInfo.particleTypeIndex;
        deadParticle.padding = 0.0f;
        gParticles[particleIndex] = deadParticle;
        return;
    }

    float2 grid = (float2(x, y) - float2(15.5f, 15.5f)) * 0.18f;
    float colorRate = float(x) / float(kColumns - 1);
    float lifeRate = useRandom ? Random01(baseSeed ^ 0x3f6e4a1bu) : float((x + y) % 16) / 15.0f;
    float2 direction = normalize(grid + float2(0.001f, 0.001f));
    float2 randomOffset = useRandom
        ? float2(
            RandomRange(baseSeed ^ 0x68bc21ebu, -0.08f, 0.08f),
            RandomRange(baseSeed ^ 0x02e5be93u, -0.08f, 0.08f))
        : float2(0.0f, 0.0f);
    float velocityScale = useRandom ? RandomRange(baseSeed ^ 0x438f34abu, particleType.speedMin, particleType.speedMax) : (particleType.speedMin + particleType.speedMax) * 0.5f;
    float velocityY = useRandom ? RandomRange(baseSeed ^ 0x9e3779b9u, 0.15f, 0.85f) : 0.25f + lifeRate * 0.45f;
    float velocityZ = useRandom ? RandomRange(baseSeed ^ 0xb5297a4du, -0.08f, 0.08f) : 0.0f;
    float3 colorJitter = useRandom
        ? float3(
            RandomRange(baseSeed ^ 0x1f123bb5u, -0.10f, 0.10f),
            RandomRange(baseSeed ^ 0xc2b2ae35u, -0.08f, 0.08f),
            RandomRange(baseSeed ^ 0x27d4eb2fu, -0.10f, 0.10f))
        : float3(0.0f, 0.0f, 0.0f);
    float3 emitterOffset = MakeEmitterOffset(baseSeed, useRandom, particleIndex, emitCount);
    float3 emitterVelocityDirection = normalize(emitterOffset + float3(0.001f, 0.25f, 0.001f));

    Particle particle;
    particle.translate = useEmitter
        ? gInitializeInfo.emitterPosition + emitterOffset
        : float3(grid.x + randomOffset.x, grid.y + randomOffset.y + 1.5f, 3.0f);
    particle.scale = particleType.startScale;
    particle.velocity = useEmitter
        ? emitterVelocityDirection * velocityScale
        : float3(direction.x * 0.25f * velocityScale, velocityY * velocityScale, velocityZ);
    particle.currentTime = 0.0f;
    particle.color = float4(saturate(particleType.baseColor.rgb + colorJitter * (0.25f + colorRate * 0.25f)), particleType.baseColor.a);
    particle.lifeTime = useRandom ? RandomRange(baseSeed ^ 0x85ebca6bu, particleType.lifeTimeMin, particleType.lifeTimeMax) : lerp(particleType.lifeTimeMin, particleType.lifeTimeMax, lifeRate);
    particle.alive = 1u;
    particle.type = gInitializeInfo.particleTypeIndex;
    particle.padding = 0.0f;

    gParticles[particleIndex] = particle;
}
