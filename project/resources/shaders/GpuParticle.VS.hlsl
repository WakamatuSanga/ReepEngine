#include "GpuParticle.hlsli"

ConstantBuffer<PerView> gPerView : register(b0);
StructuredBuffer<Particle> gParticles : register(t0);

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

VertexShaderOutput main(uint vertexId : SV_VertexID, uint instanceId : SV_InstanceID)
{
    Particle particle = gParticles[instanceId];
    float alive = particle.alive != 0u ? 1.0f : 0.0f;

    float3 localPosition = float3(kPositions[vertexId] * particle.scale, 0.0f);
    float3 billboardOffset = mul(float4(localPosition, 0.0f), gPerView.billboardMatrix).xyz;
    float4 worldPosition = float4(particle.translate + billboardOffset, 1.0f);

    VertexShaderOutput output;
    output.position = mul(worldPosition, gPerView.viewProjection);
    output.texcoord = kTexcoords[vertexId];
    output.alive = alive;
    output.color = particle.color;
    return output;
}
