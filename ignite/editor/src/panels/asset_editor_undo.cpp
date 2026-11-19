// Copyright (c) 2026 Evangelion Manuhutu

#include "pch.hpp"
#include "asset_editor_undo.hpp"
#include "asset_editor_panel.hpp"
#include "asset_editor_data.hpp"

#include "ignite/animation/animation_2d.hpp"
#include "ignite/animation/animation_montage.hpp"
#include "ignite/animation/animator/animator_controller.hpp"
#include "ignite/animation/animator/animator_controller_2d.hpp"
#include "ignite/animation/blend_space.hpp"
#include "ignite/animation/skeletal_animation.hpp"
#include "ignite/animation/skeleton.hpp"
#include "ignite/graphics/objects/material.hpp"
#include "ignite/graphics/objects/material_2d.hpp"
#include "ignite/graphics/objects/mesh.hpp"
#include "ignite/graphics/texture.hpp"
#include "ignite/graphics/ui/widget_canvas.hpp"
#include "ignite/scene/sprite_sheet.hpp"
#include "ignite/scripting/scriptable_object.hpp"
#include "ignite/terrain/terrain.hpp"

#include <algorithm>
#include <cstring>

namespace ignite
{
    namespace
    {
        bool EqualAnimConditions(const AnimCondition &a, const AnimCondition &b)
        {
            return a.paramName == b.paramName
                && a.strThreshold == b.strThreshold
                && a.floatThreshold == b.floatThreshold
                && a.intThreshold == b.intThreshold
                && a.boolThreshold == b.boolThreshold
                && a.op == b.op;
        }

        bool EqualAnimTransitions(const AnimTransition &a, const AnimTransition &b)
        {
            if (a.fromState != b.fromState
                || a.toState != b.toState
                || a.hasExitTime != b.hasExitTime
                || a.exitTime != b.exitTime
                || a.transitionDuration != b.transitionDuration
                || a.conditions.size() != b.conditions.size())
            {
                return false;
            }
            for (size_t i = 0; i < a.conditions.size(); ++i)
            {
                if (!EqualAnimConditions(a.conditions[i], b.conditions[i]))
                    return false;
            }
            return true;
        }

        bool EqualAnimParams(const AnimParam &a, const AnimParam &b)
        {
            return a.name == b.name
                && a.strVal == b.strVal
                && a.floatVal == b.floatVal
                && a.intVal == b.intVal
                && a.boolVal == b.boolVal
                && a.type == b.type;
        }

        bool EqualTransform(const Transform &a, const Transform &b)
        {
            return a.translation == b.translation
                && a.rotation == b.rotation
                && a.scale == b.scale;
        }

        bool EqualMaterialGpuData(const Material_GPUData &a, const Material_GPUData &b)
        {
            return a.baseColorFactor == b.baseColorFactor
                && a.emissiveFactor == b.emissiveFactor
                && a.metallicFactor == b.metallicFactor
                && a.roughnessFactor == b.roughnessFactor
                && a.occlusionStrength == b.occlusionStrength
                && a.metallicChannel == b.metallicChannel
                && a.roughnessChannel == b.roughnessChannel
                && a.blendMode == b.blendMode
                && a.tilingFactor == b.tilingFactor
                && a.baseColorTextureIndex == b.baseColorTextureIndex
                && a.emissiveTextureIndex == b.emissiveTextureIndex
                && a.metallicTextureIndex == b.metallicTextureIndex
                && a.roughnessTextureIndex == b.roughnessTextureIndex
                && a.normalTextureIndex == b.normalTextureIndex
                && a.occlusionTextureIndex == b.occlusionTextureIndex
                && a.baseWeight == b.baseWeight
                && a.specularWeight == b.specularWeight
                && a.specularIOR == b.specularIOR
                && a.coatWeight == b.coatWeight
                && a.specularColor == b.specularColor
                && a.coatColor == b.coatColor
                && a.coatRoughness == b.coatRoughness
                && a.coatIOR == b.coatIOR
                && a.coatDarkening == b.coatDarkening
                && a.specularAnisotropy == b.specularAnisotropy
                && a.subsurfaceWeight == b.subsurfaceWeight
                && a.subsurfaceScale == b.subsurfaceScale
                && a.subsurfaceColor == b.subsurfaceColor
                && a.subsurfaceRadius == b.subsurfaceRadius
                && a.transmissionWeight == b.transmissionWeight
                && a.transmissionDepth == b.transmissionDepth
                && a.transmissionColor == b.transmissionColor
                && a.fuzzWeight == b.fuzzWeight
                && a.fuzzRoughness == b.fuzzRoughness
                && a.fuzzColor == b.fuzzColor
                && a.useMultiScatter == b.useMultiScatter
                && a.emissionLuminance == b.emissionLuminance;
        }

        std::vector<std::pair<std::string, AnimParam>> CaptureSortedParams(const std::unordered_map<std::string, AnimParam> &params)
        {
            std::vector<std::pair<std::string, AnimParam>> out(params.begin(), params.end());
            std::sort(out.begin(), out.end(), [](const auto &lhs, const auto &rhs)
            {
                return lhs.first < rhs.first;
            });
            return out;
        }

        size_t EstimateStringVectorBytes(const std::vector<AnimTransition> &transitions)
        {
            size_t total = transitions.capacity() * sizeof(AnimTransition);
            for (const auto &t : transitions)
            {
                total += t.fromState.capacity() + t.toState.capacity() + t.conditions.capacity() * sizeof(AnimCondition);
                for (const auto &c : t.conditions)
                {
                    total += c.paramName.capacity() + c.strThreshold.capacity();
                }
            }
            return total;
        }

