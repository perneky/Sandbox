#include "Lighting.hlsli"

float3 CalcReflection( float3 worldPosition, float3 worldNormal, FrameParamsCB frameParams )
{
  float3 toCamera     = normalize( frameParams.cameraPosition.xyz - worldPosition );
  float3 toReflection = reflect( -toCamera, worldNormal );

  HitGeometry hitGeom = TraceRay( worldPosition, toReflection, castMinDistance, 1000 );

  [branch]
  if ( hitGeom.t < 0 )
    return engineCubeTextures[ SkyTextureSlot ].SampleLevel( wrapSampler, toReflection, 0 ).rgb;

  MaterialSlotCB material = materials[ hitGeom.materialIndex ];

  float3 surfaceWorldNormal = hitGeom.worldNormal;
  float3 surfaceAlbedo      = material.albedo;
  float3 surfaceEmissive    = 0;
  float  surfaceRoughness   = material.roughness;
  float  surfaceMetallic    = material.metallic;

  float3 probeGI         = 0.5;
  float3 effectiveAlbedo = surfaceAlbedo;
  float3 directLighting  = TraceDirectLighting( effectiveAlbedo, surfaceRoughness, surfaceMetallic, hitGeom.worldPosition, surfaceWorldNormal, 1, frameParams );

  float3 diffuseIBL;
  float3 specularIBL;
  TraceIndirectLighting( probeGI, engine2DTextures[ SpecBRDFLUTSlot ], effectiveAlbedo, surfaceRoughness, surfaceMetallic, hitGeom.worldPosition, surfaceWorldNormal, frameParams, diffuseIBL, specularIBL );
  return surfaceEmissive + directLighting + diffuseIBL;
}
