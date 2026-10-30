struct TransformationMatrix
{
    float4x4 WVP;
    float4x4 World;
    float4x4 WorldInverseTranspose;
    float4 outline;
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

struct VertexShaderInput
{
    float4 pos : POSITION;
    float2 uv : TEXCOORD;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float jointWarning : TEXCOORD1;
};

struct VertexShaderOutput
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
    float3 normal : NORMAL;
    float3 worldPos : TEXCOORD1;
    float3 tangent : TANGENT;
    float jointWarning : TEXCOORD2;
    nointerpolation float outline : TEXCOORD3;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    
    output.pos = mul(input.pos, gTransformationMatrix.WVP);
    output.outline=gTransformationMatrix.outline.w;
    if(output.outline>0 && output.pos.w>0){
        float4 expanded=mul(float4(input.pos.xyz+normalize(input.normal),1),gTransformationMatrix.WVP);
        float2 delta=expanded.xy/max(expanded.w,.001)-output.pos.xy/output.pos.w;
        float2 invSize=gTransformationMatrix.outline.yz;
        float2 pixels=delta/max(invSize,.000001);
        float magnitude=length(pixels);
        if(magnitude>.0001)output.pos.xy+=pixels/magnitude*invSize*gTransformationMatrix.outline.x*output.pos.w;
    }
    output.uv = input.uv;
    output.jointWarning = input.jointWarning;
    
    // 法線の変換に逆転置行列の3x3部分を使用
    output.normal = normalize(mul(input.normal, (float3x3) gTransformationMatrix.WorldInverseTranspose));
    output.tangent = normalize(mul(input.tangent, (float3x3) gTransformationMatrix.World));
    output.worldPos = mul(input.pos, gTransformationMatrix.World).xyz;
    
    return output;
}
