#pragma once
#include <entt/entt.hpp>
#include <string>
#include "Engine/Scene.h"
#include "Engine/Components.h"

namespace Engine
{
    class ScriptEntity
    {
    public:
        ScriptEntity(entt::entity entity, Scene* scene)
            : m_entity(entity), m_scene(scene)
        {
        }

        TransformComponent& GetTransform()
        {
            return m_scene->registry.get<TransformComponent>(m_entity);
        }

        std::string GetName()
        {
            if (m_scene->registry.all_of<NameComponent>(m_entity))
            {
                return m_scene->registry.get<NameComponent>(m_entity).name;
            }
            return "Unknown";
        }

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
        entt::entity m_entity;
        Scene* m_scene;
    };
}