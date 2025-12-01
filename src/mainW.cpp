#include <DirectXTemplatePCH.h>
using namespace DirectX;

// APPLICATION WINDOW PROPERTIES
const LONG g_WindowWidth = 1280;					        // the size of the renderable area (or client area) of the window, actual window size is slightly bigger
const LONG g_WindowHeight = 720;
// note:: changed from 'LPCWSTR' to 'const wchar_t*' for compatibility
const wchar_t* g_WindowClassName = L"DirectXWindowClass";	// type for wide characters, for passing read-only text to Windows functions
const wchar_t* g_WindowName      = L"DirectX Game Engine Demo";			
HWND g_WindowHandle = 0;							        // Handle to a Window, used to uniquely identify a window on the screen

const BOOL g_EnableVSync = TRUE;



// DIRECTX PROPERTIES
// Direct3D device and swap chain.
ID3D11Device* g_d3dDevice = nullptr;							// for allocating GPU resources such as buffers, textures, shaders, state objects, etc.
ID3D11DeviceContext* g_d3dDeviceContext = nullptr;				// to configure the rendering pipeline and draw geometry
IDXGISwapChain* g_d3dSwapChain = nullptr;						// stores the buffers that are used for rendering data, determines how the buffers are swapped when the rendered image should be presented to the screen

// g_d3dRenderTargetView and g_d3dDepthStencilView variables are used to define the subresource view of the area of a buffer to which we will draw to
// A resource view defines an area of a buffer that can be used for rendering

// Render target view for the back buffer of the swap chain. (color buffer)
ID3D11RenderTargetView* g_d3dRenderTargetView = nullptr;
// Depth/stencil view for use as a depth buffer. (depth/stencil buffer)
ID3D11DepthStencilView* g_d3dDepthStencilView = nullptr;
// A texture to associate to the depth stencil view. (stores depth/stencil buffer)
ID3D11Texture2D* g_d3dDepthStencilBuffer = nullptr;

// Define the functionality of the depth/stencil stages. (used by output-merger stage)
ID3D11DepthStencilState* g_d3dDepthStencilState = nullptr;
// Define the functionality of the rasterizer stage.
ID3D11RasterizerState* g_d3dRasterizerState = nullptr;
D3D11_VIEWPORT g_Viewport = { 0 };



// SPECIFIC TO THIS DEMO
// Vertex buffer data
ID3D11InputLayout* g_d3dInputLayout = nullptr;		// order and type of data that is expected by the vertex shader
//g_d3dVertexBuffer and g_d3dIndexBuffer variables will be used to store the vertex data and the index list that defines the geometry which will be rendered
ID3D11Buffer* g_d3dVertexBuffer = nullptr;			// stores the data for each unique vertex in the geometry
ID3D11Buffer* g_d3dIndexBuffer = nullptr;			// stores a list of indices into the vertex buffer, determines the order that vertices in the vertex buffer are sent to the GPU for rendering

// Shader data
ID3D11VertexShader* g_d3dVertexShader = nullptr;
ID3D11PixelShader* g_d3dPixelShader = nullptr;



// used for updating the constant variables that are declared in the vertex shader
// Shader resources
// Constant buffers are used to store shader variables that remain constant during current draw call
enum ConstantBuffer
{
    CB_Application,     // application level, usually updated once during application startup
    CB_Frame,           // stores variables that change each frame (e.g. camera view matrix)
    CB_Object,          // stores variables that are different for every object being rendered (e.g. object world matrix)
    NumConstantBuffers
};

ID3D11Buffer* g_d3dConstantBuffers[NumConstantBuffers];



// Demo parameters
// updated by the application and used to populate the variables in the constant buffers of the shader
// XMMATRIX: 16-byte aligned 4x4 floating-point matrix
XMMATRIX g_WorldMatrix;
XMMATRIX g_ViewMatrix;          // stores the cameraÅfs view matrix that will transform the objectÅfs vertices from world space into view space, updated once per frame
XMMATRIX g_ProjectionMatrix;    // stores the projection matrix of the camera, updated once at the beginning of the application



// Vertex data for a colored cube.
// properties of a single vertex
// XMFLOAT3: holds three single-precision floating-point values
struct VertexPosColor
{
    XMFLOAT3 Position;
    XMFLOAT3 Color;
};

// 8 vertices for cube geometry
VertexPosColor g_Vertices[8] =
{
    { XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f) }, // 0
    { XMFLOAT3(-1.0f,  1.0f, -1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f) }, // 1
    { XMFLOAT3(1.0f,  1.0f, -1.0f), XMFLOAT3(1.0f, 1.0f, 0.0f) }, // 2
    { XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) }, // 3
    { XMFLOAT3(-1.0f, -1.0f,  1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f) }, // 4
    { XMFLOAT3(-1.0f,  1.0f,  1.0f), XMFLOAT3(0.0f, 1.0f, 1.0f) }, // 5
    { XMFLOAT3(1.0f,  1.0f,  1.0f), XMFLOAT3(1.0f, 1.0f, 1.0f) }, // 6
    { XMFLOAT3(1.0f, -1.0f,  1.0f), XMFLOAT3(1.0f, 0.0f, 1.0f) }  // 7
};

// We cannot simply send the cube geometry directly to the rendering pipeline as-is 
// because the rendering pipeline only knows about points, lines, and triangles
// (not cubes, spheres, or any other complex shape).

