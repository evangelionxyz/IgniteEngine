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

#ifndef FRAMEBUFFER_KEY_HPP
#define FRAMEBUFFER_KEY_HPP

#include <vector>
#include <cstdint>
#include <functional>
#include <nvrhi/nvrhi.h>

namespace ignite
{
    // A key that describes framebuffer compatibility for PSO caching.
    // IMPORTANT: intentionally excludes width/height so pipelines survive resizes.
    struct FramebufferKey
    {
        std::vector<nvrhi::Format> colorFormats;
        nvrhi::Format depthFormat = nvrhi::Format::UNKNOWN;
        uint32_t sampleCount = 1; // assumed common across attachments
        nvrhi::RasterFillMode fillMode = nvrhi::RasterFillMode::Solid; // include for variants

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
            size_t h = std::hash<uint32_t>{}(static_cast<uint32_t>(k.fillMode));
            h ^= (std::hash<uint32_t>{}(k.sampleCount) + 0x9e3779b9 + (h<<6) + (h>>2));
            h ^= (std::hash<int>{}(static_cast<int>(k.depthFormat)) + 0x9e3779b9 + (h<<6) + (h>>2));
            for (auto fmt : k.colorFormats)
            {
                h ^= (std::hash<int>{}(static_cast<int>(fmt)) + 0x9e3779b9 + (h<<6) + (h>>2));
            }
            return h;
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
            key.sampleCount = td.sampleCount; // use depth sample count if present
        }

        // Color attachments
        key.colorFormats.reserve(fbDesc.colorAttachments.size());
        for (const nvrhi::FramebufferAttachment &rtv : fbDesc.colorAttachments)
        {
            if (!rtv.texture) continue;
            const auto &cd = rtv.texture->getDesc();
            const nvrhi::Format fmt = (rtv.format != nvrhi::Format::UNKNOWN) ? rtv.format : cd.format;
            key.colorFormats.push_back(fmt);
            // If no depth attachment defined sampleCount yet, take from first color.
            if (key.sampleCount == 1 && cd.sampleCount > 1)
                key.sampleCount = cd.sampleCount;
        }

        return key;
    }
}

#endif
