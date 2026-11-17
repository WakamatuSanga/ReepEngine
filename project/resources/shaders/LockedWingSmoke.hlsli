struct SmokeConstants
{
    row_major float4x4 viewToWorld;
    float4 projection;
    uint2 outputSize;
    uint2 smokeSize;
    uint tileColumns;
    float density;
    float nearPlane;
    float padding;
};

ConstantBuffer<SmokeConstants> gSmoke : register(b0);
Texture2D<float> gDepth : register(t0);

float LoadSceneZ(uint2 pixel)
{
    float depth = gDepth.Load(int3(min(pixel, gSmoke.outputSize - 1), 0));
    return min(60000.0, gSmoke.projection.w / min(depth - gSmoke.projection.z, -0.000001));
}
