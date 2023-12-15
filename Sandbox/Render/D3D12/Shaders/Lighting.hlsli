#include "PBRUtils.hlsli"
#include "RootSignatures/ShaderStructures.hlsli"

static const half LightClippingThreshold = 0.001;

struct LightCalcData
{
  half3  toLight;
  float3 lightCenter;
  half3  lightDirection;
  half   attenuationConstant;
  half   attenuationLinear;
  half   attenuationQuadratic;
  half   theta;
  half   phi;
  bool   castShadow;
};

LightCalcData CalcLightData( LightParams light, float3 worldPosition )
{
  LightCalcData lcd;
  lcd.castShadow           = light.castShadow;
  lcd.theta                = 0;
  lcd.phi                  = 0;
  lcd.lightDirection       = light.direction.xyz;
  lcd.attenuationConstant  = light.attenuation.x;
  lcd.attenuationLinear    = light.attenuation.y;
  lcd.attenuationQuadratic = light.attenuation.z;

  switch ( light.type )
  {
  case LightType::Directional:
    lcd.lightCenter = worldPosition - float3( light.direction.xyz ) * 1000;
    lcd.toLight     = -light.direction.xyz;
    break;

  case LightType::Spot:
    lcd.theta = light.theta_phi.x;
    lcd.phi   = light.theta_phi.y;
    // fall through

  case LightType::Point:
    lcd.lightCenter = light.origin.xyz;
    lcd.toLight     = half3( normalize( lcd.lightCenter - worldPosition ) );
    break;
  }

  return lcd;
}

half CalcDiffuseTerm( LightCalcData lcd, float3 worldPosition, half3 worldNormal )
{
  half NdotL = saturate( dot( worldNormal, lcd.toLight ) );

  // Attenuation
  float distance   = length( lcd.lightCenter - worldPosition );
  half attenuation = half( 1.0 / ( lcd.attenuationConstant + lcd.attenuationLinear * distance + lcd.attenuationQuadratic * distance * distance ) );
  NdotL *= attenuation;

  // Spot light cone
  if ( lcd.theta > 0 )
  {
    half angle = dot( -lcd.toLight, lcd.lightDirection );
    half cone  = saturate( ( angle - lcd.phi ) / ( lcd.theta - lcd.phi ) );
    NdotL *= cone;
  }

  return NdotL;
}

half3 CalcDirectLight( half3 albedo
                     , half roughness
                     , half metallic
                     , LightParams light
                     , float3 worldPosition
                     , half3 worldNormal
                     , float3 cameraPosition
                     , half3 toCamera
                     , half3 F0
                     , half  NdotC )
{
  LightCalcData lcd = CalcLightData( light, worldPosition );

  half NdotL = CalcDiffuseTerm( lcd, worldPosition, worldNormal );
  [branch]
  if ( NdotL <= LightClippingThreshold )
    return 0;

  half3 halfVector = normalize( lcd.toLight + toCamera );

  half NdotH = saturate( dot( worldNormal, halfVector ) );

  half3 fresnel = fresnelSchlick( F0, saturate( dot( halfVector, toCamera ) ) );

  // Calculate normal distribution for specular BRDF.
  half D = ndfGGX( NdotH, roughness );
  // Calculate geometric attenuation for specular BRDF.
  half G = gaSchlickGGX( NdotL, NdotC, roughness );

  // Diffuse scattering happens due to light being refracted multiple times by a dielectric medium.
  // Metals on the other hand either reflect or absorb energy, so diffuse contribution is always zero.
  // To be energy conserving we must scale diffuse BRDF contribution based on Fresnel factor & metalness.
  half3 kd = lerp( 1.0 - fresnel, 0.0, metallic );
  
  half3 diffuseBRDF = kd * albedo;
  half3 specularBRDF = half3( min( float3(fresnel * D * G) / max( PBREpsilon, 4.0h * NdotL * NdotC ), 128 ) );

  half3 directLighting = ( diffuseBRDF + specularBRDF ) * NdotL * light.color.rgb;

  return directLighting;
}

half3 TraceDirectLighting( half3 albedo
                         , half roughness
                         , half metallic
                         , float3 worldPosition
                         , half3 worldNormal
                         , half3 shadow
                         , float3 cameraPosition
                         , uint lightCount )
{
  half3  toCamera = half3( normalize( cameraPosition - worldPosition ) );
  half   NdotC    = saturate( dot( worldNormal, toCamera ) );
  half3  F0       = lerp( Fdielectric, albedo, metallic );
  
  half3 directLighting = 0;

  if ( any( shadow > 0 ) )
    directLighting += CalcDirectLight( albedo, roughness, metallic, lights[ 0 ], worldPosition, worldNormal, cameraPosition, toCamera, F0, NdotC ) * shadow;

  // There is something wrong if this is enabled, and some objects becoming purple.
  // for ( uint lightIx = 1; lightIx < lightCount; ++lightIx )
  //   directLighting += CalcDirectLight( albedo, roughness, metallic, lights[ lightIx ], worldPosition, worldNormal, cameraPosition, toCamera, F0, NdotC );

  return directLighting;
}

void TraceIndirectLighting( half3 gi
                          , Texture2D< half2 > specBRDFLUT
                          , half3 albedo
                          , half roughness
                          , half metallic
                          , float3 worldPosition
                          , half3 worldNormal
                          , float3 cameraPosition
                          , out half3 diffuseIBL
                          , out half3 specularIBL )
{
  half3 toCamera     = half3( normalize( cameraPosition - worldPosition ) );
  half  NdotC        = saturate( dot( worldNormal, toCamera ) );
  half3 specRef      = 2.0 * NdotC * worldNormal - toCamera;
  half3 F0           = lerp( Fdielectric, albedo, metallic );
  half3 fresnel      = fresnelSchlick( F0, NdotC );
  half3 kd           = lerp( 1.0 - fresnel, 0.0, metallic );
  half2 specularBRDF = half2( specBRDFLUT.Sample( trilinearClampSampler, float2( NdotC, roughness ) ).rg );
  
  diffuseIBL  = kd * albedo * gi;
  specularIBL = F0 * specularBRDF.x + specularBRDF.y;
}
