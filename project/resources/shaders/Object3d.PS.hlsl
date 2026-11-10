struct DirectionalLight
{
    float4 color;
    float3 direction;
    float intensity;
    float3 cameraPosition;
    float shininess;
    float4 styleControl,styleShadow,styleHighlight,styleRim,styleBands,styleSun,styleSunColor;
};

struct Material
{
    float4 color;
    int enableLighting;
    float4x4 uvTransform;
    float alphaReference;
    int usePBR;
    float metallicFactor;
    float roughnessFactor;
    float normalScale;
    int hasNormalMap;
    int hasMetallicRoughnessMap;
    int hasSpecularF0Map;
};

struct EnvironmentMapData
{
    int enableEnvironmentMap;
    float intensity;
    float2 padding;
};

struct DissolveData
{
    int enableDissolve;
    float threshold;
    float edgeWidth;
    float edgeGlowStrength;
    float edgeNoiseStrength;
    float3 padding;
    float4 edgeColor;
};

struct RandomNoiseData
{
    int enableRandom;
    int previewRandom;
    float intensity;
    float time;
};

struct RingAppearanceData
{
    int enableRingAppearance;
    int uvDirection;
    float innerRadiusRatio;
    float startAlpha;
    float endAlpha;
    float startFadeRange;
    float endFadeRange;
    float padding;
    float4 innerColor;
    float4 outerColor;
};

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);
ConstantBuffer<EnvironmentMapData> gEnvironmentMapData : register(b2);
ConstantBuffer<DissolveData> gDissolveData : register(b3);
ConstantBuffer<RandomNoiseData> gRandomNoiseData : register(b4);
ConstantBuffer<RingAppearanceData> gRingAppearanceData : register(b5);

Texture2D<float4> gTexture : register(t0);
TextureCube<float4> gEnvironmentTexture : register(t1);
Texture2D<float4> gDissolveMaskTexture : register(t2);
Texture2D<float4> gNormalTexture : register(t3);
Texture2D<float4> gMetallicRoughnessTexture : register(t4);
Texture2D<float4> gSpecularF0Texture : register(t5);
SamplerState gSampler : register(s0);

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

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
    float4 normal : SV_TARGET1;
};

float rand2dTo1d(float2 value)
{
    return frac(sin(dot(value, float2(12.9898f, 78.233f))) * 43758.5453f);
}

float2 GetRingLocalPosition(float2 uv)
{
    return float2((uv.x - 0.5f) * 2.0f, (0.5f - uv.y) * 2.0f);
}

float ComputeRingRadial(float2 uv)
{
    float radius = length(GetRingLocalPosition(uv));
    float innerRadiusRatio = saturate(gRingAppearanceData.innerRadiusRatio);
    return saturate((radius - innerRadiusRatio) / max(1.0f - innerRadiusRatio, 0.0001f));
}

float ComputeRingProgress(float2 uv)
{
    float2 local = GetRingLocalPosition(uv);
    return frac(atan2(local.y, local.x) / (2.0f * 3.14159265359f) + 1.0f);
}

float ComputeRingStartAlpha(float progress)
{
    float fadeRange = max(gRingAppearanceData.startFadeRange, 0.0001f);
    float blend = 1.0f - smoothstep(0.0f, fadeRange, progress);
    return lerp(1.0f, gRingAppearanceData.startAlpha, blend);
}

float ComputeRingEndAlpha(float progress)
{
    float fadeRange = max(gRingAppearanceData.endFadeRange, 0.0001f);
    float blend = smoothstep(1.0f - fadeRange, 1.0f, progress);
    return lerp(1.0f, gRingAppearanceData.endAlpha, blend);
}

float3 ResolveSurfaceNormal(VertexShaderOutput input, float2 uv)
{
    float3 N = normalize(input.normal);
    if (gMaterial.usePBR == 0 || gMaterial.hasNormalMap == 0)
    {
        return N;
    }

    float3 T = normalize(input.tangent - N * dot(N, input.tangent));
    float3 B = normalize(cross(N, T));
    float3 tangentNormal = gNormalTexture.Sample(gSampler, uv).xyz * 2.0f - 1.0f;
    tangentNormal.xy *= gMaterial.normalScale;
    tangentNormal = normalize(tangentNormal);
    return normalize(mul(tangentNormal, float3x3(T, B, N)));
}

