#define PS
#include "RootSignatures/ScreenQuad.hlsli"

[ RootSignature( _RootSignature ) ]
float4 main( VertexOutput input ) : SV_Target0
{
  bool   isMapped = false;
  float4 texel    = 0;

  if ( sceneTextureId < 0xFFFF )
  {
    #if ENABLE_TEXTURE_STREAMING
      uint2 texSize;
      float noLevels;
      scene2DTextures[ sceneTextureId ].GetDimensions( mipLevel, texSize.x, texSize.y, noLevels );

      if ( mipLevel >= noLevels - 1 )
      {
        uint mipOffset = texture.Load( uint3( 0, 0, noLevels - 1 ) ).x;
        texel = scene2DMipTailTextures[ sceneTextureId ].SampleLevel( textureSampler, input.texcoord, mipLevel - mipOffset );
        isMapped = true;
      }
      else
      {
        // Check if the index texture has this
        int2 tci = input.texcoord * texSize;
        tci %= texSize;
        uint2 tileInfo = scene2DTextures[ sceneTextureId ].Load( uint3( tci, mipLevel ) );
        if ( tileInfo.y < 0xFFFE )
        {
          uint  tileTextureIndex = tileInfo.y;
          uint2 tileCoord        = uint2( tileInfo.x & 255, tileInfo.x >> 8 );
  
          float2 pc = fmod( input.texcoord * texSize * TileSizeF, TileSizeF );
          pc /= TileTextureSizeF;
          pc += tileCoord * ( TileSizeWithBorderF / TileTextureSizeF );
  
          texel = engine2DTileTextures[ tileTextureIndex ].SampleLevel( textureSampler, pc, 0 );
          isMapped = true;
        } 
      }
    #else
      return scene2DTextures[ sceneTextureId ].SampleLevel( textureSampler, input.texcoord, mipLevel );
    #endif
  }
  else
  {
    texel = texture.SampleLevel( textureSampler, input.texcoord, mipLevel );
    isMapped = true;
  }
  
  if ( !isMapped )
  {
    uint2 pos  = uint2( input.screenPosition.xy );
    uint2 rate = pos % 4;
    bool  red  = all( rate < 2 ) || all( rate > 1 );
    texel = red ? float4( 1, 0, 0, 1 ) : float4( 1, 1, 1, 1 );
  }
  
  if ( texel.a == 0 )
    discard;
  
  return texel;
}