// For creating a set of triangles; index list which determines the order the vertices are sent to the GPU for rendering
// each face of the cube consists of two triangles, an upper and a lower triangle
// The first face of the triangle consists of six vertices: { {0, 1, 2}, {0, 2, 3} }
// WORD: a unit of data that represents 16 bits of unsigned memory
WORD g_Indicies[36] =
{
    0, 1, 2, 0, 2, 3,
    4, 6, 5, 4, 7, 6,
    4, 5, 1, 4, 1, 0,
    3, 2, 6, 3, 6, 7,
    1, 5, 6, 1, 6, 2,
    4, 0, 3, 4, 3, 7
};



// FORWARD DECLARATIONS
// will handle any mouse, keyboard, and window events that are sent to our application window
LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

// used to load and compile a shader at runtime, templated on the type of shader that is being loaded
template< class ShaderClass >
ShaderClass* LoadShader(const std::wstring& fileName, const std::string& entryPoint, const std::string& profile);

// load the demo specific resources such as the vertex buffer and index buffer GPU resources for our cube geometry
bool LoadContent();
void UnloadContent();

// used to release any DirectX specific resources like the device, device context, and swap chain
void Update(float deltaTime);
void Render();
void Cleanup();



// Register and create the application window
int InitApplication(HINSTANCE hInstance, int cmdShow)
{
	WNDCLASSEXW wndClass = { 0 };                               // WNDCLASSEXW structure to hold information for registering the window class
	wndClass.cbSize = sizeof(WNDCLASSEXW);                      // size of the structure
	wndClass.style = CS_HREDRAW | CS_VREDRAW;                   // class styles (CS_HREDRAW: redraws the entire window if a movement or size adjustment changes the width of the client area; CS_VREDRAW: redraws the entire window if a movement or size adjustment changes the height of the client area)
	wndClass.lpfnWndProc = WndProc;                             // pointer to the window procedure function that will handle events for windows of this class
	wndClass.hInstance = hInstance;                             // handle to the instance of the module that owns this window class
	wndClass.hCursor = LoadCursor(nullptr, IDC_ARROW);          // handle to the cursor resource to be used by windows of this class
	wndClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);        // handle to the background brush for windows of this class, a system color value + 1 is specified
	wndClass.lpszMenuName = nullptr;                            // pointer to a null-terminated string that specifies the resource name of the class menu, nullptr indicates no menu
	wndClass.lpszClassName = g_WindowClassName;                 // pointer to a null-terminated string that specifies the name of this window class

	// register the window class with the operating system
    if (!RegisterClassEx(&wndClass))
    {
        return -1;
    }

	// calculate the size of the window rectangle based on the desired client area size
    RECT windowRect = { 0, 0, g_WindowWidth, g_WindowHeight };
	// adjust the window rectangle to account for window decorations (title bar, borders, etc.)
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

	// create the application window
    g_WindowHandle = CreateWindowW(
		g_WindowClassName,                      // window class name        
		g_WindowName,                           // window title
		WS_OVERLAPPEDWINDOW,                    // window style, an overlapped window is a typical top-level window with a title bar and border
		CW_USEDEFAULT,                          // initial horizontal position of the window
		CW_USEDEFAULT,                          // initial vertical position of the window
		windowRect.right - windowRect.left,     // width of the window
		windowRect.bottom - windowRect.top,     // height of the window
		nullptr,                                // handle to the parent window, nullptr indicates no parent
		nullptr,                                // handle to the menu, nullptr indicates no menu
		hInstance,                              // handle to the instance of the module creating the window
		nullptr                                 // pointer to window creation data, nullptr indicates no additional data
    );

    if (!g_WindowHandle)
    {
        return -1;
    }

	// display the window on the screen
    ShowWindow(g_WindowHandle, cmdShow);
	// force the window to repaint its client area
    UpdateWindow(g_WindowHandle);

    return 0;
}



// minimum window procedure function that handles messages sent to our application window
LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT paintStruct;    // structure to hold information for painting the window
	HDC hDC;                    // handle to the device context for painting

    switch (message)
    {
	// handle the paint message to repaint the window's client area
    case WM_PAINT:
    {
		hDC = BeginPaint(hwnd, &paintStruct); // prepare the window for painting and retrieve the device context
		EndPaint(hwnd, &paintStruct);         // signal that painting is complete
    }
    break;
	// handle the destroy message to clean up and exit the application
    case WM_DESTROY:
    {
		PostQuitMessage(0); // post a quit message to the message queue to signal application termination
    }
    break;
	// default case to handle any messages not explicitly processed
    default:
        return DefWindowProc(hwnd, message, wParam, lParam);
    }

    return 0;
}



