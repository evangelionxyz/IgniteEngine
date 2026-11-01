// Copyright (c) 2026 Evangelion Manuhutu

#ifndef IGN_FRAMEBUFFER_KEY_HPP
#define IGN_FRAMEBUFFER_KEY_HPP

#include "ignite/core/hashing.hpp"
#include "render_target.hpp"
#include <NRI.h>
#include <vector>

namespace ignite
{
    struct FramebufferKey
    {
        std::vector<nri::Format> colorFormats;
        nri::Format depthFormat = nri::Format::UNKNOWN;
        uint32_t sampleCount = 1;
        nri::FillMode fillMode = nri::FillMode::SOLID;
        nri::CullMode cullMode = nri::CullMode::FRONT;

        bool operator==(const FramebufferKey &other) const noexcept
        {
            return colorFormats == other.colorFormats
                && depthFormat == other.depthFormat
                && sampleCount == other.sampleCount
                && fillMode == other.fillMode
                && cullMode == other.cullMode;
        }
    };

    struct FramebufferKeyHash
    {
        size_t operator()(const FramebufferKey &k) const noexcept
        {
            size_t seed = Hashing::HashCombineAll(k.fillMode, k.cullMode, k.sampleCount, k.depthFormat);
            Hashing::HashVector(seed, k.colorFormats);
            return seed;
        }
    };

    inline FramebufferKey MakeFramebufferKey(const RenderTarget *fb, nri::FillMode fillMode = nri::FillMode::SOLID, nri::CullMode cullMode = nri::CullMode::FRONT)
    {
        FramebufferKey key;
        key.fillMode = fillMode;
        key.cullMode = cullMode;
        if (!fb)
            return key;

        key.depthFormat = fb->GetDepthFormat();
        key.sampleCount = std::max(fb->GetSampleCount(), 1u);

        const uint32_t colorCount = fb->GetColorAttachmentCount();
        key.colorFormats.reserve(colorCount);
        for (uint32_t i = 0; i < colorCount; ++i)
        {
            const nri::Format fmt = fb->GetColorFormat(i);
            if (fmt != nri::Format::UNKNOWN)
            {
                key.colorFormats.push_back(fmt);
            }
        }

        return key;
    }

    struct DebugGridBindingKey
    {
        nri::PipelineLayout *layout = nullptr;
        nri::Descriptor *cameraBuffer = nullptr;
        nri::Descriptor *gridBuffer = nullptr;

        bool operator==(const DebugGridBindingKey &other) const noexcept
        {
            return layout == other.layout && cameraBuffer == other.cameraBuffer && gridBuffer == other.gridBuffer;
        }
    };

    struct DebugGridBindingKeyHash
    {
        size_t operator()(const DebugGridBindingKey &k) const noexcept
        {
            return Hashing::HashCombineAll(k.layout, k.cameraBuffer, k.gridBuffer);
        }
    };

    struct CompositeBindingKey
    {
        nri::PipelineLayout *layout = nullptr;
        nri::Descriptor *sceneTex = nullptr;
        nri::Descriptor *uiTex = nullptr;
        nri::Descriptor *edgeTex = nullptr;
        nri::Descriptor *bloomTex = nullptr;
        nri::Descriptor *ssaoTex = nullptr;
        nri::Descriptor *depthTex = nullptr;
        nri::Descriptor *debugTex = nullptr;
        nri::Descriptor *objectIDTex = nullptr;
        nri::Descriptor *taaHistoryTex = nullptr;
        nri::Descriptor *postProcessBuffer = nullptr;
        nri::Descriptor *sampler = nullptr;

        bool operator==(const CompositeBindingKey &other) const noexcept
        {
            return layout == other.layout && sceneTex == other.sceneTex
                && uiTex == other.uiTex && edgeTex == other.edgeTex && bloomTex == other.bloomTex
                && ssaoTex == other.ssaoTex && depthTex == other.depthTex && debugTex == other.debugTex
                && objectIDTex == other.objectIDTex && taaHistoryTex == other.taaHistoryTex
                && postProcessBuffer == other.postProcessBuffer && sampler == other.sampler;
        }
    };

    struct CompositeBindingKeyHash
    {
        size_t operator()(const CompositeBindingKey &k) const noexcept
        {
            return Hashing::HashCombineAll(k.layout, k.sceneTex, k.uiTex,
                k.edgeTex, k.bloomTex, k.ssaoTex, k.depthTex, k.debugTex,
                k.objectIDTex, k.taaHistoryTex, k.postProcessBuffer, k.sampler);
        }
    };

    struct CameraBindingKey
    {
        nri::PipelineLayout *layout = nullptr;
        nri::Descriptor *cameraBuffer = nullptr;

        bool operator==(const CameraBindingKey &other) const noexcept
        {
            return layout == other.layout && cameraBuffer == other.cameraBuffer;
        }
    };

    struct CameraBindingKeyHash
    {
        size_t operator()(const CameraBindingKey &k) const noexcept
        {
            return Hashing::HashCombineAll(k.layout, k.cameraBuffer);
        }
    };

    struct CameraLightingBindingKey
    {
        nri::PipelineLayout *layout = nullptr;
        nri::Descriptor *cameraBuffer = nullptr;
        nri::Descriptor *lightingBuffer = nullptr;

        bool operator==(const CameraLightingBindingKey &other) const noexcept
        {
            return layout == other.layout && cameraBuffer == other.cameraBuffer && lightingBuffer == other.lightingBuffer;
        }
    };

    struct CameraLightingBindingKeyHash
    {
        size_t operator()(const CameraLightingBindingKey &k) const noexcept
        {
            return Hashing::HashCombineAll(k.layout, k.cameraBuffer, k.lightingBuffer);
        }
    };

    struct FXAABindingKey
    {
        nri::PipelineLayout *layout = nullptr;
        nri::Descriptor *inputTexture = nullptr;

        bool operator==(const FXAABindingKey &other) const noexcept
        {
            return layout == other.layout && inputTexture == other.inputTexture;
        }
    };

    struct FXAABindingKeyHash
    {
        size_t operator()(const FXAABindingKey &k) const noexcept
        {
            return Hashing::HashCombineAll(k.layout, k.inputTexture);
        }
    };
}

#endif
