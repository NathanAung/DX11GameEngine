#pragma once
#include <string>
#include <entt/entt.hpp>
#include "Engine/Components.h"

namespace Engine
{
    class Scene
    {
    public:
        // Exposed by design to allow direct registry access as requested
        entt::registry registry;

        // Create a generic entity with ID, Name and default Transform
        entt::entity CreateEntity(const std::string& name);

        // Create a cube entity with MeshRenderer and visible transform
        entt::entity CreateCube(const std::string& name);
    };
}