// QUERY REFRESH RATE OF THE MONITOR
// DXGI_RATIONAL: structure that represents a rational number using a numerator and a denominator, e.g., 60Hz is 60/1
DXGI_RATIONAL QueryRefreshRate(UINT screenWidth, UINT screenHeight, BOOL vsync)
{
	DXGI_RATIONAL refreshRate = { 0, 1 };   // default to 0Hz (no refresh)
    if (vsync)
    {
		IDXGIFactory* factory;              // interface for creating DXGI objects
		IDXGIAdapter* adapter;              // interface for representing a graphics adapter (video card)
		IDXGIOutput* adapterOutput;         // interface for representing an output (monitor) on the adapter
		DXGI_MODE_DESC* displayModeList;    // array to hold the list of display modes supported by the output


		// ENUMERATE GRAPHICS ADAPTERS AND OUTPUTS
        

        // Create a DirectX graphics interface factory.
        // starting point for enumerating all graphics cards and displays
        HRESULT hr = CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&factory);
        if (FAILED(hr))
        {
            MessageBox(0,
                TEXT("Could not create DXGIFactory instance."),
                TEXT("Query Refresh Rate"),
                MB_OK);

            throw new std::exception("Failed to create DXGIFactory.");
        }

		// Enumerate the primary adapter (graphics card)
        hr = factory->EnumAdapters(0, &adapter);
        if (FAILED(hr))
        {
            MessageBox(0,
                TEXT("Failed to enumerate adapters."),
                TEXT("Query Refresh Rate"),
                MB_OK);

            throw new std::exception("Failed to enumerate adapters.");
        }

		// Enumerate the primary output (monitor) on the adapter
        hr = adapter->EnumOutputs(0, &adapterOutput);
        if (FAILED(hr))
        {
            MessageBox(0,
                TEXT("Failed to enumerate adapter outputs."),
                TEXT("Query Refresh Rate"),
                MB_OK);

            throw new std::exception("Failed to enumerate adapter outputs.");
        }


		// QUERYING AND READING DISPLAY MODES


		UINT numDisplayModes;   // number of display modes supported by the output

		// First, query the number of display modes with the standart color format and interlaced scanline ordering
		// This determines how many display modes are supported by the monitor
        // numDisplayModes is filled with the count
		// DXGI_FORMAT_B8G8R8A8_UNORM: standard 32-bit color format with 8 bits for blue, green, red, and alpha channels
		// DXGI_ENUM_MODES_INTERLACED: enumerates display modes with interlaced scanline ordering
        hr = adapterOutput->GetDisplayModeList(DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_ENUM_MODES_INTERLACED, &numDisplayModes, nullptr);
        if (FAILED(hr))
        {
            MessageBox(0,
                TEXT("Failed to query display mode list."),
                TEXT("Query Refresh Rate"),
                MB_OK);

            throw new std::exception("Failed to query display mode list.");
        }

		// a dynamic array is allocated on the heap to hold the list of display modes
        displayModeList = new DXGI_MODE_DESC[numDisplayModes];
        assert(displayModeList);

		// Now actually get the display mode list
		// the displayModeList array is filled with the supported display modes each containing width, height, refresh rate, etc.
        hr = adapterOutput->GetDisplayModeList(DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_ENUM_MODES_INTERLACED, &numDisplayModes, displayModeList);
        if (FAILED(hr))
        {
            MessageBox(0,
                TEXT("Failed to query display mode list."),
                TEXT("Query Refresh Rate"),
                MB_OK);

            throw new std::exception("Failed to query display mode list.");
        }


		// SEARCH FOR THE CORRECT REFRESH RATE AND CLEANUP


		// Loop through all the display modes to find the one that matches the requested screen width and height
        for (UINT i = 0; i < numDisplayModes; ++i)
        {
            if (displayModeList[i].Width == screenWidth && displayModeList[i].Height == screenHeight)
            {
                refreshRate = displayModeList[i].RefreshRate;
            }
        }

		// Release the allocated resources
        delete[] displayModeList;
        SafeRelease(adapterOutput);
        SafeRelease(adapter);
        SafeRelease(factory);
    }

    return refreshRate;
}



