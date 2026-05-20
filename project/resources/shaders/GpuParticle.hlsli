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
};
