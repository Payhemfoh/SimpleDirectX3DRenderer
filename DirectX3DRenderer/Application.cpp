#include "Application.h"
#include <wincodec.h>
#include <DirectXColors.h>
#include <iostream>
#include <d3dcompiler.h>
#include "WICTextureLoader.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "dxguid.lib")

POINT lastMousePos;
bool leftMouseClicked = false;
bool rightMouseClicked = false;

LRESULT CALLBACK Application::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hWnd, &ps);
		EndPaint(hWnd, &ps);
	}
	break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	case WM_SIZE:
	{
		Application* application = reinterpret_cast<Application*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
		if(application != nullptr)
			application->UpdateWindowSize(LOWORD(lParam), HIWORD(lParam));
		break;
	}
	case WM_KEYDOWN:
	{
		Application* application = reinterpret_cast<Application*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
		if (application != nullptr)
		{
			switch (wParam)
			{
			case VK_HOME:
			{
				application->InitializeCamera();
				break;
			}
			case 'W':
			{
				application->MoveCamera(Direction::FRONT);
				break;
			}
			case 'S':
			{
				application->MoveCamera(Direction::BACK);
				break;
			}
			case 'A':
			{
				application->MoveCamera(Direction::LEFT);
				break;
			}
			case 'D':
			{
				application->MoveCamera(Direction::RIGHT);
				break;
			}
			}
		}
		
		break; 
	}
	case WM_KEYUP:
	{
		Application* application = reinterpret_cast<Application*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
		if (application != nullptr)
		{
			switch (wParam)
			{
			case 'W':
			{
				application->StopCamera(Direction::FRONT);
				break;
			}
			case 'S':
			{
				application->StopCamera(Direction::BACK);
				break;
			}
			case 'A':
			{
				application->StopCamera(Direction::LEFT);
				break;
			}
			case 'D':
			{
				application->StopCamera(Direction::RIGHT);
				break;
			}
			}
		}
		break;
	}
	case WM_LBUTTONDOWN:
	{
		lastMousePos.x = LOWORD(lParam);
		lastMousePos.y = HIWORD(lParam);

		if (!leftMouseClicked && !rightMouseClicked)
			SetCapture(hWnd);

		leftMouseClicked = true;
		break;
	}
	case WM_RBUTTONDOWN:
	{
		lastMousePos.x = LOWORD(lParam);
		lastMousePos.y = HIWORD(lParam);

		if (!leftMouseClicked && !rightMouseClicked)
			SetCapture(hWnd);

		rightMouseClicked = true;
		break;
	}
	case WM_MOUSEMOVE:
	{
		int x = LOWORD(lParam);
		int y = HIWORD(lParam);

		float dx = DirectX::XMConvertToRadians(0.25f * static_cast<float>(x - lastMousePos.x));
		float dy = DirectX::XMConvertToRadians(0.25f * static_cast<float>(y - lastMousePos.y));

		Application* application = reinterpret_cast<Application*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
		if (application != nullptr)
		{
			if(leftMouseClicked)
				application->RotateModel(dx, dy);
			if (rightMouseClicked)
				application->PanModel(dx, dy);
		}

		lastMousePos.x = x;
		lastMousePos.y = y;
		break;
	}
	case WM_LBUTTONUP:
	{
		leftMouseClicked = false;

		if(!leftMouseClicked && !rightMouseClicked)
			ReleaseCapture();
		break;
	}
	case WM_RBUTTONUP:
	{
		rightMouseClicked = false;

		if (!leftMouseClicked && !rightMouseClicked)
			ReleaseCapture();
		break;
	}
	case WM_MOUSEWHEEL:
	{
		int deltaZ = GET_WHEEL_DELTA_WPARAM(wParam);

		Application* application = reinterpret_cast<Application*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
		if (application != nullptr)
			application->ZoomView(deltaZ);
	}
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

template <UINT TDebugNameLength>
inline void SetDebugName(_In_ ID3D11DeviceChild* deviceResource, _In_z_ const char(&debugName)[TDebugNameLength])
{
	deviceResource->SetPrivateData(WKPDID_D3DDebugObjectName, TDebugNameLength - 1, debugName);
}

Application::Application(HINSTANCE hinst, int _nCmdShow)
{
	_hinst = hinst;
	initialize(_nCmdShow);
}

Application::~Application()
{
	_deviceContext->Flush();

	_samplerState.Reset();
	_rasterState.Reset();
	_depthState.Reset();
	_depthTarget.Reset();
	_computeShader.Reset();
	_vertexShader.Reset();
	_pixelShader.Reset();
	_inputLayout.Reset();
	DestroySwapchainResources();
	_swapChain.Reset();
	_dxgiFactory.Reset();
	_deviceContext.Reset();
	_perFrameConstantBuffer.Reset();
	_perObjectConstantBuffer.Reset();
	_vertexBuffer.Reset();
	_indicesBuffer.Reset();
	#ifndef NDEBUG
	_debug->ReportLiveDeviceObjects(D3D11_RLDO_FLAGS::D3D11_RLDO_DETAIL);
	_debug.Reset();
	#endif
	_device.Reset();
}