// INITIALIZE DIRECTX DEVICE AND SWAP CHAIN
int InitDirectX(HINSTANCE hInstance, BOOL vSync)
{
    // A window handle must have been created already.
    assert(g_WindowHandle != 0);

	// store the dimensions of the client area of the window
	RECT clientRect;    
    GetClientRect(g_WindowHandle, &clientRect);

    // Compute the exact client dimensions. This will be used
    // to initialize the render targets for our swap chain.
    unsigned int clientWidth = clientRect.right - clientRect.left;
    unsigned int clientHeight = clientRect.bottom - clientRect.top;

	// Describe the swap chain
    DXGI_SWAP_CHAIN_DESC swapChainDesc;
	ZeroMemory(&swapChainDesc, sizeof(DXGI_SWAP_CHAIN_DESC));   // clear out the structure for use

	swapChainDesc.BufferCount = 1;          // number of buffers in the swap chain
	// BufferDesc describes a display mode
	swapChainDesc.BufferDesc.Width = clientWidth;                   // resolution width
	swapChainDesc.BufferDesc.Height = clientHeight;                 // resolution height
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;   // 32-bit color format (8 bits for red, green, blue, and alpha channels)
	swapChainDesc.BufferDesc.RefreshRate = QueryRefreshRate(clientWidth, clientHeight, vSync);  // refresh rate of the display
	// define how the swap chain's back buffer will be used
	// DXGI_USAGE_RENDER_TARGET_OUTPUT: the buffer can be used as a render target
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;    
    swapChainDesc.OutputWindow = g_WindowHandle;
	// SampleDesc describes multi-sampling parameters
	swapChainDesc.SampleDesc.Count = 1;                     // number of samples per pixel, no multi-sampling
	swapChainDesc.SampleDesc.Quality = 0;                   // quality level of multi-sampling, 0 is the lowest quality
	// SwapEffect specifies how the swap chain should handle presenting the back buffer to the front buffer
	// DXGI_SWAP_EFFECT_DISCARD: the contents of the back buffer are discarded after presenting
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    swapChainDesc.Windowed = TRUE;


	// CREATE THE DIRECT3D DEVICE AND SWAP CHAIN


	UINT createDeviceFlags = 0;     // bitfield that specifies creation options for the Direct3D device
#if _DEBUG
	createDeviceFlags = D3D11_CREATE_DEVICE_DEBUG;  // enable debug layer if in debug mode
#endif

    // These are the feature levels that will be accepted
	// feature levels indicate the set of Direct3D features that the application can use
	// here we are specifying a range from Direct3D 9.1 up to Direct3D 11.1
    D3D_FEATURE_LEVEL featureLevels[] =
    {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_3,
        D3D_FEATURE_LEVEL_9_2,
        D3D_FEATURE_LEVEL_9_1
    };

    // This will be the feature level that 
    // is used to create our device and swap chain.
    D3D_FEATURE_LEVEL featureLevel;

	// Create the Direct3D device and swap chain
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
		nullptr,                    // pointer to the video adapter to use, nullptr indicates the default adapter
		D3D_DRIVER_TYPE_HARDWARE,   // type of driver to create, hardware driver uses the GPU; this is the primary option for performance
		nullptr,                    // handle to a software rasterizer DLL, nullptr indicates no software rasterizer
		createDeviceFlags,          // creation flags for the device
		featureLevels,              // array of feature levels to attempt to create
		_countof(featureLevels),    // number of feature levels in the array
		D3D11_SDK_VERSION,          // SDK version to use
		&swapChainDesc,             // pointer to the swap chain description
		&g_d3dSwapChain,            // pointer to the swap chain interface that will be created
		&g_d3dDevice,               // pointer to the Direct3D device interface that will be created
		&featureLevel,              // pointer to the feature level that was created
		&g_d3dDeviceContext         // pointer to the device context interface that will be created
    );

	// if the call failed due to an invalid argument (e.g., Direct3D 11.1 not supported), retry without Direct3D 11.1
    if (hr == E_INVALIDARG)
    {
		// Retry without D3D11.1 and attempt to create a Direct3D 11.0 device instead
        hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE,
            nullptr, createDeviceFlags, &featureLevels[1], _countof(featureLevels) - 1,
            D3D11_SDK_VERSION, &swapChainDesc, &g_d3dSwapChain, &g_d3dDevice, &featureLevel,
            &g_d3dDeviceContext);
    }

    if (FAILED(hr))
    {
        return -1;
    }


	// CREATE RENDER TARGET VIEW


    // Next initialize the back buffer of the swap chain and associate it to a render target view.
	// the swap chain's back buffer is automatically created based on the content of the DXGI_SWAP_CHAIN_DESC structure when the swap chain is created
    ID3D11Texture2D* backBuffer;
	// GetBuffer retrieves a pointer to one of the swap chain's back buffers
    hr = g_d3dSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&backBuffer);
    if (FAILED(hr))
    {
        return -1;
    }

	// Associate the back buffer with the render target view.
	// CreateRenderTargetView creates a render target view for accessing resource data in the back buffer
    hr = g_d3dDevice->CreateRenderTargetView(
		backBuffer,             // pointer to the resource (back buffer) to create the render target view for
		nullptr,                // pointer to a D3D11_RENDER_TARGET_VIEW_DESC structure that describes the render target view to be created, nullptr indicates default settings
		&g_d3dRenderTargetView  // pointer to the render target view interface that will be created
    );

    if (FAILED(hr))
    {
        return -1;
    }

    SafeRelease(backBuffer);


	// CREATE DEPTH/STENCIL BUFFER


    // Create the depth buffer for use with the depth/stencil view.
    D3D11_TEXTURE2D_DESC depthStencilBufferDesc;
	// Clear out the structure for use.
    ZeroMemory(&depthStencilBufferDesc, sizeof(D3D11_TEXTURE2D_DESC));

	depthStencilBufferDesc.ArraySize = 1;                           // number of textures in the texture array, we only need one texture
    depthStencilBufferDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    depthStencilBufferDesc.CPUAccessFlags = 0; // No CPU access required.
	depthStencilBufferDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;  // texture format for the depth/stencil buffer (24 bits for depth, 8 bits for stencil)
	depthStencilBufferDesc.Width = clientWidth;                     // texture width in texels, match the client area width
	depthStencilBufferDesc.Height = clientHeight;                   // texture height in texels, match the client area height
	depthStencilBufferDesc.MipLevels = 1;                           // maximum number of mipmap levels, use 1 for multisampling
	// SampleDesc describes multi-sampling parameters
	// values of SampleDesc must match those used to create the swap chain
	depthStencilBufferDesc.SampleDesc.Count = 1;                    // number of samples per pixel, no multi-sampling
	depthStencilBufferDesc.SampleDesc.Quality = 0;                  // quality level of multi-sampling, 0 is the lowest quality
	depthStencilBufferDesc.Usage = D3D11_USAGE_DEFAULT;             // expected usage of the texture, GPU read and write access

	// Create the texture for the depth/stencil buffer.
    hr = g_d3dDevice->CreateTexture2D(
		&depthStencilBufferDesc,    // pointer to the texture description
		nullptr,                    // pointer to the initial data for the texture, nullptr indicates no initial data
		&g_d3dDepthStencilBuffer    // pointer to the texture interface that will be created
    );
    if (FAILED(hr))
    {
        return -1;
    }


	// CREATE DEPTH/STENCIL VIEW


    hr = g_d3dDevice->CreateDepthStencilView(
		g_d3dDepthStencilBuffer,    // pointer to the texture resource to create the depth/stencil view for
		nullptr,                    // pointer to a D3D11_DEPTH_STENCIL_VIEW_DESC structure that describes the depth/stencil view to be created, nullptr indicates default settings
		&g_d3dDepthStencilView      // pointer to the depth/stencil view interface that will be created
    );
    if (FAILED(hr))
    {
        return -1;
    }


	// CREATE DEPTH/STENCIL STATE OBJECT


	// Define the depth/stencil state description.
    D3D11_DEPTH_STENCIL_DESC depthStencilStateDesc;
	// Clear out the structure for use.
    ZeroMemory(&depthStencilStateDesc, sizeof(D3D11_DEPTH_STENCIL_DESC));

	depthStencilStateDesc.DepthEnable = TRUE;                           // enable depth testing
	depthStencilStateDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;  // identifies whether depth data can be written to the depth buffer, enable writing
	depthStencilStateDesc.DepthFunc = D3D11_COMPARISON_LESS;            // comparison function for depth testing, pixel passes if its depth is less than the existing depth
	depthStencilStateDesc.StencilEnable = FALSE;                        // disable stencil testing

    hr = g_d3dDevice->CreateDepthStencilState(&depthStencilStateDesc, &g_d3dDepthStencilState);


	// CREATE RASTERIZER STATE OBJECT


	// Define the rasterizer state description.
    D3D11_RASTERIZER_DESC rasterizerDesc;
    ZeroMemory(&rasterizerDesc, sizeof(D3D11_RASTERIZER_DESC));

	rasterizerDesc.AntialiasedLineEnable = FALSE;       // disable anti-aliasing for lines
	rasterizerDesc.CullMode = D3D11_CULL_BACK;          // cull back-facing triangles
	rasterizerDesc.DepthBias = 0;                       // no depth bias
	rasterizerDesc.DepthBiasClamp = 0.0f;               // maximum depth bias of 0
	rasterizerDesc.DepthClipEnable = TRUE;              // enable clipping based on distance
	rasterizerDesc.FillMode = D3D11_FILL_SOLID;         // fill mode, solid fill
	rasterizerDesc.FrontCounterClockwise = FALSE;       // determines the winding order of front-facing triangles, clockwise vertices are front-facing
	rasterizerDesc.MultisampleEnable = FALSE;           // disable multi-sampling
	rasterizerDesc.ScissorEnable = FALSE;               // disable scissor-rectangle culling
	rasterizerDesc.SlopeScaledDepthBias = 0.0f;         // scale factor for depth bias based on triangle slope, no slope-based depth bias

    // Create the rasterizer state object.
    hr = g_d3dDevice->CreateRasterizerState(&rasterizerDesc, &g_d3dRasterizerState);

    if (FAILED(hr))
    {
        return -1;
    }


	// INITIALIZE THE VIEWPORT


    g_Viewport.Width = static_cast<float>(clientWidth);
    g_Viewport.Height = static_cast<float>(clientHeight);
    g_Viewport.TopLeftX = 0.0f;
    g_Viewport.TopLeftY = 0.0f;
    g_Viewport.MinDepth = 0.0f;
    g_Viewport.MaxDepth = 1.0f;

    return 0;
}


