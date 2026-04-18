// Copyright (c) 2026 Evangelion Manuhutu

#include "font.hpp"

#include "ignite/core/logger.hpp"
#include "ignite/core/application.hpp"
#include "ignite/core/device/device_manager.hpp"
#include "ignite/graphics/gpu_upload_sync.hpp"

#include "GlyphGeometry.h"
#include "FontGeometry.h"
#include "BitmapAtlasStorage.h"

#include <algorithm>
#include <limits>
#include <thread>
#include <type_traits>

namespace ignite
{
#define DEFAULT_ANGLE_THRESHOLD 3.0
#define LCG_MULTIPLIER 6364136223846793005ull
#define LCG_INCREMENT 1442695040888963407ull
#define THREAD_COUNT 8

    template<typename T, typename S, int N, msdf_atlas::GeneratorFunction<S, N> GenFunc>
    static Ref<Texture> CreateAndCacheAtlas(const std::vector<msdf_atlas::GlyphGeometry> &glyphs, uint32_t width, uint32_t height)
    {
        msdf_atlas::GeneratorAttributes attributes;
        attributes.config.overlapSupport = true;
        attributes.scanlinePass = true;

        msdf_atlas::ImmediateAtlasGenerator<S, N, GenFunc, msdf_atlas::BitmapAtlasStorage<T, N>> generator(width, height);
        generator.setAttributes(attributes);
        generator.setThreadCount(static_cast<int>(std::max(1u, std::thread::hardware_concurrency())));
        generator.generate(glyphs.data(), static_cast<int>(glyphs.size()));

        msdfgen::BitmapConstRef<T, N> bitmap = static_cast<msdfgen::BitmapConstRef<T, N>>(generator.atlasStorage());

        const size_t pixelCount = static_cast<size_t>(bitmap.width) * static_cast<size_t>(bitmap.height);
        std::vector<uint8_t> rgbaPixels(pixelCount * 4u, 255u);

        float minValueF = std::numeric_limits<float>::max();
        float maxValueF = std::numeric_limits<float>::lowest();
        uint8_t minValue = 255;
        uint8_t maxValue = 0;
        size_t nonZeroCount = 0;
        for (size_t i = 0; i < pixelCount; ++i)
        {
            const size_t sourceOffset = i * static_cast<size_t>(N);
            const size_t targetOffset = i * 4u;

            auto readChannel = [&](size_t channel) -> float
            {
                if constexpr (std::is_floating_point_v<T>)
                {
                    return std::clamp(static_cast<float>(bitmap.pixels[sourceOffset + channel]), 0.0f, 1.0f);
                }
                else
                {
                    return std::clamp(static_cast<float>(bitmap.pixels[sourceOffset + channel]) / 255.0f, 0.0f, 1.0f);
                }
            };

            const float rf = readChannel(0);
            const float gf = readChannel(1);
            const float bf = readChannel(2);

            const uint8_t r = static_cast<uint8_t>(rf * 255.0f + 0.5f);
            const uint8_t g = static_cast<uint8_t>(gf * 255.0f + 0.5f);
            const uint8_t b = static_cast<uint8_t>(bf * 255.0f + 0.5f);
            rgbaPixels[targetOffset + 0] = r;
            rgbaPixels[targetOffset + 1] = g;
            rgbaPixels[targetOffset + 2] = b;

            minValueF = std::min(minValueF, std::min(rf, std::min(gf, bf)));
            maxValueF = std::max(maxValueF, std::max(rf, std::max(gf, bf)));
            minValue = std::min(minValue, std::min(r, std::min(g, b)));
            maxValue = std::max(maxValue, std::max(r, std::max(g, b)));
            nonZeroCount += (r != 0) + (g != 0) + (b != 0);
        }

        LOG_INFO("[Font] MSDF atlas float range: min={} max={}", minValueF, maxValueF);
        LOG_INFO("[Font] MSDF atlas 8-bit range: min={} max={} nonZeroChannels={}/{}", minValue, maxValue, nonZeroCount, pixelCount * 3ull);

        TextureCreateInfo createInfo;
        createInfo.width = width;
        createInfo.height = height;
        createInfo.mipLevels = 1;
        createInfo.format = nvrhi::Format::RGBA8_UNORM;
        createInfo.initialState = nvrhi::ResourceStates::ShaderResource;
        createInfo.keepInitialState = true;
        createInfo.keepCpuData = true;

        Buffer buffer(rgbaPixels.data(), rgbaPixels.size());
        Ref<Texture> atlas = Texture::Create(buffer, createInfo, nullptr, "MSDF Font Atlas");
        atlas->SetReadyFlag(false);

        Application::SubmitToRenderThread([atlas]()
        {
            if (atlas)
            {
				nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();
				nvrhi::CommandListHandle cmd = device->createCommandList();

				cmd->open();
				atlas->SetData(cmd, 4);
				cmd->close();

				{
					std::lock_guard<std::mutex> queueLock(GPUUploadSync::GetQueueMutex());
					device->executeCommandList(cmd);
				}

				atlas->SetReadyFlag(true);
            }
        });

        return atlas;
    }

    Font::Font(const std::filesystem::path &filepath)
    {
        LoadGlyphs(filepath);
    }