void Application::initialize(int _nCmdShow)
{
	_window = InitializeWindow(_nCmdShow);
	initialize_directX();
}

void Application::initialize_directX()
{
	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if (FAILED(hr))
		throw std::exception("Failed to initialize COM");

	// The factory pointer
	IWICImagingFactory* pFactory = NULL;

	// Create the COM imaging factory
	hr = CoCreateInstance(
		CLSID_WICImagingFactory,
		NULL,
		CLSCTX_INPROC_SERVER,
		IID_PPV_ARGS(&pFactory)
	);

	if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&_dxgiFactory))))
		throw std::exception("Failed to create dxgi factory");

	//create device and device context so we can interact with direct3d hardware later
	constexpr D3D_FEATURE_LEVEL levels[] = {
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
		D3D_FEATURE_LEVEL_9_3,
		D3D_FEATURE_LEVEL_9_2,
		D3D_FEATURE_LEVEL_9_1,
	};

	// This flag adds support for surfaces with a color-channel ordering different
	// from the API default. It is required for compatibility with Direct2D.
	UINT deviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

	#ifndef NDEBUG
	deviceFlags |= D3D11_CREATE_DEVICE_FLAG::D3D11_CREATE_DEVICE_DEBUG;
	#endif

	hr = D3D11CreateDevice(
		nullptr,                    // Specify nullptr to use the default adapter.
		D3D_DRIVER_TYPE_HARDWARE,   // Create a device using the hardware graphics driver.
		0,                          // Should be 0 unless the driver is D3D_DRIVER_TYPE_SOFTWARE.
		deviceFlags,                // Set debug and Direct2D compatibility flags.
		levels,                     // List of feature levels this app can support.
		ARRAYSIZE(levels),          // Size of the list above.
		D3D11_SDK_VERSION,          // Always set this to D3D11_SDK_VERSION for Windows Store apps.
		&_device,                    // Returns the Direct3D device created.
		nullptr,            // Returns feature level of device created.
		&_deviceContext             // Returns the device immediate context.
	);

	if (FAILED(hr))
		throw std::exception("Failed to create D3D11 Device");

	#ifndef NDEBUG
	if (FAILED(_device.As(&_debug)))
		throw std::exception("Failed to create D3D11 debug layer");
	#endif

	constexpr char deviceName[] = "DEV_Main";
	_device->SetPrivateData(WKPDID_D3DDebugObjectName, sizeof(deviceName), deviceName);
	SetDebugName(_deviceContext.Get(), "CTX_Main");

	DXGI_SWAP_CHAIN_DESC1 swapChainDescriptor = {};
	swapChainDescriptor.Width = window_width;
	swapChainDescriptor.Height = window_height;
	swapChainDescriptor.Format = DXGI_FORMAT::DXGI_FORMAT_B8G8R8A8_UNORM;
	swapChainDescriptor.SampleDesc.Count = 1;
	swapChainDescriptor.SampleDesc.Quality = 0;
	swapChainDescriptor.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDescriptor.BufferCount = 3; //triple buffering
	swapChainDescriptor.SwapEffect = DXGI_SWAP_EFFECT::DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDescriptor.Scaling = DXGI_SCALING::DXGI_SCALING_STRETCH;
	swapChainDescriptor.Flags = {};

	DXGI_SWAP_CHAIN_FULLSCREEN_DESC swapChainFullscreenDescriptor = {};
	swapChainFullscreenDescriptor.Windowed = true;

	if (FAILED(_dxgiFactory->CreateSwapChainForHwnd(
		_device.Get(),
		_window,
		&swapChainDescriptor,
		&swapChainFullscreenDescriptor,
		nullptr,
		&_swapChain)))
		throw std::exception("Failed to create swap chain");

	if (!CreateSwapchainResources())
		throw std::exception("Failed to create swap chain resources");

	CreateSamplerState();
	CreateRasterState();
	CreateDepthStencilView();
	CreateDepthState();
	CreateConstantBuffer();

	InitializeCamera();
}

void Application::InitializeCamera()
{
	cameraPosition = { 1.0f, 1.0f, -1.0f };
	cameraTarget = { 0.0f, 0.0f, 0.0f };
	cameraUp = { 0.0f, 1.0f, 0.0f };

	cameraSpeed = 1.0f;
	modelScale = 1.0f;
	viewOffsetX = 0.0f;
	viewOffsetY = 0.0f;
	cameraOffsetZ = 0.0f;
}
 
