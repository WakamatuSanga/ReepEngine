struct TransformationMatrix {
    float4x4 WVP;
    float4 styleControl,styleZenith,styleHorizon,styleCloudShadow,styleCloudHighlight,styleSun,styleSunColor,styleCumulus,styleCamera,styleCumulusExtra,styleCumulusDistribution,styleCloudRelief,styleCloudLayout;
    float4 blueFraming;
    float4 depthControl,depthStyle;
    float4 bankCenters[16],bankRadii[16];
};
ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);
struct VertexShaderInput
{
    float4 position : POSITION0;
};

struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float3 direction : TEXCOORD0;
    float3 clipXYW : TEXCOORD1;
    nointerpolation float blueFramingStrength : TEXCOORD2;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;

    float4 clipPosition = mul(input.position, gTransformationMatrix.WVP);
    output.position = clipPosition.xyww;
    output.direction = input.position.xyz;
    // Pass homogeneous XY/W through a perspective interpolant, rather than
    // dividing cube vertices behind the eye. Pixel shader reconstructs NDC.
    output.clipXYW = clipPosition.xyw;
    output.blueFramingStrength = gTransformationMatrix.blueFraming.x;

    return output;
}
