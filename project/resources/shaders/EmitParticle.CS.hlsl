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

struct EmitterInfo
{
    uint emitCount;
    uint randomSeed;
    uint randomEnabled;
    uint padding0;
    float3 emitterPosition;
    float emitterRadius;
};

RWStructuredBuffer<Particle> gParticles : register(u0);
ConsumeStructuredBuffer<uint> gFreeList : register(u1);
ConstantBuffer<EmitterInfo> gEmitterInfo : register(b0);

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
    uint emitIndex = dispatchThreadId.x;
    if (emitIndex >= gEmitterInfo.emitCount)
    {
        return;
    }

    uint particleIndex = gFreeList.Consume();
    uint baseSeed = particleIndex ^ (emitIndex * 1973u) ^ (gEmitterInfo.randomSeed * 747796405u) ^ 2891336453u;
    bool useRandom = gEmitterInfo.randomEnabled != 0u;

    float angle = float(emitIndex) / max(float(gEmitterInfo.emitCount), 1.0f) * 6.28318530718f;
    float3 deterministicDirection = normalize(float3(cos(angle), sin(angle), 0.25f) + float3(0.001f, 0.001f, 0.001f));
    float3 emitterDirection = useRandom ? RandomUnitVector(baseSeed ^ 0x7f4a7c15u) : deterministicDirection;
    float distanceRate = useRandom ? pow(Random01(baseSeed ^ 0x94d049bbu), 1.0f / 3.0f) : float(emitIndex % 16u) / 15.0f;
    float3 emitterOffset = emitterDirection * distanceRate * gEmitterInfo.emitterRadius;
    float3 velocityDirection = normalize(emitterOffset + float3(0.001f, 0.25f, 0.001f));
    float colorRate = Random01(baseSeed ^ 0x3f6e4a1bu);
    float3 colorJitter = useRandom
        ? float3(
            RandomRange(baseSeed ^ 0x1f123bb5u, -0.10f, 0.10f),
            RandomRange(baseSeed ^ 0xc2b2ae35u, -0.08f, 0.08f),
            RandomRange(baseSeed ^ 0x27d4eb2fu, -0.10f, 0.10f))
        : float3(0.0f, 0.0f, 0.0f);

    Particle particle;
    particle.translate = gEmitterInfo.emitterPosition + emitterOffset;
    particle.scale = useRandom ? RandomRange(baseSeed ^ 0x165667b1u, 0.035f, 0.075f) : 0.05f;
    particle.velocity = velocityDirection * (useRandom ? RandomRange(baseSeed ^ 0x438f34abu, 0.25f, 1.15f) : 0.65f);
    particle.currentTime = 0.0f;
    particle.color = float4(saturate(float3(0.2f + colorRate * 0.8f, 0.85f, 1.0f - colorRate * 0.5f) + colorJitter), 0.75f);
    particle.lifeTime = useRandom ? RandomRange(baseSeed ^ 0x85ebca6bu, 1.0f, 3.5f) : 2.0f;
    particle.alive = 1u;
    particle.padding = float2(0.0f, 0.0f);

    gParticles[particleIndex] = particle;
}
