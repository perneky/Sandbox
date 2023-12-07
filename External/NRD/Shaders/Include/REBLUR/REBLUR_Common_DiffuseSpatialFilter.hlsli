/*
Copyright (c) 2022, NVIDIA CORPORATION. All rights reserved.

NVIDIA CORPORATION and its licensors retain all intellectual property
and proprietary rights in and to this software, related documentation
and any modifications thereto. Any use, reproduction, disclosure or
distribution of this software and related documentation without an express
license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#ifndef REBLUR_SPATIAL_MODE
    #error REBLUR_SPATIAL_MODE must be defined!
#endif

#if( REBLUR_SPATIAL_MODE == REBLUR_PRE_BLUR )
    #define POISSON_SAMPLE_NUM      REBLUR_PRE_BLUR_POISSON_SAMPLE_NUM
    #define POISSON_SAMPLES( i )    REBLUR_PRE_BLUR_POISSON_SAMPLES( i )
#else
    #define POISSON_SAMPLE_NUM      REBLUR_POISSON_SAMPLE_NUM
    #define POISSON_SAMPLES( i )    REBLUR_POISSON_SAMPLES( i )
#endif

{
#if( REBLUR_SPATIAL_MODE == REBLUR_PRE_BLUR )
    float sum = hasData;

    if( gDiffPrepassBlurRadius != 0.0 )
    {
        float diffNonLinearAccumSpeed = REBLUR_PRE_BLUR_NON_LINEAR_ACCUM_SPEED;
#else
        float sum = 1.0;
#endif

        float fractionScale = 1.0;
    #if( REBLUR_SPATIAL_MODE == REBLUR_PRE_BLUR )
        fractionScale = REBLUR_PRE_BLUR_FRACTION_SCALE;
    #elif( REBLUR_SPATIAL_MODE == REBLUR_BLUR )
        float radiusScale = 1.0;
        fractionScale = REBLUR_BLUR_FRACTION_SCALE;
    #elif( REBLUR_SPATIAL_MODE == REBLUR_POST_BLUR )
        float radiusScale = REBLUR_POST_BLUR_RADIUS_SCALE;
        fractionScale = REBLUR_POST_BLUR_FRACTION_SCALE;
    #endif

        float lobeAngleFractionScale = saturate( gLobeAngleFraction * fractionScale );

        float hitDistScale = _REBLUR_GetHitDistanceNormalization( viewZ, gHitDistParams, 1.0 );
        float hitDist = ExtractHitDist( diff ) * hitDistScale;

        // Hit distance factor ( tests 76, 95, 120 )
        // TODO: if luminance stoppers are used, blur radius should depend less on "hitDistFactor"
        float frustumSize = GetFrustumSize( gMinRectDimMulUnproject, gOrthoMode, viewZ );
        float hitDistFactor = GetHitDistFactor( hitDist, frustumSize );

        // Blur radius
    #if( REBLUR_SPATIAL_MODE == REBLUR_PRE_BLUR )
        // Blur radius - main
        float blurRadius = gDiffPrepassBlurRadius;
        blurRadius *= hitDistFactor;
    #else
        // Test 53
        hitDistFactor = lerp( hitDistFactor, 1.0, data1.y );

        // IMPORTANT: keep an eye on tests:
        // - 51 and 128: outlines without TAA
        // - 81 and 117: cleanness in disoccluded regions
        float boost = 1.0 - GetFadeBasedOnAccumulatedFrames( data1.x );
        boost *= 1.0 - STL::BRDF::Pow5( NoV );

        float diffNonLinearAccumSpeed = 1.0 / ( 1.0 + REBLUR_SAMPLES_PER_FRAME * ( 1.0 - boost ) * data1.x );

        // Blur radius - main
        float blurRadius = gBlurRadius * ( 1.0 + 2.0 * boost ) / 3.0;
        blurRadius *= hitDistFactor;

        // Blur radius - addition to avoid underblurring
        blurRadius += 1.0;

        // Blur radius - scaling
        blurRadius *= radiusScale;
        blurRadius *= float( gBlurRadius != 0 );
    #endif

        // Weights
        float2 geometryWeightParams = GetGeometryWeightParams( gPlaneDistSensitivity, frustumSize, Xv, Nv, diffNonLinearAccumSpeed );
        float normalWeightParams = GetNormalWeightParams( diffNonLinearAccumSpeed, lobeAngleFractionScale );
        float2 px = float2( geometryWeightParams.x, normalWeightParams );
        float2 py = float2( geometryWeightParams.y, 0.0 );

        float2 hitDistanceWeightParams = GetHitDistanceWeightParams( ExtractHitDist( diff ), diffNonLinearAccumSpeed );
        float minHitDistWeight = REBLUR_HIT_DIST_MIN_WEIGHT( 1.0 ) * fractionScale;

        // Sampling
        diff *= sum;
    #ifdef REBLUR_SH
        diffSh *= sum;
    #endif

        float2x3 TvBv = GetKernelBasis( Nv, Nv, 1.0 ); // D = N

        float worldRadius = PixelRadiusToWorld( gUnproject, gOrthoMode, blurRadius, viewZ );
        TvBv[ 0 ] *= worldRadius;
        TvBv[ 1 ] *= worldRadius;

        [unroll]
        for( uint n = 0; n < POISSON_SAMPLE_NUM; n++ )
        {
            float3 offset = POISSON_SAMPLES( n );

            // Sample coordinates
        #if( REBLUR_USE_SCREEN_SPACE_SAMPLING == 1 || REBLUR_SPATIAL_MODE == REBLUR_PRE_BLUR )
            float2 uv = pixelUv + STL::Geometry::RotateVector( rotator, offset.xy ) * gInvScreenSize * blurRadius;
        #else
            float2 uv = GetKernelSampleCoordinates( gViewToClip, offset, Xv, TvBv[ 0 ], TvBv[ 1 ], rotator );
        #endif

        #if( REBLUR_SPATIAL_MODE == REBLUR_PRE_BLUR )
            if( gDiffCheckerboard != 2 )
                uv = ApplyCheckerboardShift( uv, gDiffCheckerboard, n, gRectSize, gInvRectSize, gFrameIndex );
        #endif

            float2 uvScaled = uv * gResolutionScale;
            float2 uvScaledWithOffset = gRectOffset + uvScaled;
            float2 checkerboardUvScaled = uvScaled;

        #if( REBLUR_SPATIAL_MODE == REBLUR_PRE_BLUR )
            checkerboardUvScaled = gRectOffset + float2( uvScaled.x * ( gDiffCheckerboard != 2 ? 0.5 : 1.0 ), uvScaled.y );
        #endif

        #if( REBLUR_USE_LOADS == 1 )
            uint2 uvScaledi = clamp( uvScaled * gScreenSize, 0.0, gScreenSize - 1.0 );
            uint2 uvScaledWithOffseti = clamp( uvScaledWithOffset * gScreenSize, 0.0, gScreenSize - 1.0 );
            uint2 checkerboardUvScaledi = clamp( checkerboardUvScaled * gScreenSize, 0.0, gScreenSize - 1.0 );
        #endif

            // Fetch data
        #if( REBLUR_SPATIAL_MODE == REBLUR_POST_BLUR )
            float zs = UnpackViewZ( REBLUR_SAMPLE_TEXTURE( gIn_ViewZ, uvScaled ) );
        #else
            float zs = abs( REBLUR_SAMPLE_TEXTURE( gIn_ViewZ, uvScaledWithOffset ) );
        #endif

            REBLUR_TYPE s = REBLUR_SAMPLE_TEXTURE( gIn_Diff, checkerboardUvScaled );

            float3 Xvs = STL::Geometry::ReconstructViewPosition( uv, gFrustum, zs, gOrthoMode );

            float materialIDs;
            float4 Ns = REBLUR_SAMPLE_TEXTURE( gIn_Normal_Roughness, uvScaledWithOffset );
            Ns = NRD_FrontEnd_UnpackNormalAndRoughness( Ns, materialIDs );

            // Sample weights
            float w = CompareMaterials( materialID, materialIDs, gDiffMaterialMask );

            float2 x;
            x.x = dot( Nv, Xvs );
            x.y = STL::Math::AcosApprox( dot( N, Ns.xyz ) );
            x = ComputeWeight( x, px, py );
            w *= x.x * x.y;

            w *= GetGaussianWeight( offset.z );
            w *= lerp( minHitDistWeight, 1.0, ComputeExponentialWeight( ExtractHitDist( s ), hitDistanceWeightParams.x, hitDistanceWeightParams.y ) );

            // Get rid of potential NANs outside of rendering rectangle or denoising range
            w = ( IsInScreen( uv ) && !isnan( w ) ) ? w : 0.0;
            s = w != 0.0 ? s : 0.0;

            // Accumulate
            sum += w;
            diff += s * w;
        #ifdef REBLUR_SH
            float4 sh = REBLUR_SAMPLE_TEXTURE( gIn_DiffSh, checkerboardUvScaled );
            diffSh += sh * w;
        #endif
        }

        float invSum = STL::Math::PositiveRcp( sum );
        diff *= invSum;
    #ifdef REBLUR_SH
        diffSh *= invSum;
    #endif

#if( REBLUR_SPATIAL_MODE == REBLUR_PRE_BLUR )
    }

    // Checkerboard resolve ( if pre-pass failed )
    [branch]
    if( !hasData && sum == 0.0 )
    {
        REBLUR_TYPE s0 = gIn_Diff[ checkerboardPos.xy ];
        REBLUR_TYPE s1 = gIn_Diff[ checkerboardPos.zy ];

        // Get rid of potential NANs outside of rendering rectangle or denoising range
        s0 = wc.x != 0.0 ? s0 : 0;
        s1 = wc.y != 0.0 ? s1 : 0;

        diff = s0 * wc.x + s1 * wc.y;

    #ifdef REBLUR_SH
        float4 sh0 = gIn_DiffSh[ checkerboardPos.xy ];
        float4 sh1 = gIn_DiffSh[ checkerboardPos.zy ];

        // Get rid of potential NANs outside of rendering rectangle or denoising range
        sh0 = wc.x != 0.0 ? sh0 : 0;
        sh1 = wc.y != 0.0 ? sh1 : 0;

        diffSh = sh0 * wc.x + sh1 * wc.y;
    #endif
    }
#endif

    // Output
    gOut_Diff[ pixelPos ] = diff;
    #ifdef REBLUR_SH
        gOut_DiffSh[ pixelPos ] = diffSh;
    #endif
}

#undef POISSON_SAMPLE_NUM
#undef POISSON_SAMPLES
