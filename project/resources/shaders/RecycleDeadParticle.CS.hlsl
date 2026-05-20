struct RecycleInfo
{
    uint recycleCount;
    float3 padding;
};

ConsumeStructuredBuffer<uint> gDeadList : register(u0);
AppendStructuredBuffer<uint> gFreeList : register(u1);
ConstantBuffer<RecycleInfo> gRecycleInfo : register(b0);

[numthreads(1024, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint recycleIndex = dispatchThreadId.x;
    if (recycleIndex >= gRecycleInfo.recycleCount)
    {
        return;
    }

    uint particleIndex = gDeadList.Consume();
    gFreeList.Append(particleIndex);
}
