struct SkinningInformation
{
    uint numVertices;
    uint3 padding;
};

struct Vertex
{
    float4 position;
    float2 texcoord;
    float3 normal;
    float3 tangent;
};

struct VertexInfluence
{
    float4 weight;
    uint4 index;
};

struct MatrixPalette
{
    row_major float4x4 matrix;
};

StructuredBuffer<Vertex> gInputVertices : register(t0);
StructuredBuffer<VertexInfluence> gVertexInfluences : register(t1);
StructuredBuffer<MatrixPalette> gMatrixPalette : register(t2);
RWStructuredBuffer<Vertex> gOutputVertices : register(u0);
ConstantBuffer<SkinningInformation> gSkinningInformation : register(b0);

[numthreads(1024, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint vertexIndex = dispatchThreadId.x;
    if (vertexIndex < gSkinningInformation.numVertices)
    {
        Vertex inputVertex = gInputVertices[vertexIndex];
        VertexInfluence influence = gVertexInfluences[vertexIndex];

        float4 sourcePosition = float4(inputVertex.position.xyz, 1.0f);
        float4 sourceNormal = float4(inputVertex.normal, 0.0f);
        float4 sourceTangent = float4(inputVertex.tangent, 0.0f);
        float4 skinnedPosition = float4(0.0f, 0.0f, 0.0f, 0.0f);
        float3 skinnedNormal = float3(0.0f, 0.0f, 0.0f);
        float3 skinnedTangent = float3(0.0f, 0.0f, 0.0f);
        float totalWeight = 0.0f;

        [unroll]
        for (uint influenceIndex = 0; influenceIndex < 4; ++influenceIndex)
        {
            float weight = influence.weight[influenceIndex];
            if (weight <= 0.000001f)
            {
                continue;
            }

            uint paletteIndex = influence.index[influenceIndex];
            row_major float4x4 skinningMatrix = gMatrixPalette[paletteIndex].matrix;

            skinnedPosition += mul(sourcePosition, skinningMatrix) * weight;
            skinnedNormal += mul(sourceNormal, skinningMatrix).xyz * weight;
            skinnedTangent += mul(sourceTangent, skinningMatrix).xyz * weight;
            totalWeight += weight;
        }

        Vertex outputVertex = inputVertex;
        if (totalWeight > 0.000001f)
        {
            outputVertex.position = float4(skinnedPosition.xyz, 1.0f);

            float normalLength = length(skinnedNormal);
            if (normalLength > 0.000001f)
            {
                outputVertex.normal = skinnedNormal / normalLength;
            }

            float tangentLength = length(skinnedTangent);
            if (tangentLength > 0.000001f)
            {
                outputVertex.tangent = skinnedTangent / tangentLength;
            }
        }

        gOutputVertices[vertexIndex] = outputVertex;
    }
}
