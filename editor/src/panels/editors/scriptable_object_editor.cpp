// Copyright (c) 2026 Evangelion Manuhutu

#include "scriptable_object_editor.hpp"
#include "ignite/scripting/script_engine.hpp"
#include "ignite/scripting/script_field.hpp"
#include "ignite/scripting/scriptable_object.hpp"

#include "ignite/scene/scene_manager.hpp"
#include "ignite/scene/entity.hpp"
#include "ignite/scene/scene.hpp"
#include "panels/asset_editor_panel.hpp"
#include "editor_layer.hpp"
#include "ext/editor_ui.hpp"
#include "states.hpp"

#include <imgui.h>
#include <glm/gtc/type_ptr.hpp>

namespace ignite
{
    bool ScriptableObjectEditor::DrawEditor(Ref<ScriptableObject> so, EditorLayer *editorLayer)
    {
        ScriptEngine *scriptEngine = ScriptEngine::GetInstance();
        Ref<ScriptClass> scriptClass = scriptEngine->GetScriptableObjectClassByName(so->GetClassName());
        if (!scriptClass)
            return false;

        auto assetManager = editorLayer->GetActiveProject()->GetAssetManager();
        Ref<Scene> activeScene = editorLayer->GetActiveScene();
        bool isRunning = activeScene && activeScene->IsRunning();

        auto &soFields = so->GetFields();

        for (const auto &name : scriptClass->GetOrderedFieldNames())
        {
            const auto &field = scriptClass->GetFields().at(name);
            ImGui::PushID(name.c_str());

            ScriptInstanceField dummy;
            dummy.field = field;
            auto it = soFields.find(name);
            if (it != soFields.end())
            {
                dummy = it->second;
            }

            bool valueChanged = false;

            switch (field.Type)
            {
                case ScriptFieldType::Bool:
                {
                    auto data = dummy.GetValue<bool>();
                    if (UI::DrawCheckbox(name.c_str(), &data))
                    {
                        dummy.SetValue<bool>(data);
                        valueChanged = true;

                    }
                    break;
                }
                case ScriptFieldType::String:
                {
                    auto data = dummy.GetValue<std::string>();
                    char buffer[256];
                    memset(buffer, 0, sizeof(buffer));
                    strncpy(buffer, data.c_str(), sizeof(buffer) - 1);

                    ImGui::BeginColumns(name.c_str(), 2);
                    ImGui::SetColumnWidth(0, 100.0f);
                    ImGui::Text("%s", name.c_str());
                    ImGui::NextColumn();

                    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
                    if (ImGui::InputText("##StringControl", buffer, sizeof(buffer)))
                    {
                        std::string newData(buffer);
                        dummy.SetValue<std::string>(newData);
                        valueChanged = true;

                    }
                    ImGui::PopItemWidth();
                    ImGui::EndColumns();
                    break;
                }
                case ScriptFieldType::Float:
                {
                    auto data = dummy.GetValue<float>();
                    if (UI::DrawFloatControl(name.c_str(), &data, 0.1f, -FLT_MAX, FLT_MAX))
                    {
                        dummy.SetValue<float>(data);
                        valueChanged = true;

                    }
                    break;
                }
                case ScriptFieldType::Int:
                {
                    auto data = dummy.GetValue<int>();
                    if (UI::DrawIntControl(name.c_str(), &data, 1.0f, INT_MIN, INT_MAX))
                    {
                        dummy.SetValue<int>(data);
                        valueChanged = true;

                    }
                    break;
                }
                case ScriptFieldType::UInt:
                {
                    auto val = dummy.GetValue<uint32_t>();
                    int data = static_cast<int>(val);
                    if (UI::DrawIntControl(name.c_str(), &data, 1.0f, 0, INT_MAX))
                    {
                        val = static_cast<uint32_t>(data);
                        dummy.SetValue<uint32_t>(val);
                        valueChanged = true;

                    }
                    break;
                }
                case ScriptFieldType::Byte:
                {
                    auto val = dummy.GetValue<uint8_t>();
                    int data = static_cast<int>(val);
                    if (UI::DrawIntControl(name.c_str(), &data, 1.0f, 0, 255))
                    {
                        val = static_cast<uint8_t>(data);
                        dummy.SetValue<uint8_t>(val);
                        valueChanged = true;

                    }
                    break;
                }
                case ScriptFieldType::SByte:
                {
                    auto val = dummy.GetValue<int8_t>();
                    int data = static_cast<int>(val);
                    if (UI::DrawIntControl(name.c_str(), &data, 1.0f, -128, 127))
                    {
                        val = static_cast<int8_t>(data);
                        dummy.SetValue<int8_t>(val);
                        valueChanged = true;

                    }
                    break;
                }
                case ScriptFieldType::Short:
                {
                    auto val = dummy.GetValue<int16_t>();
                    int data = static_cast<int>(val);
                    if (UI::DrawIntControl(name.c_str(), &data, 1.0f, SHRT_MIN, SHRT_MAX))
                    {
                        val = static_cast<int16_t>(data);
                        dummy.SetValue<int16_t>(val);
                        valueChanged = true;

                    }
                    break;
                }
                case ScriptFieldType::UShort:
                {
                    auto val = dummy.GetValue<uint16_t>();
                    int data = static_cast<int>(val);
                    if (UI::DrawIntControl(name.c_str(), &data, 1.0f, 0, USHRT_MAX))
                    {
                        val = static_cast<uint16_t>(data);
                        dummy.SetValue<uint16_t>(val);
                        valueChanged = true;

                    }
                    break;
                }
                case ScriptFieldType::Long:
                {
                    auto val = dummy.GetValue<int64_t>();
                    int data = static_cast<int>(val);
                    if (UI::DrawIntControl(name.c_str(), &data, 1.0f, INT_MIN, INT_MAX))
                    {
                        val = static_cast<int64_t>(data);
                        dummy.SetValue<int64_t>(val);
                        valueChanged = true;

                    }
                    break;
                }
                case ScriptFieldType::ULong:
                {
                    auto val = dummy.GetValue<uint64_t>();
                    int data = static_cast<int>(val);
                    if (UI::DrawIntControl(name.c_str(), &data, 1.0f, 0, INT_MAX))
                    {
                        val = static_cast<uint64_t>(data);
                        dummy.SetValue<uint64_t>(val);
                        valueChanged = true;

                    }
                    break;
                }
                case ScriptFieldType::Enum:
                {
                    auto data = dummy.GetValue<int>();
                    const auto &enumNames = field.EnumNames;
                    const auto &enumValues = field.EnumValues;

                    int selectedIndex = -1;
                    for (int i = 0; i < (int)enumValues.size(); ++i)
                    {
                        if (enumValues[i] == data)
                        {
                            selectedIndex = i;
                            break;
                        }
                    }

                    std::vector<const char *> labels;
                    for (const auto &n : enumNames)
                        labels.push_back(n.c_str());

                    if (UI::DrawComboBox(name.c_str(), labels.data(), (int)labels.size(), &selectedIndex))
                    {
                        data = enumValues[selectedIndex];
                        dummy.SetValue<int>(data);
                        valueChanged = true;

                    }
                    break;
                }
                case ScriptFieldType::Double:
                {
                    auto dData = dummy.GetValue<double>();
                    float data = static_cast<float>(dData);
                    if (UI::DrawFloatControl(name.c_str(), &data, 0.1f, -FLT_MAX, FLT_MAX))
                    {
                        dData = static_cast<double>(data);
                        dummy.SetValue<double>(dData);
                        valueChanged = true;

                    }
                    break;
                }
                case ScriptFieldType::Vector2:
                {
                    auto data = dummy.GetValue<glm::vec2>();
                    if (UI::DrawVec2Control(name.c_str(), data, 0.1f))
                    {
                        dummy.SetValue<glm::vec2>(data);
                        valueChanged = true;

                    }
                    break;
                }
                case ScriptFieldType::Vector3:
                {
                    auto data = dummy.GetValue<glm::vec3>();
                    if (UI::DrawVec3Control(name.c_str(), data, 0.1f))
                    {
                        dummy.SetValue<glm::vec3>(data);
                        valueChanged = true;

                    }
                    break;
                }
                case ScriptFieldType::Vector4:
                {
                    auto data = dummy.GetValue<glm::vec4>();
                    if (UI::DrawVec4Control(name.c_str(), data, 0.1f))
                    {
                        dummy.SetValue<glm::vec4>(data);
                        valueChanged = true;

                    }
                    break;
                }
                case ScriptFieldType::Quat:
                {
                    auto data = dummy.GetValue<glm::quat>();
                    glm::vec4 vec = { data.x, data.y, data.z, data.w };
                    if (UI::DrawVec4Control(name.c_str(), vec, 0.1f))
                    {
                        data = { vec.w, vec.x, vec.y, vec.z };
                        dummy.SetValue<glm::quat>(data);
                        valueChanged = true;

                    }
                    break;
                }
                case ScriptFieldType::Color:
                {
                    auto data = dummy.GetValue<glm::vec4>();
                    if (UI::DrawColorVec4(name.c_str(), data))
                    {
                        dummy.SetValue<glm::vec4>(data);
                        valueChanged = true;

                    }
                    break;
                }
                case ScriptFieldType::Entity:
                case ScriptFieldType::Asset:
                {
                    auto handle = dummy.GetValue<uint64_t>();
                    std::string label = "Drag Here";

                    if (field.Type == ScriptFieldType::Entity)
                    {
                        if (handle)
                        {
                            Entity e = SceneManager::GetEntity(activeScene.get(), UUID(handle));
                            if (e)
                                label = e.GetName();
                        }
                    }
                    else if (field.Type == ScriptFieldType::Asset)
                    {
                        if (handle)
                        {
                            label = assetManager->GetAssetDisplayName(AssetHandle(handle));
                        }
                    }

                    ImGui::BeginDisabled(isRunning);
                    UI::DrawButtonWithColumn(name.c_str(), label.c_str(), nullptr, [&name, &dummy, &valueChanged, handle, assetManager, activeScene, &field]()
                    {
                        if (ImGui::BeginDragDropTarget())
                        {
                            if (field.Type == ScriptFieldType::Entity)
                            {
                                if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_ENTITY_SOURCE_ITEM))
                                {
                                    LOG_ASSERT(payload->DataSize == sizeof(Entity), "WRONG ENTITY ITEM");
                                    if (payload->DataSize == sizeof(Entity))
                                    {
                                        Entity src { *static_cast<entt::entity *>(payload->Data), activeScene.get() };
                                        uint64_t id = (uint64_t)src.GetUUID();
                                        dummy.SetValue<uint64_t>(id);
                                        valueChanged = true;

                                    }
                                }
                            }
                            else if (field.Type == ScriptFieldType::Asset)
                            {
                                if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
                                {
                                    LOG_ASSERT(payload->DataSize == sizeof(AssetHandle), "WRONG ASSET ITEM");
                                    if (payload->DataSize == sizeof(AssetHandle))
                                    {
                                        AssetHandle h = *static_cast<AssetHandle *>(payload->Data);
                                        uint64_t id = (uint64_t)h;
                                        dummy.SetValue<uint64_t>(id);
                                        valueChanged = true;

                                    }
                                }
                            }
                            ImGui::EndDragDropTarget();
                        }

                        if (ImGui::IsItemHovered())
                        {
                            if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
                            {
                                uint64_t id = 0;
                                dummy.SetValue<uint64_t>(id);
                                valueChanged = true;

                            }

                            ImGui::BeginTooltip();
                            if (handle)
                            {
                                if (field.Type == ScriptFieldType::Entity)
                                {
                                    Entity e = SceneManager::GetEntity(activeScene.get(), UUID(handle));
                                    if (e) ImGui::Text("Entity Name : %s\nEntity ID : %llu", e.GetName().c_str(), handle);
                                    else ImGui::Text("Invalid Entity");
                                }
                                else
                                {
                                    AssetMetaData metadata = assetManager->GetMetaData(AssetHandle(handle));
                                    ImGui::Text("Asset Name : %s\nAsset ID : %llu\nType : %s", assetManager->GetAssetDisplayName(AssetHandle(handle)).c_str(), handle, AssetTypeToString(metadata.type).c_str());
                                }
                            }
                            else
                            {
                                ImGui::Text(field.Type == ScriptFieldType::Entity ? "Empty Entity" : "Empty Asset");
                            }
                            ImGui::EndTooltip();
                        }
                    });
                    if (handle)
                    {
                        ImGui::SameLine();
                        if (ImGui::Button("X"))
                        {
                            dummy.SetValue<uint64_t>(0);
                            valueChanged = true;

                        }
                    }
                    ImGui::EndDisabled();

                    break;
                }
            }


            if (valueChanged)
            {
                so->SetField(name, dummy);
                so->SetDirtyFlag(true);
            }

            ImGui::PopID();
        }


        return true;
    }
}
