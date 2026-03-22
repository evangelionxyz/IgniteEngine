/* MIT License
* 
* Copyright (c) 2025 Evangelion Manuhutu | IGNITE STUDIO
* 
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
* 
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
* 
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#include "serializer.hpp"

#include "ignite/scripting/script_class.hpp"
#include "ignite/scripting/script_engine.hpp"

#include "ignite/asset/asset_importer.hpp"
#include "ignite/scene/scene.hpp"
#include "ignite/project/project.hpp"
#include "ignite/core/device/device_manager.hpp"
#include "ignite/core/logger.hpp"
#include "ignite/graphics/objects/environment.hpp"

#include "ignite/scene/entity.hpp"
#include "ignite/scene/component.hpp"
#include "ignite/scene/scene_manager.hpp"

#include <fstream>
#include <ranges>

namespace ignite {

    Serializer::Serializer(const std::filesystem::path &filepath)
        : m_Filepath(filepath)
    {
    }

    void Serializer::Serialize() const
    {
        std::ofstream outFile(m_Filepath);
        outFile << m_Emitter.c_str();
        outFile.close();
    }

    void Serializer::Serialize(const std::filesystem::path &filepath)
    {
        m_Filepath = filepath;

        std::ofstream outFile(m_Filepath);
        outFile << m_Emitter.c_str();
        outFile.close();
    }

    void Serializer::BeginMap(const std::string &mapName)
    {
        m_Emitter << YAML::Key << mapName;
        m_Emitter << YAML::BeginMap;
    }

    void Serializer::BeginMap()
    {
        m_Emitter << YAML::BeginMap;
    }

    void Serializer::EndMap()
    {
        m_Emitter << YAML::EndMap;
    }

    void Serializer::BeginSequence()
    {
        m_Emitter << YAML::BeginSeq;
    }

    void Serializer::BeginSequence(const std::string &sequenceName)
    {
        m_Emitter << YAML::Key << sequenceName << YAML::Value << YAML::BeginSeq;
    }

    void Serializer::EndSequence()
    {
        m_Emitter << YAML::EndSeq;
    }

    YAML::Node Serializer::Deserialize(const std::filesystem::path &filepath)
    {
        std::ifstream inFile(filepath);
        std::stringstream buffer;
        buffer << inFile.rdbuf();
        inFile.close();
        return YAML::Load(buffer.str());
    }

    SceneSerializer::SceneSerializer(const Ref<Scene> &scene, Project *project)
        : m_Scene(scene), m_Project(project)
    {
    }

    bool SceneSerializer::Serialize(const std::filesystem::path &filepath)
    {
        if (!m_Scene || !m_Project)
            return false;

        Serializer sr(filepath);

        sr.BeginMap(); // START

        sr.BeginMap("Scene"); // scene file header
        sr.AddKeyValue<std::string>("Version", ENGINE_VERSION);
        sr.AddKeyValue<std::string>("Title", m_Scene->name);
        sr.BeginSequence("Entities");

        // entities sequence
        for (const entt::entity e : m_Scene->entities | std::views::values)
        {
            Entity entity = { e, m_Scene.get() };
            const IDComponent &idComp = entity.GetComponent<IDComponent>();

            const bool isPrefab = idComp.IsInType(EntityType_Prefab);

            if (isPrefab)
                continue;

            sr.BeginMap(); // START Entity
            {
                // ID Component
                sr.AddKeyValue("ID", idComp.uuid);
                sr.AddKeyValue("Name", idComp.name);
                sr.AddKeyValue("Type", EntityTypeFlagsToString(idComp.type));
                sr.AddKeyValue("Parent", idComp.parent);

                // Transform Component
                if (entity.HasComponent<TransformComponent>())
                {
                    const TransformComponent &comp = entity.GetComponent<TransformComponent>();
                    sr.BeginMap("Transform");
                    {
                        sr.AddKeyValue("WorldTranslation", comp.translation);
                        sr.AddKeyValue("WorldRotation", comp.rotation);
                        sr.AddKeyValue("WorldScale", comp.scale);

                        sr.AddKeyValue("LocalTranslation", comp.localTranslation);
                        sr.AddKeyValue("LocalRotation", comp.localRotation);
                        sr.AddKeyValue("LocalScale", comp.localScale);

                        sr.AddKeyValue("Visible", comp.visible);
                    }
                    sr.EndMap();
                }

                // Camera
                if (entity.HasComponent<CameraComponent>())
                {
                    const CameraComponent &comp = entity.GetComponent<CameraComponent>();
                    sr.BeginMap("Camera");
                    {
                        int projectionType = static_cast<int>(comp.camera.projectionType);
                        sr.AddKeyValue("ProjectionType", projectionType);
                        sr.AddKeyValue("NearClip", comp.camera.nearPlane);
                        sr.AddKeyValue("FarClip", comp.camera.farPlane);
                        sr.AddKeyValue("Fov", comp.camera.fov);
                        sr.AddKeyValue("Primary", comp.primary);
                    }
                    sr.EndMap();
                }

                // Sprite 2D component
                if (entity.HasComponent<Sprite2DComponent>())
                {
                    const Sprite2DComponent &comp = entity.GetComponent<Sprite2DComponent>();
                    sr.BeginMap("Sprite2D");
                    {
                        sr.AddKeyValue("Handle", comp.handle);
                        sr.AddKeyValue("Color", comp.color);
                        sr.AddKeyValue("TilingFactor", comp.tilingFactor);
                    }
                    sr.EndMap();
                }

				// Circle 2D component
				if (entity.HasComponent<Circle2DComponent>())
				{
					const Circle2DComponent &comp = entity.GetComponent<Circle2DComponent>();
					sr.BeginMap("Circle2D");
					{
						sr.AddKeyValue("Color", comp.color);
						sr.AddKeyValue("Thickness", comp.thickness);
						sr.AddKeyValue("Fade", comp.fade);
					}
					sr.EndMap();
				}

                // Rigidbody 2D
                if (entity.HasComponent<Rigidbody2DComponent>())
                {
                    const Rigidbody2DComponent &comp = entity.GetComponent<Rigidbody2DComponent>();
                    sr.BeginMap("Rigidbody2D");
                    {
                        sr.AddKeyValue("Type", BodyTypeToString(comp.type));
                        sr.AddKeyValue("LinearVelocity", comp.linearVelocity);
                        sr.AddKeyValue("AngularVelocity", comp.angularVelocity);
                        sr.AddKeyValue("GravityScale", comp.gravityScale);
                        sr.AddKeyValue("LinearDamping", comp.linearDamping);
                        sr.AddKeyValue("AngularDamping", comp.angularDamping);
                        sr.AddKeyValue("IsAwake", comp.isAwake);
                        sr.AddKeyValue("IsEnabled", comp.isEnabled);
                        sr.AddKeyValue("IsEnableSleep", comp.isEnableSleep);
                    }
                    sr.EndMap();
                }

                // Box collider 2D
                if (entity.HasComponent<BoxCollider2DComponent>())
                {
                    const BoxCollider2DComponent &comp = entity.GetComponent<BoxCollider2DComponent>();
                    sr.BeginMap("BoxCollider2D");
                    {
                        sr.AddKeyValue("Size", comp.size);
                        sr.AddKeyValue("Offset", comp.offset);
                        sr.AddKeyValue("Restitution", comp.restitution);
                        sr.AddKeyValue("Friction", comp.friction);
                        sr.AddKeyValue("Density", comp.density);
                        sr.AddKeyValue("IsSensor", comp.isSensor);
                    }
                    sr.EndMap();
                }

				// Circle collider 2D
				if (entity.HasComponent<CircleCollider2DComponent>())
				{
					const CircleCollider2DComponent &comp = entity.GetComponent<CircleCollider2DComponent>();
					sr.BeginMap("CircleCollider2D");
					{
						sr.AddKeyValue("Radius", comp.radius);
						sr.AddKeyValue("Center", comp.center);
						sr.AddKeyValue("Restitution", comp.restitution);
						sr.AddKeyValue("Friction", comp.friction);
						sr.AddKeyValue("Density", comp.density);
						sr.AddKeyValue("IsSensor", comp.isSensor);
					}
					sr.EndMap();
				}

				// Static Mesh
				if (entity.HasComponent<StaticMeshComponent>())
				{
					const StaticMeshComponent &comp = entity.GetComponent<StaticMeshComponent>();
					sr.BeginMap("StaticMesh");
					{
						sr.AddKeyValue("Handle", static_cast<uint64_t>(comp.handle));
					}
					sr.EndMap();
				}

                // skinned mesh
                // if (entity.HasComponent<SkeletalMesh>())
                // {
                //     const SkeletalMesh &comp = entity.GetComponent<SkeletalMesh>();
                //     sr.BeginMap("SkeletalMesh");
                //     sr.AddKeyValue("MeshHandle", static_cast<uint64_t>(comp.meshHandle));
                //     sr.AddKeyValue("SkeletonHandle", static_cast<uint64_t>(comp.skeletonHandle));
                //     sr.BeginSequence("Animations");
                //     for (const auto &anim : comp.animationHandle)
                //     {
                //         sr.BeginMap("Anim");
                //         sr.AddKeyValue("Handle", static_cast<uint64_t>(anim));
                //         sr.EndMap();
                //     }
                //     sr.EndSequence();
                //     sr.EndMap();
                // }

                // Rigidbody
                if (entity.HasComponent<RigibodyComponent>())
                {
                    const RigibodyComponent &comp = entity.GetComponent<RigibodyComponent>();
                    sr.BeginMap("Rigidbody");
                    {
                        sr.AddKeyValue("MotionQuality", static_cast<int>(comp.MotionQuality));
                        sr.AddKeyValue("UseGravity", comp.useGravity);
                        sr.AddKeyValue("RotateX", comp.rotateX);
                        sr.AddKeyValue("RotateY", comp.rotateY);
                        sr.AddKeyValue("RotateZ", comp.rotateZ);
                        sr.AddKeyValue("MoveX", comp.moveX);
                        sr.AddKeyValue("MoveY", comp.moveY);
                        sr.AddKeyValue("MoveZ", comp.moveZ);
                        sr.AddKeyValue("IsStatic", comp.isStatic);
                        sr.AddKeyValue("Mass", comp.mass);
                        sr.AddKeyValue("AllowSleeping", comp.allowSleeping);
                        sr.AddKeyValue("RetainAcceleration", comp.retainAcceleration);
                        sr.AddKeyValue("GravityFactor", comp.gravityFactor);
                        sr.AddKeyValue("CenterMass", comp.centerMass);
                    }
                    sr.EndMap();
                }

                if (entity.HasComponent<BoxColliderComponent>())
                {
                    const BoxColliderComponent &comp = entity.GetComponent<BoxColliderComponent>();
                    sr.BeginMap("BoxCollider");
                    {
                        sr.AddKeyValue("Scale", comp.scale);
                        sr.AddKeyValue("Friction", comp.friction);
                        sr.AddKeyValue("StaticFriction", comp.staticFriction);
                        sr.AddKeyValue("Restitution", comp.restitution);
                        sr.AddKeyValue("Density", comp.density);
                    }
                    sr.EndMap();
                }

                // SphereCollider
                if (entity.HasComponent<SphereColliderComponent>())
                {
                    const SphereColliderComponent &comp = entity.GetComponent<SphereColliderComponent>();
                    sr.BeginMap("SphereCollider");
                    {
                        sr.AddKeyValue("Radius", comp.radius);
                        sr.AddKeyValue("Friction", comp.friction);
                        sr.AddKeyValue("StaticFriction", comp.staticFriction);
                        sr.AddKeyValue("Restitution", comp.restitution);
                        sr.AddKeyValue("Density", comp.density);
                    }
                    sr.EndMap();
                }

                // CapsuleCollider
                if (entity.HasComponent<CapsuleColliderComponent>())
                {
                    const CapsuleColliderComponent &comp = entity.GetComponent<CapsuleColliderComponent>();
                    sr.BeginMap("CapsuleCollider");
                    {
                        sr.AddKeyValue("Radius", comp.radius);
                        sr.AddKeyValue("Height", comp.height);
                        sr.AddKeyValue("Friction", comp.friction);
                        sr.AddKeyValue("StaticFriction", comp.staticFriction);
                        sr.AddKeyValue("Restitution", comp.restitution);
                        sr.AddKeyValue("Density", comp.density);
                    }
                    sr.EndMap();
                }

                // MeshCollider
                if (entity.HasComponent<MeshColliderComponent>())
                {
                    const MeshColliderComponent &comp = entity.GetComponent<MeshColliderComponent>();
                    sr.BeginMap("MeshCollider");
                    {
                        sr.AddKeyValue("Convex", comp.convex);
                        sr.AddKeyValue("Friction", comp.friction);
                        sr.AddKeyValue("StaticFriction", comp.staticFriction);
                        sr.AddKeyValue("Restitution", comp.restitution);
                        sr.AddKeyValue("Density", comp.density);
                        
                        // Serialize vertices
                        sr.BeginSequence("Vertices");
                        for (const auto &vertex : comp.vertices)
                        {
                            sr.AddValue(vertex);
                        }
                        sr.EndSequence();
                        
                        // Serialize indices
                        sr.BeginSequence("Indices");
                        for (const auto &index : comp.indices)
                        {
                            sr.AddValue(index);
                        }
                        sr.EndSequence();
                    }
                    sr.EndMap();
                }

                // Audio Source
                if (entity.HasComponent<AudioSourceComponent>())
                {
                    const AudioSourceComponent &comp = entity.GetComponent<AudioSourceComponent>();
                    sr.BeginMap("AudioSource");
                    {
                        sr.AddKeyValue("Handle", static_cast<uint64_t>(comp.handle));
                        sr.AddKeyValue("Volume", comp.volume);
                        sr.AddKeyValue("Pitch", comp.pitch);
                        sr.AddKeyValue("Pan", comp.pan);
                        sr.AddKeyValue("PlayOnStart", comp.playOnStart);
                    }
                    sr.EndMap();
                }

				// World Environment
				if (entity.HasComponent<WorldEnvironment>())
				{
					const WorldEnvironment &comp = entity.GetComponent<WorldEnvironment>();
					sr.BeginMap("AudioSource");
					{
						sr.AddKeyValue("HDRHandle", static_cast<uint64_t>(comp.hdrHandle));
					}
					sr.EndMap();
				}

                // Script
                if (entity.HasComponent<ScriptComponent>())
                {
                    const ScriptComponent &comp = entity.GetComponent<ScriptComponent>();
                    sr.BeginMap("Script");
                    {
                        sr.AddKeyValue("ClassName", comp.className);

                        // Fields
                        const Ref<ScriptClass> scriptClass = ScriptEngine::GetInstance()->GetEntityClassesByName(comp.className);

                        if (scriptClass)
                        {
                            const auto &classFields = scriptClass->GetFields();

                            if (!classFields.empty())
                            {
                                auto &fields = ScriptEngine::GetInstance()->GetScriptFieldMap(entity);

                                sr.BeginSequence("Fields");
                                for (const auto &[fieldName, field] : classFields)
                                {
                                    if (!fields.contains(fieldName) || field.Type == ScriptFieldType::Invalid)
                                    {
                                        continue;
                                    }

                                    sr.BeginMap();
                                    sr.AddKeyValue("Name", fieldName);
                                    sr.AddKeyValue("Type", Utils::ScriptFieldTypeToString(field.Type));
                                    
                                    ScriptFieldInstance fieldInstance = fields.at(fieldName);
                                    switch (field.Type)
                                    {
                                    case ScriptFieldType::Float: sr.AddKeyValue("Value", fieldInstance.GetValue<float>()); break;
                                    case ScriptFieldType::Double: sr.AddKeyValue("Value", fieldInstance.GetValue<double>()); break;
                                    case ScriptFieldType::Bool: sr.AddKeyValue("Value", fieldInstance.GetValue<bool>()); break;
                                    case ScriptFieldType::Char: sr.AddKeyValue("Value", fieldInstance.GetValue<char>()); break;
                                    case ScriptFieldType::Byte: sr.AddKeyValue("Value", fieldInstance.GetValue<int8_t>()); break;
                                    case ScriptFieldType::Short: sr.AddKeyValue("Value", fieldInstance.GetValue<int16_t>()); break;
                                    case ScriptFieldType::Long: sr.AddKeyValue("Value", fieldInstance.GetValue<int64_t>()); break;
                                    case ScriptFieldType::UByte: sr.AddKeyValue("Value", fieldInstance.GetValue<uint8_t>()); break;
                                    case ScriptFieldType::UShort: sr.AddKeyValue("Value", fieldInstance.GetValue<uint16_t>()); break;
                                    case ScriptFieldType::UInt: sr.AddKeyValue("Value", fieldInstance.GetValue<uint32_t>()); break;
                                    case ScriptFieldType::ULong: sr.AddKeyValue("Value", fieldInstance.GetValue<uint64_t>()); break;
                                    case ScriptFieldType::Int: sr.AddKeyValue("Value", fieldInstance.GetValue<int>()); break;
                                    case ScriptFieldType::Vector2: sr.AddKeyValue("Value", fieldInstance.GetValue<glm::vec2>()); break;
                                    case ScriptFieldType::Vector3: sr.AddKeyValue("Value", fieldInstance.GetValue<glm::vec3>()); break;
                                    case ScriptFieldType::Vector4: sr.AddKeyValue("Value", fieldInstance.GetValue<glm::vec4>()); break;
                                    case ScriptFieldType::Entity: sr.AddKeyValue("Value", fieldInstance.GetValue<uint64_t>()); break;
                                    }

                                    sr.EndMap();
                                }

                                sr.EndSequence();
                            }
                        }

                    }
                    sr.EndMap();
                }

            }
            sr.EndMap(); // END Entity
        }

        sr.EndSequence(); // Entities
        sr.EndMap(); // scene

        sr.EndMap(); // END

#if 0
        // Example
        sr.BeginMap(); // START

        sr.BeginMap("Scene"); // scene file header

        sr.AddKeyValue<std::string>("Title", m_Scene->name);
        sr.AddKeyValue<std::string>("Version", ENGINE_VERSION);

        sr.BeginSequence("Entities");


        // entities sequence
        for (int i = 0; i < 10; ++i)
        {
            sr.BeginMap(); // START Entity
            {
                sr.AddKeyValue<std::string>("ID", "ENTITY_ID");
                sr.AddKeyValue<std::string>("Type", "ENTITY_TYPE");
                sr.AddKeyValue<std::string>("Parent", "PARENT_ID");
                sr.BeginMap("Component A");
                {
                    sr.AddKeyValue("Var A", "value");
                    sr.AddKeyValue("Var B", "value");

                    sr.BeginSequence("List Var");
                    {
                        sr.BeginMap();
                        {
                            sr.AddKeyValue("List Var A", "value");
                            sr.AddKeyValue("List Var B", "value");
                        }
                        sr.EndMap();
                    }
                    sr.EndSequence();
                }
                sr.EndMap();
            }
            sr.EndMap(); // END Entity
        }

        sr.EndSequence(); // Entities
        sr.EndMap(); // scene

        sr.EndMap(); // END

#endif
        sr.Serialize();

        // Scene should be not dirty
        m_Scene->SetDirtyFlag(false);

        return true;
    }

    Ref<Scene> SceneSerializer::Deserialize(const std::filesystem::path &filepath, Project *project)
    {
        LOG_ASSERT(std::filesystem::exists(filepath), "[Scene SR] File does not exists!\n{}", filepath.generic_string());
        LOG_ASSERT(project,"[Scene SR] Invalid project");
        
        YAML::Node sceneFileNode = Serializer::Deserialize(filepath);
        YAML::Node sceneNode = sceneFileNode["Scene"];

        LOG_ASSERT(sceneNode, "[Scene SR] Invalid scene file");
        if (!sceneNode)
            return nullptr;

        std::string title = sceneNode["Title"].as<std::string>();
        Ref<Scene> desScene = Scene::Create(project, title);

        // Open commandlist for asset deserialization
        auto device = DeviceManager::GetInstance()->GetDevice();
        nvrhi::CommandListHandle cmd = device->createCommandList();

        for (YAML::Node entityNode : sceneNode["Entities"])
        {
            UUID uuid = UUID(entityNode["ID"].as<uint64_t>());
            std::string name = entityNode["Name"].as<std::string>();
            EntityType type = EntityTypeFromStringFlags(entityNode["Type"].as<std::string>());

            Entity desEntity = SceneManager::CreateEntity(desScene.get(), name, type, uuid);
            UUID parent = UUID(entityNode["Parent"].as<uint64_t>());
            desEntity.GetComponent<IDComponent>().parent = parent;

            // Transform component
            if (YAML::Node node = entityNode["Transform"])
            {
                TransformComponent &comp = desEntity.AddComponent<TransformComponent>();
                comp.translation = node["WorldTranslation"].as<glm::vec3>();
                comp.rotation = node["WorldRotation"].as<glm::quat>();
                comp.scale = node["WorldScale"].as<glm::vec3>();
                comp.localTranslation = node["WorldTranslation"].as<glm::vec3>();
                comp.localRotation = node["WorldRotation"].as<glm::quat>();
                comp.localScale = node["WorldScale"].as<glm::vec3>();
                comp.visible = node["Visible"].as<bool>();
            }

            // Camera component
            if (YAML::Node node = entityNode["Camera"])
            {
                CameraComponent &comp = desEntity.AddComponent<CameraComponent>();
                comp.camera.projectionType = static_cast<ProjectionType>(node["ProjectionType"].as<int>());
                comp.camera.nearPlane = node["NearClip"].as<float>();
                comp.camera.farPlane = node["FarClip"].as<float>();
                comp.camera.fov = node["Fov"].as<float>();
                comp.primary = node["Primary"].as<bool>();
            }

            // Sprite 2D component
            if (YAML::Node node = entityNode["Sprite2D"])
            {
                Sprite2DComponent &comp = desEntity.AddComponent<Sprite2DComponent>();
                comp.handle = AssetHandle(node["Handle"].as<uint64_t>());
                comp.color = node["Color"].as<glm::vec4>();
                comp.tilingFactor = node["TilingFactor"].as<glm::vec2>();
            }

			// Circle 2D component
			if (YAML::Node node = entityNode["Circle2D"])
			{
				Circle2DComponent &comp = desEntity.AddComponent<Circle2DComponent>();
				comp.color = node["Color"].as<glm::vec4>();
				comp.thickness = node["Thickness"].as<float>();
				comp.fade = node["Fade"].as<float>();
			}

            // Rigidbody 2D
            if (YAML::Node node = entityNode["Rigidbody2D"])
            {
                Rigidbody2DComponent &comp = desEntity.AddComponent<Rigidbody2DComponent>();
                comp.type = BodyTypeFromString(node["Type"].as<std::string>());
                comp.linearVelocity = node["LinearVelocity"].as<glm::vec2>();
                comp.angularVelocity = node["AngularVelocity"].as<float>();
                comp.gravityScale = node["GravityScale"].as<float>();
                comp.linearDamping = node["LinearDamping"].as<float>();
                comp.angularDamping = node["AngularDamping"].as<float>();
                comp.isAwake = node["IsAwake"].as<bool>();
                comp.isEnabled = node["IsEnabled"].as<bool>();
                comp.isEnableSleep = node["IsEnableSleep"].as<bool>();
            }

            // BoxCollider 2D
            if (YAML::Node node = entityNode["BoxCollider2D"])
            {
                BoxCollider2DComponent &comp = desEntity.AddComponent<BoxCollider2DComponent>();
                comp.size = node["Size"].as<glm::vec2>();
                comp.offset = node["Offset"].as<glm::vec2>();
                comp.restitution = node["Restitution"].as<float>();
                comp.friction = node["Friction"].as<float>();
                comp.density = node["Density"].as<float>();
                comp.isSensor = node["IsSensor"].as<bool>();
            }

			// CircleCollider 2D
			if (YAML::Node node = entityNode["CircleCollider2D"])
			{
				CircleCollider2DComponent &comp = desEntity.AddComponent<CircleCollider2DComponent>();
				comp.center = node["Center"].as<glm::vec2>();
				comp.radius = node["Radius"].as<float>();
				comp.restitution = node["Restitution"].as<float>();
				comp.friction = node["Friction"].as<float>();
				comp.density = node["Density"].as<float>();
				comp.isSensor = node["IsSensor"].as<bool>();
			}

            // Rigidbody
            if (YAML::Node node = entityNode["Rigidbody"])
            {
                RigibodyComponent &comp = desEntity.AddComponent<RigibodyComponent>();
                comp.MotionQuality = static_cast<RigibodyComponent::EMotionQuality>(node["MotionQuality"].as<int>());
                comp.useGravity = node["UseGravity"].as<bool>();
                comp.rotateX = node["RotateX"].as<bool>();
                comp.rotateY = node["RotateY"].as<bool>();
                comp.rotateZ = node["RotateZ"].as<bool>();
                comp.moveX = node["MoveX"].as<bool>();
                comp.moveY = node["MoveY"].as<bool>();
                comp.moveZ = node["MoveZ"].as<bool>();
                comp.isStatic = node["IsStatic"].as<bool>();
                comp.mass = node["Mass"].as<float>();
                comp.allowSleeping = node["AllowSleeping"].as<bool>();
                comp.retainAcceleration = node["RetainAcceleration"].as<bool>();
                comp.gravityFactor = node["GravityFactor"].as<float>();
                comp.centerMass = node["CenterMass"].as<glm::vec3>();
            }

            // BoxCollider
            if (YAML::Node node = entityNode["BoxCollider"])
            {
                BoxColliderComponent &comp = desEntity.AddComponent<BoxColliderComponent>();
                comp.scale = node["Scale"].as<glm::vec3>();
                comp.friction = node["Friction"].as<float>();
                comp.staticFriction = node["StaticFriction"].as<float>();
                comp.restitution = node["Restitution"].as<float>();
                comp.density = node["Density"].as<float>();
            }

            // SphereCollider
            if (YAML::Node node = entityNode["SphereCollider"])
            {
                SphereColliderComponent &comp = desEntity.AddComponent<SphereColliderComponent>();
                comp.radius = node["Radius"].as<float>();
                comp.friction = node["Friction"].as<float>();
                comp.staticFriction = node["StaticFriction"].as<float>();
                comp.restitution = node["Restitution"].as<float>();
                comp.density = node["Density"].as<float>();
            }

            // CapsuleCollider
            if (YAML::Node node = entityNode["CapsuleCollider"])
            {
                CapsuleColliderComponent &comp = desEntity.AddComponent<CapsuleColliderComponent>();
                comp.radius = node["Radius"].as<float>();
                comp.height = node["Height"].as<float>();
                comp.friction = node["Friction"].as<float>();
                comp.staticFriction = node["StaticFriction"].as<float>();
                comp.restitution = node["Restitution"].as<float>();
                comp.density = node["Density"].as<float>();
            }

            // MeshCollider
            if (YAML::Node node = entityNode["MeshCollider"])
            {
                MeshColliderComponent &comp = desEntity.AddComponent<MeshColliderComponent>();
                comp.convex = node["Convex"].as<bool>();
                comp.friction = node["Friction"].as<float>();
                comp.staticFriction = node["StaticFriction"].as<float>();
                comp.restitution = node["Restitution"].as<float>();
                comp.density = node["Density"].as<float>();
                
                // Deserialize vertices
                if (YAML::Node verticesNode = node["Vertices"])
                {
                    comp.vertices.clear();
                    for (const auto &vertexNode : verticesNode)
                    {
                        comp.vertices.push_back(vertexNode.as<glm::vec3>());
                    }
                }
                
                // Deserialize indices
                if (YAML::Node indicesNode = node["Indices"])
                {
                    comp.indices.clear();
                    for (const auto &indexNode : indicesNode)
                    {
                        comp.indices.push_back(indexNode.as<uint32_t>());
                    }
                }
            }

            // Audio Source
            if (YAML::Node node = entityNode["AudioSource"])
            {
                AudioSourceComponent& comp = desEntity.AddComponent<AudioSourceComponent>();
                comp.handle = AssetHandle(node["Handle"].as<uint64_t>());
                comp.volume = node["Volume"].as<float>();
                comp.pitch = node["Pitch"].as<float>();
                comp.pan = node["Pan"].as<float>();
                comp.playOnStart = node["PlayOnStart"].as<bool>();
            }

            // World Environment
            if (YAML::Node node = entityNode["WorldEnvironment"])
            {
                WorldEnvironment &world = desEntity.AddComponent<WorldEnvironment>();
                world.environment = Environment::Create(desScene.get());
                world.hdrHandle = AssetHandle(node["HDRHandle"].as<uint64_t>());
            }

			// Static Mesh
			if (YAML::Node node = entityNode["StaticMesh"])
			{
				StaticMeshComponent &comp = desEntity.AddComponent<StaticMeshComponent>();
				comp.handle = AssetHandle(node["Handle"].as<uint64_t>());
			}

            // Script
            if (YAML::Node node = entityNode["Script"])
            {
                ScriptComponent &sc = desEntity.AddComponent<ScriptComponent>();
                sc.className = node["ClassName"].as<std::string>();

                if (YAML::Node classFieldsNode = node["Fields"])
                {
                    if (Ref<ScriptClass> scriptClass = ScriptEngine::GetInstance()->GetEntityClassesByName(sc.className))
                    {
                        const auto &classFields = scriptClass->GetFields();
                        ScriptFieldMap &fieldMap = ScriptEngine::GetInstance()->GetScriptFieldMap(desEntity);

                        for (YAML::Node fieldNode : classFieldsNode)
                        {
                            std::string fieldName = fieldNode["Name"].as<std::string>();
                            ScriptFieldType fieldType = Utils::ScriptFieldTypeFromString(fieldNode["Type"].as<std::string>());

                            ScriptFieldInstance &fieldInstance = fieldMap[fieldName];

                            if (!fieldMap.contains(fieldName))
                                continue;

                            fieldInstance.Field = classFields.at(fieldName);

                            switch (fieldType)
                            {
                            case ScriptFieldType::Float: fieldInstance.SetValue(fieldNode["Value"].as<float>()); break;
                            case ScriptFieldType::Double: fieldInstance.SetValue(fieldNode["Value"].as<double>()); break;
                            case ScriptFieldType::Bool: fieldInstance.SetValue(fieldNode["Value"].as<bool>()); break;
                            case ScriptFieldType::Char: fieldInstance.SetValue(fieldNode["Value"].as<char>()); break;
                            case ScriptFieldType::Byte: fieldInstance.SetValue(fieldNode["Value"].as<int8_t>()); break;
                            case ScriptFieldType::Short: fieldInstance.SetValue(fieldNode["Value"].as<int16_t>()); break;
                            case ScriptFieldType::Long: fieldInstance.SetValue(fieldNode["Value"].as<int64_t>()); break;
                            case ScriptFieldType::UByte: fieldInstance.SetValue(fieldNode["Value"].as<uint8_t>()); break;
                            case ScriptFieldType::UShort: fieldInstance.SetValue(fieldNode["Value"].as<uint16_t>()); break;
                            case ScriptFieldType::UInt: fieldInstance.SetValue(fieldNode["Value"].as<uint32_t>()); break;
                            case ScriptFieldType::ULong: fieldInstance.SetValue(fieldNode["Value"].as<uint64_t>()); break;
                            case ScriptFieldType::Int: fieldInstance.SetValue(fieldNode["Value"].as<int>()); break;
                            case ScriptFieldType::Entity: fieldInstance.SetValue(fieldNode["Value"].as<uint64_t>()); break;
                            case ScriptFieldType::Vector2: fieldInstance.SetValue(fieldNode["Value"].as<glm::vec2>()); break;
                            case ScriptFieldType::Vector3: fieldInstance.SetValue(fieldNode["Value"].as<glm::vec3>()); break;
                            case ScriptFieldType::Vector4: fieldInstance.SetValue(fieldNode["Value"].as<glm::vec4>()); break;
                            }
                        }
                    }
                }
            }
        }

        // attach each node to it's parent
        for (auto &[uuid, e] : desScene->entities)
        {
            Entity entity{ e, desScene.get() };

            if (entity.GetParentUUID() != UUID(0))
            {
                Entity parent = SceneManager::GetEntity(desScene.get(), entity.GetParentUUID());
                SceneManager::AddChild(desScene.get(), parent, entity);
            }
        }

        return desScene;
    }


    ProjectSerializer::ProjectSerializer(Project *project)
        : m_Project(project)
    {
    }

    bool ProjectSerializer::Serialize(const std::filesystem::path &filepath)
    {
        if (!m_Project)
            return false;

        const auto &projectInfo = m_Project->GetInfo();

        Serializer projectSr(filepath);

        projectSr.BeginMap(); // START

        projectSr.BeginMap("Project");

        projectSr.AddKeyValue("Version", ENGINE_VERSION);
        projectSr.AddKeyValue("Name", projectInfo.name);
        projectSr.AddKeyValue("AssetPath", projectInfo.assetDirectory.generic_string());
        projectSr.AddKeyValue("AssetRegistry", projectInfo.assetRegistryFilepath.generic_string());
        projectSr.AddKeyValue("ScriptModule", projectInfo.scriptModuleFilepath.generic_string());
        projectSr.AddKeyValue("DefaultSceneHandle", projectInfo.defaultSceneHandle);

        projectSr.EndMap();

        projectSr.EndMap(); // END

        projectSr.Serialize();

        // set dirty flags
        m_Project->SetDirtyFlag(false);

        // Serialize asset manager
        auto &assetManager = m_Project->GetAssetManager();
        auto &assetRegistry = assetManager.GetAssetAssetRegistry();

        {
            const std::filesystem::path assetRegFilepath = filepath.parent_path() / projectInfo.assetRegistryFilepath;
            Serializer assetSr(assetRegFilepath);

            assetSr.BeginMap(); // Start

            assetSr.BeginMap("AssetRegistry");

            assetSr.BeginSequence("Assets"); // Asset sequence
            for (auto &[handle, metadata] : assetRegistry)
            {
                assetSr.BeginMap(); // Begin Metadata

                assetSr.AddKeyValue("Handle", static_cast<uint64_t>(handle));
                assetSr.AddKeyValue("Type", AssetTypeToString(metadata.type));
                assetSr.AddKeyValue("Filepath", metadata.filepath.generic_string());

                assetSr.EndMap();
            }

            assetSr.EndSequence(); // Asset sequence

            assetSr.EndMap(); // End

            assetSr.Serialize();
        }

        return true;
    }

    Ref<Project> ProjectSerializer::Deserialize(const std::filesystem::path &filepath)
    {
        bool exists = std::filesystem::exists(filepath);
        LOG_ASSERT(exists, "[Project Serializer] File does not exists");
        if (!exists)
        {
            return nullptr;
        }

        YAML::Node projectFileNode = Serializer::Deserialize(filepath);
        YAML::Node projectNode = projectFileNode["Project"];

        ProjectInfo info;
        info.name = projectNode["Name"].as<std::string>();
        info.filepath = filepath;
        info.assetDirectory = projectNode["AssetPath"].as<std::string>();
        info.assetRegistryFilepath = projectNode["AssetRegistry"].as<std::string>();
        info.defaultSceneHandle = AssetHandle(projectNode["DefaultSceneHandle"].as<uint64_t>());
        info.scriptModuleFilepath = projectNode["ScriptModule"].as<std::string>();

        Ref<Project> project = Project::Create(info);

        auto &assetManager = project->GetAssetManager();

        // import registry
        if (!info.assetRegistryFilepath.empty())
        {
            // project filepath / asset filename (.ixreg)
            std::filesystem::path assetRegFilepath = filepath.parent_path() / info.assetRegistryFilepath;
            YAML::Node assetRegFileNode = Serializer::Deserialize(assetRegFilepath);
            YAML::Node assetRegNode = assetRegFileNode["AssetRegistry"];

            for (YAML::Node assetNode : assetRegNode["Assets"])
            {
                AssetHandle handle = AssetHandle(assetNode["Handle"].as<uint64_t>());
                AssetMetaData metadata;
                metadata.type = AssetTypeFromString(assetNode["Type"].as<std::string>());
                metadata.filepath = assetNode["Filepath"].as<std::string>();

                assetManager.AssignMetaData(handle, metadata);
            }
        }

        return project;
    }


    AnimationSerializer::AnimationSerializer(const SkeletalAnimation &animation)
        : m_Animation(animation)
    {
    }

    bool AnimationSerializer::Serialize(const std::filesystem::path &filepath)
    {
        Serializer sr(filepath);

        sr.BeginMap(); // START

        sr.BeginMap("Animation");
        sr.AddKeyValue("Version", ENGINE_VERSION);
        sr.AddKeyValue("Name", m_Animation.name);
        sr.AddKeyValue("Duration", m_Animation.duration);
        sr.AddKeyValue("TicksPerSeconds", m_Animation.ticksPerSeconds);

        sr.BeginSequence("Channels");

        for (auto &[name, channel] : m_Animation.channels)
        {
            sr.BeginMap();

            sr.AddKeyValue("Name", name);

            // Translation
            sr.BeginSequence("TranslationKeys");
            for (auto &f : channel.translationKeys.frames)
            {
                sr.BeginMap();
                sr.AddKeyValue("Timestamp", f.Timestamp);
                sr.AddKeyValue("Value", f.Value);
                sr.EndMap();
            }
            sr.EndSequence();

            // Rotation
            sr.BeginSequence("RotationKeys");
            for (auto &f : channel.rotationKeys.frames)
            {
                sr.BeginMap();
                sr.AddKeyValue("Timestamp", f.Timestamp);
                sr.AddKeyValue("Value", f.Value);
                sr.EndMap();
            }
            sr.EndSequence();

            // Scale
            sr.BeginSequence("ScaleKeys");
            for (auto &f : channel.scaleKeys.frames)
            {
                sr.BeginMap();
                sr.AddKeyValue("Timestamp", f.Timestamp);
                sr.AddKeyValue("Value", f.Value);
                sr.EndMap();
            }
            sr.EndSequence();

            sr.EndMap();
        }

        sr.EndSequence();

        sr.EndMap();

        sr.EndMap(); // END

        sr.Serialize(filepath);

        return true;
    }

    SkeletalAnimation AnimationSerializer::Deserialize(const std::filesystem::path &filepath)
    {
        SkeletalAnimation animation;

        return animation;
    }
}