// LOAD DEMO CONTENT (SHADERS, GEOMETRY, TEXTURES, ETC.)
bool LoadContent()
{
    assert(g_d3dDevice);


	// CREATE VERTEX BUFFER
    

	// Create the vertex buffer description.
    D3D11_BUFFER_DESC vertexBufferDesc;
    ZeroMemory(&vertexBufferDesc, sizeof(D3D11_BUFFER_DESC));

	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;                          // type of buffer, vertex buffer
	vertexBufferDesc.ByteWidth = sizeof(VertexPosColor) * _countof(g_Vertices);     // size of the buffer in bytes
	vertexBufferDesc.CPUAccessFlags = 0;                                            // no CPU access required
	vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;                                   // expected usage of the buffer, GPU read and write access

	// Provide the initial data for the vertex buffer.
    D3D11_SUBRESOURCE_DATA resourceData;
    ZeroMemory(&resourceData, sizeof(D3D11_SUBRESOURCE_DATA));

	resourceData.pSysMem = g_Vertices;  // pointer to the vertex data to initialize the buffer with

	// Create the vertex buffer which can be bound to the input assembler stage of the rendering pipeline to render geometry.
    HRESULT hr = g_d3dDevice->CreateBuffer(
		&vertexBufferDesc,      // pointer to the buffer description
		&resourceData,          // pointer to the initial data for the buffer
		&g_d3dVertexBuffer      // pointer to the buffer interface that will be created
    );

    if (FAILED(hr))
    {
        return false;
    }


	// CREATE INDEX BUFFER


	// Create the index buffer description.
    D3D11_BUFFER_DESC indexBufferDesc;
    ZeroMemory(&indexBufferDesc, sizeof(D3D11_BUFFER_DESC));

	indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;                // type of buffer, index buffer      
    indexBufferDesc.ByteWidth = sizeof(WORD) * _countof(g_Indicies);
    indexBufferDesc.CPUAccessFlags = 0;
    indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;

    resourceData.pSysMem = g_Indicies;

    hr = g_d3dDevice->CreateBuffer(&indexBufferDesc, &resourceData, &g_d3dIndexBuffer);
    if (FAILED(hr))
    {
        return false;
    }


    // CONSTANT BUFFERS


    // Create the constant buffers for the variables defined in the vertex shader.
    D3D11_BUFFER_DESC constantBufferDesc;
    ZeroMemory(&constantBufferDesc, sizeof(D3D11_BUFFER_DESC));

	constantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;  // type of buffer, constant buffer
    constantBufferDesc.ByteWidth = sizeof(XMMATRIX);
    constantBufferDesc.CPUAccessFlags = 0;
    constantBufferDesc.Usage = D3D11_USAGE_DEFAULT;

	// application constant buffer
    hr = g_d3dDevice->CreateBuffer(&constantBufferDesc, nullptr, &g_d3dConstantBuffers[CB_Application]);
    if (FAILED(hr))
    {
        return false;
    }
	// frame constant buffer
    hr = g_d3dDevice->CreateBuffer(&constantBufferDesc, nullptr, &g_d3dConstantBuffers[CB_Frame]);
    if (FAILED(hr))
    {
        return false;
    }
	// object constant buffer
    hr = g_d3dDevice->CreateBuffer(&constantBufferDesc, nullptr, &g_d3dConstantBuffers[CB_Object]);
    if (FAILED(hr))
    {
        return false;
    }


	// LOAD SHADERS


    // Load the compiled vertex shader.
	// vertexShaderBlob will hold the compiled shader bytecode
    ID3DBlob* vertexShaderBlob;
#if _DEBUG
    LPCWSTR compiledVertexShaderObject = L"data\\Shaders\\SimpleVertexShader_d.cso";
#else
    LPCWSTR compiledVertexShaderObject = L"data\\Shaders\\SimpleVertexShader.cso";
#endif

	// D3DReadFileToBlob reads the compiled shader object file into a blob
    hr = D3DReadFileToBlob(compiledVertexShaderObject, &vertexShaderBlob);
    if (FAILED(hr))
    {
        return false;
    }

	// Create the vertex shader from the compiled shader bytecode.
    hr = g_d3dDevice->CreateVertexShader(
		vertexShaderBlob->GetBufferPointer(),   // pointer to the compiled shader bytecode
		vertexShaderBlob->GetBufferSize(),      // size of the compiled shader bytecode
		nullptr,                                // optional class linkage for shader interfaces, nullptr indicates no linkage
		&g_d3dVertexShader                      // pointer to the vertex shader interface that will be created
    );

    if (FAILED(hr))
    {
        return false;
    }


	// INPUT LAYOUT


	// The input layout interface is used to define how the vertex data attached to the input assembler stage is laid out in memory.
	// Define the vertex input layout description.
	// each element describes a single attribute in the vertex buffer that is bound to the input assembler stage
    D3D11_INPUT_ELEMENT_DESC vertexLayoutDesc[] =
    {
        { 
			"POSITION",                         // semantic name, matches the semantic in the vertex shader
			0,                                  // semantic index, used if there are multiple elements with the same semantic name
			DXGI_FORMAT_R32G32B32_FLOAT,        // data format of the element, here it's a 3-component float vector
			0,                                  // input slot, identifies the input assembler slot the data comes from, 0 if only one vertex buffer is used
			offsetof(VertexPosColor,Position),  // byte offset of this element in the vertex structure, VertexPosColor.Position is used here (vertex position of the cube's geometry)
			D3D11_INPUT_PER_VERTEX_DATA,        // indicates this data is per-vertex (not per-instance)
			0                                   // instance data step rate, not used for per-vertex data
        },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(VertexPosColor,Color), D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

	// Create the input layout object.
    hr = g_d3dDevice->CreateInputLayout(
		vertexLayoutDesc,                       // pointer to the array of input element descriptions which are the input-assembler stage input data types
		_countof(vertexLayoutDesc),             // number of input elements in the vertexLayoutDesc array        
		vertexShaderBlob->GetBufferPointer(),   // pointer to the compiled vertex shader bytecode which contains the input signature valided against the array elements
		vertexShaderBlob->GetBufferSize(),      // size of the compiled vertex shader bytecode
		&g_d3dInputLayout                       // pointer to the input layout interface that will be created
    );

    if (FAILED(hr))
    {
        return false;
    }

	// Release the vertex shader blob as it is no longer needed.
    SafeRelease(vertexShaderBlob);


	// LOAD PIXEL SHADER


	// Load the compiled pixel shader. No need to define an input layout for the pixel shader.
    ID3DBlob* pixelShaderBlob;
#if _DEBUG
    LPCWSTR compiledPixelShaderObject = L"data\\Shaders\\SimplePixelShader_d.cso";
#else
    LPCWSTR compiledPixelShaderObject = L"data\\Shaders\\SimplePixelShader.cso";
#endif

    hr = D3DReadFileToBlob(compiledPixelShaderObject, &pixelShaderBlob);
    if (FAILED(hr))
    {
        return false;
    }

    hr = g_d3dDevice->CreatePixelShader(pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize(), nullptr, &g_d3dPixelShader);
    if (FAILED(hr))
    {
        return false;
    }

    SafeRelease(pixelShaderBlob);


	// PROJECTION MATRIX


    // Setup the projection matrix.
	// Get the client rectangle size.
    RECT clientRect;
    GetClientRect(g_WindowHandle, &clientRect);

    // Compute the exact client dimensions.
    // This is required for a correct projection matrix.
    float clientWidth = static_cast<float>(clientRect.right - clientRect.left);
    float clientHeight = static_cast<float>(clientRect.bottom - clientRect.top);

	// Create a perspective projection matrix with a 45-degree field of view.
	// XMMatrixPerspectiveFovLH creates a left-handed perspective projection matrix based on a field of view
    g_ProjectionMatrix = XMMatrixPerspectiveFovLH(
		XMConvertToRadians(45.0f),      // field of view in radians
		clientWidth / clientHeight,     // aspect ratio
		0.1f,                           // near clipping plane
		100.0f                          // far clipping plane
    );

	// Update the application constant buffer with the projection matrix.
    g_d3dDeviceContext->UpdateSubresource(
		g_d3dConstantBuffers[CB_Application],   // destination resource
		0,                                      // destination subresource, 0 for non-texture resources
		nullptr,                                // pointer to the source data, nullptr indicates the entire resource is updated
		&g_ProjectionMatrix,                    // pointer to the data to copy (the projection matrix)
		0,                                      // source row pitch, not used for non-texture resources
		0                                       // source depth pitch, not used for non-texture resources
    );

    return true;
}



// MAIN APPLICATION LOOP
int Run()
{
    MSG msg = { 0 };    // structure to hold message information

    static DWORD previousTime = timeGetTime();                  // get the current time in milliseconds
    static const float targetFramerate = 30.0f;                 // target framerate of 30 frames per second
    static const float maxTimeStep = 1.0f / targetFramerate;    // maximum time step per frame

    while (msg.message != WM_QUIT)
    {
        // retreive the next message from the message queue
        // PR_REMOVE: removes the message from the queue after it has been retrieved
        if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);     // translates virtual-key messages into character messages
            DispatchMessage(&msg);      // dispatches the message to the window procedure for processing
        }
        else
        {
            DWORD currentTime = timeGetTime();
            float deltaTime = (currentTime - previousTime) / 1000.0f;   // calculate the time elapsed since the last frame in seconds
            previousTime = currentTime;                                 // update the previous time to the current time

            // Cap the delta time to the max time step (useful if your 
            // debugging and you don't want the deltaTime value to explode.
            deltaTime = std::min<float>(deltaTime, maxTimeStep);

            Update( deltaTime );
            Render();
        }
    }

    // return the exit code from the quit message
    return static_cast<int>(msg.wParam);
}