        // 1. Material
        class MaterialSnapshot final : public IAssetSnapshot
        {
        public:
            explicit MaterialSnapshot(const Material &mat)
                : name(mat.name)
                , baseColorTextureHandle(mat.baseColorTextureHandle)
                , emissiveTextureHandle(mat.emissiveTextureHandle)
                , metallicTextureHandle(mat.metallicTextureHandle)
                , roughnessTextureHandle(mat.roughnessTextureHandle)
                , normalTextureHandle(mat.normalTextureHandle)
                , occlusionTextureHandle(mat.occlusionTextureHandle)
                , type(mat.GetType())
                , cullMode(mat.GetCullMode())
                , gpuData(mat.gpuData)
            {
            }

            bool Equals(const IAssetSnapshot &other) const override
            {
                const auto *o = dynamic_cast<const MaterialSnapshot *>(&other);
                if (!o) return false;
                return name == o->name
                    && baseColorTextureHandle == o->baseColorTextureHandle
                    && emissiveTextureHandle == o->emissiveTextureHandle
                    && metallicTextureHandle == o->metallicTextureHandle
                    && roughnessTextureHandle == o->roughnessTextureHandle
                    && normalTextureHandle == o->normalTextureHandle
                    && occlusionTextureHandle == o->occlusionTextureHandle
                    && type == o->type
                    && cullMode == o->cullMode
                    && EqualMaterialGpuData(gpuData, o->gpuData);
            }

            void Apply(UI::AssetEditorData &assetData) const override
            {
                if (!assetData.asset) return;
                if (Ref<Material> mat = assetData.asset->As<Material>())
                {
                    mat->name = name;
                    mat->baseColorTextureHandle = baseColorTextureHandle;
                    mat->emissiveTextureHandle = emissiveTextureHandle;
                    mat->metallicTextureHandle = metallicTextureHandle;
                    mat->roughnessTextureHandle = roughnessTextureHandle;
                    mat->normalTextureHandle = normalTextureHandle;
                    mat->occlusionTextureHandle = occlusionTextureHandle;
                    mat->SetType(type);
                    mat->SetCullMode(cullMode);
                    mat->gpuData = gpuData;
                    mat->InvalidateBindingSet();
                    mat->NotifyChange();
                }
            }

            size_t GetMemoryUsage() const override { return sizeof(*this) + name.capacity(); }

        private:
            std::string name;
            AssetHandle baseColorTextureHandle;
            AssetHandle emissiveTextureHandle;
            AssetHandle metallicTextureHandle;
            AssetHandle roughnessTextureHandle;
            AssetHandle normalTextureHandle;
            AssetHandle occlusionTextureHandle;
            MaterialType type;
            nri::CullMode cullMode;
            Material_GPUData gpuData;
        };

        // 2. Material2D
        class Material2DSnapshot final : public IAssetSnapshot
        {
        public:
            explicit Material2DSnapshot(const Material2D &mat)
                : name(mat.name)
                , textureHandle(mat.textureHandle)
                , data(mat.data)
            {
            }

            bool Equals(const IAssetSnapshot &other) const override
            {
                const auto *o = dynamic_cast<const Material2DSnapshot *>(&other);
                if (!o) return false;
                return name == o->name
                    && textureHandle == o->textureHandle
                    && data.baseColor == o->data.baseColor
                    && data.additiveColor == o->data.additiveColor
                    && data.tilingFactor == o->data.tilingFactor
                    && data.flipX == o->data.flipX
                    && data.flipY == o->data.flipY
                    && data.type == o->data.type;
            }

            void Apply(UI::AssetEditorData &assetData) const override
            {
                if (!assetData.asset) return;
                if (Ref<Material2D> mat = assetData.asset->As<Material2D>())
                {
                    mat->name = name;
                    mat->textureHandle = textureHandle;
                    mat->data = data;
                    mat->NotifyChange();
                }
            }

            size_t GetMemoryUsage() const override { return sizeof(*this) + name.capacity(); }

        private:
            std::string name;
            AssetHandle textureHandle;
            Material2DData data;
        };

        // 3. SpriteSheet
        class SpriteSheetSnapshot final : public IAssetSnapshot
        {
        public:
            explicit SpriteSheetSnapshot(const SpriteSheet &ss)
                : textureHandle(ss.GetTextureHandle())
                , atlasSize(ss.GetAtlasSize())
                , sprites(ss.GetSprites())
            {
            }

            bool Equals(const IAssetSnapshot &other) const override
            {
                const auto *o = dynamic_cast<const SpriteSheetSnapshot *>(&other);
                if (!o) return false;
                if (textureHandle != o->textureHandle || atlasSize != o->atlasSize || sprites.size() != o->sprites.size())
                    return false;
                for (size_t i = 0; i < sprites.size(); ++i)
                {
                    if (sprites[i].uv0 != o->sprites[i].uv0 || sprites[i].uv1 != o->sprites[i].uv1)
                        return false;
                }
                return true;
            }

            void Apply(UI::AssetEditorData &assetData) const override
            {
                if (!assetData.asset) return;
                if (Ref<SpriteSheet> ss = assetData.asset->As<SpriteSheet>())
                {
                    ss->SetTextureHandle(textureHandle);
                    ss->SetAtlasSize(atlasSize);
                    ss->GetSprites() = sprites;
                    ss->NotifyChange();
                }
            }

            size_t GetMemoryUsage() const override
            {
                return sizeof(*this) + sprites.capacity() * sizeof(SpriteSheet::Data);
            }

        private:
            AssetHandle textureHandle;
            glm::vec2 atlasSize;
            std::vector<SpriteSheet::Data> sprites;
        };

        // 4. Animation2D
        class Animation2DSnapshot final : public IAssetSnapshot
        {
        public:
            explicit Animation2DSnapshot(const Animation2D &anim)
                : name(anim.name)
                , textureHandle(anim.textureHandle)
                , fps(anim.fps)
                , loop(anim.loop)
                , frames(anim.frames)
            {
            }

