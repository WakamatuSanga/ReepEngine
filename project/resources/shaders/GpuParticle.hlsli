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

struct PerView
{
    row_major float4x4 viewProjection;
    row_major float4x4 billboardMatrix;
};

struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float alive : TEXCOORD1;
    float4 color : COLOR0;
    float4 typeColor : COLOR1;
    nointerpolation uint textureIndex : TEXCOORD2;
};
