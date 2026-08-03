// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_PROCEDURAL_SKY_HPP
#define IGN_PROCEDURAL_SKY_HPP

#include "ignite/core/base.hpp"
#include "ignite/graphics/texture.hpp"
#include "ignite/graphics/shader.hpp"
#include <nvrhi/nvrhi.h>
#include <glm/glm.hpp>

namespace ignite
{
    enum class SkyType : uint32_t
    {
        HDRI = 0,
        ProceduralSky = 1
    };

    struct AtmosphereParams
    {
        glm::vec3 rayleighScattering = glm::vec3(5.802e-3f, 13.558e-3f, 33.1e-3f); // per km
        float     rayleighDensityH   = 8.0f;                                       // km

        glm::vec3 mieScattering      = glm::vec3(3.996e-3f);                       // per km
        float     mieDensityH        = 1.2f;                                       // km

        glm::vec3 mieExtinction      = glm::vec3(3.996e-3f * 1.11f);               // per km
        float     mieG               = 0.8f;

        glm::vec3 ozoneAbsorption    = glm::vec3(0.650e-3f, 1.881e-3f, 0.085e-3f); // per km
        float     planetRadius       = 6360.0f;                                    // km

        glm::vec3 groundAlbedo       = glm::vec3(0.1f);
        float     atmosphereRadius   = 6460.0f;                                    // km

        glm::vec4 sunDirectionAndIntensity = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);    // xyz = direction, w = intensity
        glm::vec4 sunColorAndRadius        = glm::vec4(1.0f, 1.0f, 1.0f, 0.00935f);  // rgb = color, a = angular radius
        glm::vec4 cameraPositionAndAltitude = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);   // xyz = camera pos (meters), w = altitude (km)
    };

    class IGN_API ProceduralSky
    {
    public:
        ProceduralSky();
        ~ProceduralSky();

        void Init();
        void RenderLUTs(nvrhi::ICommandList *cmd, const glm::vec3 &sunDir, const glm::vec3 &sunColor, float sunIntensity, float sunAngularRadius, const glm::vec3 &cameraPos = glm::vec3(0.0f));

        Ref<Texture> GetSkyViewLUT() const { return m_SkyViewLUTTexture; }
        Ref<Texture> GetTransmittanceLUT() const { return m_TransmittanceLUTTexture; }

        AtmosphereParams &GetParams() { return m_Params; }
        const AtmosphereParams &GetParams() const { return m_Params; }

        void MarkDirty() { m_DirtyAtmosphere = true; }

    private:
        void CreateTextures();
        void CreatePipelines();

        AtmosphereParams m_Params;
        bool m_DirtyAtmosphere = true;
        bool m_Initialized = false;

        Ref<Texture> m_TransmittanceLUTTexture;
        Ref<Texture> m_MultiScatteringLUTTexture;
        Ref<Texture> m_SkyViewLUTTexture;

        Ref<Shader> m_TransmittanceShader;
        Ref<Shader> m_MultiScatteringShader;
        Ref<Shader> m_SkyViewShader;

        nvrhi::BindingLayoutHandle m_TransmittanceLayout;
        nvrhi::BindingLayoutHandle m_MultiScatteringLayout;
        nvrhi::BindingLayoutHandle m_SkyViewLayout;

        nvrhi::ComputePipelineHandle m_TransmittancePipeline;
        nvrhi::ComputePipelineHandle m_MultiScatteringPipeline;
        nvrhi::ComputePipelineHandle m_SkyViewPipeline;

        nvrhi::BufferHandle m_AtmosphereBuffer;
        nvrhi::SamplerHandle m_LinearSampler;
    };
}

#endif
