AppendStructuredBuffer<uint> gFreeList : register(u0);

static const uint kMaxParticles = 1024;

[numthreads(1024, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint particleIndex = dispatchThreadId.x;
    if (particleIndex < kMaxParticles)
    {
        gFreeList.Append(particleIndex);
    }
}