            bool Equals(const IAssetSnapshot &other) const override
            {
                const auto *o = dynamic_cast<const Animation2DSnapshot *>(&other);
                if (!o) return false;
                if (name != o->name || textureHandle != o->textureHandle || fps != o->fps || loop != o->loop || frames.size() != o->frames.size())
                    return false;
                for (size_t i = 0; i < frames.size(); ++i)
                {
                    if (frames[i].uv0 != o->frames[i].uv0 || frames[i].uv1 != o->frames[i].uv1)
                        return false;
                }
                return true;
            }

            void Apply(UI::AssetEditorData &assetData) const override
            {
                if (!assetData.asset) return;
                if (Ref<Animation2D> anim = assetData.asset->As<Animation2D>())
                {
                    anim->name = name;
                    anim->textureHandle = textureHandle;
                    anim->fps = fps;
                    anim->loop = loop;
                    anim->frames = frames;
                    anim->NotifyChange();
                }
            }

            size_t GetMemoryUsage() const override
            {
                return sizeof(*this) + name.capacity() + frames.capacity() * sizeof(Animation2D::Frame);
            }

        private:
            std::string name;
            AssetHandle textureHandle;
            float fps;
            bool loop;
            std::vector<Animation2D::Frame> frames;
        };

        // 5. AnimatorController2D
        class AnimatorController2DSnapshot final : public IAssetSnapshot
        {
        public:
            struct StateData
            {
                std::string name;
                glm::vec2 editorPos;
                AssetHandle animHandle;
            };

            explicit AnimatorController2DSnapshot(const AnimatorController2D &ctrl)
                : defaultState(ctrl.defaultState)
                , params(CaptureSortedParams(ctrl.params))
                , transitions(ctrl.transitions)
            {
                states.reserve(ctrl.states.size());
                for (const AnimState2D &st : ctrl.states)
                {
                    states.push_back({ st.name, st.editorPos, st.GetAnimationAssetHandle() });
                }
            }

            bool Equals(const IAssetSnapshot &other) const override
            {
                const auto *o = dynamic_cast<const AnimatorController2DSnapshot *>(&other);
                if (!o) return false;
                if (defaultState != o->defaultState || states.size() != o->states.size()
                    || params.size() != o->params.size() || transitions.size() != o->transitions.size())
                {
                    return false;
                }
                for (size_t i = 0; i < states.size(); ++i)
                {
                    if (states[i].name != o->states[i].name
                        || states[i].editorPos != o->states[i].editorPos
                        || states[i].animHandle != o->states[i].animHandle)
                        return false;
                }
                for (size_t i = 0; i < params.size(); ++i)
                {
                    if (params[i].first != o->params[i].first || !EqualAnimParams(params[i].second, o->params[i].second))
                        return false;
                }
                for (size_t i = 0; i < transitions.size(); ++i)
                {
                    if (!EqualAnimTransitions(transitions[i], o->transitions[i]))
                        return false;
                }
                return true;
            }

            void Apply(UI::AssetEditorData &assetData) const override
            {
                if (!assetData.asset) return;
                if (Ref<AnimatorController2D> ctrl = assetData.asset->As<AnimatorController2D>())
                {
                    ctrl->defaultState = defaultState;
                    ctrl->states.clear();
                    ctrl->states.reserve(states.size());
                    for (const StateData &st : states)
                    {
                        AnimState2D item;
                        item.name = st.name;
                        item.editorPos = st.editorPos;
                        item.SetAnimationHandle(st.animHandle);
                        ctrl->states.push_back(std::move(item));
                    }
                    ctrl->params.clear();
                    for (const auto &[k, v] : params)
                    {
                        ctrl->params[k] = v;
                    }
                    ctrl->transitions = transitions;
                    ctrl->NotifyChange();
                }
            }

            size_t GetMemoryUsage() const override
            {
                size_t total = sizeof(*this) + defaultState.capacity() + states.capacity() * sizeof(StateData);
                for (const auto &st : states) total += st.name.capacity();
                total += params.capacity() * sizeof(std::pair<std::string, AnimParam>);
                total += EstimateStringVectorBytes(transitions);
                return total;
            }

        private:
            std::string defaultState;
            std::vector<StateData> states;
            std::vector<std::pair<std::string, AnimParam>> params;
            std::vector<AnimTransition> transitions;
        };

        // 6. AnimatorController
        class AnimatorControllerSnapshot final : public IAssetSnapshot
        {
        public:
            struct StateData
            {
                std::string key;
                std::string name;
                glm::vec2 editorPos;
                AnimState::MotionType motionType;
                AssetHandle motionHandle;
            };

            explicit AnimatorControllerSnapshot(const AnimatorController &ctrl)
                : defaultState(ctrl.defaultState)
                , skeletonHandle(ctrl.GetSkeletonHandle())
                , params(CaptureSortedParams(ctrl.params))
                , transitions(ctrl.transitions)
            {
                states.reserve(ctrl.states.size());
                for (const auto &[key, st] : ctrl.states)
                {
                    states.push_back({ key, st.name, st.editorPos, st.GetMotionType(), st.GetMotionHandle() });
                }
                std::sort(states.begin(), states.end(), [](const StateData &a, const StateData &b)
                {
                    return a.key < b.key;
                });
            }

            bool Equals(const IAssetSnapshot &other) const override
            {
                const auto *o = dynamic_cast<const AnimatorControllerSnapshot *>(&other);
                if (!o) return false;
                if (defaultState != o->defaultState || skeletonHandle != o->skeletonHandle
                    || states.size() != o->states.size()
                    || params.size() != o->params.size()
                    || transitions.size() != o->transitions.size())
                {
                    return false;
                }
                for (size_t i = 0; i < states.size(); ++i)
                {
                    if (states[i].key != o->states[i].key
                        || states[i].name != o->states[i].name
                        || states[i].editorPos != o->states[i].editorPos
                        || states[i].motionType != o->states[i].motionType
                        || states[i].motionHandle != o->states[i].motionHandle)
                        return false;
                }
                for (size_t i = 0; i < params.size(); ++i)
                {
                    if (params[i].first != o->params[i].first || !EqualAnimParams(params[i].second, o->params[i].second))
                        return false;
                }
                for (size_t i = 0; i < transitions.size(); ++i)
                {
                    if (!EqualAnimTransitions(transitions[i], o->transitions[i]))
                        return false;
                }
                return true;
            }

