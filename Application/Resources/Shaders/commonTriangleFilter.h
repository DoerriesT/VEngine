#define CULL_DEGENERATES_BIT (1 << 0)
#define CULL_BACKFACE_BIT (1 << 1)
#define CULL_SMALL_TRIANGLES_BIT (1 << 2)
#define CULL_FRUSTUM_BIT (1 << 3)

bool cullTriangle(in const uint indices[3], in vec4 vertices[3], in vec2 resolution, bool cullBackface)
{
	bool cull = false;
	
	// cull degenerate triangles
	{
		if (indices[0] == indices[1] 
		|| indices[1] == indices[2] 
		|| indices[2] == indices[0])
		{
			cull = true;
		}
	}
	
	// Culling in homogenous coordinates
    // Read: "Triangle Scan Conversion using 2D Homogeneous Coordinates"
    //       by Marc Olano, Trey Greer
	//       http://www.cs.unc.edu/~olano/papers/2dh-tri/2dh-tri.pdf
	mat3 m = transpose(mat3(vertices[0].xyw, vertices[1].xyw, vertices[2].xyw));
	
	// cull backface
	if (cullBackface)
	{
		cull = cull || (determinant(m) > 0.0);
	}
	
	uint verticesInFrontOfNearPlane = 0;
	
	// transform vertices.xy into normalized 0..1 screen space
	for (uint i = 0; i < 3; ++i)
	{
		vertices[i].xy /= vertices[i].w;
		vertices[i].xy *= 0.5;
		vertices[i].xy += 0.5;
		if (vertices[i].w < 0)
		{
			++verticesInFrontOfNearPlane;
		}
	}
	
	// cull small triangles
	{
		const uint SUBPIXEL_BITS = 8;
		const uint SUBPIXEL_MASK = 0xFF;
		const uint SUBPIXEL_SAMPLES = 1 << SUBPIXEL_BITS;
		
		/*
        Computing this in float-point is not precise enough
        We switch to a 23.8 representation here which should match the
        HW subpixel resolution.
        We use a 8-bit wide guard-band to avoid clipping. If
        a triangle is outside the guard-band, it will be ignored.
        That is, the actual viewport supported here is 31 bit, one bit is
        unused, and the guard band is 1 << 23 bit large (8388608 pixels)
        */
		
		ivec2 minBB = ivec2(1 << 30);
		ivec2 maxBB = ivec2(-(1 << 30));
		
		bool insideGuardBand = true;
		
		for (uint i = 0; i < 3; ++i)
		{
			vec2 screenSpacePositionFP = vertices[i].xy * resolution;
			
			// check if we would overflow after conversion
			if (screenSpacePositionFP.x < -(1 << 23)
				|| screenSpacePositionFP.x > (1 << 23)
				|| screenSpacePositionFP.y < -(1 << 23)
				|| screenSpacePositionFP.y > (1 << 23))
			{
				insideGuardBand = false;
			}
			
			ivec2 screenSpacePosition = ivec2(screenSpacePositionFP * SUBPIXEL_SAMPLES);
			minBB = min(screenSpacePosition, minBB);
			maxBB = max(screenSpacePosition, maxBB);
		}
		
		if (verticesInFrontOfNearPlane == 0 && insideGuardBand)
		{
			/*
            Test is:
            Is the minimum of the bounding box right or above the sample
            point and is the width less than the pixel width in samples in
            one direction.
            This will also cull very long triangles which fall between
            multiple samples.
            */
			
			cull = cull
			|| (
					((minBB.x & SUBPIXEL_MASK) > SUBPIXEL_SAMPLES / 2)
				&&	((maxBB.x - ((minBB.x & ~SUBPIXEL_MASK) + SUBPIXEL_SAMPLES / 2)) < (SUBPIXEL_SAMPLES - 1)))
			|| (
					((minBB.y & SUBPIXEL_MASK) > SUBPIXEL_SAMPLES / 2)
				&&	((maxBB.y - ((minBB.y & ~SUBPIXEL_MASK) + SUBPIXEL_SAMPLES / 2)) < (SUBPIXEL_SAMPLES - 1)));
		}
	}
	
	// cull frustum
	{
		if (verticesInFrontOfNearPlane == 3)
		{
			cull = true;
		}
		
		if (verticesInFrontOfNearPlane == 0)
		{
			float minX = min(min(vertices[0].x, vertices[1].x), vertices[2].x);
			float minY = min(min(vertices[0].y, vertices[1].y), vertices[2].y);
			float maxX = max(max(vertices[0].x, vertices[1].x), vertices[2].x);
			float maxY = max(max(vertices[0].y, vertices[1].y), vertices[2].y);
		
			cull = cull || (maxX < 0.0) || (maxY < 0.0) || (minX > 1.0) || (minY > 1.0);
		}
	}
	
	return cull;
}