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
    particle.translate += particle.velocity * gUpdateInfo.deltaTime;
    particle.currentTime = min(particle.currentTime, particle.lifeTime);
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
