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
    uint physicsFlags;
    float collisionPlaneY;
    float restitution;
    float friction;
    float bounceVelocityThreshold;
    uint maxBounceCount;
    float collisionDamping;
    uint padding1;
    float padding2;
};

struct UpdateInfo
{
    uint particleCount;
    float deltaTime;
    uint freeListEnabled;
    uint deadListEnabled;
};

static const uint kParticlePhysicsEnable = 1u << 0;
static const uint kParticlePhysicsPlaneCollision = 1u << 1;
static const uint kParticlePhysicsKillBelowPlane = 1u << 2;

RWStructuredBuffer<Particle> gParticles : register(u0);
AppendStructuredBuffer<uint> gFreeList : register(u1);
AppendStructuredBuffer<uint> gDeadList : register(u2);
StructuredBuffer<ParticleType> gParticleTypes : register(t0);
ConstantBuffer<UpdateInfo> gUpdateInfo : register(b0);

void RecycleParticle(uint particleIndex, inout Particle particle)
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

uint GetBounceCount(Particle particle)
{
    return asuint(particle.padding);
}

void SetBounceCount(inout Particle particle, uint bounceCount)
{
    particle.padding = asfloat(bounceCount);
}

void ResolvePlaneCollision(uint particleIndex, ParticleType particleType, inout Particle particle)
{
    const uint physicsFlags = particleType.physicsFlags;
    if ((physicsFlags & kParticlePhysicsEnable) == 0u)
    {
        return;
    }
    if ((physicsFlags & kParticlePhysicsPlaneCollision) == 0u)
    {
        if ((physicsFlags & kParticlePhysicsKillBelowPlane) != 0u && particle.translate.y < particleType.collisionPlaneY)
        {
            RecycleParticle(particleIndex, particle);
        }
        return;
    }
    if (particle.translate.y >= particleType.collisionPlaneY)
    {
        return;
    }

    particle.translate.y = particleType.collisionPlaneY;
    uint bounceCount = GetBounceCount(particle);
    const float threshold = max(particleType.bounceVelocityThreshold, 0.0f);
    const bool canBounce = particle.velocity.y < -threshold && bounceCount < particleType.maxBounceCount;
    if (canBounce)
    {
        particle.velocity.y = -particle.velocity.y * saturate(particleType.restitution);
        particle.velocity.xz *= saturate(particleType.friction);
        particle.velocity *= max(particleType.collisionDamping, 0.0f);
        SetBounceCount(particle, bounceCount + 1u);
        return;
    }

    particle.velocity.y = 0.0f;
    particle.velocity.xz *= saturate(particleType.friction);
    if ((physicsFlags & kParticlePhysicsKillBelowPlane) != 0u && bounceCount >= particleType.maxBounceCount)
    {
        RecycleParticle(particleIndex, particle);
    }
}

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
    ResolvePlaneCollision(particleIndex, particleType, particle);
    particle.currentTime = min(particle.currentTime, particle.lifeTime);
    particle.scale = lerp(particleType.startScale, particleType.endScale, saturate(particle.currentTime / max(particle.lifeTime, 0.0001f)));
    if (particle.alive != 0u && particle.currentTime >= particle.lifeTime)
    {
        RecycleParticle(particleIndex, particle);
    }

    gParticles[particleIndex] = particle;
}
