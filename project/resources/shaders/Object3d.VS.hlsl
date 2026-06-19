struct TransformationMatrix
{
    float4x4 WVP;
    float4x4 World;
    float4x4 WorldInverseTranspose;
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

struct VertexShaderInput
{
    float4 pos : POSITION;
    float2 uv : TEXCOORD;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
};

struct VertexShaderOutput
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
    float3 normal : NORMAL;
    float3 worldPos : TEXCOORD1;
    float3 tangent : TANGENT;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    
    output.pos = mul(input.pos, gTransformationMatrix.WVP);
    output.uv = input.uv;
    
    // 法線の変換に逆転置行列の3x3部分を使用
    output.normal = normalize(mul(input.normal, (float3x3) gTransformationMatrix.WorldInverseTranspose));
    output.tangent = normalize(mul(input.tangent, (float3x3) gTransformationMatrix.World));
    output.worldPos = mul(input.pos, gTransformationMatrix.World).xyz;
    
    return output;
}