            void Apply(UI::AssetEditorData &assetData) const override
            {
                if (!assetData.asset) return;
                if (Ref<AnimatorController> ctrl = assetData.asset->As<AnimatorController>())
                {
                    ctrl->defaultState = defaultState;
                    ctrl->SetSkeletonHandle(skeletonHandle);
                    ctrl->states.clear();
                    for (const StateData &st : states)
                    {
                        AnimState item;
                        item.name = st.name;
                        item.editorPos = st.editorPos;
                        item.SetMotion(st.motionType, st.motionHandle);
                        ctrl->states[st.key] = std::move(item);
                    }
                    ctrl->params.clear();
                    for (const auto &[k, v] : params)
                    {
                        ctrl->params[k] = v;
                    }
                    ctrl->transitions = transitions;
                    ctrl->NotifyChange();
                }
            }

            size_t GetMemoryUsage() const override
            {
                size_t total = sizeof(*this) + defaultState.capacity() + states.capacity() * sizeof(StateData);
                for (const auto &st : states) total += st.key.capacity() + st.name.capacity();
                total += params.capacity() * sizeof(std::pair<std::string, AnimParam>);
                total += EstimateStringVectorBytes(transitions);
                return total;
            }

        private:
            std::string defaultState;
            AssetHandle skeletonHandle;
            std::vector<StateData> states;
            std::vector<std::pair<std::string, AnimParam>> params;
            std::vector<AnimTransition> transitions;
        };

        // 7. BlendSpace
        class BlendSpaceSnapshot final : public IAssetSnapshot
        {
        public:
            struct SampleData
            {
                glm::vec2 position;
                AssetHandle animHandle;
            };

            explicit BlendSpaceSnapshot(const BlendSpace &bs)
                : skeletonHandle(bs.GetSkeletonAssetHandle())
                , axisXName(bs.axisXName)
                , axisYName(bs.axisYName)
                , axisMin(bs.axisMin)
                , axisMax(bs.axisMax)
                , gridDivisions(bs.gridDivisions)
                , snapToGrid(bs.snapToGrid)
                , smoothingTime(bs.smoothingTime)
                , smoothingType(bs.smoothingType)
                , dampingRatio(bs.dampingRatio)
            {
                samples.reserve(bs.samples.size());
                for (const auto &s : bs.samples)
                {
                    samples.push_back({ s.position, s.GetAnimationAssetHandle() });
                }
            }

            bool Equals(const IAssetSnapshot &other) const override
            {
                const auto *o = dynamic_cast<const BlendSpaceSnapshot *>(&other);
                if (!o) return false;
                if (skeletonHandle != o->skeletonHandle
                    || axisXName != o->axisXName
                    || axisYName != o->axisYName
                    || axisMin != o->axisMin
                    || axisMax != o->axisMax
                    || gridDivisions != o->gridDivisions
                    || snapToGrid != o->snapToGrid
                    || smoothingTime != o->smoothingTime
                    || smoothingType != o->smoothingType
                    || dampingRatio != o->dampingRatio
                    || samples.size() != o->samples.size())
                {
                    return false;
                }
                for (size_t i = 0; i < samples.size(); ++i)
                {
                    if (samples[i].position != o->samples[i].position || samples[i].animHandle != o->samples[i].animHandle)
                        return false;
                }
                return true;
            }

            void Apply(UI::AssetEditorData &assetData) const override
            {
                if (!assetData.asset) return;
                if (Ref<BlendSpace> bs = assetData.asset->As<BlendSpace>())
                {
                    bs->SetSkeletonAssetHandle(skeletonHandle);
                    bs->axisXName = axisXName;
                    bs->axisYName = axisYName;
                    bs->axisMin = axisMin;
                    bs->axisMax = axisMax;
                    bs->gridDivisions = gridDivisions;
                    bs->snapToGrid = snapToGrid;
                    bs->smoothingTime = smoothingTime;
                    bs->smoothingType = smoothingType;
                    bs->dampingRatio = dampingRatio;
                    bs->samples.clear();
                    bs->samples.reserve(samples.size());
                    for (const SampleData &s : samples)
                    {
                        BlendSpaceSample item;
                        item.position = s.position;
                        item.SetAnimationHandle(s.animHandle);
                        bs->samples.push_back(item);
                    }
                    bs->NotifyChange();
                }
            }

            size_t GetMemoryUsage() const override
            {
                return sizeof(*this) + axisXName.capacity() + axisYName.capacity() + samples.capacity() * sizeof(SampleData);
            }

        private:
            AssetHandle skeletonHandle;
            std::string axisXName;
            std::string axisYName;
            glm::vec2 axisMin;
            glm::vec2 axisMax;
            glm::ivec2 gridDivisions;
            bool snapToGrid;
            glm::vec2 smoothingTime;
            BlendSpaceSmoothingType smoothingType;
            float dampingRatio;
            std::vector<SampleData> samples;
        };

        // 8. SkeletalAnimation
        class SkeletalAnimationSnapshot final : public IAssetSnapshot
        {
        public:
            struct EventData
            {
                float normalizedTime;
                std::string name;
                AnimationTimelineEvent::Action action;
                AssetHandle audioHandle;
                AssetHandle callbackAsset;
            };

            explicit SkeletalAnimationSnapshot(const SkeletalAnimation &anim)
                : name(anim.name)
                , skeletonHandle(anim.GetSkeletonHandle())
                , duration(anim.duration)
                , ticksPerSeconds(anim.ticksPerSeconds)
                , rootMotion(anim.rootMotion)
                , inPlace(anim.inPlace)
            {
                events.reserve(anim.timelineEvents.size());
                for (const auto &ev : anim.timelineEvents)
                {
                    events.push_back({ ev.normalizedTime, ev.name, ev.action, ev.GetAudioHandle(), ev.GetCallbackAsset() });
                }
            }