float3 FresnelSchlick(float cosTheta, float3 f0)
{
    return f0 + (1.0f - f0) * pow(1.0f - saturate(cosTheta), 5.0f);
}

float3 ComputeSimplePbrLighting(float3 baseColor, float3 N, float3 worldPos, float2 uv)
{
    float metallic = saturate(gMaterial.metallicFactor);
    float roughness = saturate(gMaterial.roughnessFactor);
    if (gMaterial.hasMetallicRoughnessMap != 0)
    {
        float4 metallicRoughness = gMetallicRoughnessTexture.Sample(gSampler, uv);
        roughness = saturate(metallicRoughness.g * gMaterial.roughnessFactor);
        metallic = saturate(metallicRoughness.b * gMaterial.metallicFactor);
    }
    roughness = clamp(roughness, 0.03f, 1.0f);

    float3 L = normalize(-gDirectionalLight.direction);
    float3 V = normalize(gDirectionalLight.cameraPosition - worldPos);
    float3 H = normalize(L + V);
    float NdotL = saturate(dot(N, L));
    float NdotV = max(saturate(dot(N, V)), 0.001f);
    float NdotH = saturate(dot(N, H));
    float VdotH = saturate(dot(V, H));

    float3 dielectricF0 = float3(0.04f, 0.04f, 0.04f);
    if (gMaterial.hasSpecularF0Map != 0)
    {
        dielectricF0 = saturate(gSpecularF0Texture.Sample(gSampler, uv).rgb);
    }
    float3 f0 = lerp(dielectricF0, baseColor, metallic);
    float3 F = FresnelSchlick(VdotH, f0);

    float shininess = lerp(192.0f, 8.0f, roughness);
    float specularPower = pow(NdotH, shininess);
    float specularEnergy = lerp(1.0f, 0.25f, roughness);
    float3 specular = F * specularPower * specularEnergy * gDirectionalLight.intensity;
    float3 diffuse = baseColor * (1.0f - metallic) * NdotL * gDirectionalLight.color.rgb * gDirectionalLight.intensity;
    float3 ambient = baseColor * 0.08f;

    // Keep a tiny view term so very grazing surfaces do not explode when roughness is low.
    specular *= saturate(NdotL) * saturate(NdotV * 2.0f);
    return ambient + diffuse + specular;
}

float3 ComputeAnimeLighting(float3 baseColor, float3 N, float3 worldPos)
{
    float3 L = normalize(-gDirectionalLight.direction);
    float3 V = normalize(gDirectionalLight.cameraPosition - worldPos);
    float ndl = saturate(dot(N,L));
    // Derivative widening keeps the two soft boundaries stable in motion.
    float width = max(gDirectionalLight.styleControl.z, fwidth(ndl));
    float floorValue = gDirectionalLight.styleControl.w;
    float band = smoothstep(.5f-width,.5f+width,ndl);
    if (gDirectionalLight.styleControl.y > 2.5f)
        band = .5f * (smoothstep(.32f-width,.32f+width,ndl) + smoothstep(.68f-width,.68f+width,ndl));
    float3 diffuse = baseColor * lerp(gDirectionalLight.styleShadow.rgb * floorValue, 1.0f, band);
    float4 h = gDirectionalLight.styleHighlight;
    float ndh = saturate(dot(N,normalize(L+V)));
    float highlightWidth = max(.02f, fwidth(ndh));
    float highlight = smoothstep(1-h.y-highlightWidth,1-h.y+highlightWidth,ndh) * h.x * ndl;
    float rim = pow(1-saturate(dot(N,V)),h.w) * h.z * (.3f+.7f*ndl);
    return diffuse * gDirectionalLight.color.rgb * gDirectionalLight.intensity + baseColor*.08f
        + highlight * gDirectionalLight.color.rgb + rim * gDirectionalLight.styleRim.rgb;
}

