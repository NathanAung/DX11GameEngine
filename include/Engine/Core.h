#pragma once

// Engine Core Header File
// This file includes all necessary headers and defines common structures and global variables for the DirectX 11 engine.

// C++ Standard Library
#include <cstdio>       // For std::fprintf
#include <stdexcept>    // For std::runtime_error
#include <string>       // For std::string
#include <vector>       // For std::vector
#include <cstdint>      // For uint16_t

// SDL2
#include <SDL.h>
#include <SDL_syswm.h>

// DirectX 11
#include <d3d11.h>
#include <dxgi.h>
#include <d3dcompiler.h>
#include <wrl/client.h>      // For Microsoft::WRL::ComPtr
#include <DirectXMath.h>

// Jolt Physics
#include "Engine/PhysicsManager.h"

// Link necessary libraries (Microsoft-specific)
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib")