#include "TextureSampling.hlsli"

half3 CalcSurfaceNormal( int normalTextureIndex, float2 texcoord, half3 worldNormal, half3 worldTangent, half3 worldBitangent, half mipLevel, uint2 screenPosition, uint feedbackPhase )
{
  half3 geometryWorldNormal = normalize( worldNormal );
  half3 worldSurfaceNormal  = geometryWorldNormal;

  [branch]
  if ( normalTextureIndex >= 0 )
  {
    half3x3 toWorldSpace  = half3x3( worldTangent, worldBitangent, worldNormal );
    half3   surfaceNormal = half3( SampleTexture( normalTextureIndex, texcoord, mipLevel, screenPosition, feedbackPhase ).rg * 2 - 1, 0 );
    surfaceNormal.z = sqrt( 1.0h - dot( surfaceNormal.xy, surfaceNormal.xy ) );
  
    worldSurfaceNormal = normalize( mul( surfaceNormal, toWorldSpace ) );
  }

  return worldSurfaceNormal;
}

half3 CalcSurfaceNormal( int normalTextureIndex, float2 texcoord, half3 worldNormal, half3 worldTangent, half3 worldBitangent, half mipLevel )
{
  half3 geometryWorldNormal = normalize( worldNormal );
  half3 worldSurfaceNormal  = geometryWorldNormal;

  [branch]
  if ( normalTextureIndex >= 0 )
  {
    half3x3 toWorldSpace  = half3x3( worldTangent, worldBitangent, worldNormal );
    half3   surfaceNormal = half3( SampleTexture( normalTextureIndex, texcoord, mipLevel ).rg * 2 - 1, 0 );
    surfaceNormal.z = sqrt( 1.0h - dot( surfaceNormal.xy, surfaceNormal.xy ) );
  
    worldSurfaceNormal = normalize( mul( surfaceNormal, toWorldSpace ) );
  }

  return worldSurfaceNormal;
}