void Application::UpdateCameraPosition()
{
	float velocity = cameraSpeed * _deltaTime;
	if (isMovingFront)
	{
		PanModel(0, velocity);
	}
	if (isMovingBack)
	{
		PanModel(0, -velocity);
	}
	if (isMovingLeft)
	{
		PanModel(velocity, 0);
	}
	if (isMovingRight)
	{
		PanModel(-velocity, 0);
	}
}

void Application::UpdateViewMatrix()
{
	using namespace DirectX;

	XMVECTOR camPos = XMLoadFloat3(&cameraPosition);
	XMVECTOR camTarget = XMLoadFloat3(&cameraTarget);
	XMVECTOR camUp = XMLoadFloat3(&cameraUp);

	XMVECTOR viewDirection = XMVector3Normalize(XMVectorSubtract(camTarget, camPos));
	XMVECTOR rightVector = XMVector3Normalize(XMVector3Cross(camUp, viewDirection));

	XMVECTOR translationX = XMVectorScale(rightVector, viewOffsetX);
	XMVECTOR translationY = XMVectorScale(camUp, viewOffsetY);
	XMVECTOR translationZ = XMVectorScale(viewDirection, cameraOffsetZ);
	XMVECTOR translation = XMVectorAdd(XMVectorAdd(translationX, translationY), translationZ);

	camPos = XMVectorAdd(camPos, translation);
	camTarget = XMVectorAdd(camTarget, translation);

	viewMatrix = XMMatrixLookAtRH(camPos, camTarget, camUp);
}

void Application::UpdateModelBuffer()
{
	using namespace DirectX;

	XMFLOAT3 modelCenter(0.5f, 0.5f, 0.5f);

	//This will define our 3D object
	XMMATRIX translationToOrigin = XMMatrixTranslation(-modelCenter.x, -modelCenter.y, -modelCenter.z);
	XMMATRIX rotation = XMMatrixRotationRollPitchYaw(0, 0, 0);
	XMMATRIX scaling = XMMatrixScaling(modelScale, modelScale, modelScale);
	//Now we create our model matrix


	XMMATRIX modelMatrix = translationToOrigin * rotation * scaling;
	XMStoreFloat4x4(&_perObjectConstantBufferData.modelMatrix, modelMatrix);
}

void Application::PanModel(float dx, float dy)
{
	using namespace DirectX;
	
	viewOffsetX += dx;
	viewOffsetY += dy;

	UpdateViewMatrix();
}

void Application::ZoomView(float dz)
{
	using namespace DirectX;
	const float scrollSensitivity = 0.0001f;
	const float minZ = -1.0f;
	const float maxZ = 4.0f;

	cameraOffsetZ += dz * scrollSensitivity;

	if (cameraOffsetZ < minZ)
		cameraOffsetZ = minZ;
	else if (cameraOffsetZ > maxZ)
		cameraOffsetZ = maxZ;

	UpdateViewMatrix();
}

void Application::MoveCamera(Direction d)
{
	switch (d)
	{
	case Direction::FRONT:
		isMovingFront = true;
		break;
	case Direction::BACK:
		isMovingBack = true;
		break;
	case Direction::LEFT:
		isMovingLeft = true;
		break;
	case Direction::RIGHT:
		isMovingRight = true;
		break;
	}
}

void Application::StopCamera(Direction d)
{
	switch (d)
	{
	case Direction::FRONT:
		isMovingFront = false;
		break;
	case Direction::BACK:
		isMovingBack = false;
		break;
	case Direction::LEFT:
		isMovingLeft = false;
		break;
	case Direction::RIGHT:
		isMovingRight = false;
		break;
	}
}

void Application::RotateModel(float dx, float dy)
{
	using namespace DirectX;

	XMMATRIX rX = XMMatrixRotationX(-dy);
	XMMATRIX rY = XMMatrixRotationY(-dx);

	XMVECTOR position = XMLoadFloat3(&cameraPosition);
	position = XMVector3TransformNormal(position, rY);
	position = XMVector3TransformNormal(position, rX);
	XMStoreFloat3(&cameraPosition, position);

	UpdateViewMatrix();
}

bool Application::CreateSwapchainResources()
{
	ComPtr<ID3D11Texture2D> backBuffer = nullptr;
	if (FAILED(_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer))))
		return false;

	if (FAILED(_device->CreateRenderTargetView(backBuffer.Get(), nullptr, &_renderTarget)))
		return false;

	return true;
}

void Application::DestroySwapchainResources()
{
	_renderTarget.Reset();
}

void Application::CreateDepthState()
{
	D3D11_DEPTH_STENCIL_DESC depthDesc{};
	depthDesc.DepthEnable = TRUE;
	depthDesc.DepthFunc = D3D11_COMPARISON_FUNC::D3D11_COMPARISON_LESS;
	depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;

	_device->CreateDepthStencilState(&depthDesc, &_depthState);
}

