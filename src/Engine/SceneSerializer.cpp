#include "Engine/SceneSerializer.h"
#include "Engine/Scene.h"
#include "Engine/Components.h"

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>
#include <fstream>
#include <iostream>

namespace Engine
{
    bool SceneSerializer::Serialize(const std::string& filepath, Scene& scene)
    {
        rapidjson::StringBuffer sb;
        rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(sb);

        writer.StartObject();
        writer.Key("Scene");
        writer.StartArray();

        // Iterate through all entities in the registry
		auto& registry = scene.registry;
        for (auto entity : registry.view<entt::entity>())
            {
                // Skip the Editor Camera so it doesn't get saved into the game level
                if (registry.all_of<EditorCamControlComponent>(entity)) continue;

                // Every valid entity must have an ID
                if (!registry.all_of<IDComponent>(entity)) continue;

                writer.StartObject();

                // ID Component
                writer.Key("EntityID");
                writer.Uint64(registry.get<IDComponent>(entity).id);

                // Name Component
                if (registry.all_of<NameComponent>(entity)) {
                    auto& name = registry.get<NameComponent>(entity);
                    writer.Key("NameComponent");
                    writer.StartObject();
                    writer.Key("Name"); writer.String(name.name.c_str());
                    writer.Key("IsActive"); writer.Bool(name.isActive);
                    writer.EndObject();
                }

                // Transform Component
                if (registry.all_of<TransformComponent>(entity)) {
                    auto& tc = registry.get<TransformComponent>(entity);
                    writer.Key("TransformComponent");
                    writer.StartObject();
                    writer.Key("Position");
                    writer.StartArray(); writer.Double(tc.position.x); writer.Double(tc.position.y); writer.Double(tc.position.z); writer.EndArray();
                    writer.Key("Rotation");
                    writer.StartArray(); writer.Double(tc.rotation.x); writer.Double(tc.rotation.y); writer.Double(tc.rotation.z); writer.Double(tc.rotation.w); writer.EndArray();
                    writer.Key("Scale");
                    writer.StartArray(); writer.Double(tc.scale.x); writer.Double(tc.scale.y); writer.Double(tc.scale.z); writer.EndArray();
                    writer.EndObject();
                }

                // MeshRenderer Component
                if (registry.all_of<MeshRendererComponent>(entity)) {
                    auto& mr = registry.get<MeshRendererComponent>(entity);
                    writer.Key("MeshRendererComponent");
                    writer.StartObject();
                    writer.Key("IsActive"); writer.Bool(mr.isActive);
                    writer.Key("MeshID"); writer.Uint64(mr.meshID);
                    writer.Key("TextureID"); writer.Uint64(mr.textureID);
                    writer.Key("MatType"); writer.Int(static_cast<int>(mr.matType));
                    writer.Key("BaseColor");
                    writer.StartArray(); writer.Double(mr.baseColor.x); writer.Double(mr.baseColor.y); writer.Double(mr.baseColor.z); writer.Double(mr.baseColor.w); writer.EndArray();
                    writer.Key("Roughness"); writer.Double(mr.roughness);
                    writer.Key("Metallic"); writer.Double(mr.metallic);
                    writer.EndObject();
                }

                // RigidBody Component
                if (registry.all_of<RigidBodyComponent>(entity)) {
                    auto& rb = registry.get<RigidBodyComponent>(entity);
                    writer.Key("RigidBodyComponent");
                    writer.StartObject();
                    writer.Key("IsActive"); writer.Bool(rb.isActive);
                    writer.Key("Shape"); writer.Int(static_cast<int>(rb.shape));
                    writer.Key("MotionType"); writer.Int(static_cast<int>(rb.motionType));
                    writer.Key("Mass"); writer.Double(rb.mass);
                    writer.Key("Friction"); writer.Double(rb.friction);
                    writer.Key("Restitution"); writer.Double(rb.restitution);
                    writer.Key("LinearDamping"); writer.Double(rb.linearDamping);
                    writer.Key("HalfExtent");
                    writer.StartArray(); writer.Double(rb.halfExtent.x); writer.Double(rb.halfExtent.y); writer.Double(rb.halfExtent.z); writer.EndArray();
                    writer.Key("Radius"); writer.Double(rb.radius);
                    writer.Key("Height"); writer.Double(rb.height);
                    writer.Key("ColliderScale");
                    writer.StartArray(); writer.Double(rb.colliderScale.x); writer.Double(rb.colliderScale.y); writer.Double(rb.colliderScale.z); writer.EndArray();
                    writer.Key("MeshID"); writer.Uint64(rb.meshID);
                    writer.Key("ShowWireframe"); writer.Bool(rb.showWireframe);
                    writer.EndObject();
                }

                // Light Component
                if (registry.all_of<LightComponent>(entity)) {
                    auto& lc = registry.get<LightComponent>(entity);
                    writer.Key("LightComponent");
                    writer.StartObject();
                    writer.Key("IsActive"); writer.Bool(lc.isActive);
                    writer.Key("Color");
                    writer.StartArray(); writer.Double(lc.color.x); writer.Double(lc.color.y); writer.Double(lc.color.z); writer.EndArray();
                    writer.Key("Intensity"); writer.Double(lc.intensity);
                    writer.Key("Type"); writer.Int(static_cast<int>(lc.type));
                    writer.Key("Range"); writer.Double(lc.range);
                    writer.Key("SpotAngle"); writer.Double(lc.spotAngle);
                    writer.EndObject();
                }

                // Audio Component
                if (registry.all_of<AudioComponent>(entity)) {
                    auto& ac = registry.get<AudioComponent>(entity);
                    writer.Key("AudioComponent");
                    writer.StartObject();
                    writer.Key("AudioID"); writer.Uint64(ac.audioID);
                    writer.Key("Is3D"); writer.Bool(ac.is3D);
                    writer.Key("Loop"); writer.Bool(ac.loop);
                    writer.Key("PlayOnCreate"); writer.Bool(ac.playOnCreate);
                    writer.Key("Volume"); writer.Double(ac.volume);
                    writer.EndObject();
                }

                // LuaScript Component
                if (registry.all_of<LuaScriptComponent>(entity)) {
                    auto& lsc = registry.get<LuaScriptComponent>(entity);
                    writer.Key("LuaScriptComponent");
                    writer.StartObject();
                    writer.Key("Scripts");
                    writer.StartArray();
                    for (const auto& script : lsc.scripts) {
                        writer.String(script.filepath.c_str());
                    }
                    writer.EndArray();
                    writer.EndObject();
                }

                // Camera Component
                if (registry.all_of<CameraComponent>(entity)) {
                    auto& cc = registry.get<CameraComponent>(entity);
                    writer.Key("CameraComponent");
                    writer.StartObject();
                    writer.Key("FOV"); writer.Double(cc.FOV);
                    writer.Key("NearClip"); writer.Double(cc.nearClip);
                    writer.Key("FarClip"); writer.Double(cc.farClip);
                    writer.Key("InvertY"); writer.Bool(cc.invertY);
                    writer.EndObject();
                }

                // Relationship Component (Hierarchy)
                // Convert transient memory addresses (entt::entity) to stable uint64_t IDs
                if (registry.all_of<RelationshipComponent>(entity)) {
                    auto& rel = registry.get<RelationshipComponent>(entity);
                    writer.Key("RelationshipComponent");
                    writer.StartObject();

                    auto getID = [&](entt::entity e) -> uint64_t {
                        if (e != entt::null && registry.all_of<IDComponent>(e))
                            return registry.get<IDComponent>(e).id;
                        return 0;
                        };

                    writer.Key("Parent"); writer.Uint64(getID(rel.parent));
                    writer.Key("FirstChild"); writer.Uint64(getID(rel.firstChild));
                    writer.Key("PrevSibling"); writer.Uint64(getID(rel.prevSibling));
                    writer.Key("NextSibling"); writer.Uint64(getID(rel.nextSibling));
                    writer.EndObject();
                }

                writer.EndObject(); // End entity object
            };

        writer.EndArray();
        writer.EndObject();

        std::ofstream file(filepath);
        if (!file.is_open()) return false;

        file << sb.GetString();
        file.close();

        std::cout << "Scene saved successfully to " << filepath << std::endl;
        return true;
    }
}