float3 ComputeEmphasizedLighting(float3 baseColor,float3 N,float3 worldPos) {
    float3 L=normalize(gDirectionalLight.styleSun.w>0?-gDirectionalLight.styleSun.xyz:-gDirectionalLight.direction);
    float3 lightColor=gDirectionalLight.styleSun.w>0?gDirectionalLight.styleSunColor.rgb:gDirectionalLight.color.rgb;
    float3 V=normalize(gDirectionalLight.cameraPosition-worldPos);
    float ndl=saturate(dot(N,L));float4 b=gDirectionalLight.styleBands;
    float x=pow(ndl,b.w),w=max(gDirectionalLight.styleControl.z,fwidth(x));
    float lo=smoothstep(b.x-w,b.x+w,x),hi=smoothstep(b.y-w,b.y+w,x);
    float luma=dot(baseColor,float3(.2126,.7152,.0722));
    float3 shadow=lerp(baseColor,luma.xxx,.2)*gDirectionalLight.styleShadow.rgb*gDirectionalLight.styleControl.w;
    float3 mid=baseColor*lerp(gDirectionalLight.styleShadow.rgb,1,.20)*b.z;
    float bright=gDirectionalLight.styleSun.w>0?.95:.82;
    float3 diffuse=gDirectionalLight.styleControl.y>2.5?lerp(lerp(shadow,mid,lo),baseColor*bright,hi):lerp(shadow,baseColor*bright,lo);
    float4 h=gDirectionalLight.styleHighlight;float ndh=saturate(dot(N,normalize(L+V))),hw=max(.012,fwidth(ndh));
    float spec=smoothstep(1-h.y-hw,1-h.y+hw,ndh)*h.x*ndl;
    float rim=pow(1-saturate(dot(N,V)),h.w)*h.z*(.65+.35*ndl)*(1-.4*ndl);
    return diffuse*lightColor*gDirectionalLight.intensity+baseColor*.025+spec*lightColor+rim*gDirectionalLight.styleRim.rgb;
}

