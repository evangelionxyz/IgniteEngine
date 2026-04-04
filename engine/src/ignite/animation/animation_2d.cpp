// Copyright (c) 2026 Evangelion Manuhutu

#include "animation_2d.hpp"
#include "ignite/core/logger.hpp"

#pragma warning(push)
#pragma warning(disable : 4275 4251)
#include <yaml-cpp/yaml.h>
#pragma warning(pop)

#include <fstream>
#include <glm/glm.hpp>

namespace ignite
{
    bool Animation2D::OnUpdate(float deltaTime)
    {
        if (frames.empty())
            return false;

        const float frameDuration = (fps > 0.0f) ? (1.0f / fps) : 1.0f;
        elapsed += deltaTime;

        if (elapsed >= frameDuration)
        {
            elapsed -= frameDuration;
            const int prevFrame = currentFrame;
            currentFrame++;

            if (currentFrame >= static_cast<int>(frames.size()))
            {
                if (loop)
                    currentFrame = 0;
                else
                {
                    currentFrame = static_cast<int>(frames.size()) - 1;
                    elapsed = 0.0f;
                }
            }

            return currentFrame != prevFrame;
        }

        return false;
    }

    const Animation2D::Frame &Animation2D::GetCurrentFrame() const
    {
        static const Frame kDefault {};
        if (frames.empty())
            return kDefault;

        const int clamped = std::max(0, std::min(currentFrame, static_cast<int>(frames.size()) - 1));
        return frames[static_cast<size_t>(clamped)];
    }

    float Animation2D::GetNormalizedTime() const
    {
        if (frames.empty() || fps <= 0.0f)
            return 0.0f;

        const float totalDuration = static_cast<float>(frames.size()) / fps;
        const float elapsed_total = (static_cast<float>(currentFrame) / fps) + elapsed;
        return std::min(elapsed_total / totalDuration, 1.0f);
    }

    bool Animation2D::Serialize(const std::filesystem::path &filepath)
    {
        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "Animation2D" << YAML::Value << YAML::BeginMap;
        out << YAML::Key << "Name" << YAML::Value << name;
        out << YAML::Key << "TextureHandle" << YAML::Value << static_cast<uint64_t>(textureHandle);
        out << YAML::Key << "FPS" << YAML::Value << fps;
        out << YAML::Key << "Loop" << YAML::Value << loop;

        out << YAML::Key << "Frames" << YAML::Value << YAML::BeginSeq;
        for (const auto &f : frames)
        {
            out << YAML::BeginMap;
            out << YAML::Key << "UV0" << YAML::Value << YAML::Flow << YAML::BeginSeq << f.uv0.x << f.uv0.y << YAML::EndSeq;
            out << YAML::Key << "UV1" << YAML::Value << YAML::Flow << YAML::BeginSeq << f.uv1.x << f.uv1.y << YAML::EndSeq;
            out << YAML::EndMap;
        }
        out << YAML::EndSeq;

        out << YAML::EndMap; // Animation2D
        out << YAML::EndMap; // root

        std::ofstream file(filepath);
        if (!file.is_open())
        {
            LOG_ERROR("[Animation2D] Failed to open file for writing: {}", filepath.string());
            return false;
        }

        file << out.c_str();
        SetDirtyFlag(false);
        LOG_INFO("[Animation2D] Serialized to {}", filepath.string());
        return true;
    }

    Ref<Animation2D> Animation2D::Deserialize(const std::filesystem::path &filepath)
    {
        if (!std::filesystem::exists(filepath))
        {
            LOG_ERROR("[Animation2D] File does not exist: {}", filepath.string());
            return nullptr;
        }

        YAML::Node root;
        try
        {
            root = YAML::LoadFile(filepath.string());
        }
        catch (const YAML::Exception &e)
        {
            LOG_ERROR("[Animation2D] YAML parse error: {}", e.what());
            return nullptr;
        }

        YAML::Node node = root["Animation2D"];
        if (!node)
            return nullptr;

        auto anim = CreateRef<Animation2D>();

        if (auto n = node["Name"])          anim->name = n.as<std::string>();
        if (auto n = node["TextureHandle"]) anim->textureHandle = AssetHandle(n.as<uint64_t>());
        if (auto n = node["FPS"])           anim->fps = n.as<float>();
        if (auto n = node["Loop"])          anim->loop = n.as<bool>();

        if (YAML::Node framesNode = node["Frames"]; framesNode && framesNode.IsSequence())
        {
            for (const auto &frameNode : framesNode)
            {
                Frame f;
                if (auto uv0 = frameNode["UV0"]; uv0 && uv0.IsSequence() && uv0.size() == 2)
                    f.uv0 = { uv0[0].as<float>(), uv0[1].as<float>() };
                if (auto uv1 = frameNode["UV1"]; uv1 && uv1.IsSequence() && uv1.size() == 2)
                    f.uv1 = { uv1[0].as<float>(), uv1[1].as<float>() };
                anim->frames.push_back(f);
            }
        }

        anim->SetDirtyFlag(false);
        anim->SetReadyFlag(true);
        return anim;
    }

    Ref<Animation2D> Animation2D::Create(const std::string &_name)
    {
        auto anim = CreateRef<Animation2D>(_name);
        anim->SetReadyFlag(true);
        return anim;
    }
}