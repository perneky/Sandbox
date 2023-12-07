#define PS
#include "RootSignatures/DSTransferBufferDebug.hlsli"

[ RootSignature( _RootSignature ) ]
float4 main( VertexOutput input ) : SV_Target0
{
  if ( any( input.screenPosition.x > 2000 ) )
    discard;
  
  uint index = input.screenPosition.y * 2000 + input.screenPosition.x;
  uint data  = transferBuffer.Load( index );

  return ( data % 256 ) / 256;
}
