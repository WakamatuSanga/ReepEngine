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
    uint affectedByInfluenceField;
    float influenceResponseScale;
    uint affectedByRailFlow;
    float railFlowScale;
    float2 padding2;
};

struct UpdateInfo
{
    uint particleCount;
    float deltaTime;
    uint freeListEnabled;
    uint deadListEnabled;
    float4 influenceCentersAndRadius[16];
    float4 influenceParams[16];
    uint influenceFieldCount;
    uint enableParticleInfluence;
    float particleInfluenceResponseScale;
    float padding;
    float4 railFlowDirectionSpeed;
    float4 railFlowSettings;
    float4 railFlowCameraPosition;
};

static const uint kParticlePhysicsEnable = 1u << 0;
static const uint kParticlePhysicsPlaneCollision = 1u << 1;
static const uint kParticlePhysicsKillBelowPlane = 1u << 2;
static const uint kInfluenceFieldFlagAffectGpuParticle = 1u << 3;

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

void ApplyInfluenceFields(ParticleType particleType, inout Particle particle)
{
    if (gUpdateInfo.enableParticleInfluence == 0u || particleType.affectedByInfluenceField == 0u)
    {
        return;
    }

    const float responseScale = max(particleType.influenceResponseScale, 0.0f) * max(gUpdateInfo.particleInfluenceResponseScale, 0.0f);
    if (responseScale <= 0.0f)
    {
        return;
    }

    const uint fieldCount = min(gUpdateInfo.influenceFieldCount, 16u);
    for (uint index = 0u; index < fieldCount; ++index)
    {
        const float4 centerAndRadius = gUpdateInfo.influenceCentersAndRadius[index];
        const float4 params = gUpdateInfo.influenceParams[index];
        const uint flags = (uint)(params.w + 0.5f);
        if ((flags & kInfluenceFieldFlagAffectGpuParticle) == 0u)
        {
            continue;
        }

        const float radius = max(centerAndRadius.w, 0.0001f);
        const float3 toParticle = particle.translate - centerAndRadius.xyz;
        const float distanceSq = dot(toParticle, toParticle);
        if (distanceSq >= radius * radius)
        {
            continue;
        }

        const float distance = sqrt(max(distanceSq, 0.000001f));
        const float3 direction = distance > 0.0001f ? toParticle / distance : float3(0.0f, 1.0f, 0.0f);
        const float falloff = pow(saturate(1.0f - distance / radius), max(params.z, 0.01f));
        particle.velocity += direction * max(params.x, 0.0f) * falloff * responseScale * gUpdateInfo.deltaTime;
    }
}
float3 GetRailFlowVelocity(ParticleType particleType)
{
    if (gUpdateInfo.railFlowSettings.x <= 0.5f || particleType.affectedByRailFlow == 0u)
    {
        return float3(0.0f, 0.0f, 0.0f);
    }

    const float directionLengthSq = dot(gUpdateInfo.railFlowDirectionSpeed.xyz, gUpdateInfo.railFlowDirectionSpeed.xyz);
    if (directionLengthSq <= 0.000001f)
    {
        return float3(0.0f, 0.0f, 0.0f);
    }

    const float3 direction = gUpdateInfo.railFlowDirectionSpeed.xyz * rsqrt(directionLengthSq);
    const float speed = max(gUpdateInfo.railFlowDirectionSpeed.w, 0.0f);
    const float globalScale = max(gUpdateInfo.railFlowSettings.y, 0.0f);
    const float typeScale = max(particleType.railFlowScale, 0.0f);
    return direction * speed * globalScale * typeScale;
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
    ApplyInfluenceFields(particleType, particle);
    const float3 railFlowVelocity = GetRailFlowVelocity(particleType);
    particle.translate += (particle.velocity + railFlowVelocity) * gUpdateInfo.deltaTime;
    ResolvePlaneCollision(particleIndex, particleType, particle);
    particle.currentTime = min(particle.currentTime, particle.lifeTime);
    particle.scale = lerp(particleType.startScale, particleType.endScale, saturate(particle.currentTime / max(particle.lifeTime, 0.0001f)));
    if (particle.alive != 0u && particle.currentTime >= particle.lifeTime)
    {
        RecycleParticle(particleIndex, particle);
    }

    gParticles[particleIndex] = particle;
}