            bool Equals(const IAssetSnapshot &other) const override
            {
                const auto *o = dynamic_cast<const SkeletalAnimationSnapshot *>(&other);
                if (!o) return false;
                if (name != o->name
                    || skeletonHandle != o->skeletonHandle
                    || duration != o->duration
                    || ticksPerSeconds != o->ticksPerSeconds
                    || rootMotion != o->rootMotion
                    || inPlace != o->inPlace
                    || events.size() != o->events.size())
                {
                    return false;
                }
                for (size_t i = 0; i < events.size(); ++i)
                {
                    if (events[i].normalizedTime != o->events[i].normalizedTime
                        || events[i].name != o->events[i].name
                        || events[i].action != o->events[i].action
                        || events[i].audioHandle != o->events[i].audioHandle
                        || events[i].callbackAsset != o->events[i].callbackAsset)
                    {
                        return false;
                    }
                }
                return true;
            }

            void Apply(UI::AssetEditorData &assetData) const override
            {
                if (!assetData.asset) return;
                if (Ref<SkeletalAnimation> anim = assetData.asset->As<SkeletalAnimation>())
                {
                    anim->name = name;
                    anim->SetSkeletonHandle(skeletonHandle);
                    anim->duration = duration;
                    anim->ticksPerSeconds = ticksPerSeconds;
                    anim->rootMotion = rootMotion;
                    anim->inPlace = inPlace;
                    anim->timelineEvents.clear();
                    anim->timelineEvents.reserve(events.size());
                    for (const EventData &ev : events)
                    {
                        AnimationTimelineEvent item;
                        item.normalizedTime = ev.normalizedTime;
                        item.name = ev.name;
                        item.action = ev.action;
                        item.SetAudioHandle(ev.audioHandle);
                        item.SetCallbackAsset(ev.callbackAsset);
                        anim->timelineEvents.push_back(std::move(item));
                    }
                    anim->NotifyChange();
                }
            }

            size_t GetMemoryUsage() const override
            {
                size_t total = sizeof(*this) + name.capacity() + events.capacity() * sizeof(EventData);
                for (const auto &ev : events) total += ev.name.capacity();
                return total;
            }

        private:
            std::string name;
            AssetHandle skeletonHandle;
            float duration;
            float ticksPerSeconds;
            bool rootMotion;
            bool inPlace;
            std::vector<EventData> events;
        };

        // 9. AnimationMontage
        class AnimationMontageSnapshot final : public IAssetSnapshot
        {
        public:
            struct NotifyData
            {
                std::string name;
                float startTime;
                float endTime;
            };

            struct CallbackData
            {
                float timestep;
                AnimationTimelineEvent::Action actionType;
                std::string callbackName;
                AssetHandle audioHandle;
            };

            explicit AnimationMontageSnapshot(const AnimationMontage &montage)
                : name(montage.name)
                , animationHandle(montage.GetAnimationHandle())
                , skeletonHandle(montage.GetSkeletonHandle())
                , maskedJoints(montage.GetMaskedJoints())
            {
                for (const auto &[k, n] : montage.GetAnimNotifies())
                {
                    notifies.push_back({ k, n.startTime, n.endTime });
                }
                std::sort(notifies.begin(), notifies.end(), [](const NotifyData &a, const NotifyData &b)
                {
                    return a.name < b.name;
                });

                for (const auto &cb : montage.GetNotifyCallbacks())
                {
                    callbacks.push_back({ cb.timestep, cb.actionType, cb.callbackName, cb.audioHandle });
                }
            }

            bool Equals(const IAssetSnapshot &other) const override
            {
                const auto *o = dynamic_cast<const AnimationMontageSnapshot *>(&other);
                if (!o) return false;
                if (name != o->name
                    || animationHandle != o->animationHandle
                    || skeletonHandle != o->skeletonHandle
                    || maskedJoints != o->maskedJoints
                    || notifies.size() != o->notifies.size()
                    || callbacks.size() != o->callbacks.size())
                {
                    return false;
                }
                for (size_t i = 0; i < notifies.size(); ++i)
                {
                    if (notifies[i].name != o->notifies[i].name
                        || notifies[i].startTime != o->notifies[i].startTime
                        || notifies[i].endTime != o->notifies[i].endTime)
                        return false;
                }
                for (size_t i = 0; i < callbacks.size(); ++i)
                {
                    if (callbacks[i].timestep != o->callbacks[i].timestep
                        || callbacks[i].actionType != o->callbacks[i].actionType
                        || callbacks[i].callbackName != o->callbacks[i].callbackName
                        || callbacks[i].audioHandle != o->callbacks[i].audioHandle)
                        return false;
                }
                return true;
            }

            void Apply(UI::AssetEditorData &assetData) const override
            {
                if (!assetData.asset) return;
                if (Ref<AnimationMontage> montage = assetData.asset->As<AnimationMontage>())
                {
                    montage->name = name;
                    montage->SetAnimationHandle(animationHandle);
                    montage->SetSkeletonHandle(skeletonHandle);
                    montage->SetMaskedJoints(maskedJoints);
                    montage->GetAnimNotifies().clear();
                    for (const auto &n : notifies)
                    {
                        montage->GetAnimNotifies()[n.name] = AnimNotif(n.startTime, n.endTime);
                    }
                    montage->GetNotifyCallbacks().clear();
                    for (const auto &cb : callbacks)
                    {
                        AnimNotifyCallback item(cb.timestep, cb.actionType, cb.callbackName);
                        item.audioHandle = cb.audioHandle;
                        montage->GetNotifyCallbacks().push_back(std::move(item));
                    }
                    montage->NotifyChange();
                }
            }