// ENTRY POINT FOR WINDOWS APPLICATION
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE prevInstance, LPWSTR cmdLine, int cmdShow)
{
    UNREFERENCED_PARAMETER(prevInstance);   // not used
    UNREFERENCED_PARAMETER(cmdLine);        // not used

    // Check for DirectX Math library support.
    if (!XMVerifyCPUSupport())
    {
        MessageBox(nullptr, TEXT("Failed to verify DirectX Math library support."), TEXT("Error"), MB_OK);
        return -1;
    }

    // Initialize the application window.
    if (InitApplication(hInstance, cmdShow) != 0)
    {
        MessageBox(nullptr, TEXT("Failed to create applicaiton window."), TEXT("Error"), MB_OK);
        return -1;
    }

    // Initialize DirectX.
    if (InitDirectX(hInstance, g_EnableVSync) != 0)
    {
        MessageBox(nullptr, TEXT("Failed to create DirectX device and swap chain."), TEXT("Error"), MB_OK);
        return -1;
    }

	// Load the demo content (shaders, geometry, textures, etc.).
    if (!LoadContent())
    {
        MessageBox(nullptr, TEXT("Failed to load content."), TEXT("Error"), MB_OK);
        return -1;
    }

    // Run the main application loop.
    int returnCode = Run();

    return returnCode;
}



