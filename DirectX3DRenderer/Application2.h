#pragma once
#include <Windows.h>
#include <string>
#include <vector>
#include <d3d11_2.h>
#include <dxgi1_3.h>
#include <wrl.h>
#include <memory>
#include <DirectXMath.h>
#include <map>
#include <chrono>

using Position = DirectX::XMFLOAT3;
using Uv = DirectX::XMFLOAT2;

struct VertexPositionUv {
	Position position;
	Uv texCoord;
};

struct PerFrameConstantBuffer
{
	DirectX::XMFLOAT4X4 viewProjectionMatrix;
};

struct PerObjectConstantBuffer
{
	DirectX::XMFLOAT4X4 modelMatrix;
};

constexpr D3D11_INPUT_ELEMENT_DESC vertexInputLayoutInfo[] ={
	{
		"POSITION",
		0,
		DXGI_FORMAT::DXGI_FORMAT_R32G32B32_FLOAT,
		0,
		offsetof(VertexPositionUv, position),
		D3D11_INPUT_CLASSIFICATION::D3D11_INPUT_PER_VERTEX_DATA,
		0
	},
	{
		"TEXCOORD",
		0,
		DXGI_FORMAT::DXGI_FORMAT_R32G32_FLOAT,
		0,
		offsetof(VertexPositionUv, texCoord),
		D3D11_INPUT_CLASSIFICATION::D3D11_INPUT_PER_VERTEX_DATA,
		0
	}
};

enum Direction {
	FRONT,
	BACK,
	LEFT,
	RIGHT
};

class Application2
{
	template <typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

private:
	#pragma region Windows Properties
	HWND _window;
	HINSTANCE _hinst;
	int window_width;
	int window_height;

	float _deltaTime = 0.016f;
	std::chrono::high_resolution_clock::time_point _oldTime;
	std::chrono::high_resolution_clock::time_point _currentTime;
	#pragma endregion

	#pragma region Camera Control
	DirectX::XMFLOAT3 cameraPosition;
	DirectX::XMFLOAT3 cameraTarget;
	DirectX::XMFLOAT3 cameraUp;
	DirectX::XMMATRIX viewMatrix;
	float cameraSpeed;
	float cameraOffsetZ;
	float modelScale;
	float viewOffsetX;
	float viewOffsetY;

	bool isMovingLeft;
	bool isMovingRight;
	bool isMovingFront;
	bool isMovingBack;

	int modelWidth;
	int modelHeight;
	#pragma endregion

	#pragma region Rendering Properties
	ComPtr<IDXGIFactory2> _dxgiFactory = nullptr;
	ComPtr<ID3D11Device> _device = nullptr;
	ComPtr<ID3D11Debug> _debug = nullptr;
	ComPtr<ID3D11DeviceContext> _deviceContext = nullptr;
	ComPtr<IDXGISwapChain1> _swapChain = nullptr;
	ComPtr<ID3D11RenderTargetView> _renderTarget = nullptr;
	ComPtr<ID3D11SamplerState> _samplerState = nullptr;
	ComPtr<ID3D11RasterizerState> _rasterState = nullptr;
	ComPtr<ID3D11DepthStencilView> _depthTarget = nullptr;
	ComPtr<ID3D11DepthStencilState> _depthState = nullptr;
	ComPtr<ID3D11Buffer> _perFrameConstantBuffer = nullptr;
	ComPtr<ID3D11Buffer> _perObjectConstantBuffer = nullptr;

	PerFrameConstantBuffer _perFrameConstantBufferData{};
	PerObjectConstantBuffer _perObjectConstantBufferData{};

	ComPtr<ID3D11VertexShader> _vertexShader = nullptr;
	ComPtr<ID3D11PixelShader> _pixelShader = nullptr;
	ComPtr<ID3D11InputLayout> _inputLayout = nullptr;
	//ComPtr<ID3D11Buffer> _cubeVertices = nullptr;
	//ComPtr<ID3D11Buffer> _cubeIndices = nullptr;
	ComPtr<ID3D11Buffer> _vertexBuffer = nullptr;
	ComPtr<ID3D11Buffer> _indicesBuffer = nullptr;
	ComPtr<ID3D11ShaderResourceView> _depthResource = nullptr;
	ComPtr<ID3D11ShaderResourceView> _skinResource = nullptr;

	std::vector<VertexPositionUv> vertices;
	std::vector<uint32_t> indices;
	#pragma region

	#pragma region Window Management
	ATOM RegisterWindowClass();
	HWND InitializeWindow(int _nCmdShow);
	void UpdateWindowSize();
	void UpdateWindowSize(UINT width, UINT height);
	#pragma endregion

	#pragma region Application Lifecycle
	bool Load();
	void Update();
	void Render();
	void initialize(int _nCmdShow);
	void initialize_directX();
	void OnWindowResized();
	#pragma endregion

	#pragma region Camera Control
	void InitializeCamera();
	void UpdateCameraPosition();
	void UpdateViewMatrix();
	void UpdateModelBuffer();
	void PanModel(float dx, float dy);
	void RotateModel(float dx, float dy);
	void ZoomView(float dz);
	void MoveCamera(Direction d);
	void StopCamera(Direction d);
	#pragma endregion

	#pragma region Rendering Pipeline
	bool CreateSwapchainResources();
	void DestroySwapchainResources();
	void CreateDepthState();
	void CreateConstantBuffer();
	void CreateRasterState();
	void CreateSamplerState();
	void CreateShaderResources();
	void CreateDepthStencilView();
	void UpdateConstantBuffer();
	void LoadAndPrepareRenderResource();

	void SetRenderTarget();
	void SetShaderResources();
	void SetConstantBuffer();
	void ClearPreviousFrame();

	ComPtr<ID3D11VertexShader> CreateVertexShader(
		ID3D11Device* device,
		const std::wstring& filePath,
		ComPtr<ID3DBlob>& vertexShaderBlob);
	ComPtr<ID3D11PixelShader>CreatePixelShader(
		ID3D11Device* device,
		const std::wstring& filePath);
	bool CompileShader(
		const std::wstring& filePath,
		const std::string& entryPoint,
		const std::string& profile,
		ComPtr<ID3DBlob>& shaderBlob);
	#pragma endregion
public:
	Application2(HINSTANCE hinst, int _nCmdShow);
	~Application2();
	void Run();
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
};

