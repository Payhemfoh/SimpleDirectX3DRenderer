struct VSInput
{
	float3 Position : POSITION;
	float2 Uv : TEXCOORD0;
};

struct VSOutput
{
	float4 Position : SV_Position;
	float2 Uv : TEXCOORD0;
};

cbuffer PerFrame : register(b0)
{
	matrix viewprojection;
};

cbuffer PerObject : register(b1)
{
	matrix modelmatrix;
};

VSOutput Main(VSInput input)
{
	matrix world = mul(viewprojection, modelmatrix);

	VSOutput output = (VSOutput)0;
	output.Position = mul(world, float4(input.Position, 1.0));
	output.Uv = float2(input.Uv.x, 1.0f - input.Uv.y);
	return output;
}