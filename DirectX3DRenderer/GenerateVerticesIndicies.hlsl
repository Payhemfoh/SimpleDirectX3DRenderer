struct VertexPositionUv {
	float3 position;
	float2 uv;
};

StructuredBuffer<float> DepthData  : register(t0); // Input depth data
RWStructuredBuffer<VertexPositionUv> Vertices : register(u0); // Output vertices
RWStructuredBuffer<uint> Indices : register(u1); // Output indices

cbuffer Constants{
	uint modelWidth;
	uint modelHeight;
	float maxDepth;
}

[numthreads(16, 16, 1)]
void Main(uint3 DTid : SV_DispatchThreadID)
{
	uint x = DTid.x;
	uint y = DTid.y;

	if (x >= modelWidth || y >= modelHeight)
		return;

	float depthValue = DepthData[y * modelWidth + x] / maxDepth;
	float posX = (float)x / modelWidth;
	float posY = (float)y / modelHeight;

	float invertedPosY = 1.0f - posY;

	uint vertexIndex = y * modelWidth + x;
	VertexPositionUv vertex;
	vertex.position = float3(posX, depthValue, posY);
	vertex.uv = float2(posX, invertedPosY);

	Vertices[vertexIndex] = vertex;

	if (x < modelWidth - 1 && y < modelHeight - 1) {
		uint baseIndex = (y * (modelWidth - 1) + x) * 6;
		Indices[baseIndex + 0] = vertexIndex;
		Indices[baseIndex + 1] = vertexIndex + 1;
		Indices[baseIndex + 2] = vertexIndex + modelWidth;

		Indices[baseIndex + 3] = vertexIndex + modelWidth;
		Indices[baseIndex + 4] = vertexIndex + 1;
		Indices[baseIndex + 5] = vertexIndex + modelWidth + 1;
	}
}