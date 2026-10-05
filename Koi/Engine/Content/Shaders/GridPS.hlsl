cbuffer PushConstants : register(b0)
{
	uint GridConstantsIndex;
};

struct GridConstants
{
	row_major float4x4 ViewProjection;
	row_major float4x4 InvViewProjection;
	float4             CameraPosition;
	float4             ThinColor;
	float4             ThickColor;
	float4             AxisXColor;
	float4             AxisZColor;
	float              CellSize;
	float              ThickEvery;
	float              FadeStart;
	float              FadeEnd;
	float              MajorLineWidth;
	float3             _Pad;
};

struct PSOutput
{
	float4 Color : SV_Target;
	float  Depth : SV_Depth;
};

// Coverage of the nearest line of `spacing` at `p`, antialiased over `widthPx` pixels.
// Dividing the distance-to-line by the screen-space derivative keeps the line a fixed
// pixel width at any distance, which is what stops the grid aliasing into moire.
float LineCoverage(float2 p, float2 dpdx, float spacing, float widthPx)
{
	float2 scaled = p / spacing;
	float2 width  = max(abs(dpdx) / spacing, 1e-8);

	// Distance to the closest grid line, in cells, then normalised to pixels
	float2 distance = abs(frac(scaled - 0.5) - 0.5) / width;

	return 1.0 - saturate(min(distance.x, distance.y) / max(widthPx, 1e-4));
}

PSOutput PSMain(float4 pos : SV_Position, float2 ndc : TEXCOORD0)
{
	ConstantBuffer<GridConstants> c = ResourceDescriptorHeap[GridConstantsIndex];

	// Unproject the far plane to get a ray through this pixel
	float4 farPoint = mul(float4(ndc, 1.0, 1.0), c.InvViewProjection);
	float3 rayDir   = normalize(farPoint.xyz / farPoint.w - c.CameraPosition.xyz);

	PSOutput output;

	// Intersect the y = 0 plane. Reject rays parallel to it or pointing away from it
	if (abs(rayDir.y) < 1e-6)
	{
		discard;
		output.Color = 0;
		output.Depth = 1;
		return output;
	}

	float t = -c.CameraPosition.y / rayDir.y;
	if (t <= 0.0)
	{
		discard;
		output.Color = 0;
		output.Depth = 1;
		return output;
	}

	float3 hit = c.CameraPosition.xyz + rayDir * t;

	// Screen-space footprint of the hit point, for the antialiasing width
	float2 footprint = float2(
		length(float2(ddx(hit.x), ddy(hit.x))),
		length(float2(ddx(hit.z), ddy(hit.z))));

	float thin  = LineCoverage(hit.xz, footprint, c.CellSize, 1.0);
	float thick = LineCoverage(hit.xz, footprint, c.CellSize * c.ThickEvery, c.MajorLineWidth);

	// Axis lines are the two major lines through the origin
	float axisX = 1.0 - saturate(abs(hit.z) / max(footprint.y * c.MajorLineWidth, 1e-8));
	float axisZ = 1.0 - saturate(abs(hit.x) / max(footprint.x * c.MajorLineWidth, 1e-8));

	// Thin under thick under axes. Coverage alone is the blend weight; each colour's own
	// alpha is what it reaches at full coverage, so the authored contrast survives
	float4 color = float4(c.ThinColor.rgb, c.ThinColor.a * thin);
	color = lerp(color, float4(c.ThickColor.rgb, c.ThickColor.a), thick);
	color = lerp(color, float4(c.AxisXColor.rgb, c.AxisXColor.a), axisX);
	color = lerp(color, float4(c.AxisZColor.rgb, c.AxisZColor.a), axisZ);

	// Fade with distance from the camera so the horizon dissolves instead of aliasing
	float fade = 1.0 - saturate((distance(hit, c.CameraPosition.xyz) - c.FadeStart) / max(c.FadeEnd - c.FadeStart, 1e-6));
	color.a *= fade;

	if (color.a <= 0.001)
	{
		discard;
		output.Color = 0;
		output.Depth = 1;
		return output;
	}

	// Real depth at the hit, so scene geometry occludes the grid through the normal depth test
	float4 clip = mul(float4(hit, 1.0), c.ViewProjection);

	output.Color = color;
	output.Depth = clip.z / clip.w;
	return output;
}