// UPDATE SCENE
// set up the camera view matrix and update the world matrix to rotate the cube over time
void Update(float deltaTime)
{
	XMVECTOR eyePosition = XMVectorSet(0, 0, -10, 1);   // camera position in world space
	XMVECTOR focusPoint = XMVectorSet(0, 0, 0, 1);      // point the camera is looking at
	XMVECTOR upDirection = XMVectorSet(0, 1, 0, 0);     // up direction for the camera

	// create the view matrix using a left-handed coordinate system
    g_ViewMatrix = XMMatrixLookAtLH(eyePosition, focusPoint, upDirection);

	// update the frame constant buffer with the view matrix
    g_d3dDeviceContext->UpdateSubresource(g_d3dConstantBuffers[CB_Frame], 0, nullptr, &g_ViewMatrix, 0, 0);


	// update the world matrix to rotate the cube over time
	static float angle = 0.0f;                          // rotation angle in degrees
	angle += 90.0f * deltaTime;                         // rotate 90 degrees per second
	XMVECTOR rotationAxis = XMVectorSet(0, 1, 1, 0);    // axis of rotation (y and z axes)

	// create the world matrix as a rotation around the specified axis
    g_WorldMatrix = XMMatrixRotationAxis(rotationAxis, XMConvertToRadians(angle));

	// update the object constant buffer with the world matrix
    g_d3dDeviceContext->UpdateSubresource(g_d3dConstantBuffers[CB_Object], 0, nullptr, &g_WorldMatrix, 0, 0);
}



// HELPER FUNCTION TO CLEAR THE RENDER TARGET AND DEPTH/ STENCIL BUFFER
// Clear the color and depth buffers.
void Clear(const FLOAT clearColor[4], FLOAT clearDepth, UINT8 clearStencil)
{
    g_d3dDeviceContext->ClearRenderTargetView(g_d3dRenderTargetView, clearColor);
	// clearDepth and clearStencil specify the values to clear the depth and stencil buffers to
    g_d3dDeviceContext->ClearDepthStencilView(g_d3dDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, clearDepth, clearStencil);
}



// HELPER FUNCTION TO PRESENT THE BACK BUFFER TO THE SCREEN
void Present(bool vSync)
{
	// Present method presents the back buffer to the front buffer (screen)
	// first parameter specifies the sync interval
	// second parameter specifies presentation options
    if (vSync)
    {
        g_d3dSwapChain->Present(1, 0);
    }
    else
    {
        g_d3dSwapChain->Present(0, 0);
    }
}