void Application::CreateConstantBuffer()
{
	D3D11_BUFFER_DESC desc{};
	desc.Usage = D3D11_USAGE::D3D11_USAGE_DYNAMIC;
	desc.BindFlags = D3D11_BIND_FLAG::D3D11_BIND_CONSTANT_BUFFER;
	desc.ByteWidth = sizeof(PerFrameConstantBuffer);
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	_device->CreateBuffer(&desc, nullptr, &_perFrameConstantBuffer);

	desc.ByteWidth = sizeof(PerObjectConstantBuffer);
	_device->CreateBuffer(&desc, nullptr, &_perObjectConstantBuffer);
}

void Application::CreateSamplerState()
{
	// Define the sampler description
	D3D11_SAMPLER_DESC samplerDesc = {};
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	samplerDesc.MinLOD = 0;
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

	_device->CreateSamplerState(&samplerDesc, &_samplerState);
}

void Application::CreateRasterState()
{
	D3D11_RASTERIZER_DESC rasterDesc{};
	rasterDesc.CullMode = D3D11_CULL_NONE;
	rasterDesc.FillMode = D3D11_FILL_SOLID;

	_device->CreateRasterizerState(&rasterDesc, &_rasterState);
}

ATOM Application::RegisterWindowClass()
{
	WNDCLASSEXW wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);


	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = _hinst;
	wcex.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wcex.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
	wcex.hCursor = LoadCursorW(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = L"3D RENDERER";

	return RegisterClassExW(&wcex);
}

