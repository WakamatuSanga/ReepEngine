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

struct UpdateInfo
{
    uint particleCount;
    float deltaTime;
    uint freeListEnabled;
    uint deadListEnabled;
};

RWStructuredBuffer<Particle> gParticles : register(u0);
AppendStructuredBuffer<uint> gFreeList : register(u1);
AppendStructuredBuffer<uint> gDeadList : register(u2);
StructuredBuffer<ParticleType> gParticleTypes : register(t0);
ConstantBuffer<UpdateInfo> gUpdateInfo : register(b0);

[numthreads(1024, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint particleIndex = dispatchThreadId.x;
    if (particleIndex >= gUpdateInfo.particleCount)
    {
        return;
    }

    Particle particle = gParticles[particleIndex];
    if (particle.alive == 0u)
    {
        return;
    }

    particle.currentTime += gUpdateInfo.deltaTime;
    ParticleType particleType = gParticleTypes[particle.type];
    particle.velocity.y += particleType.gravity * gUpdateInfo.deltaTime;
    particle.velocity *= exp(-max(particleType.drag, 0.0f) * gUpdateInfo.deltaTime);
    particle.translate += particle.velocity * gUpdateInfo.deltaTime;
    particle.currentTime = min(particle.currentTime, particle.lifeTime);
    particle.scale = lerp(particleType.startScale, particleType.endScale, saturate(particle.currentTime / max(particle.lifeTime, 0.0001f)));
    if (particle.currentTime >= particle.lifeTime)
    {
        particle.alive = 0u;
        if (gUpdateInfo.freeListEnabled != 0u)
        {
            if (gUpdateInfo.deadListEnabled != 0u)
            {
                gDeadList.Append(particleIndex);
            }
            else
            {
                gFreeList.Append(particleIndex);
            }
        }
    }

    gParticles[particleIndex] = particle;
}
