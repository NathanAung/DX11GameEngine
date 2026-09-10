#include "Engine/SceneSerializer.h"
#include "Engine/Scene.h"
#include "Engine/Components.h"
#include "Engine/AssetManager.h"

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>
#include <fstream>
#include <iostream>
#include <filesystem>

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
                        // Ensure the script is tracked in the registry before the registry is saved
                        if (scene.GetAssetManager() && !script.filepath.empty()) {
                            scene.GetAssetManager()->ImportAsset(script.filepath, Engine::AssetType::LuaScript);
                        }
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

                // EditorCamControl Component
                if (registry.all_of<EditorCamControlComponent>(entity)) {
                    auto& ecc = registry.get<EditorCamControlComponent>(entity);
                    writer.Key("EditorCamControlComponent");
                    writer.StartObject();
                    writer.Key("Mode"); writer.Int(static_cast<int>(ecc.mode));
                    writer.Key("MoveSpeed"); writer.Double(ecc.moveSpeed);
                    writer.Key("LookSensitivity"); writer.Double(ecc.lookSensitivity);
                    writer.Key("SprintMultiplier"); writer.Double(ecc.sprintMultiplier);
                    writer.Key("Yaw"); writer.Double(ecc.yaw);
                    writer.Key("Pitch"); writer.Double(ecc.pitch);
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

        if (scene.GetAssetManager())
        {
            scene.GetAssetManager()->SaveRegistry("enginefiles/AssetRegistry.json");
        }

        std::cout << "Scene saved successfully to " << filepath << std::endl;
        return true;
    }


    bool SceneSerializer::Deserialize(const std::string& filepath, Scene& scene, PhysicsManager& physicsManager, AssetManager& assetManager, std::string& outErrorMsg)
    {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            outErrorMsg = "Failed to open scene file: " + filepath;
            return false;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        file.close();

        rapidjson::Document doc;
        doc.Parse(buffer.str().c_str());

        if (doc.HasParseError() || !doc.HasMember("Scene") || !doc["Scene"].IsArray()) {
            outErrorMsg = "Invalid JSON scene format.";
            return false;
        }

        const auto& entities = doc["Scene"];

        // --- PASS 1: VALIDATION (Check for Missing Assets) ---
        // Helper lambda to check if an asset UUID maps to a real file
        auto checkAsset = [&](uint64_t uuid) -> bool {
            if (uuid == 0) return true; // 0 means 'None', which is valid

            const AssetMetadata* meta = assetManager.GetMetadata(uuid);
            if (!meta) return false; // UUID not in the registry at all

            // Ignore virtual paths (like primitive://cube)
            if (meta->filepath.find("primitive://") == 0) return true;
            if (meta->filepath.find("shader://") == 0) return true;
            if (meta->filepath.find("cubemap://") == 0) return true;

            // The assets are safely packed in memory, so we trust the Asset Ledger.
            if (assetManager.IsVFSActive()) return true;

            // Strip sub-mesh query strings (e.g., "?mesh=0") before checking OS filesystem
            std::string pathToCheck = meta->filepath;
            size_t queryPos = pathToCheck.find('?');
            if (queryPos != std::string::npos) {
                pathToCheck = pathToCheck.substr(0, queryPos);
            }

            if (!std::filesystem::exists(pathToCheck)) return false;

            return true;
            };

        for (rapidjson::SizeType i = 0; i < entities.Size(); i++) {
            const auto& item = entities[i];

            if (item.HasMember("MeshRendererComponent")) {
                if (!checkAsset(item["MeshRendererComponent"]["MeshID"].GetUint64())) {
                    outErrorMsg = "Missing Mesh Asset in Scene."; return false;
                }
                if (!checkAsset(item["MeshRendererComponent"]["TextureID"].GetUint64())) {
                    outErrorMsg = "Missing Texture Asset in Scene."; return false;
                }
            }
            if (item.HasMember("RigidBodyComponent")) {
                if (!checkAsset(item["RigidBodyComponent"]["MeshID"].GetUint64())) {
                    outErrorMsg = "Missing Physics Mesh Asset in Scene."; return false;
                }
            }
            if (item.HasMember("AudioComponent")) {
                if (!checkAsset(item["AudioComponent"]["AudioID"].GetUint64())) {
                    outErrorMsg = "Missing Audio Asset in Scene."; return false;
                }
            }
            if (item.HasMember("LuaScriptComponent")) {
                const auto& scripts = item["LuaScriptComponent"]["Scripts"];
                for (rapidjson::SizeType j = 0; j < scripts.Size(); j++) {
                    std::string scriptPath = scripts[j].GetString();
                    // Bypass physical check for scripts if VFS is active
                    if (!scriptPath.empty() && !assetManager.IsVFSActive() && !std::filesystem::exists(scriptPath)) {
                        outErrorMsg = "Missing Script File: " + scriptPath; return false;
                    }
                }
            }
        }

        // --- PASS 2: CLEAR CURRENT SCENE ---
        // Validation passed! It is now safe to delete the current active world.
        scene.Clear(physicsManager);
        scene.SetCurrentScenePath(filepath);

        // Map to translate saved uint64_t IDs into fresh EnTT entity handles
        std::unordered_map<uint64_t, entt::entity> idMap;

        // --- PASS 3: REBUILD ENTITIES & COMPONENTS ---
        for (rapidjson::SizeType i = 0; i < entities.Size(); i++) {
            const auto& item = entities[i];
            entt::entity entity = scene.registry.create();

            uint64_t savedID = item["EntityID"].GetUint64();
            scene.registry.emplace<IDComponent>(entity, IDComponent{ savedID });
            idMap[savedID] = entity;

            if (item.HasMember("NameComponent")) {
                scene.registry.emplace<NameComponent>(entity, NameComponent{
                    item["NameComponent"]["Name"].GetString(),
                    item["NameComponent"]["IsActive"].GetBool()
                    });
            }

            if (item.HasMember("TransformComponent")) {
                const auto& pos = item["TransformComponent"]["Position"];
                const auto& rot = item["TransformComponent"]["Rotation"];
                const auto& scl = item["TransformComponent"]["Scale"];
                scene.registry.emplace<TransformComponent>(entity, TransformComponent{
                    DirectX::XMFLOAT3(static_cast<float>(pos[0].GetDouble()), static_cast<float>(pos[1].GetDouble()), static_cast<float>(pos[2].GetDouble())),
                    DirectX::XMFLOAT4(static_cast<float>(rot[0].GetDouble()), static_cast<float>(rot[1].GetDouble()), static_cast<float>(rot[2].GetDouble()), static_cast<float>(rot[3].GetDouble())),
                    DirectX::XMFLOAT3(static_cast<float>(scl[0].GetDouble()), static_cast<float>(scl[1].GetDouble()), static_cast<float>(scl[2].GetDouble())),
                    DirectX::XMFLOAT4X4(), true
                    });
            }

            if (item.HasMember("MeshRendererComponent")) {
                const auto& color = item["MeshRendererComponent"]["BaseColor"];
                scene.registry.emplace<MeshRendererComponent>(entity, MeshRendererComponent{
                    item["MeshRendererComponent"]["IsActive"].GetBool(),
                    item["MeshRendererComponent"]["MeshID"].GetUint64(),
                    item["MeshRendererComponent"]["TextureID"].GetUint64(),
                    static_cast<MaterialType>(item["MeshRendererComponent"]["MatType"].GetInt()),
                    DirectX::XMFLOAT4(static_cast<float>(color[0].GetDouble()), static_cast<float>(color[1].GetDouble()), static_cast<float>(color[2].GetDouble()), static_cast<float>(color[3].GetDouble())),
                    static_cast<float>(item["MeshRendererComponent"]["Roughness"].GetDouble()),
                    static_cast<float>(item["MeshRendererComponent"]["Metallic"].GetDouble())
                    });
            }

            if (item.HasMember("RigidBodyComponent")) {
                const auto& he = item["RigidBodyComponent"]["HalfExtent"];
                const auto& cs = item["RigidBodyComponent"]["ColliderScale"];
                scene.registry.emplace<RigidBodyComponent>(entity, RigidBodyComponent{
                    item["RigidBodyComponent"]["IsActive"].GetBool(),
                    static_cast<RBShape>(item["RigidBodyComponent"]["Shape"].GetInt()),
                    static_cast<RBMotion>(item["RigidBodyComponent"]["MotionType"].GetInt()),
                    static_cast<float>(item["RigidBodyComponent"]["Mass"].GetDouble()),
                    static_cast<float>(item["RigidBodyComponent"]["Friction"].GetDouble()),
                    static_cast<float>(item["RigidBodyComponent"]["Restitution"].GetDouble()),
                    static_cast<float>(item["RigidBodyComponent"]["LinearDamping"].GetDouble()),
                    DirectX::XMFLOAT3(static_cast<float>(he[0].GetDouble()), static_cast<float>(he[1].GetDouble()), static_cast<float>(he[2].GetDouble())),
                    static_cast<float>(item["RigidBodyComponent"]["Radius"].GetDouble()),
                    static_cast<float>(item["RigidBodyComponent"]["Height"].GetDouble()),
                    DirectX::XMFLOAT3(static_cast<float>(cs[0].GetDouble()), static_cast<float>(cs[1].GetDouble()), static_cast<float>(cs[2].GetDouble())),
                    item["RigidBodyComponent"]["MeshID"].GetUint64(),
                    JPH::BodyID(), false,
                    item["RigidBodyComponent"]["ShowWireframe"].GetBool()
                    });
            }

            if (item.HasMember("LightComponent")) {
                const auto& col = item["LightComponent"]["Color"];
                scene.registry.emplace<LightComponent>(entity, LightComponent{
                    item["LightComponent"]["IsActive"].GetBool(),
                    DirectX::XMFLOAT3(static_cast<float>(col[0].GetDouble()), static_cast<float>(col[1].GetDouble()), static_cast<float>(col[2].GetDouble())),
                    static_cast<float>(item["LightComponent"]["Intensity"].GetDouble()),
                    static_cast<LightType>(item["LightComponent"]["Type"].GetInt()),
                    static_cast<float>(item["LightComponent"]["Range"].GetDouble()),
                    static_cast<float>(item["LightComponent"]["SpotAngle"].GetDouble())
                    });
            }

            if (item.HasMember("AudioComponent")) {
                scene.registry.emplace<AudioComponent>(entity, AudioComponent{
                    item["AudioComponent"]["AudioID"].GetUint64(),
                    item["AudioComponent"]["Is3D"].GetBool(),
                    item["AudioComponent"]["Loop"].GetBool(),
                    item["AudioComponent"]["PlayOnCreate"].GetBool(),
                    static_cast<float>(item["AudioComponent"]["Volume"].GetDouble()),
                    nullptr, false
                    });
            }

            if (item.HasMember("LuaScriptComponent")) {
                LuaScriptComponent lsc;
                const auto& scripts = item["LuaScriptComponent"]["Scripts"];
                for (rapidjson::SizeType j = 0; j < scripts.Size(); j++) {
                    ScriptInstance inst;
                    inst.filepath = scripts[j].GetString();
                    lsc.scripts.push_back(inst);
                }
                scene.registry.emplace<LuaScriptComponent>(entity, lsc);
            }

            if (item.HasMember("CameraComponent")) {
                scene.registry.emplace<CameraComponent>(entity, CameraComponent{
                    static_cast<float>(item["CameraComponent"]["FOV"].GetDouble()),
                    static_cast<float>(item["CameraComponent"]["NearClip"].GetDouble()),
                    static_cast<float>(item["CameraComponent"]["FarClip"].GetDouble()),
                    item["CameraComponent"]["InvertY"].GetBool()
                    });
                scene.registry.emplace<ViewportComponent>(entity);

                // Force the Editor Camera to become the active render camera, even if another camera loaded first
                if (scene.m_activeRenderCamera == entt::null || item.HasMember("EditorCamControlComponent")) {
                    scene.m_activeRenderCamera = entity;
                }
            }

            if (item.HasMember("EditorCamControlComponent")) {
                scene.registry.emplace<EditorCamControlComponent>(entity, EditorCamControlComponent{
                    static_cast<CameraControlMode>(item["EditorCamControlComponent"]["Mode"].GetInt()),
                    static_cast<float>(item["EditorCamControlComponent"]["MoveSpeed"].GetDouble()),
                    static_cast<float>(item["EditorCamControlComponent"]["LookSensitivity"].GetDouble()),
                    static_cast<float>(item["EditorCamControlComponent"]["SprintMultiplier"].GetDouble()),
                    static_cast<float>(item["EditorCamControlComponent"]["Yaw"].GetDouble()),
                    static_cast<float>(item["EditorCamControlComponent"]["Pitch"].GetDouble())
                    });
            }
        }

        // --- PASS 4: RESTORE HIERARCHY ---
        // Using the Translation Map to map old uint64_t to new entt::entity handles
        for (rapidjson::SizeType i = 0; i < entities.Size(); i++) {
            const auto& item = entities[i];
            if (item.HasMember("RelationshipComponent")) {
                uint64_t savedID = item["EntityID"].GetUint64();
                entt::entity entity = idMap[savedID];

                uint64_t p = item["RelationshipComponent"]["Parent"].GetUint64();
                uint64_t fc = item["RelationshipComponent"]["FirstChild"].GetUint64();
                uint64_t ps = item["RelationshipComponent"]["PrevSibling"].GetUint64();
                uint64_t ns = item["RelationshipComponent"]["NextSibling"].GetUint64();

                scene.registry.emplace<RelationshipComponent>(entity, RelationshipComponent{
                    p == 0 ? entt::null : idMap[p],
                    fc == 0 ? entt::null : idMap[fc],
                    ps == 0 ? entt::null : idMap[ps],
                    ns == 0 ? entt::null : idMap[ns]
                    });
            }
        }

        std::cout << "Scene loaded successfully from " << filepath << std::endl;
        return true;
    }
}