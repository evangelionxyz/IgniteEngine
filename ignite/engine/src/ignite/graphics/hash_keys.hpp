// Copyright (c) 2026 Evangelion Manuhutu

#ifndef IGN_FRAMEBUFFER_KEY_HPP
#define IGN_FRAMEBUFFER_KEY_HPP

#include "ignite/core/hashing.hpp"
#include <nvrhi/nvrhi.h>

namespace ignite
{
    struct FramebufferKey
    {
        std::vector<nvrhi::Format> colorFormats;
        nvrhi::Format depthFormat = nvrhi::Format::UNKNOWN;
        uint32_t sampleCount = 1;
        nvrhi::RasterFillMode fillMode = nvrhi::RasterFillMode::Solid;

        bool operator==(const FramebufferKey &other) const noexcept
        {
            return colorFormats == other.colorFormats
                && depthFormat == other.depthFormat
                && sampleCount == other.sampleCount
                && fillMode == other.fillMode;
        }
    };

    struct FramebufferKeyHash
    {
        size_t operator()(const FramebufferKey &k) const noexcept
        {
			size_t seed = Hashing::HashCombineAll(k.fillMode, k.sampleCount, k.depthFormat);
			Hashing::HashVector(seed, k.colorFormats);
            return seed;
        }
    };

    inline FramebufferKey MakeFramebufferKey(nvrhi::IFramebuffer *fb, nvrhi::RasterFillMode fillMode = nvrhi::RasterFillMode::Solid)
    {
        FramebufferKey key;
        key.fillMode = fillMode;
        if (!fb)
            return key;

        const nvrhi::FramebufferDesc &fbDesc = fb->getDesc();

        // Depth attachment
        const nvrhi::FramebufferAttachment &datt = fbDesc.depthAttachment;
        if (datt.texture)
        {
            const auto &td = datt.texture->getDesc();
            key.depthFormat = (datt.format != nvrhi::Format::UNKNOWN) ? datt.format : td.format;
            key.sampleCount = td.sampleCount;
        }

        // Color attachments
        key.colorFormats.reserve(fbDesc.colorAttachments.size());
        for (const nvrhi::FramebufferAttachment &rtv : fbDesc.colorAttachments)
        {
            if (!rtv.texture) continue;
            const auto &cd = rtv.texture->getDesc();
            const nvrhi::Format fmt = (rtv.format != nvrhi::Format::UNKNOWN) ? rtv.format : cd.format;
            key.colorFormats.push_back(fmt);

            if (key.sampleCount == 1 && cd.sampleCount > 1)
                key.sampleCount = cd.sampleCount;
        }

        return key;
    }

	struct DebugGridBindingKey
	{
		nvrhi::IBindingLayout *layout = nullptr;
		nvrhi::IBuffer *gridBuffer = nullptr;

		bool operator==(const DebugGridBindingKey &other) const noexcept
		{
			return layout == other.layout && gridBuffer == other.gridBuffer;
		}
	};

	struct DebugGridBindingKeyHash
	{
		size_t operator()(const DebugGridBindingKey &k) const noexcept
		{
			return Hashing::HashCombineAll(k.layout, k.gridBuffer);
		}
	};

	struct CompositeBindingKey
	{
		nvrhi::IBindingLayout *layout = nullptr;
		nvrhi::ITexture *sceneTex = nullptr;
		nvrhi::ITexture *uiTex = nullptr;
		nvrhi::ITexture *edgeTex = nullptr;
		nvrhi::ITexture *bloomTex = nullptr;
		nvrhi::ITexture *ssaoTex = nullptr;
		nvrhi::ITexture *depthTex = nullptr;
		nvrhi::ITexture *debugTex = nullptr;
		nvrhi::ITexture *objectIDTex = nullptr;
		nvrhi::IBuffer *postProcessBuffer = nullptr;
		nvrhi::ISampler *sampler = nullptr;

		bool operator==(const CompositeBindingKey &other) const noexcept
		{
			return layout == other.layout && sceneTex == other.sceneTex
				&& uiTex == other.uiTex && edgeTex == other.edgeTex && bloomTex == other.bloomTex
				&& ssaoTex == other.ssaoTex && depthTex == other.depthTex && debugTex == other.debugTex
				&& objectIDTex == other.objectIDTex && postProcessBuffer == other.postProcessBuffer 
				&& sampler == other.sampler;
		}
	};

	struct CompositeBindingKeyHash
	{
		size_t operator()(const CompositeBindingKey &k) const noexcept
		{
			return Hashing::HashCombineAll(k.layout, k.sceneTex, k.uiTex, 
				k.edgeTex, k.bloomTex, k.ssaoTex, k.depthTex, k.debugTex, 
				k.objectIDTex, k.postProcessBuffer, k.sampler);
		}
	};

	struct CameraBindingKey
	{
		nvrhi::IBindingLayout *layout = nullptr;
		nvrhi::IBuffer *cameraBuffer = nullptr;

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
		nvrhi::IBindingLayout *layout = nullptr;
		nvrhi::IBuffer *cameraBuffer = nullptr;
		nvrhi::IBuffer *lightingBuffer = nullptr;

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
}

#endif
