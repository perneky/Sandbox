#pragma once

static const half3 Fdielectric = 0.04;
static const half  PBREpsilon  = 0.00001;

// GGX/Towbridge-Reitz normal distribution function.
// Uses Disney's reparametrization of alpha = roughness^2.
half ndfGGX( half cosLh, half roughness )
{
  half alpha = roughness * roughness;
  half alphaSq = alpha * alpha;

  half denom = ( cosLh * cosLh ) * ( alphaSq - 1.0 ) + 1.0;
  return alphaSq / ( PI * denom * denom );
}

// Single term for separable Schlick-GGX below.
half gaSchlickG1( half cosTheta, half k )
{
	return cosTheta / ( cosTheta * ( 1.0 - k ) + k );
}

// Schlick-GGX approximation of geometric attenuation function using Smith's method.
half gaSchlickGGX( half cosLi, half cosLo, half roughness )
{
	half r = roughness + 1.0;
	half k = ( r * r ) / 8.0; // Epic suggests using this roughness remapping for analytic lights.
	return gaSchlickG1( cosLi, k ) * gaSchlickG1( cosLo, k );
}

// Shlick's approximation of the Fresnel factor.
half3 fresnelSchlick( half3 F0, half cosTheta )
{
	return F0 + ( 1.0 - F0 ) * pow( 1.0 - cosTheta, 5.0 );
}
