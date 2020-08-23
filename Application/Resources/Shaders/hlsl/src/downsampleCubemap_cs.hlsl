// Copyright 2016 Activision Publishing, Inc.
// 
// Permission is hereby granted, free of charge, to any person obtaining 
// a copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation 
// the rights to use, copy, modify, merge, publish, distribute, sublicense, 
// and/or sell copies of the Software, and to permit persons to whom the Software 
// is furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all 
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
// SOFTWARE.

#include "bindingHelper.hlsli"
#include "common.hlsli"

TextureCube tex_hi_res : register( t0 );
RWTexture2DArray<float4> tex_lo_res : register( u1 );

SamplerState g_Samplers[SAMPLER_COUNT] : REGISTER_SAMPLER(0, 1);

void get_dir_0( out float3 dir, in float u, in float v )
{
	dir[0] = 1;
	dir[1] = v;
	dir[2] = -u;
}
void get_dir_1( out float3 dir, in float u, in float v )
{
	dir[0] = -1;
	dir[1] = v;
	dir[2] = u;
}
void get_dir_2( out float3 dir, in float u, in float v )
{
	dir[0] = u;
	dir[1] = 1;
	dir[2] = -v;
}
void get_dir_3( out float3 dir, in float u, in float v )
{
	dir[0] = u;
	dir[1] = -1;
	dir[2] = v;
}
void get_dir_4( out float3 dir, in float u, in float v )
{
	dir[0] = u;
	dir[1] = v;
	dir[2] = 1;
}
void get_dir_5( out float3 dir, in float u, in float v )
{
	dir[0] = -u;
	dir[1] = v;
	dir[2] = -1;
}

void get_dir( out float3 dir, in float u, in float v, in int face )
{
	switch ( face )
	{
	case 0:
		get_dir_0( dir, u, v );
		break;
	case 1:
		get_dir_1( dir, u, v );
		break;
	case 2:
		get_dir_2( dir, u, v );
		break;
	case 3:
		get_dir_3( dir, u, v );
		break;
	case 4:
		get_dir_4( dir, u, v );
		break;
	default:
		get_dir_5( dir, u, v );
		break;
	}
}

float calcWeight( float u, float v )
{
	float val = u*u + v*v + 1;
	return val*sqrt( val );
}