// RENDER THE GEOMETRY
void Render()
{
    assert(g_d3dDevice);
    assert(g_d3dDeviceContext);

	// Clear the back buffer and depth/stencil buffer with a cornflower blue color and default depth/stencil values
    Clear(Colors::CornflowerBlue, 1.0f, 0);


	// SET UP INPUT ASSEMBLER STAGE
    

	// Set the vertex buffer, input layout, index buffer, and primitive topology for the input assembler stage.
    
	const UINT vertexStride = sizeof(VertexPosColor);   // size of each vertex in the vertex buffer
	const UINT offset = 0;                              // offset in the vertex buffer to start reading from

	// bind the vertex buffer to the input assembler stage
    g_d3dDeviceContext->IASetVertexBuffers(
		0,                      // start slot for binding the vertex buffer
		1,                      // number of vertex buffers to bind
		&g_d3dVertexBuffer,     // pointer to the vertex buffer interface
		&vertexStride,          // pointer to the size of each vertex
		&offset                 // pointer to the offset to start reading from
    );

	// set the input layout for interpreting the vertex data
    g_d3dDeviceContext->IASetInputLayout(g_d3dInputLayout);

	// bind the index buffer to the input assembler stage
    g_d3dDeviceContext->IASetIndexBuffer(
		g_d3dIndexBuffer,       // pointer to the index buffer interface
		DXGI_FORMAT_R16_UINT,   // format of the indices in the index buffer (16-bit unsigned integers)
		0                       // offset in the index buffer to start reading from
    );

	// set the primitive topology to triangle list (each group of three indices represents a separate triangle)
	// D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST: primitive topology where each group of three vertices forms an independent triangle
    g_d3dDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);


	// SET UP VERTEX SHADER

	// Bind the vertex shader to the vertex shader stage
    g_d3dDeviceContext->VSSetShader(
		g_d3dVertexShader,  // pointer to the vertex shader interface
		nullptr,            // optional class linkage for shader interfaces, nullptr indicates no linkage
		0                   // number of class instances, 0 indicates no instances
    );  

	// Bind the constant buffers to the vertex shader stage
    g_d3dDeviceContext->VSSetConstantBuffers(
		0,                      // start slot for binding the constant buffers
		3,                      // number of constant buffers to bind
		g_d3dConstantBuffers    // pointer to the array of constant buffer interfaces
    );


	// SET UP RASTERIZER STAGE


	// the rasterizer stage is responsible for interpolating vertex attributes output from the vertex shader and invoking the pixel shader program for each screen pixel which is affected by the rendered geometry
	// set the rasterizer state 
    g_d3dDeviceContext->RSSetState(g_d3dRasterizerState);

	// set the viewport for the rasterizer stage
    g_d3dDeviceContext->RSSetViewports(
		1,              // number of viewports to set
		&g_Viewport     // pointer to the viewport structure
    );


	// SET UP PIXEL SHADER


	// Bind the pixel shader to the pixel shader stage
    g_d3dDeviceContext->PSSetShader(
		g_d3dPixelShader,   // pointer to the pixel shader interface
		nullptr,            // optional class linkage for shader interfaces, nullptr indicates no linkage
		0                   // number of class instances, 0 indicates no instances
    );


	// SET UP OUTPUT MERGER STAGE


	// the output merger stage is responsible for combining various outputs from the rendering pipeline into the final render target (back buffer) and depth/stencil buffer
	// set the render target for the output merger stage
    g_d3dDeviceContext->OMSetRenderTargets(
		1,                          // number of render target views to set
		&g_d3dRenderTargetView,     // pointer to the array of render target view interfaces
		g_d3dDepthStencilView       // pointer to the depth/stencil view interface
    );

	// set the depth/stencil state for the output merger stage
    g_d3dDeviceContext->OMSetDepthStencilState(
		g_d3dDepthStencilState,     // pointer to the depth/stencil state interface
		1                           // stencil reference value used for stencil testing
    );


	// DRAW THE GEOMETRY

	// Draw the indexed geometry (the cube).
	// DrawIndexed issues a draw call to render indexed primitives
    g_d3dDeviceContext->DrawIndexed(
		_countof(g_Indicies),   // number of indices to draw
		0,                      // starting index location
		0                       // base vertex location (offset added to each index in the index buffer)
    );


	// PRESENT THE BACK BUFFER TO THE SCREEN


    Present(g_EnableVSync);
}



// CLEANUP

// Release all game-specific assets
void UnloadContent()
{
    SafeRelease(g_d3dConstantBuffers[CB_Application]);
    SafeRelease(g_d3dConstantBuffers[CB_Frame]);
    SafeRelease(g_d3dConstantBuffers[CB_Object]);
    SafeRelease(g_d3dIndexBuffer);
    SafeRelease(g_d3dVertexBuffer);
    SafeRelease(g_d3dInputLayout);
    SafeRelease(g_d3dVertexShader);
    SafeRelease(g_d3dPixelShader);
}


// Release all core system resources
void Cleanup()
{
    SafeRelease(g_d3dDepthStencilView);
    SafeRelease(g_d3dRenderTargetView);
    SafeRelease(g_d3dDepthStencilBuffer);
    SafeRelease(g_d3dDepthStencilState);
    SafeRelease(g_d3dRasterizerState);
    SafeRelease(g_d3dSwapChain);
    SafeRelease(g_d3dDeviceContext);
    SafeRelease(g_d3dDevice);
}