HWND Application::InitializeWindow(int _nCmdShow)
{
	RegisterWindowClass();

	HWND _hwnd = CreateWindowW(L"3D RENDERER", L"3D Renderer", WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, _hinst, nullptr);

	if (!_hwnd)
	{
		throw std::exception("Failed to create main window");
	}

	ShowWindow(_hwnd, _nCmdShow);
	UpdateWindow(_hwnd);

	static RECT size;
	if (GetWindowRect(_hwnd, &size))
	{
		this->window_width = size.right - size.left;
		this->window_height = size.bottom - size.top;
	}
	SetWindowLongPtrW(_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

	return _hwnd;
}

void Application::UpdateWindowSize()
{
	static RECT size;
	if (GetWindowRect(this->_window, &size))
	{
		this->window_width = size.right - size.left;
		this->window_height = size.bottom - size.top;
	}

	OnWindowResized();
}

void Application::UpdateWindowSize(UINT width, UINT height)
{
	this->window_width = width;
	this->window_height = height;

	OnWindowResized();
}

void Application::OnWindowResized()
{
	ID3D11RenderTargetView* nullRTV = nullptr;
	_deviceContext->OMSetRenderTargets(1, &nullRTV, nullptr);
	_deviceContext->Flush();

	DestroySwapchainResources();

	if (FAILED(_swapChain->ResizeBuffers(0, window_width, window_height, DXGI_FORMAT::DXGI_FORMAT_B8G8R8A8_UNORM, 0)))
		return;

	CreateSwapchainResources();

	CreateDepthStencilView();
}

void Application::Run()
{
	if (!Load())
		return;

	//pool event, and update UI
	bool bGotMsg;
	MSG  msg = { 0 };

	while (true)
	{
		_oldTime = _currentTime;
		_currentTime = std::chrono::high_resolution_clock::now();

		std::chrono::duration<double, std::milli> timeSpan = (_currentTime - _oldTime);
		_deltaTime = static_cast<float>(timeSpan.count() / 1000.0);

		// Process window events.
		// Use PeekMessage() so we can use idle time to render the scene. 
		bGotMsg = PeekMessageW(&msg, NULL, 0U, 0U, PM_REMOVE);

		if (bGotMsg)
		{
			if (msg.message == WM_QUIT)
				break;

			// Translate and dispatch the message
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
		
		Update();
		Render();
		Sleep(50);
	}
}

void Application::LoadAndPrepareRenderResource()
{
	//load and process height map
	
	//load rgb skin
	if (FAILED(DirectX::CreateWICTextureFromFile(_device.Get(), L"C:\\Users\\Payhemfoh\\source\\repos\\DirectX3DRenderer\\data\\rgb.jpg", nullptr, &_skinResource)))
	{
		std::cerr << "Error loading skin texture" << std::endl;
		return;
	}

	//load depth map
	ComPtr<ID3D11Resource> resource;
	if (FAILED(DirectX::CreateWICTextureFromFile(_device.Get(), L"C:\\Users\\Payhemfoh\\source\\repos\\DirectX3DRenderer\\data\\depth.jpg", &resource, &_depthResource)))
	{
		std::cerr << "Error loading texture" << std::endl;
		return;
	}

	//copy depth map into 2d array
	ComPtr<ID3D11Texture2D> depthTexture2D;
	resource.As(&depthTexture2D);

	// Get the description of the original depth texture
	D3D11_TEXTURE2D_DESC desc;
	depthTexture2D->GetDesc(&desc);
	
	// Modify the description for a staging texture
	D3D11_TEXTURE2D_DESC stagingDesc = desc;
	stagingDesc.Usage = D3D11_USAGE_STAGING;
	stagingDesc.BindFlags = 0;
	stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	stagingDesc.MiscFlags = 0;

	// Create the staging texture
	ComPtr<ID3D11Texture2D> stagingTexture;
	HRESULT hr = _device->CreateTexture2D(&stagingDesc, nullptr, stagingTexture.GetAddressOf());
	if (FAILED(hr)) {
		// Handle error
		return;
	}

	_deviceContext->CopyResource(stagingTexture.Get(), depthTexture2D.Get());

	// Map the staging texture
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	hr = _deviceContext->Map(stagingTexture.Get(), 0, D3D11_MAP_READ, 0, &mappedResource);
	if (FAILED(hr)) {
		// Handle error
		return;
	}

	// Copy depth data to a 2D array
	std::vector<unsigned char> depthData(desc.Width * desc.Height);
	if (mappedResource.pData) {
		BYTE* src = reinterpret_cast<BYTE*>(mappedResource.pData);
		for (int y = 0; y < desc.Height; ++y)
		{
			for (int x = 0; x < desc.Width; ++x)
			{
				//only get r channel from rgba image
				depthData[y * desc.Width + x] = src[(y * mappedResource.RowPitch) + x];
			}
		}
		_deviceContext->Unmap(stagingTexture.Get(), 0);
	}
	else {
		// Handle error
		return;
	}

	modelWidth = desc.Width;
	modelHeight = desc.Height;
	//convert depth map into mesh

	auto max = std::max_element(depthData.begin(), depthData.end());

	#pragma region CPU Code
	_vertices.clear();
	_indices.clear();

	for (UINT y = 0; y < modelHeight; ++y) {
		for (UINT x = 0; x < modelWidth; ++x) {
			// Calculate depth value along the y-axis
			float depthValue = static_cast<float>(depthData[y * modelWidth + x]) / *max;

			// Calculate x and z positions
			float posX = static_cast<float>(x) / modelWidth;
			float posZ = static_cast<float>(y) / modelHeight;
			float invertedPosZ = 1.0 - posZ;
			// Use depthValue as the y-coordinate
			float posY = depthValue;

			// Calculate UV coordinates
			float uvX = posX;
			float uvY = invertedPosZ;

			_vertices.push_back(VertexPositionUv{
				{ posX, posY, invertedPosZ },   // Position
				{ uvX, uvY }            // UV coordinates
				});

			if (x < modelWidth - 1 && y < modelHeight - 1) {
				_indices.push_back(y * modelWidth + x);
				_indices.push_back(y * modelWidth + x + 1);
				_indices.push_back((y + 1) * modelWidth + x);

				_indices.push_back((y + 1) * modelWidth + x);
				_indices.push_back(y * modelWidth + x + 1);
				_indices.push_back((y + 1) * modelWidth + x + 1);
			}
		}
	}

	// Create vertex buffer
	D3D11_BUFFER_DESC vertexBufferDesc = {};
	vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	vertexBufferDesc.ByteWidth = sizeof(VertexPositionUv) * _vertices.size();
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vertexBufferDesc.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA vertexData = {};
	vertexData.pSysMem = _vertices.data();

	_device->CreateBuffer(&vertexBufferDesc, &vertexData, &_vertexBuffer);

	// Create index buffer
	D3D11_BUFFER_DESC indexBufferDesc = {};
	indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	indexBufferDesc.ByteWidth = sizeof(UINT) * _indices.size();
	indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	indexBufferDesc.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA indexData = {};
	indexData.pSysMem = _indices.data();

	_device->CreateBuffer(&indexBufferDesc, &indexData, &_indicesBuffer);

	#pragma endregion

	/*
	#pragma region GPU Code
	struct CBuffer {
		unsigned int modelWidth;
		unsigned int modelHeight;
		float maxDepth;
	} buffer = {modelWidth, modelHeight, *max};

	// Create constant buffer
	D3D11_BUFFER_DESC constantBufferDesc = {};
	constantBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	constantBufferDesc.ByteWidth = static_cast<UINT>(sizeof(CBuffer));
	constantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	D3D11_SUBRESOURCE_DATA cData = {};
	cData.pSysMem = &buffer;

	_device->CreateBuffer(&constantBufferDesc, &cData, _constantBuffer.GetAddressOf());

	// Create structured buffer
	D3D11_BUFFER_DESC bufferDesc = {};
	bufferDesc.Usage = D3D11_USAGE_DEFAULT;
	bufferDesc.ByteWidth = sizeof(VertexPositionUv) * modelWidth * modelHeight;
	bufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_VERTEX_BUFFER;
	bufferDesc.CPUAccessFlags = 0;
	bufferDesc.StructureByteStride = sizeof(VertexPositionUv);
	bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	_device->CreateBuffer(&bufferDesc, nullptr, _vertexBuffer.GetAddressOf());

	// Create the UAV for the vertex buffer
	D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.NumElements = modelWidth * modelHeight;

	ID3D11UnorderedAccessView* vertexDataUAV;
	_device->CreateUnorderedAccessView(_vertexBuffer.Get(), &uavDesc, &vertexDataUAV);


	// Create the structured buffer for index data
	D3D11_BUFFER_DESC indexBufferDesc = {};
	indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	indexBufferDesc.ByteWidth = sizeof(UINT) * modelWidth * modelHeight * 6; // Assuming 6 indices per quad
	indexBufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_INDEX_BUFFER;
	indexBufferDesc.CPUAccessFlags = 0;
	indexBufferDesc.StructureByteStride = sizeof(UINT);
	indexBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	_device->CreateBuffer(&indexBufferDesc, nullptr, _indicesBuffer.GetAddressOf());

	// Create the UAV for the index buffer
	D3D11_UNORDERED_ACCESS_VIEW_DESC indexUavDesc = {};
	indexUavDesc.Format = DXGI_FORMAT_UNKNOWN;
	indexUavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	indexUavDesc.Buffer.FirstElement = 0;
	indexUavDesc.Buffer.NumElements = modelWidth * modelHeight * 6; // Assuming 6 indices per quad

	ID3D11UnorderedAccessView* indexDataUAV;
	_device->CreateUnorderedAccessView(_indicesBuffer.Get(), &indexUavDesc, &indexDataUAV);

	// Set compute shader
	_deviceContext->CSSetShader(_computeShader.Get(), nullptr, 0);

	// Set constant buffer
	_deviceContext->CSSetConstantBuffers(0, 1, &_constantBuffer);

	// Bind the input and output structured buffers
	_deviceContext->CSSetShaderResources(0, 1, &_depthResource);
	_deviceContext->CSSetUnorderedAccessViews(0, 1, &vertexDataUAV, nullptr);
	_deviceContext->CSSetUnorderedAccessViews(1, 1, &indexDataUAV, nullptr);

	// Dispatch the compute shader
	_deviceContext->Dispatch((modelWidth + 15) / 16, (modelHeight + 15) / 16, 1);

	// Unbind the buffers
	ID3D11UnorderedAccessView* nullUAVs[2] = { nullptr, nullptr };
	_deviceContext->CSSetUnorderedAccessViews(0, 2, nullUAVs, nullptr);
	_deviceContext->CSSetShader(nullptr, nullptr, 0);
	#pragma endregion*/
}

bool Application::Load()
{
	CreateShaderResources();
	LoadAndPrepareRenderResource();

	return true;
}

void Application::Update()
{
	using namespace DirectX;

	//static float _yRotation = 0.0f;
	//_yRotation += _deltaTime;

	//////////////////////////
   //This will be our "camera"
	UpdateCameraPosition();
	UpdateViewMatrix();
	
	XMMATRIX proj = XMMatrixPerspectiveFovRH(90.0f * 0.0174533f,
		static_cast<float>(window_width) / static_cast<float>(window_height),
		0.1f,
		100.0f);

	//combine the view & proj matrix
	XMMATRIX viewProjection = XMMatrixMultiply(viewMatrix, proj);
	XMStoreFloat4x4(&_perFrameConstantBufferData.viewProjectionMatrix, viewProjection);

	UpdateModelBuffer();
	UpdateConstantBuffer();
}

void Application::UpdateConstantBuffer()
{
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	_deviceContext->Map(_perFrameConstantBuffer.Get(), 0, D3D11_MAP::D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	memcpy(mappedResource.pData, &_perFrameConstantBufferData, sizeof(PerFrameConstantBuffer));
	_deviceContext->Unmap(_perFrameConstantBuffer.Get(), 0);

	_deviceContext->Map(_perObjectConstantBuffer.Get(), 0, D3D11_MAP::D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	memcpy(mappedResource.pData, &_perObjectConstantBufferData, sizeof(PerObjectConstantBuffer));
	_deviceContext->Unmap(_perObjectConstantBuffer.Get(), 0);
}

void Application::CreateShaderResources()
{
	std::wstring VertexShaderFilePath = L"Main.vs.hlsl";
	std::wstring PixelShaderFilePath = L"Main.ps.hlsl";
	std::wstring ComputeShaderFilePath = L"GenerateVerticesIndicies.hlsl";

	ComPtr<ID3DBlob> vertexShaderBlob;
	
	_computeShader = CreateComputeShader(_device.Get(), ComputeShaderFilePath);
	_vertexShader = CreateVertexShader(_device.Get(), VertexShaderFilePath, vertexShaderBlob);
	_pixelShader = CreatePixelShader(_device.Get(),PixelShaderFilePath);

	if (FAILED(_device->CreateInputLayout(
		vertexInputLayoutInfo,
		_countof(vertexInputLayoutInfo),
		vertexShaderBlob->GetBufferPointer(),
		vertexShaderBlob->GetBufferSize(),
		&_inputLayout)))
		throw std::exception("D3D11: Failed to create the input layout");
}

void Application::CreateDepthStencilView()
{
	_depthTarget.Reset();

	D3D11_TEXTURE2D_DESC texDesc{};
	texDesc.Height = window_height;
	texDesc.Width = window_width;
	texDesc.ArraySize = 1;
	texDesc.SampleDesc.Count = 1;
	texDesc.MipLevels = 1;
	texDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	texDesc.Format = DXGI_FORMAT_R32_TYPELESS;

	ID3D11Texture2D* texture = nullptr;
	if (FAILED(_device->CreateTexture2D(&texDesc, nullptr, &texture)))
		throw std::exception("DXGI: Failed to create texture for DepthStencilView");
	

	D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
	dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	if (FAILED(_device->CreateDepthStencilView(texture, &dsvDesc, &_depthTarget)))
	{
		texture->Release();
		throw std::exception("DXGI: Failed to create DepthStencilView");
	}

	texture->Release();
}

Application::ComPtr<ID3D11ComputeShader> Application::CreateComputeShader(
	ID3D11Device* device,
	const std::wstring& filePath
)
{
	ComPtr<ID3DBlob> computeShaderBlob = nullptr;

	if (!CompileShader(filePath, "Main", "cs_5_0", computeShaderBlob))
	{
		return nullptr;
	}

	Application::ComPtr<ID3D11ComputeShader> computeShader;
	if (FAILED(device->CreateComputeShader(
		computeShaderBlob->GetBufferPointer(),
		computeShaderBlob->GetBufferSize(),
		nullptr,
		&computeShader)))
	{
		std::cerr << "D3D11: Failed to compile compute shader\n";
		return nullptr;
	}

	return computeShader;
}

Application::ComPtr<ID3D11VertexShader> Application::CreateVertexShader(
	ID3D11Device* device,
	const std::wstring& filePath,
	Application::ComPtr<ID3DBlob>& vertexShaderBlob)
{
	if (!CompileShader(filePath, "Main", "vs_5_0", vertexShaderBlob))
	{
		return nullptr;
	}

	Application::ComPtr<ID3D11VertexShader> vertexShader;
	if (FAILED(device->CreateVertexShader(
		vertexShaderBlob->GetBufferPointer(),
		vertexShaderBlob->GetBufferSize(),
		nullptr,
		&vertexShader)))
	{
		std::cerr << "D3D11: Failed to compile vertex shader\n";
		return nullptr;
	}

	return vertexShader;
}

Application::ComPtr<ID3D11PixelShader> Application::CreatePixelShader(
	ID3D11Device* device,
	const std::wstring& filePath)
{
	ComPtr<ID3DBlob> pixelShaderBlob = nullptr;
	if (!CompileShader(filePath, "Main", "ps_5_0", pixelShaderBlob))
	{
		return nullptr;
	}

	ComPtr<ID3D11PixelShader> pixelShader;
	if (FAILED(device->CreatePixelShader(
		pixelShaderBlob->GetBufferPointer(),
		pixelShaderBlob->GetBufferSize(),
		nullptr,
		&pixelShader)))
	{
		std::cerr << "D3D11: Failed to compile pixel shader\n";
		return nullptr;
	}

	return pixelShader;
}

bool Application::CompileShader(
	const std::wstring& filePath,
	const std::string& entryPoint,
	const std::string& profile,
	ComPtr<ID3DBlob>& shaderBlob)
{
	constexpr uint32_t compileFlags = D3DCOMPILE_ENABLE_STRICTNESS;

	ComPtr<ID3DBlob> tempShaderBlob = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	if (FAILED(D3DCompileFromFile(
		filePath.data(),
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		entryPoint.data(),
		profile.data(),
		compileFlags,
		0,
		&tempShaderBlob,
		&errorBlob)))
	{
		std::cerr << "D3D11: Failed to read shader from file\n";
		if (errorBlob != nullptr)
		{
			MessageBoxA(NULL, (std::string("D3D11: With message: ") + static_cast<const char*>(errorBlob->GetBufferPointer())).c_str(), "ERROR", MB_OK);
		}

		return false;
	}

	shaderBlob = tempShaderBlob;
	return true;
}

void Application::ClearPreviousFrame()
{
	float clearColor[] = { 0.1f, 0.1f, 0.1f, 1.0f };
	ID3D11RenderTargetView* nullRTV = nullptr;
	_deviceContext->OMSetRenderTargets(1, &nullRTV, nullptr);
	_deviceContext->ClearRenderTargetView(_renderTarget.Get(), clearColor);
	_deviceContext->ClearDepthStencilView(_depthTarget.Get(), D3D11_CLEAR_FLAG::D3D11_CLEAR_DEPTH, 1.0f, 0);
}

void Application::SetRenderTarget()
{
	_deviceContext->OMSetRenderTargets(1, _renderTarget.GetAddressOf(), _depthTarget.Get());
	_deviceContext->OMSetDepthStencilState(_depthState.Get(), 0);

	D3D11_VIEWPORT viewport = {};
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.Width = static_cast<float>(window_width);
	viewport.Height = static_cast<float>(window_height);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	_deviceContext->RSSetViewports(1, &viewport);
}

void Application::SetShaderResources()
{
	constexpr UINT vertexOffset = 0;

	_deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY::D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	UINT stride = static_cast<UINT>(sizeof(VertexPositionUv));
	
	_deviceContext->IASetVertexBuffers(
		0,
		1,
		_vertexBuffer.GetAddressOf(),
		&stride,
		&vertexOffset);

	_deviceContext->IASetIndexBuffer(
		_indicesBuffer.Get(),
		DXGI_FORMAT::DXGI_FORMAT_R32_UINT,
		0);

	_deviceContext->IASetInputLayout(_inputLayout.Get());
	_deviceContext->VSSetShader(_vertexShader.Get(), nullptr, 0);
	_deviceContext->PSSetShader(_pixelShader.Get(), nullptr, 0);

	_deviceContext->PSSetSamplers(0, 1, _samplerState.GetAddressOf());
	//_deviceContext->PSSetShaderResources(0, 1, _depthResource.GetAddressOf());
	_deviceContext->PSSetShaderResources(0, 1, _skinResource.GetAddressOf());
}

void Application::SetConstantBuffer()
{
	ID3D11Buffer* constantBuffers[2] =
	{
		_perFrameConstantBuffer.Get(),
		_perObjectConstantBuffer.Get()
	};

	_deviceContext->VSSetConstantBuffers(0, 2, constantBuffers);
}

void Application::Render()
{
	if (_renderTarget.Get() == nullptr)
		return;

	ClearPreviousFrame();
	
	SetShaderResources();
	SetRenderTarget();
	_deviceContext->RSSetState(_rasterState.Get());
	SetConstantBuffer();

	_deviceContext->DrawIndexed(_indices.size(), 0, 0);

	//draw base
	std::vector<VertexPositionUv> baseVertices = {
		{{0.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},  // Bottom-left corner (red)
		{{1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},  // Bottom-right corner (red)
		{{1.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},  // Top-right corner (red)
		{{0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}}   // Top-left corner (red)
	};

	std::vector<UINT> baseIndices = {
		0, 1, 2,  // First triangle
		2, 3, 0   // Second triangle
	};

	// Create vertex buffer for base
	D3D11_BUFFER_DESC baseVertexBufferDesc = {};
	baseVertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	baseVertexBufferDesc.ByteWidth = sizeof(VertexPositionUv) * baseVertices.size();
	baseVertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	baseVertexBufferDesc.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA baseVertexData = {};
	baseVertexData.pSysMem = baseVertices.data();

	ID3D11Buffer* baseVertexBuffer;
	_device->CreateBuffer(&baseVertexBufferDesc, &baseVertexData, &baseVertexBuffer);

	// Create index buffer for base
	D3D11_BUFFER_DESC baseIndexBufferDesc = {};
	baseIndexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	baseIndexBufferDesc.ByteWidth = sizeof(UINT) * baseIndices.size();
	baseIndexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	baseIndexBufferDesc.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA baseIndexData = {};
	baseIndexData.pSysMem = baseIndices.data();

	ID3D11Buffer* baseIndexBuffer;
	_device->CreateBuffer(&baseIndexBufferDesc, &baseIndexData, &baseIndexBuffer);

	// Set the vertex buffer for the base
	UINT stride = sizeof(VertexPositionUv);
	UINT offset = 0;
	_deviceContext->IASetVertexBuffers(0, 1, &baseVertexBuffer, &stride, &offset);

	// Set the index buffer for the base
	_deviceContext->IASetIndexBuffer(baseIndexBuffer, DXGI_FORMAT_R32_UINT, 0);

	// Draw the base
	_deviceContext->DrawIndexed(baseIndices.size(), 0, 0);


	_swapChain->Present(1, 0);
}