[numthreads( 8, 8, 1 )]
void main( uint3 id : SV_DispatchThreadID )
{
	uint res_lo;
	{
		uint h, e;
		tex_lo_res.GetDimensions( res_lo, h, e );
	}


	if ( id.x < res_lo && id.y < res_lo )
	{
		float inv_res_lo = rcp( (float)res_lo );

		float u0 = ( (float)id.x * 2.0f + 1.0f - .75f ) * inv_res_lo - 1.0f;
		float u1 = ( (float)id.x * 2.0f + 1.0f + .75f ) * inv_res_lo - 1.0f;

		float v0 = ( (float)id.y * 2.0f + 1.0f - .75f ) * -inv_res_lo + 1.0f;
		float v1 = ( (float)id.y * 2.0f + 1.0f + .75f ) * -inv_res_lo + 1.0f;

		float weights[4];
		weights[0] = calcWeight( u0, v0 );
		weights[1] = calcWeight( u1, v0 );
		weights[2] = calcWeight( u0, v1 );
		weights[3] = calcWeight( u1, v1 );

		const float wsum = 0.5f / ( weights[0] + weights[1] + weights[2] + weights[3] );
		[unroll]
		for ( int i = 0; i < 4; i++ )
			weights[i] = weights[i] * wsum + .125f;

#if 1
		float3 dir;
		float4 color;
		switch ( id.z )
		{
		case 0:
			get_dir_0( dir, u0, v0 );
			color = tex_hi_res.SampleLevel( g_Samplers[SAMPLER_LINEAR_CLAMP], dir, 0 ) * weights[0];

			get_dir_0( dir, u1, v0 );
			color += tex_hi_res.SampleLevel( g_Samplers[SAMPLER_LINEAR_CLAMP], dir, 0 ) * weights[1];

			get_dir_0( dir, u0, v1 );
			color += tex_hi_res.SampleLevel( g_Samplers[SAMPLER_LINEAR_CLAMP], dir, 0 ) * weights[2];

			get_dir_0( dir, u1, v1 );
			color += tex_hi_res.SampleLevel( g_Samplers[SAMPLER_LINEAR_CLAMP], dir, 0 ) * weights[3];
			break;
		case 1:
			get_dir_1( dir, u0, v0 );
			color = tex_hi_res.SampleLevel( g_Samplers[SAMPLER_LINEAR_CLAMP], dir, 0 ) * weights[0];

			get_dir_1( dir, u1, v0 );
			color += tex_hi_res.SampleLevel( g_Samplers[SAMPLER_LINEAR_CLAMP], dir, 0 ) * weights[1];

			get_dir_1( dir, u0, v1 );
			color += tex_hi_res.SampleLevel( g_Samplers[SAMPLER_LINEAR_CLAMP], dir, 0 ) * weights[2];

			get_dir_1( dir, u1, v1 );
			color += tex_hi_res.SampleLevel( g_Samplers[SAMPLER_LINEAR_CLAMP], dir, 0 ) * weights[3];
			break;
		case 2:
			get_dir_2( dir, u0, v0 );
			color = tex_hi_res.SampleLevel( g_Samplers[SAMPLER_LINEAR_CLAMP], dir, 0 ) * weights[0];

			get_dir_2( dir, u1, v0 );
			color += tex_hi_res.SampleLevel( g_Samplers[SAMPLER_LINEAR_CLAMP], dir, 0 ) * weights[1];

			get_dir_2( dir, u0, v1 );
			color += tex_hi_res.SampleLevel( g_Samplers[SAMPLER_LINEAR_CLAMP], dir, 0 ) * weights[2];

			get_dir_2( dir, u1, v1 );
			color += tex_hi_res.SampleLevel( g_Samplers[SAMPLER_LINEAR_CLAMP], dir, 0 ) * weights[3];
			break;
		case 3:
			get_dir_3( dir, u0, v0 );
			color = tex_hi_res.SampleLevel( g_Samplers[SAMPLER_LINEAR_CLAMP], dir, 0 ) * weights[0];

			get_dir_3( dir, u1, v0 );
			color += tex_hi_res.SampleLevel( g_Samplers[SAMPLER_LINEAR_CLAMP], dir, 0 ) * weights[1];

			get_dir_3( dir, u0, v1 );
			color += tex_hi_res.SampleLevel( g_Samplers[SAMPLER_LINEAR_CLAMP], dir, 0 ) * weights[2];

			get_dir_3( dir, u1, v1 );
			color += tex_hi_res.SampleLevel( g_Samplers[SAMPLER_LINEAR_CLAMP], dir, 0 ) * weights[3];
			break;
		case 4:
			get_dir_4( dir, u0, v0 );
			color = tex_hi_res.SampleLevel( g_Samplers[SAMPLER_LINEAR_CLAMP], dir, 0 ) * weights[0];

			get_dir_4( dir, u1, v0 );
			color += tex_hi_res.SampleLevel( g_Samplers[SAMPLER_LINEAR_CLAMP], dir, 0 ) * weights[1];

			get_dir_4( dir, u0, v1 );
			color += tex_hi_res.SampleLevel( g_Samplers[SAMPLER_LINEAR_CLAMP], dir, 0 ) * weights[2];

			get_dir_4( dir, u1, v1 );
			color += tex_hi_res.SampleLevel( g_Samplers[SAMPLER_LINEAR_CLAMP], dir, 0 ) * weights[3];
			break;
		default:
			get_dir_5( dir, u0, v0 );
			color = tex_hi_res.SampleLevel( g_Samplers[SAMPLER_LINEAR_CLAMP], dir, 0 ) * weights[0];

			get_dir_5( dir, u1, v0 );
			color += tex_hi_res.SampleLevel( g_Samplers[SAMPLER_LINEAR_CLAMP], dir, 0 ) * weights[1];

			get_dir_5( dir, u0, v1 );
			color += tex_hi_res.SampleLevel( g_Samplers[SAMPLER_LINEAR_CLAMP], dir, 0 ) * weights[2];

			get_dir_5( dir, u1, v1 );
			color += tex_hi_res.SampleLevel( g_Samplers[SAMPLER_LINEAR_CLAMP], dir, 0 ) * weights[3];
			break;
		}
#else
		float3 dir;
		get_dir( dir, u0, v0, id.z );
		float4 color = tex_hi_res.SampleLevel( g_Samplers[SAMPLER_LINEAR_CLAMP], dir, 0 ) * weights[0];

		get_dir( dir, u1, v0, id.z );
		color += tex_hi_res.SampleLevel( g_Samplers[SAMPLER_LINEAR_CLAMP], dir, 0 ) * weights[1];

		get_dir( dir, u0, v1, id.z );
		color += tex_hi_res.SampleLevel( g_Samplers[SAMPLER_LINEAR_CLAMP], dir, 0 ) * weights[2];

		get_dir( dir, u1, v1, id.z );
		color += tex_hi_res.SampleLevel( g_Samplers[SAMPLER_LINEAR_CLAMP], dir, 0 ) * weights[3];
#endif

		tex_lo_res[id] = color;
	}
}
