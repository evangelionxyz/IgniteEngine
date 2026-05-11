// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef ANIMATION_2D_HPP
#define ANIMATION_2D_HPP

#include "ignite/asset/asset.hpp"
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include "ignite/core/path.hpp"

namespace ignite
{
    // Animation2D: a single animation clip (e.g. "Idle", "Run")
    // Serialized to .anim2d (YAML)
    class Animation2D : public Asset
    {
    public:
        struct Frame
        {
            glm::vec2 uv0 = glm::vec2(0.0f);
            glm::vec2 uv1 = glm::vec2(1.0f);
        };

        std::string   name;
        AssetHandle   textureHandle = AssetHandle(0);
        std::vector<Frame> frames;
        float fps  = 12.0f;
        bool  loop = true;

        // Runtime state (not serialized, per-instance on component)
        int   currentFrame = 0;
        float elapsed      = 0.0f;

        Animation2D() = default;
        explicit Animation2D(const std::string &_name) : name(_name) {}

        // Advance playback. Returns true if the frame changed.
        bool OnUpdate(float deltaTime);

        void Reset() { currentFrame = 0; elapsed = 0.0f; }

        const Frame &GetCurrentFrame() const;

        // Returns normalized playback position [0..1]
        float GetNormalizedTime() const;

        virtual bool Serialize(const ignite::Path &filepath) override;
        static Ref<Animation2D> Deserialize(const ignite::Path &filepath);
        static Ref<Animation2D> Create(const std::string &name);

        virtual AssetType GetAssetType() override { return AssetType::Animation2D; }
    };
}

#endif // ANIMATION_2D_HPP