    Font::~Font()
    {
        m_Glyphs.clear();
    }

	Ref<Font> Font::Create(const std::filesystem::path &filepath)
	{
		return CreateRef<Font>(filepath);
	}

	glm::vec2 Font::MeasureString(const std::string &str, float kerning, float linespacing) const
	{
		double x = 0.0;
		double y = 0.0;
		double maxX = 0.0;
		double minY = 0.0;

		const auto &metrics = m_FontGeometry.getMetrics();
		double fsScale = 1.0 / (metrics.ascenderY - metrics.descenderY);
		const double spaceGlypAdvance = m_FontGeometry.getGlyph(' ')->getAdvance();

		for (size_t i = 0; i < str.size(); ++i)
		{
			char character = str[i];
			if (character == '\r') continue;

			if (character == '\n')
			{
				maxX = std::max(maxX, x);
				x = 0.0;
				y -= fsScale * metrics.lineHeight + linespacing;
				minY = std::min(minY, y);
				continue;
			}

			if (character == ' ')
			{
				float advance = static_cast<float>(spaceGlypAdvance);
				if (i < str.size() - 1)
				{
					char nextCharacter = str[i + 1];
					double dAdvance;
					m_FontGeometry.getAdvance(dAdvance, character, nextCharacter);
					advance = static_cast<float>(dAdvance);
				}
				x += fsScale * advance + kerning;
				continue;
			}

			if (character == '\t')
			{
				x += 4.0 * (fsScale * spaceGlypAdvance + kerning);
				continue;
			}

			auto glyph = m_FontGeometry.getGlyph(character);
			if (!glyph) glyph = m_FontGeometry.getGlyph('?');
			if (!glyph) continue;

			double advance = glyph->getAdvance();
			if (i < str.size() - 1)
			{
				char nextCharacter = str[i + 1];
				m_FontGeometry.getAdvance(advance, character, nextCharacter);
			}
			x += fsScale * advance + kerning;
		}

		maxX = std::max(maxX, x);
		y -= fsScale * metrics.lineHeight;
		minY = std::min(minY, y);

		return glm::vec2(static_cast<float>(maxX), static_cast<float>(-minY));
	}

	void Font::LoadGlyphs(const std::filesystem::path &filepath)
	{
        msdfgen::FreetypeHandle *ft = msdfgen::initializeFreetype();

        std::string filepathStr = filepath.generic_string();
        msdfgen::FontHandle *font = msdfgen::loadFont(ft, filepathStr.c_str());
        if (!font)
        {
            LOG_ERROR("[Font] Failed to load \"{}\"", filepathStr);
            return;
        }

        struct CharsetRange
        {
            uint32_t begin;
            uint32_t end;
        };

        static const CharsetRange charsetRanges[] = { {0x0020, 0x00FF} };

        msdf_atlas::Charset charset;
        for (CharsetRange range : charsetRanges)
        {
            for (uint32_t c = range.begin; c <= range.end; ++c)
                charset.add(c);
        }

        const double fontScale = 1.0;

        m_FontGeometry = msdf_atlas::FontGeometry(&m_Glyphs);
        const int glyphsCount = m_FontGeometry.loadCharset(font, fontScale, charset);
        LOG_INFO("[Font] Loaded {} glyphs from font (out of {})", glyphsCount, charset.size());

        double emSize = 40.0;
        msdf_atlas::TightAtlasPacker packer;
        packer.setPixelRange(2.0);
        packer.setMiterLimit(1.0);
        packer.setPadding(0);
        packer.setScale(emSize);

        const int remaining = packer.pack(m_Glyphs.data(), static_cast<int>(m_Glyphs.size()));
        LOG_ASSERT(remaining == 0, "");

        int width, height;
        packer.getDimensions(width, height);
        emSize = packer.getScale();

        uint64_t coloringSeed = 0;
        bool expensiveColoring = false;
        if (expensiveColoring)
        {
            msdf_atlas::Workload([&glyphs = m_Glyphs, &coloringSeed](int i, int threadNo) -> bool
            {
                const uint64_t glyphSeed = (LCG_MULTIPLIER * (coloringSeed ^ i) + LCG_INCREMENT) * !!coloringSeed;
                glyphs[i].edgeColoring (msdfgen::edgeColoringInkTrap, DEFAULT_ANGLE_THRESHOLD, glyphSeed);
                return true;
            }, 
                static_cast<int>(m_Glyphs.size()))
                .finish(THREAD_COUNT);
        }
        else
        {
            uint64_t glyphSeed = coloringSeed;
            for (msdf_atlas::GlyphGeometry &glyph : m_Glyphs)
            {
                glyphSeed *= LCG_MULTIPLIER;
                glyph.edgeColoring(msdfgen::edgeColoringInkTrap, DEFAULT_ANGLE_THRESHOLD, glyphSeed);
            }
        }

        m_AtlasTexture = CreateAndCacheAtlas<float, float, 3, msdf_atlas::msdfGenerator>(m_Glyphs, width, height);
        SetReadyFlag(m_AtlasTexture && m_AtlasTexture->IsReady());

        msdfgen::destroyFont(font);
        msdfgen::deinitializeFreetype(ft);
	}
}
