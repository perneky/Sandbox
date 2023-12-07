#include "RootSignatures/ShaderStructures.hlsli"
#include "Utils.hlsli"

struct Attributes
{
  half4  randomValues;
  float3 worldPosition;
  half3  worldTangent;
  half3  worldBitangent;
  half3  worldNormal;
  float2 texcoord;
  half   textureMip;
  uint   materialIndex;
  float  rawDepth;
  float  viewZ;
};

bool GetAttributes( uint2 location, float2 resolution, out Attributes attribs )
{
  attribs = (Attributes)0;

  float2 tc  = location + 0.5;
  uint2  tci = location;
  
  attribs.rawDepth = depthTexture[ tci ];
  [branch]
  if ( attribs.rawDepth == 0 )
    return false;

  float4 clipDepth = float4( 0, 0, attribs.rawDepth, 1 );
  float4 viewDepth = mul( frameParams.invProjTransform, clipDepth );
  attribs.viewZ = viewDepth.z / viewDepth.w;
  
  float4 clipPos        = float4( ( tc / resolution ) * float2( 2, -2 ) + float2( -1, 1 ), attribs.rawDepth, 1 );
  float4 worldPosition4 = mul( frameParams.invVpTransform, clipPos );
  worldPosition4.xyz /= worldPosition4.w;
  
  float3 worldPosition = worldPosition4.xyz;
  
  half  textureMip  = textureMipTexture[ tci ];
  uint2 geometryIds = geometryIdsTexture[ tci ];
  
  bool isFrontFace = geometryIds.x >> 15;
  uint indirectId  = ( geometryIds.x >> 13 ) & 3;
  uint instanceId  = geometryIds.x & 0x1FFF;
  uint triangleId  = geometryIds.y;

  IndirectRender indirectData = indirectBufers[ indirectId ][ instanceId ];

  uint ibIndex            = indirectData.ibIndex;
  uint vbIndex            = indirectData.vbIndex;
  uint materialIndex      = indirectData.materialIndex;

  uint         index0, index1, index2;
  VertexFormat vertex0, vertex1, vertex2;
  
  index0 = meshIndices[ ibIndex ].Load( triangleId * 3 + 0 );
  index1 = meshIndices[ ibIndex ].Load( triangleId * 3 + 1 );
  index2 = meshIndices[ ibIndex ].Load( triangleId * 3 + 2 );

  bool windingFlipped = materials[ materialIndex ].flags & MaterialSlot::FlipWinding;
  if ( windingFlipped )
  {
    uint tmp = index0;
    index0 = index1;
    index1 = tmp;
  }

  vertex0 = meshVertices[ vbIndex ].Load( index0 );
  vertex1 = meshVertices[ vbIndex ].Load( index1 );
  vertex2 = meshVertices[ vbIndex ].Load( index2 );

  float3 worldPosition0, worldPosition1, worldPosition2;

  worldPosition0 = mul( indirectData.worldTransform, vertex0.position ).xyz;
  worldPosition1 = mul( indirectData.worldTransform, vertex1.position ).xyz;
  worldPosition2 = mul( indirectData.worldTransform, vertex2.position ).xyz;

  float3 barycentricsF = CalcBaryCentrics( worldPosition, worldPosition0, worldPosition1, worldPosition2 );
  half3  barycentrics  = half3( barycentricsF );
  
  float2 texcoord           = vertex0.texcoord      * barycentricsF.x + vertex1.texcoord      * barycentricsF.y + vertex2.texcoord      * barycentricsF.z;
  half3  localPosition      = vertex0.position.xyz  * barycentrics.x  + vertex1.position.xyz  * barycentrics.y  + vertex2.position.xyz  * barycentrics.z;
  half3  tangent            = vertex0.tangent.xyz   * barycentrics.x  + vertex1.tangent.xyz   * barycentrics.y  + vertex2.tangent.xyz   * barycentrics.z;
  half3  bitangent          = vertex0.bitangent.xyz * barycentrics.x  + vertex1.bitangent.xyz * barycentrics.y  + vertex2.bitangent.xyz * barycentrics.z;
  half3  normal             = vertex0.normal.xyz    * barycentrics.x  + vertex1.normal.xyz    * barycentrics.y  + vertex2.normal.xyz    * barycentrics.z;
  half3  worldTangent       = mul( (half3x3)indirectData.worldTransform, tangent   );
  half3  worldBitangent     = mul( (half3x3)indirectData.worldTransform, bitangent );
  half3  worldNormal        = mul( (half3x3)indirectData.worldTransform, normal    ) * ( isFrontFace ? 1 : -1 );
  
  attribs.randomValues   = indirectData.randomValues * half4( localPosition, 1 );
  attribs.worldPosition  = worldPosition;
  attribs.worldTangent   = worldTangent;
  attribs.worldBitangent = worldBitangent;
  attribs.worldNormal    = worldNormal;
  attribs.materialIndex  = materialIndex;
  attribs.textureMip     = textureMip;
  attribs.texcoord       = texcoord;

  return true;
}
