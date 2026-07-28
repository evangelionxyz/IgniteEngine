// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_BLEND_SPACE_HPP
#define IGN_BLEND_SPACE_HPP

#include "ignite/asset/asset.hpp"

#include <vector>
#include <glm/glm.hpp>

namespace ignite
{
    struct IGN_API BlendSpaceSample
    {
        glm::vec2 position = glm::vec2(0.0f);

        BlendSpaceSample() = default;

        void SetAnimationHandle(const AssetHandle &animationHandle) { m_AnimationHandle = animationHandle; }
        const AssetHandle &GetAnimationAssetHandle() const { return m_AnimationHandle; }

    private:
        AssetHandle m_AnimationHandle;
    };

    struct IGN_API BlendSpaceWeight
    {
        float weight = 0.0f;

        BlendSpaceWeight() = default;

        BlendSpaceWeight(const AssetHandle &animationHandle, float weight)
            : m_AnimationHandle(animationHandle), weight(weight)
        {
        }

		void SetAnimationHandle(const AssetHandle &animationHandle) { m_AnimationHandle = animationHandle; }
		const AssetHandle &GetAnimationAssetHandle() const { return m_AnimationHandle; }

    private:
		AssetHandle m_AnimationHandle;
    };

    class IGN_API BlendSpace : public Asset
    {
    public:
        virtual ~BlendSpace() override;

        std::string axisXName = "Horizontal";
        std::string axisYName = "Vertical";
        glm::vec2 axisMin = glm::vec2(0.0f);
        glm::vec2 axisMax = glm::vec2(1.0f);

        std::vector<BlendSpaceSample> samples;

        static Ref<BlendSpace> Create();

        void SetSkeletonAssetHandle(AssetHandle skeletonHandle);
        const AssetHandle &GetSkeletonAssetHandle() const { return m_SkeletonHandle; }

        // Returns normalized weights for the samples nearest to input.  The
        // inverse-distance form deliberately supports sparse and irregular 2D
        // layouts while still giving an exact sample a weight of one.
        std::vector<BlendSpaceWeight> Evaluate(const glm::vec2 &input) const;
        glm::vec2 ClampInput(const glm::vec2 &input) const;
        glm::vec2 NormalizePosition(const glm::vec2 &pos) const;

        virtual bool Serialize(const ignite::Path &filepath) override;
        static Ref<BlendSpace> Deserialize(const ignite::Path &filepath);

        static AssetType GetStaticType() { return AssetType::BlendSpace; }
        virtual AssetType GetAssetType() override { return GetStaticType(); }

    private:
        AssetHandle m_SkeletonHandle = AssetHandle(0);
    };
}

#endif
