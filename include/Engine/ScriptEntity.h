#pragma once
#include <entt/entt.hpp>
#include <string>
#include "Engine/Scene.h"
#include "Engine/Components.h"

// ScriptEntity is a wrapper around an EnTT entity that provides convenient access to components and scene context for scripting purposes. 
// It allows Lua scripts to interact with entities without needing direct access to the registry or scene internals.

namespace Engine
{
    class ScriptEntity
    {
    public:
		// Constructor takes an EnTT entity and a pointer to the scene for context
        ScriptEntity(entt::entity entity, Scene* scene)
            : m_entity(entity), m_scene(scene)
        {
        }

		// Accessor for the TransformComponent, which is commonly used for position/rotation/scale
        TransformComponent& GetTransform()
        {
            return m_scene->registry.get<TransformComponent>(m_entity);
        }

		// Accessor for the NameComponent, returns the name or "Unknown" if not present
        std::string GetName()
        {
            if (m_scene->registry.all_of<NameComponent>(m_entity))
            {
                return m_scene->registry.get<NameComponent>(m_entity).name;
            }
            return "Unknown";
        }

		// Method to apply a linear impulse to the entity's rigid body, if it has one
        void ApplyLinearImpulse(float x, float y, float z)
        {
            if (m_scene && m_scene->GetPhysicsManager() && m_scene->registry.all_of<RigidBodyComponent>(m_entity)) {
                auto& rb = m_scene->registry.get<RigidBodyComponent>(m_entity);
                if (!rb.bodyID.IsInvalid()) {
                    m_scene->GetPhysicsManager()->GetBodyInterface().AddImpulse(rb.bodyID, JPH::Vec3(x, y, z));
                }
            }
        }

    private:
		entt::entity m_entity;  // The EnTT entity ID that this ScriptEntity wraps
		Scene* m_scene;         // Pointer to the scene for accessing the registry and physics manager
    };
}