            size_t GetMemoryUsage() const override
            {
                size_t total = sizeof(*this) + name.capacity() + maskedJoints.capacity() * sizeof(int32_t);
                total += notifies.capacity() * sizeof(NotifyData) + callbacks.capacity() * sizeof(CallbackData);
                for (const auto &n : notifies) total += n.name.capacity();
                for (const auto &cb : callbacks) total += cb.callbackName.capacity();
                return total;
            }

        private:
            std::string name;
            AssetHandle animationHandle;
            AssetHandle skeletonHandle;
            std::vector<int32_t> maskedJoints;
            std::vector<NotifyData> notifies;
            std::vector<CallbackData> callbacks;
        };

        // 10. Skeleton
        class SkeletonSnapshot final : public IAssetSnapshot
        {
        public:
            struct JointData
            {
                std::string name;
                Transform defaultTransform;
            };

            explicit SkeletonSnapshot(const Skeleton &skeleton)
                : sockets(skeleton.sockets)
            {
                joints.reserve(skeleton.joints.size());
                for (const Joint &j : skeleton.joints)
                {
                    joints.push_back({ j.name, j.defaultTransform });
                }
            }

            bool Equals(const IAssetSnapshot &other) const override
            {
                const auto *o = dynamic_cast<const SkeletonSnapshot *>(&other);
                if (!o) return false;
                if (joints.size() != o->joints.size() || sockets.size() != o->sockets.size())
                    return false;
                for (size_t i = 0; i < joints.size(); ++i)
                {
                    if (joints[i].name != o->joints[i].name || !EqualTransform(joints[i].defaultTransform, o->joints[i].defaultTransform))
                        return false;
                }
                for (size_t i = 0; i < sockets.size(); ++i)
                {
                    if (sockets[i].name != o->sockets[i].name
                        || sockets[i].parentJointId != o->sockets[i].parentJointId
                        || !EqualTransform(sockets[i].local, o->sockets[i].local))
                        return false;
                }
                return true;
            }

            void Apply(UI::AssetEditorData &assetData) const override
            {
                if (!assetData.asset) return;
                if (Ref<Skeleton> skeleton = assetData.asset->As<Skeleton>())
                {
                    skeleton->nameToJointMap.clear();
                    for (size_t i = 0; i < joints.size() && i < skeleton->joints.size(); ++i)
                    {
                        skeleton->joints[i].name = joints[i].name;
                        skeleton->joints[i].defaultTransform = joints[i].defaultTransform;
                        skeleton->nameToJointMap[joints[i].name] = skeleton->joints[i].id;
                    }
                    skeleton->sockets = sockets;
                    skeleton->RebuildSocketMap();
                    skeleton->UpdateGlobalTransforms();
                    skeleton->NotifyChange();
                }
            }

            size_t GetMemoryUsage() const override
            {
                size_t total = sizeof(*this) + joints.capacity() * sizeof(JointData) + sockets.capacity() * sizeof(JointSocket);
                for (const auto &j : joints) total += j.name.capacity();
                for (const auto &s : sockets) total += s.name.capacity();
                return total;
            }

        private:
            std::vector<JointData> joints;
            std::vector<JointSocket> sockets;
        };

        // 11. StaticMesh
        class StaticMeshSnapshot final : public IAssetSnapshot
        {
        public:
            explicit StaticMeshSnapshot(const StaticMesh &mesh)
            {
                const auto &instances = mesh.GetMeshInstances();
                materialHandles.reserve(instances.size());
                for (const auto &inst : instances)
                {
                    materialHandles.push_back(inst ? inst->GetMaterialAssetHandle() : AssetHandle(0));
                }
            }

            bool Equals(const IAssetSnapshot &other) const override
            {
                const auto *o = dynamic_cast<const StaticMeshSnapshot *>(&other);
                return o && materialHandles == o->materialHandles;
            }

            void Apply(UI::AssetEditorData &assetData) const override
            {
                if (!assetData.asset) return;
                if (Ref<StaticMesh> mesh = assetData.asset->As<StaticMesh>())
                {
                    const auto &instances = mesh->GetMeshInstances();
                    for (size_t i = 0; i < instances.size() && i < materialHandles.size(); ++i)
                    {
                        if (instances[i])
                            instances[i]->SetMaterial(materialHandles[i]);
                    }
                    mesh->NotifyChange();
                }
            }

            size_t GetMemoryUsage() const override
            {
                return sizeof(*this) + materialHandles.capacity() * sizeof(AssetHandle);
            }

        private:
            std::vector<AssetHandle> materialHandles;
        };

        // 12. SkeletalMesh
        class SkeletalMeshSnapshot final : public IAssetSnapshot
        {
        public:
            explicit SkeletalMeshSnapshot(const SkeletalMesh &mesh)
                : skeletonHandle(mesh.GetSkeletonHandle())
                , animatorHandle(mesh.GetAnimatorHandle())
            {
                const auto &instances = mesh.GetMeshInstances();
                materialHandles.reserve(instances.size());
                for (const auto &inst : instances)
                {
                    materialHandles.push_back(inst ? inst->GetMaterialAssetHandle() : AssetHandle(0));
                }
            }

            bool Equals(const IAssetSnapshot &other) const override
            {
                const auto *o = dynamic_cast<const SkeletalMeshSnapshot *>(&other);
                return o && skeletonHandle == o->skeletonHandle && animatorHandle == o->animatorHandle && materialHandles == o->materialHandles;
            }

            void Apply(UI::AssetEditorData &assetData) const override
            {
                if (!assetData.asset) return;
                if (Ref<SkeletalMesh> mesh = assetData.asset->As<SkeletalMesh>())
                {
                    mesh->SetSkeleton(skeletonHandle);
                    mesh->SetAnimator(animatorHandle);
                    const auto &instances = mesh->GetMeshInstances();
                    for (size_t i = 0; i < instances.size() && i < materialHandles.size(); ++i)
                    {
                        if (instances[i])
                            instances[i]->SetMaterial(materialHandles[i]);
                    }
                    mesh->NotifyChange();
                }
            }

