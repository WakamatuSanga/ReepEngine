struct Particle
{
    float3 translate;
    float scale;
    float3 velocity;
    float currentTime;
    float4 color;
    float lifeTime;
    uint alive;
    float2 padding;
};

struct InitializeInfo
{
    uint particleCount;
    uint randomEnabled;
    uint randomSeed;
    uint emitterEnabled;
    float3 emitterPosition;
    float emitterRadius;
    uint emitCount;
    float3 padding;
};

RWStructuredBuffer<Particle> gParticles : register(u0);
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
        deadParticle.padding = float2(0.0f, 0.0f);
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
    float velocityScale = useRandom ? RandomRange(baseSeed ^ 0x438f34abu, 0.7f, 1.45f) : 1.0f;
    float velocityY = useRandom ? RandomRange(baseSeed ^ 0x9e3779b9u, 0.15f, 0.85f) : 0.25f + lifeRate * 0.45f;
    float velocityZ = useRandom ? RandomRange(baseSeed ^ 0xb5297a4du, -0.08f, 0.08f) : 0.0f;
    float3 colorJitter = useRandom
        ? float3(
            RandomRange(baseSeed ^ 0x1f123bb5u, -0.10f, 0.10f),
            RandomRange(baseSeed ^ 0xc2b2ae35u, -0.08f, 0.08f),
            RandomRange(baseSeed ^ 0x27d4eb2fu, -0.10f, 0.10f))
        : float3(0.0f, 0.0f, 0.0f);
    float angle = float(particleIndex) / max(float(emitCount), 1.0f) * 6.28318530718f;
    float3 deterministicEmitterDirection = normalize(float3(cos(angle), sin(angle), 0.25f) + float3(0.001f, 0.001f, 0.001f));
    float3 emitterDirection = useRandom ? RandomUnitVector(baseSeed ^ 0x7f4a7c15u) : deterministicEmitterDirection;
    float emitterDistance = (useRandom ? pow(Random01(baseSeed ^ 0x94d049bbu), 1.0f / 3.0f) : lifeRate) * gInitializeInfo.emitterRadius;
    float3 emitterOffset = emitterDirection * emitterDistance;
    float3 emitterVelocityDirection = normalize(emitterOffset + float3(0.001f, 0.25f, 0.001f));

    Particle particle;
    particle.translate = useEmitter
        ? gInitializeInfo.emitterPosition + emitterOffset
        : float3(grid.x + randomOffset.x, grid.y + randomOffset.y + 1.5f, 3.0f);
    particle.scale = useRandom ? RandomRange(baseSeed ^ 0x165667b1u, 0.035f, 0.075f) : 0.05f;
    particle.velocity = useEmitter
        ? emitterVelocityDirection * (useRandom ? RandomRange(baseSeed ^ 0x438f34abu, 0.25f, 1.15f) : 0.65f)
        : float3(direction.x * 0.25f * velocityScale, velocityY, velocityZ);
    particle.currentTime = 0.0f;
    particle.color = float4(saturate(float3(0.2f + colorRate * 0.8f, 0.85f, 1.0f - colorRate * 0.5f) + colorJitter), 0.75f);
    particle.lifeTime = useRandom ? RandomRange(baseSeed ^ 0x85ebca6bu, 1.0f, 3.5f) : lerp(1.5f, 3.0f, lifeRate);
    particle.alive = 1u;
    particle.padding = float2(0.0f, 0.0f);

    gParticles[particleIndex] = particle;
}
