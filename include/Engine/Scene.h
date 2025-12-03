#pragma once
#include <string>
#include <entt/entt.hpp>
#include "Engine/Components.h"

// Scene class manages entities and components using EnTT ECS

namespace Engine
{
    class Scene
    {
    public:
        // Exposed by design to allow direct registry access as requested
        entt::registry registry;

        // Active camera entity used for rendering
        entt::entity m_activeRenderCamera = entt::null;

        // Create a generic entity with ID, Name and default Transform
        entt::entity CreateEntity(const std::string& name);

        // Create a cube entity with MeshRenderer and visible transform
        entt::entity CreateCube(const std::string& name);

        // New: Create camera entity and set as active if none set
        entt::entity CreateCamera(const std::string& name, unsigned width, unsigned height);
    };
}