PixelShaderOutput main(VertexShaderOutput input)
{
    float4 transformedUV = mul(float4(input.uv, 0.0f, 1.0f), gMaterial.uvTransform);
    float4 texColor = gTexture.Sample(gSampler, transformedUV.xy);
    if (gRingAppearanceData.enableRingAppearance != 0)
    {
        float radial = ComputeRingRadial(transformedUV.xy);
        float progress = ComputeRingProgress(transformedUV.xy);
        float2 ringSampleUV = (gRingAppearanceData.uvDirection == 0) ?
            float2(radial, 0.5f) :
            float2(0.5f, radial);
        float4 ringTint = lerp(gRingAppearanceData.innerColor, gRingAppearanceData.outerColor, radial);
        float alphaFactor = ComputeRingStartAlpha(progress) * ComputeRingEndAlpha(progress);
        texColor = gTexture.Sample(gSampler, ringSampleUV) * ringTint;
        texColor.a *= alphaFactor;
    }
    float dissolveEdge = 0.0f;
    if (gDissolveData.enableDissolve != 0)
    {
        float dissolveMask = gDissolveMaskTexture.Sample(gSampler, transformedUV.xy).r;
        clip(dissolveMask - gDissolveData.threshold);
        float edgeWidth = max(gDissolveData.edgeWidth, 0.0001f);
        dissolveEdge = 1.0f - smoothstep(gDissolveData.threshold, gDissolveData.threshold + edgeWidth, dissolveMask);
    }
    
    // Zバッファの不具合を防ぐため透明な部分は破棄
    float alphaDiscardThreshold = (gRingAppearanceData.enableRingAppearance != 0) ? 0.01f : gMaterial.alphaReference;
    if (texColor.a <= alphaDiscardThreshold)
    {
        discard;
    }
    
    float4 outputColor = gMaterial.color * texColor;

    if (gMaterial.enableLighting != 0)
    {
        float3 N = ResolveSurfaceNormal(input, transformedUV.xy);
        float3 baseColor=outputColor.rgb;
        if (gMaterial.usePBR != 0)
        {
            outputColor.rgb = ComputeSimplePbrLighting(outputColor.rgb, N, input.worldPos, transformedUV.xy);
        }
        else
        {
            float3 L = normalize(-gDirectionalLight.direction);
        
            // 拡散反射 (Lambert)
            float NdotL = saturate(dot(N, L));
            float3 diffuse = gDirectionalLight.color.rgb * NdotL * gDirectionalLight.intensity;
        
            // 鏡面反射 (Blinn-Phong)
            float3 V = normalize(gDirectionalLight.cameraPosition - input.worldPos);
            float3 H = normalize(L + V);
            float NdotH = saturate(dot(N, H));
            float specularWeight = pow(NdotH, gDirectionalLight.shininess);
            float3 specular = gDirectionalLight.color.rgb * specularWeight * gDirectionalLight.intensity;
        
            // 環境光 (Ambient)
            float3 ambient = float3(0.1f, 0.1f, 0.1f);
        
            // 合成
            outputColor.rgb = outputColor.rgb * (diffuse + ambient) + specular;
        }
        if(gDirectionalLight.styleControl.x>0) {
            float3 anime=gDirectionalLight.styleBands.w>0?ComputeEmphasizedLighting(baseColor,normalize(input.normal),input.worldPos):ComputeAnimeLighting(baseColor,N,input.worldPos);
            outputColor.rgb=lerp(outputColor.rgb,anime,gDirectionalLight.styleControl.x);
        }
    }

    if (gEnvironmentMapData.enableEnvironmentMap != 0)
    {
        float3 N = ResolveSurfaceNormal(input, transformedUV.xy);
        float3 V = normalize(gDirectionalLight.cameraPosition - input.worldPos);
        float3 reflection = reflect(-V, N);
        float3 reflectionColor = gEnvironmentTexture.Sample(gSampler, reflection).rgb;
        outputColor.rgb = lerp(outputColor.rgb, reflectionColor, saturate(gEnvironmentMapData.intensity)*(1-.75*gDirectionalLight.styleControl.x));
    }

    if (gDissolveData.enableDissolve != 0)
    {
        float edgeNoise = rand2dTo1d(transformedUV.xy * 32.0f + gRandomNoiseData.time.xx);
        float edgeVariation = lerp(
            1.0f - gDissolveData.edgeNoiseStrength,
            1.0f + gDissolveData.edgeNoiseStrength,
            edgeNoise);
        float edgeGlow = dissolveEdge * edgeVariation;
        outputColor.rgb = lerp(outputColor.rgb, gDissolveData.edgeColor.rgb, saturate(edgeGlow * gDissolveData.edgeColor.a));
        outputColor.rgb += gDissolveData.edgeColor.rgb * edgeGlow * gDissolveData.edgeGlowStrength;
    }

    if (gRandomNoiseData.enableRandom != 0)
    {
        float random = rand2dTo1d(transformedUV.xy * 32.0f + gRandomNoiseData.time.xx);
        if (gRandomNoiseData.previewRandom != 0)
        {
            outputColor.rgb = random.xxx;
        }
        else
        {
            float noiseFactor = lerp(1.0f, random, saturate(gRandomNoiseData.intensity));
            outputColor.rgb *= noiseFactor;
        }
    }

    // Disabled vertices take the original shading path unchanged, including alpha.
    if (input.jointWarning > 0.0f)
    {
        float3 N = ResolveSurfaceNormal(input, transformedUV.xy);
        float facingLight = saturate(dot(N, normalize(-gDirectionalLight.direction)));
        float textureDetail = saturate(dot(texColor.rgb, float3(0.2126f, 0.7152f, 0.0722f)));
        float3 warningColor = float3(0.95f, 0.035f, 0.02f)
            * (0.45f + 0.55f * facingLight) * (0.65f + 0.35f * textureDetail);
        outputColor.rgb = lerp(outputColor.rgb, warningColor, saturate(input.jointWarning));
    }

    if(gDirectionalLight.styleControl.x>0 && gDirectionalLight.styleRim.w>0) {
        float3 L=normalize(gDirectionalLight.styleSun.w>0?-gDirectionalLight.styleSun.xyz:-gDirectionalLight.direction);
        float3 N=gDirectionalLight.styleBands.w>0?normalize(input.normal):ResolveSurfaceNormal(input,transformedUV.xy);
        outputColor.rgb=gDirectionalLight.styleRim.w<1.5?(gDirectionalLight.styleShadow.w<1.5?float3(.03,1,.08):float3(1,.03,.8)):saturate(dot(N,L)).xxx;
    }
    if(input.outline>0)outputColor.rgb=float3(.012,.024,.062);
    PixelShaderOutput output;
    output.color = outputColor;
    output.normal = float4(ResolveSurfaceNormal(input, transformedUV.xy) * 0.5f + 0.5f, 1.0f);
    return output;
}
