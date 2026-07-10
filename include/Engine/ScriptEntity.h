#pragma once
#include <entt/entt.hpp>
#include <string>
#include "Engine/AssetManager.h"
#include "Engine/Scene.h"
#include "Engine/Components.h"
#include "Engine/PhysicsManager.h"
#include "Engine/AudioManager.h"

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

        // Spawning API
        ScriptEntity Instantiate(const std::string& name)
        {
            if (m_scene) return ScriptEntity(m_scene->CreateEntity(name), m_scene);
            return ScriptEntity(entt::null, nullptr);
        }

        ScriptEntity InstantiateCube(const std::string& name)
        {
            if (m_scene) return ScriptEntity(m_scene->CreateCube(name), m_scene);
            return ScriptEntity(entt::null, nullptr);
        }

        ScriptEntity InstantiateSphere(const std::string& name)
        {
            if (m_scene) return ScriptEntity(m_scene->CreateSphere(name), m_scene);
            return ScriptEntity(entt::null, nullptr);
        }

        // Destruction API
        void Destroy()
        {
            if (m_scene) m_scene->SubmitForDestruction(m_entity);
        }

        // Component Addition API
        void AddMeshRenderer(MeshType type)
        {
            if (m_scene && m_scene->registry.valid(m_entity)) {
                auto& mr = m_scene->registry.get_or_emplace<MeshRendererComponent>(m_entity);

                if (type == MeshType::Cube) mr.meshID = m_scene->GetCubeMeshID();
                else if (type == MeshType::Sphere) mr.meshID = m_scene->GetSphereMeshID();
                else if (type == MeshType::Capsule) mr.meshID = m_scene->GetCapsuleMeshID();

                mr.matType = MaterialType::LitColor;
            }
        }

        void AddRigidBody(RBShape shape, RBMotion motion, float size, float mass, float restitution)
        {
            if (m_scene && m_scene->registry.valid(m_entity)) {
                auto& rb = m_scene->registry.get_or_emplace<RigidBodyComponent>(m_entity);
                rb.shape = shape;
                rb.motionType = motion;
                rb.mass = mass;
                rb.restitution = restitution;

                // Map the 'size' parameter appropriately based on the selected shape
                if (shape == RBShape::Sphere) {
                    rb.radius = size;
                }
                else if (shape == RBShape::Box) {
                    rb.halfExtent = { size, size, size };
                }
                else if (shape == RBShape::Capsule) {
                    rb.radius = size;
                    rb.height = size * 2.0f; // Calculate a sensible default height based on radius
                }

                // If it already had a body, remove it so Jolt generates a fresh one next frame
                if (!rb.bodyID.IsInvalid() && m_scene->GetPhysicsManager()) {
                    m_scene->GetPhysicsManager()->RemoveRigidBody(rb.bodyID);
                }
                rb.bodyID = JPH::BodyID();
                rb.bodyCreated = false;
            }
        }


		// SCRIPTING API

        void AddLuaScript(const std::string& filepath)
        {
            if (m_scene && m_scene->registry.valid(m_entity)) {
                auto& scriptComp = m_scene->registry.get_or_emplace<LuaScriptComponent>(m_entity);
                ScriptInstance newScript;
                newScript.filepath = filepath;
                scriptComp.scripts.push_back(newScript);
            }
        }


		// AUDIO API

        void AddAudioComponent(const std::string& filepath, bool is3D, bool loop, bool playOnCreate)
        {
            if (m_scene && m_scene->registry.valid(m_entity)) {
                auto& ac = m_scene->registry.get_or_emplace<AudioComponent>(m_entity);

                // Translate the human-readable string into a UUID 
                if (m_scene->GetAssetManager()) {
                    ac.audioID = m_scene->GetAssetManager()->ImportAsset(filepath, Engine::AssetType::Audio);
                }

                ac.is3D = is3D;
                ac.loop = loop;
                ac.playOnCreate = playOnCreate;
            }
        }

        void PlayAudio()
        {
            if (m_scene && m_scene->registry.valid(m_entity) && m_scene->registry.all_of<AudioComponent>(m_entity)) {
                auto& ac = m_scene->registry.get<AudioComponent>(m_entity);
                if (m_scene->GetAudioManager() && ac.soundHandle) {
                    // Force restart if already playing
                    if (ac.isPlaying) m_scene->GetAudioManager()->StopAudio(ac.soundHandle);
                    m_scene->GetAudioManager()->PlayAudio(ac.soundHandle);
                    ac.isPlaying = true;
                }
                else if (!ac.soundHandle && ac.audioID != 0 && m_scene->GetAudioManager() && m_scene->GetAssetManager()) {
                    // Load and play immediately if not loaded yet (Pass AssetManager for UUID lookup)
                    ac.soundHandle = m_scene->GetAudioManager()->LoadSound(ac.audioID, *m_scene->GetAssetManager(), ac.is3D, ac.loop);
                    if (ac.soundHandle) {
                        m_scene->GetAudioManager()->PlayAudio(ac.soundHandle);
                        ac.isPlaying = true;
                    }
                }
            }
        }

        void StopAudio()
        {
            if (m_scene && m_scene->registry.valid(m_entity) && m_scene->registry.all_of<AudioComponent>(m_entity)) {
                auto& ac = m_scene->registry.get<AudioComponent>(m_entity);
                if (m_scene->GetAudioManager() && ac.soundHandle && ac.isPlaying) {
                    m_scene->GetAudioManager()->StopAudio(ac.soundHandle);
                    ac.isPlaying = false;
                }
            }
        }

        void SetAudioVolume(float volume)
        {
            if (m_scene && m_scene->registry.valid(m_entity) && m_scene->registry.all_of<AudioComponent>(m_entity)) {
                auto& ac = m_scene->registry.get<AudioComponent>(m_entity);
                // Clamp mathematically to prevent script errors pushing negative volumes
                ac.volume = std::max(0.0f, std::min(volume, 1.0f));
            }
        }

    private:
		entt::entity m_entity;  // The EnTT entity ID that this ScriptEntity wraps
		Scene* m_scene;         // Pointer to the scene for accessing the registry and physics manager
    };
}