            size_t GetMemoryUsage() const override
            {
                return sizeof(*this) + materialHandles.capacity() * sizeof(AssetHandle);
            }

        private:
            AssetHandle skeletonHandle;
            AssetHandle animatorHandle;
            std::vector<AssetHandle> materialHandles;
        };

        // 13. WidgetCanvas
        class WidgetCanvasSnapshot final : public IAssetSnapshot
        {
        public:
            explicit WidgetCanvasSnapshot(const WidgetCanvas &canvas)
                : jsonState(canvas.SerializeToString())
            {
            }

            bool Equals(const IAssetSnapshot &other) const override
            {
                const auto *o = dynamic_cast<const WidgetCanvasSnapshot *>(&other);
                return o && jsonState == o->jsonState;
            }

            void Apply(UI::AssetEditorData &assetData) const override
            {
                if (!assetData.asset) return;
                if (Ref<WidgetCanvas> canvas = assetData.asset->As<WidgetCanvas>())
                {
                    canvas->DeserializeFromString(jsonState);
                    canvas->NotifyChange();
                }
            }

            size_t GetMemoryUsage() const override { return sizeof(*this) + jsonState.capacity(); }

        private:
            std::string jsonState;
        };

        // 14. ScriptableObject
        class ScriptableObjectSnapshot final : public IAssetSnapshot
        {
        public:
            explicit ScriptableObjectSnapshot(const ScriptableObject &so)
                : className(so.GetClassName())
            {
                fields.assign(so.GetFields().begin(), so.GetFields().end());
                std::sort(fields.begin(), fields.end(), [](const auto &a, const auto &b)
                {
                    return a.first < b.first;
                });
            }

            bool Equals(const IAssetSnapshot &other) const override
            {
                const auto *o = dynamic_cast<const ScriptableObjectSnapshot *>(&other);
                if (!o || className != o->className || fields.size() != o->fields.size())
                    return false;
                for (size_t i = 0; i < fields.size(); ++i)
                {
                    if (fields[i].first != o->fields[i].first)
                        return false;
                    const ScriptInstanceField &a = fields[i].second;
                    const ScriptInstanceField &b = o->fields[i].second;
                    if (a.field.Type != b.field.Type || a.field.Name != b.field.Name)
                        return false;
                    if (std::memcmp(a.GetBufferRaw(), b.GetBufferRaw(), 64) != 0)
                        return false;
                }
                return true;
            }

            void Apply(UI::AssetEditorData &assetData) const override
            {
                if (!assetData.asset) return;
                if (Ref<ScriptableObject> so = assetData.asset->As<ScriptableObject>())
                {
                    so->SetClassName(className);
                    so->GetFields().clear();
                    for (const auto &[k, v] : fields)
                    {
                        so->SetField(k, v);
                    }
                    so->NotifyChange();
                }
            }

            size_t GetMemoryUsage() const override
            {
                size_t total = sizeof(*this) + className.capacity() + fields.capacity() * sizeof(std::pair<std::string, ScriptInstanceField>);
                for (const auto &[k, v] : fields)
                {
                    total += k.capacity() + v.field.Name.capacity() + v.field.ManagedTypeName.capacity();
                }
                return total;
            }

        private:
            std::string className;
            std::vector<std::pair<std::string, ScriptInstanceField>> fields;
        };

        // 15. TerrainData
        class TerrainDataSnapshot final : public IAssetSnapshot
        {
        public:
            explicit TerrainDataSnapshot(const TerrainData &terrain)
                : resolution(terrain.resolution)
                , worldSize(terrain.worldSize)
                , maxHeight(terrain.maxHeight)
                , heightmapTextureHandle(terrain.heightmapTextureHandle)
                , heightmap(terrain.heightmap)
            {
            }

            bool Equals(const IAssetSnapshot &other) const override
            {
                const auto *o = dynamic_cast<const TerrainDataSnapshot *>(&other);
                return o && resolution == o->resolution && worldSize == o->worldSize
                    && maxHeight == o->maxHeight && heightmapTextureHandle == o->heightmapTextureHandle
                    && heightmap == o->heightmap;
            }

            void Apply(UI::AssetEditorData &assetData) const override
            {
                if (!assetData.asset) return;
                if (Ref<TerrainData> terrain = assetData.asset->As<TerrainData>())
                {
                    terrain->resolution = resolution;
                    terrain->worldSize = worldSize;
                    terrain->maxHeight = maxHeight;
                    terrain->heightmapTextureHandle = heightmapTextureHandle;
                    terrain->heightmap = heightmap;
                    terrain->dirty = true;
                    terrain->NotifyChange();
                }
            }

            size_t GetMemoryUsage() const override
            {
                return sizeof(*this) + heightmap.capacity() * sizeof(float);
            }

        private:
            uint32_t resolution;
            float worldSize;
            float maxHeight;
            AssetHandle heightmapTextureHandle;
            std::vector<float> heightmap;
        };

        // 16. Texture
        class TextureSnapshot final : public IAssetSnapshot
        {
        public:
            explicit TextureSnapshot(const TextureCreateInfo &info)
                : createInfo(info)
            {
            }

            bool Equals(const IAssetSnapshot &other) const override
            {
                const auto *o = dynamic_cast<const TextureSnapshot *>(&other);
                if (!o) return false;
                return createInfo.mipLevels == o->createInfo.mipLevels
                    && createInfo.arraySize == o->createInfo.arraySize
                    && createInfo.sampleCount == o->createInfo.sampleCount
                    && createInfo.sampleQuality == o->createInfo.sampleQuality
                    && createInfo.format == o->createInfo.format
                    && createInfo.flip == o->createInfo.flip
                    && createInfo.bindless == o->createInfo.bindless
                    && createInfo.keepCpuData == o->createInfo.keepCpuData
                    && createInfo.keepInitialState == o->createInfo.keepInitialState
                    && createInfo.samplerAddressU == o->createInfo.samplerAddressU
                    && createInfo.samplerAddressV == o->createInfo.samplerAddressV
                    && createInfo.samplerAddressW == o->createInfo.samplerAddressW
                    && createInfo.samplerLinearFiltering == o->createInfo.samplerLinearFiltering;
            }

