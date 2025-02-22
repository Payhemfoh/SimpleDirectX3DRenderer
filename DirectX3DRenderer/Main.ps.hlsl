Texture2D rgbTexture : register(t0);
SamplerState samLinear : register(s0);

struct VSOutput
{
    float4 Position : SV_Position;
    float2 Uv : TEXCOORD0;
};

float4 Main(VSOutput input) : SV_Target
{
    float4 color = rgbTexture.Sample(samLinear, input.Uv);

	return color;
}