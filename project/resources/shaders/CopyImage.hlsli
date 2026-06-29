struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

struct PostEffectParameters
{
    uint gaussianEnabled;
    float gaussianIntensity;
    uint smoothingEnabled;
    float smoothingIntensity;
    uint grayscaleEnabled;
    float grayscaleIntensity;
    uint sepiaEnabled;
    float sepiaIntensity;
    uint invertEnabled;
    float invertIntensity;
    uint vignetteEnabled;
    float vignetteIntensity;
    uint radialBlurEnabled;
    float radialBlurStrength;
    float2 radialBlurCenter;
    uint radialBlurSampleCount;
    float radialBlurCenterClearRadius;
    float radialBlurOuterEffectRadius;
    uint dissolveEnabled;
    float dissolveThreshold;
    float dissolveEdgeWidth;
    uint outlineMode;
    float outlineIntensity;
    float outlineThickness;
    float outlineThreshold;
    float outlineSoftness;
    float outlineDepthThreshold;
    float outlineDepthStrength;
    float outlineNormalThreshold;
    float outlineNormalStrength;
    uint hybridColorSource;
    float hybridColorWeight;
    float hybridDepthWeight;
    float hybridNormalWeight;
    float depthNear;
    float depthFar;
    float postEffectPadding;
    float4 dissolveEdgeColor;
    float4 outlineColor;
};