            void Apply(UI::AssetEditorData &assetData) const override
            {
                assetData.textureCreateInfo = createInfo;
                assetData.textureCreateInfoInitialized = true;
                if (assetData.asset)
                {
                    assetData.asset->SetDirtyFlag(true);
                }
            }

            size_t GetMemoryUsage() const override { return sizeof(*this); }

        private:
            TextureCreateInfo createInfo;
        };
    }

    Ref<IAssetSnapshot> CaptureAssetSnapshot(const UI::AssetEditorData &assetData)
    {
        if (!assetData.asset)
            return nullptr;

        switch (assetData.metadata.type)
        {
            case AssetType::Material:
                if (auto a = assetData.asset->As<Material>()) return CreateRef<MaterialSnapshot>(*a);
                break;
            case AssetType::Material2D:
                if (auto a = assetData.asset->As<Material2D>()) return CreateRef<Material2DSnapshot>(*a);
                break;
            case AssetType::SpriteSheet:
                if (auto a = assetData.asset->As<SpriteSheet>()) return CreateRef<SpriteSheetSnapshot>(*a);
                break;
            case AssetType::Animation2D:
                if (auto a = assetData.asset->As<Animation2D>()) return CreateRef<Animation2DSnapshot>(*a);
                break;
            case AssetType::AnimatorController2D:
                if (auto a = assetData.asset->As<AnimatorController2D>()) return CreateRef<AnimatorController2DSnapshot>(*a);
                break;
            case AssetType::AnimatorController:
                if (auto a = assetData.asset->As<AnimatorController>()) return CreateRef<AnimatorControllerSnapshot>(*a);
                break;
            case AssetType::BlendSpace:
                if (auto a = assetData.asset->As<BlendSpace>()) return CreateRef<BlendSpaceSnapshot>(*a);
                break;
            case AssetType::SkeletalAnimation:
                if (auto a = assetData.asset->As<SkeletalAnimation>()) return CreateRef<SkeletalAnimationSnapshot>(*a);
                break;
            case AssetType::AnimationMontage:
                if (auto a = assetData.asset->As<AnimationMontage>()) return CreateRef<AnimationMontageSnapshot>(*a);
                break;
            case AssetType::Skeleton:
                if (auto a = assetData.asset->As<Skeleton>()) return CreateRef<SkeletonSnapshot>(*a);
                break;
            case AssetType::StaticMesh:
                if (auto a = assetData.asset->As<StaticMesh>()) return CreateRef<StaticMeshSnapshot>(*a);
                break;
            case AssetType::SkeletalMesh:
                if (auto a = assetData.asset->As<SkeletalMesh>()) return CreateRef<SkeletalMeshSnapshot>(*a);
                break;
            case AssetType::Widget:
                if (auto a = assetData.asset->As<WidgetCanvas>()) return CreateRef<WidgetCanvasSnapshot>(*a);
                break;
            case AssetType::ScriptableObject:
                if (auto a = assetData.asset->As<ScriptableObject>()) return CreateRef<ScriptableObjectSnapshot>(*a);
                break;
            case AssetType::Terrain:
                if (auto a = assetData.asset->As<TerrainData>()) return CreateRef<TerrainDataSnapshot>(*a);
                break;
            case AssetType::Texture:
            {
                TextureCreateInfo info{};
                if (assetData.textureCreateInfoInitialized)
                {
                    info = assetData.textureCreateInfo;
                }
                else if (auto tex = assetData.asset->As<Texture>())
                {
                    info = tex->GetCreateInfo();
                }
                return CreateRef<TextureSnapshot>(info);
            }
            default:
                break;
        }
        return nullptr;
    }

    AssetSnapshotCommand::AssetSnapshotCommand(
        AssetHandle assetHandle,
        AssetEditorPanel *panel,
        std::string name,
        Ref<IAssetSnapshot> before,
        Ref<IAssetSnapshot> after)
        : m_AssetHandle(assetHandle)
        , m_Panel(panel)
        , m_Name(std::move(name))
        , m_Before(std::move(before))
        , m_After(std::move(after))
    {
    }

    void AssetSnapshotCommand::Execute()
    {
        ApplySnapshot(m_After);
    }

    void AssetSnapshotCommand::Undo()
    {
        ApplySnapshot(m_Before);
    }

    size_t AssetSnapshotCommand::GetMemoryUsage() const
    {
        size_t total = sizeof(*this) + m_Name.capacity();
        if (m_Before) total += m_Before->GetMemoryUsage();
        if (m_After) total += m_After->GetMemoryUsage();
        return total;
    }

    void AssetSnapshotCommand::ApplySnapshot(const Ref<IAssetSnapshot> &snapshot)
    {
        if (!m_Panel || !snapshot)
            return;

        UI::AssetEditorData *assetData = m_Panel->FindAssetEditorData(m_AssetHandle);
        if (!assetData || !assetData->asset)
            return;

        snapshot->Apply(*assetData);
        assetData->undoBaselineSnapshot = snapshot;
        assetData->undoPendingBefore.reset();
        assetData->undoWasInteracting = false;

        if (auto *cmdMgr = CommandManager::GetInstance())
        {
            if (const CommandHistory *hist = cmdMgr->FindAssetHistory(static_cast<uint64_t>(m_AssetHandle)))
            {
                assetData->asset->SetDirtyFlag(!hist->IsAtSavedRevision());
            }
        }
    }
}
