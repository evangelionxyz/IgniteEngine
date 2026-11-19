// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "thumbnail_generator.hpp"
#include "ignite/asset/asset_manager.hpp"
#include "ignite/asset/asset_worker.hpp"
#include "ignite/core/application.hpp"
#include "ignite/core/device/device_manager.hpp"
#include "ignite/core/logger.hpp"
#include "ignite/core/profiler/profiler.hpp"
#include "ignite/graphics/bindless_system.hpp"
#include "ignite/graphics/gpu_upload_sync.hpp"
#include "ignite/graphics/objects/material.hpp"
#include "ignite/graphics/objects/mesh.hpp"
#include "ignite/graphics/renderer.hpp"
#include "ignite/project/project.hpp"
#include "ignite/serializer/binary_serializer.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <format>
#include <thread>

namespace ignite
{
    namespace
    {
        uint64_t HashPathFNV1a(const std::string &str)
        {
            uint64_t hash = 14695981039346656037ULL;
            for (unsigned char c : str)
            {
                hash ^= static_cast<uint64_t>(c);
                hash *= 1099511628211ULL;
            }
            return hash;
        }

        uint32_t AlignUp(uint32_t value, uint32_t alignment)
        {
            if (alignment <= 1)
                return value;
            return (value + alignment - 1u) & ~(alignment - 1u);
        }

        bool IsMaterialReadyForThumbnail(const Ref<Material> &material, AssetManager *assetManager)
        {
            if (!material || !material->IsReady())
                return false;

            if (!assetManager)
                return true;

            const AssetHandle texHandles[6] = {
                material->baseColorTextureHandle,
                material->emissiveTextureHandle,
                material->metallicTextureHandle,
                material->roughnessTextureHandle,
                material->normalTextureHandle,
                material->occlusionTextureHandle
            };

            for (AssetHandle texHandle : texHandles)
            {
                if (texHandle == AssetHandle(0) || !assetManager->IsAssetHandleValid(texHandle))
                    continue;

                const AssetState state = assetManager->GetAssetState(texHandle);
                if (state == AssetState::Failed)
                    continue;

                Ref<Texture> tex = assetManager->GetAsset<Texture>(texHandle);
                if (!tex || !tex->IsReady())
                    return false;
            }

            return true;
        }
    }

    ThumbnailGenerator::ThumbnailGenerator()
    {
        m_Camera.fov = 38.0f;
        m_Camera.nearPlane = 0.05f;
        m_Camera.farPlane = 500.0f;
        m_Camera.projectionType = ProjectionType::Perspective;
    }

    ThumbnailGenerator::~ThumbnailGenerator()
    {
        Shutdown();
    }

    ThumbnailGenerator &ThumbnailGenerator::Get()
    {
        static ThumbnailGenerator s_Instance;
        return s_Instance;
    }

    bool ThumbnailGenerator::IsSupportedAssetType(AssetType type)
    {
        return type == AssetType::Material
            || type == AssetType::StaticMesh
            || type == AssetType::SkeletalMesh;
    }

    std::filesystem::path ThumbnailGenerator::GetCacheDirectory(const Project *project)
    {
        if (!project)
            return {};
        return project->GetCacheDirectory() / "thumbnails";
    }

    std::filesystem::path ThumbnailGenerator::GetCacheFilepath(const Project *project, AssetHandle handle, const std::filesystem::path &assetPath)
    {
        const std::filesystem::path cacheDir = GetCacheDirectory(project);
        if (cacheDir.empty())
            return {};

        uint64_t key = static_cast<uint64_t>(handle);
        if (key == 0)
        {
            std::filesystem::path rel = project->GetProjectRelativeFilepath(assetPath);
            key = HashPathFNV1a(rel.lexically_normal().generic_string());
        }

        return cacheDir / std::format("{:016x}.bin", key);
    }

    int64_t ThumbnailGenerator::GetFileTimestamp(const std::filesystem::path &filepath)
    {
        if (filepath.empty())
            return 0;

        std::error_code ec;
        if (!std::filesystem::exists(filepath, ec) || ec)
            return 0;

        const auto writeTime = std::filesystem::last_write_time(filepath, ec);
        if (ec)
            return 0;

        return static_cast<int64_t>(writeTime.time_since_epoch().count());
    }

    std::string ThumbnailGenerator::MakeDedupeKey(const std::filesystem::path &assetPath, uint32_t size)
    {
        return std::format("{}#{}", assetPath.lexically_normal().generic_string(), size);
    }

    void ThumbnailGenerator::RequestThumbnail(ThumbnailRequest request)
    {
        IGN_PROFILE_FUNCTION();

        if (!IsSupportedAssetType(request.type) || request.size == 0 || request.assetPath.empty())
        {
            if (request.onComplete)
            {
                Application::SubmitToRenderThread([cb = std::move(request.onComplete)]() { cb(nullptr); },
                    "ThumbnailGenerator::RequestThumbnail - Invalid request");
            }
            return;
        }

        request.size = std::clamp(request.size, 16u, 512u);
        if (request.sourceTimestamp == 0)
        {
            request.sourceTimestamp = GetFileTimestamp(request.assetPath);
        }

        const uint64_t gen = (request.generation != 0) ? request.generation : m_Generation.load(std::memory_order_acquire);
        request.generation = gen;

        const std::string dedupeKey = MakeDedupeKey(request.assetPath, request.size);
        {
            std::lock_guard<std::mutex> lock(m_QueueMutex);
            if (!m_QueuedKeys.insert(dedupeKey).second)
            {
                return;
            }
        }

        // Fast path: check disk cache off the main/render thread so cached thumbnails never stall or re-render.
        AssetWorker::SubmitJob([this, req = std::move(request), dedupeKey]() mutable
        {
            if (m_Shutdown || req.generation != m_Generation.load(std::memory_order_acquire))
            {
                std::lock_guard<std::mutex> lock(m_QueueMutex);
                m_QueuedKeys.erase(dedupeKey);
                return;
            }

            if (!req.cachePath.empty())
            {
                ThumbnailBinaryData cached{};
                if (BinarySerializer::DeserializeThumbnail(req.cachePath, cached)
                    && cached.width == req.size
                    && cached.height == req.size
                    && cached.channels == 4
                    && (req.sourceTimestamp == 0 || cached.sourceTimestamp == req.sourceTimestamp))
                {
                    Application::SubmitToRenderThread(
                        [this, req = std::move(req), pixels = std::move(cached.pixels), dedupeKey]() mutable
                        {
                            {
                                std::lock_guard<std::mutex> lock(m_QueueMutex);
                                m_QueuedKeys.erase(dedupeKey);
                            }

                            if (m_Shutdown || req.generation != m_Generation.load(std::memory_order_acquire))
                                return;

                            Ref<Texture> tex = CreateThumbnailTextureFromPixels(
                                pixels, req.size, req.size,
                                std::format("[Thumbnail Cache] {}", req.assetPath.filename().string()));

                            if (req.onComplete)
                            {
                                req.onComplete(tex);
                            }
                        },
                        "ThumbnailGenerator::UploadCachedThumbnail");
                    return;
                }
            }

            // Cache miss: enqueue for amortized offscreen GPU scene rendering.
            std::lock_guard<std::mutex> lock(m_QueueMutex);
            if (m_Shutdown || req.generation != m_Generation.load(std::memory_order_acquire))
            {
                m_QueuedKeys.erase(dedupeKey);
                return;
            }

            m_PendingRenders.push_back(std::move(req));
        });
    }

    void ThumbnailGenerator::ProcessFrame(FrameContext *frameContext)
    {
        IGN_PROFILE_FUNCTION();

        if (m_Shutdown)
            return;

        // Always drain completed async GPU readbacks first (completely non-blocking).
        PollCompletedReadbacks();

        if (!frameContext)
            return;

        ++m_FrameCounter;
        if ((m_FrameCounter % m_RenderIntervalFrames) != 0)
            return;

        // Find a free readback slot (never stall waiting on the GPU).
        ReadbackSlot *freeSlot = nullptr;
        for (auto &slot : m_ReadbackSlots)
        {
            if (!slot.inFlight)
            {
                freeSlot = &slot;
                break;
            }
        }
        if (!freeSlot)
            return;

        // Inspect queued requests; skip entries still waiting on async asset/texture loads
        // so ready items behind them can proceed without stalling.
        ThumbnailRequest readyRequest;
        Ref<Asset> readyAsset;
        bool foundReady = false;
        std::vector<ThumbnailRequest> failedRequests;

        {
            std::lock_guard<std::mutex> lock(m_QueueMutex);
            const uint64_t currentGen = m_Generation.load(std::memory_order_acquire);
            const size_t scanCount = m_PendingRenders.size();

            for (size_t i = 0; i < scanCount; ++i)
            {
                ThumbnailRequest candidate = std::move(m_PendingRenders.front());
                m_PendingRenders.pop_front();

                if (candidate.generation != currentGen)
                {
                    m_QueuedKeys.erase(MakeDedupeKey(candidate.assetPath, candidate.size));
                    continue;
                }

                Ref<Asset> resolvedAsset;
                const ReadinessResult readiness = CheckRequestReadiness(candidate, resolvedAsset);
                if (readiness == ReadinessResult::Ready)
                {
                    readyRequest = std::move(candidate);
                    readyAsset = std::move(resolvedAsset);
                    foundReady = true;
                    break;
                }
                else if (readiness == ReadinessResult::Waiting)
                {
                    m_PendingRenders.push_back(std::move(candidate));
                }
                else
                {
                    m_QueuedKeys.erase(MakeDedupeKey(candidate.assetPath, candidate.size));
                    failedRequests.push_back(std::move(candidate));
                }
            }
        }

        for (auto &failed : failedRequests)
        {
            if (failed.onComplete)
            {
                failed.onComplete(nullptr);
            }
        }

        if (!foundReady)
            return;

        if (!RenderAndEnqueueReadback(readyRequest, readyAsset, frameContext, *freeSlot))
        {
            {
                std::lock_guard<std::mutex> lock(m_QueueMutex);
                m_QueuedKeys.erase(MakeDedupeKey(readyRequest.assetPath, readyRequest.size));
            }
            if (readyRequest.onComplete)
            {
                readyRequest.onComplete(nullptr);
            }
        }
    }

    bool ThumbnailGenerator::FlushPendingReadbacks(uint32_t timeoutMs)
    {
        auto *dm = DeviceManager::GetInstance();
        if (!dm || !dm->GetDevice() || !m_ReadbackFence)
            return true;

        const auto &nri = dm->GetCoreInterface();
        const auto start = std::chrono::steady_clock::now();

        while (true)
        {
            PollCompletedReadbacks();

            uint64_t maxPendingFence = 0;
            for (const auto &slot : m_ReadbackSlots)
            {
                if (slot.inFlight && slot.fenceValue > maxPendingFence)
                {
                    maxPendingFence = slot.fenceValue;
                }
            }

            if (maxPendingFence == 0)
                return true;

            if (nri.GetFenceValue(*m_ReadbackFence) < maxPendingFence)
            {
                const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start).count();
                if (elapsedMs >= static_cast<int64_t>(timeoutMs))
                    return false;

                nri.Wait(*m_ReadbackFence, maxPendingFence);
            }
        }
    }

    void ThumbnailGenerator::InvalidateThumbnail(const Project *project, AssetHandle handle, const std::filesystem::path &assetPath)
    {
        const std::filesystem::path cachePath = GetCacheFilepath(project, handle, assetPath);
        if (!cachePath.empty())
        {
            std::error_code ec;
            std::filesystem::remove(cachePath, ec);
        }

        const std::string normalizedPath = assetPath.lexically_normal().generic_string();
        std::lock_guard<std::mutex> lock(m_QueueMutex);
        for (auto it = m_PendingRenders.begin(); it != m_PendingRenders.end();)
        {
            const bool handleMatch = (handle != AssetHandle(0) && it->handle == handle);
            const bool pathMatch = (!normalizedPath.empty() && it->assetPath.lexically_normal().generic_string() == normalizedPath);
            if (handleMatch || pathMatch)
            {
                m_QueuedKeys.erase(MakeDedupeKey(it->assetPath, it->size));
                it = m_PendingRenders.erase(it);
            }
            else
            {
                ++it;
            }
        }

        for (auto it = m_QueuedKeys.begin(); it != m_QueuedKeys.end();)
        {
            if (!normalizedPath.empty() && it->starts_with(normalizedPath + "#"))
            {
                it = m_QueuedKeys.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    void ThumbnailGenerator::ClearPending()
    {
        IncrementGeneration();
        std::lock_guard<std::mutex> lock(m_QueueMutex);
        m_PendingRenders.clear();
        m_QueuedKeys.clear();
    }

    uint64_t ThumbnailGenerator::IncrementGeneration()
    {
        return m_Generation.fetch_add(1, std::memory_order_acq_rel) + 1;
    }

    bool ThumbnailGenerator::HasPendingWork() const
    {
        {
            std::lock_guard<std::mutex> lock(m_QueueMutex);
            if (!m_PendingRenders.empty() || !m_QueuedKeys.empty())
                return true;
        }
        for (const auto &slot : m_ReadbackSlots)
        {
            if (slot.inFlight)
                return true;
        }
        return false;
    }

    size_t ThumbnailGenerator::GetPendingRenderCount() const
    {
        std::lock_guard<std::mutex> lock(m_QueueMutex);
        return m_PendingRenders.size();
    }

    void ThumbnailGenerator::Shutdown()
    {
        m_Shutdown = true;
        ClearPending();

        if (auto *dm = DeviceManager::GetInstance(); dm && dm->GetDevice())
        {
            if (m_ReadbackFence && m_NextReadbackFenceValue > 1)
            {
                const auto &nri = dm->GetCoreInterface();
                uint64_t maxFence = 0;
                for (const auto &slot : m_ReadbackSlots)
                {
                    if (slot.inFlight && slot.fenceValue > maxFence)
                        maxFence = slot.fenceValue;
                }
                if (maxFence > 0 && nri.GetFenceValue(*m_ReadbackFence) < maxFence)
                {
                    nri.Wait(*m_ReadbackFence, maxFence);
                }
            }
        }

        DestroyReadbackResources();

        m_SceneRenderer.reset();
        m_SceneRT.reset();
        m_UiRT.reset();
        m_CompositeRT.reset();
        m_CurrentRTSize = 0;
        m_Shutdown = false;
    }

    bool ThumbnailGenerator::EnsureRenderResources(uint32_t size)
    {
        if (size == 0)
            return false;

        if (!m_SceneRenderer)
        {
            m_SceneRenderer = CreateRef<AssetSceneRenderer>();
            if (!m_SceneRenderer)
                return false;

            auto &pp = m_SceneRenderer->GetPostProcessingSettings();
            pp.tonemapMode = TonemapMode::Filmic;
            pp.enableBloom = false;
            pp.enableSSAO = false;
            pp.enableVignette = false;
            pp.enableChromAb = false;
        }

        if (m_SceneRT && m_UiRT && m_CompositeRT && m_CurrentRTSize == size)
            return true;

        // Ensure no in-flight GPU readbacks still reference the old compositeRT before resizing/recreating.
        FlushPendingReadbacks(2000);

        if (!m_SceneRT || !m_UiRT || !m_CompositeRT)
        {
            RenderTargetCreateInfo rtCreateInfo{};
            rtCreateInfo.width = size;
            rtCreateInfo.height = size;
            rtCreateInfo.attachments = {
                FramebufferAttachments{ "[Thumbnail DepthAttachment]", nri::Format::D32_SFLOAT_S8_UINT, nri::Layout::DEPTH_STENCIL_ATTACHMENT },
                FramebufferAttachments{ "[Thumbnail ColorAttachment]", nri::Format::RGBA8_UNORM, nri::Layout::COLOR_ATTACHMENT },
                FramebufferAttachments{ "[Thumbnail ObjectIDAttachment]", nri::Format::R32_UINT, nri::Layout::COLOR_ATTACHMENT }
            };

            m_SceneRT = RenderTarget::Create(rtCreateInfo, "[Thumbnail Scene RT]");
            m_UiRT = RenderTarget::Create(rtCreateInfo, "[Thumbnail UI RT]");

            RenderTargetCreateInfo compositeInfo{};
            compositeInfo.width = size;
            compositeInfo.height = size;
            compositeInfo.attachments = {
                FramebufferAttachments{ "[Thumbnail Composite ColorAttachment]", nri::Format::RGBA8_UNORM, nri::Layout::COLOR_ATTACHMENT }
            };
            m_CompositeRT = RenderTarget::Create(compositeInfo, "[Thumbnail Composite RT]");
        }
        else
        {
            m_SceneRT->Resize(size, size, true);
            m_UiRT->Resize(size, size, true);
            m_CompositeRT->Resize(size, size, true);
        }

        m_CurrentRTSize = size;
        return m_SceneRT && m_UiRT && m_CompositeRT;
    }

    bool ThumbnailGenerator::EnsureReadbackResources()
    {
        auto *dm = DeviceManager::GetInstance();
        if (!dm || !dm->GetDevice() || !dm->GetGraphicsQueue())
            return false;

        const auto &nri = dm->GetCoreInterface();
        nri::Device *device = dm->GetDevice();
        nri::Queue *queue = dm->GetGraphicsQueue();

        if (!m_ReadbackFence)
        {
            nri::FenceDesc fenceDesc{};
            fenceDesc.initialValue = 0;
            if (nri.CreateFence(*device, fenceDesc, m_ReadbackFence) != nri::Result::SUCCESS || !m_ReadbackFence)
                return false;
        }

        for (auto &slot : m_ReadbackSlots)
        {
            if (!slot.allocator)
            {
                if (nri.CreateCommandAllocator(*queue, slot.allocator) != nri::Result::SUCCESS || !slot.allocator)
                    return false;
            }
            if (!slot.commandBuffer)
            {
                if (nri.CreateCommandBuffer(*slot.allocator, slot.commandBuffer) != nri::Result::SUCCESS || !slot.commandBuffer)
                    return false;
            }
        }

        return true;
    }

    bool ThumbnailGenerator::EnsureReadbackSlotBuffer(ReadbackSlot &slot, uint32_t width, uint32_t height)
    {
        auto *dm = DeviceManager::GetInstance();
        if (!dm || !dm->GetDevice())
            return false;

        const auto &nri = dm->GetCoreInterface();
        nri::Device *device = dm->GetDevice();
        const nri::DeviceDesc &deviceDesc = nri.GetDeviceDesc(*device);

        const uint32_t rowAlignment = std::max(1u, deviceDesc.memoryAlignment.uploadBufferTextureRow);
        const uint32_t sliceAlignment = std::max(1u, deviceDesc.memoryAlignment.uploadBufferTextureSlice);

        const uint32_t unalignedRowPitch = width * 4u;
        const uint32_t alignedRowPitch = AlignUp(unalignedRowPitch, rowAlignment);
        const uint32_t slicePitch = AlignUp(alignedRowPitch * height, sliceAlignment);
        const uint64_t requiredSize = static_cast<uint64_t>(slicePitch);

        slot.width = width;
        slot.height = height;
        slot.rowPitch = alignedRowPitch;

        if (slot.buffer && slot.bufferSize >= requiredSize)
            return true;

        if (slot.buffer)
        {
            nri.DestroyBuffer(slot.buffer);
            slot.buffer = nullptr;
            slot.bufferSize = 0;
        }

        nri::BufferDesc bufDesc{};
        bufDesc.size = requiredSize;
        bufDesc.usage = nri::BufferUsageBits::NONE;

        if (nri.CreateCommittedBuffer(*device, nri::MemoryLocation::HOST_READBACK, 1.0f, bufDesc, slot.buffer) != nri::Result::SUCCESS || !slot.buffer)
        {
            LOG_ERROR("[ThumbnailGenerator] Failed to create HOST_READBACK buffer ({} bytes)", requiredSize);
            return false;
        }

        slot.bufferSize = requiredSize;
        return true;
    }

    void ThumbnailGenerator::DestroyReadbackResources()
    {
        auto *dm = DeviceManager::GetInstance();
        const bool hasDevice = dm && dm->GetDevice();

        for (auto &slot : m_ReadbackSlots)
        {
            slot.inFlight = false;
            slot.fenceValue = 0;
            slot.request = {};

            if (hasDevice)
            {
                const auto &nri = dm->GetCoreInterface();
                if (slot.buffer)
                {
                    nri.DestroyBuffer(slot.buffer);
                }
                if (slot.commandBuffer)
                {
                    nri.DestroyCommandBuffer(slot.commandBuffer);
                }
                if (slot.allocator)
                {
                    nri.DestroyCommandAllocator(slot.allocator);
                }
            }

            slot.buffer = nullptr;
            slot.bufferSize = 0;
            slot.commandBuffer = nullptr;
            slot.allocator = nullptr;
        }

        if (hasDevice && m_ReadbackFence)
        {
            dm->GetCoreInterface().DestroyFence(m_ReadbackFence);
        }
        m_ReadbackFence = nullptr;
        m_NextReadbackFenceValue = 1;
    }

    ThumbnailGenerator::ReadinessResult ThumbnailGenerator::CheckRequestReadiness(ThumbnailRequest &request, Ref<Asset> &outAsset)
    {
        AssetManager *assetManager = AssetManager::GetInstance();
        if (!assetManager || request.handle == AssetHandle(0))
            return ReadinessResult::Failed;

        if (!assetManager->IsAssetHandleValid(request.handle))
            return ReadinessResult::Failed;

        const AssetState state = assetManager->GetAssetState(request.handle);
        if (state == AssetState::Failed)
            return ReadinessResult::Failed;

        Ref<Asset> asset = assetManager->GetAsset<Asset>(request.handle);
        if (!asset || !asset->IsReady())
        {
            if (++request.waitFrames > kMaxAssetWaitFrames)
                return ReadinessResult::Failed;
            return ReadinessResult::Waiting;
        }

        // Wait for dependent materials/textures to finish async GPU upload so we don't capture
        // un-textured white placeholders into the persistent disk cache.
        bool dependenciesReady = true;
        if (request.type == AssetType::Material)
        {
            Ref<Material> mat = asset->As<Material>();
            if (!mat)
                return ReadinessResult::Failed;
            dependenciesReady = IsMaterialReadyForThumbnail(mat, assetManager);
        }
        else if (request.type == AssetType::StaticMesh)
        {
            Ref<StaticMesh> mesh = asset->As<StaticMesh>();
            if (!mesh)
                return ReadinessResult::Failed;

            for (const auto &inst : mesh->GetMeshInstances())
            {
                if (!inst)
                    continue;
                const AssetHandle matHandle = inst->GetMaterialAssetHandle();
                if (matHandle == AssetHandle(0) || !assetManager->IsAssetHandleValid(matHandle))
                    continue;
                if (assetManager->GetAssetState(matHandle) == AssetState::Failed)
                    continue;

                Ref<Material> mat = assetManager->GetAsset<Material>(matHandle);
                if (!IsMaterialReadyForThumbnail(mat, assetManager))
                {
                    dependenciesReady = false;
                    break;
                }
            }
        }
        else if (request.type == AssetType::SkeletalMesh)
        {
            Ref<SkeletalMesh> mesh = asset->As<SkeletalMesh>();
            if (!mesh)
                return ReadinessResult::Failed;

            for (const auto &inst : mesh->GetMeshInstances())
            {
                if (!inst)
                    continue;
                const AssetHandle matHandle = inst->GetMaterialAssetHandle();
                if (matHandle == AssetHandle(0) || !assetManager->IsAssetHandleValid(matHandle))
                    continue;
                if (assetManager->GetAssetState(matHandle) == AssetState::Failed)
                    continue;

                Ref<Material> mat = assetManager->GetAsset<Material>(matHandle);
                if (!IsMaterialReadyForThumbnail(mat, assetManager))
                {
                    dependenciesReady = false;
                    break;
                }
            }
        }

        if (!dependenciesReady && ++request.waitFrames < kMaxDependencyWaitFrames)
        {
            return ReadinessResult::Waiting;
        }

        outAsset = std::move(asset);
        return ReadinessResult::Ready;
    }

    void ThumbnailGenerator::SetupCameraForBounds(const AABB &bounds, uint32_t size)
    {
        glm::vec3 center = (bounds.min + bounds.max) * 0.5f;
        glm::vec3 halfExtents = glm::abs(bounds.max - bounds.min) * 0.5f;
        float radius = glm::length(halfExtents);

        if (!std::isfinite(radius) || radius < 1e-4f)
        {
            center = glm::vec3(0.0f);
            radius = 0.5f;
        }

        const float halfFovRad = glm::radians(m_Camera.fov * 0.5f);
        const float distance = (radius / std::max(std::sin(halfFovRad), 0.1f)) * 1.12f;

        // 3/4 front-top studio angle for strong silhouette and specular readability.
        const float azimuth = glm::radians(35.0f);
        const float elevation = glm::radians(20.0f);
        const glm::vec3 offsetDir(
            std::cos(elevation) * std::sin(azimuth),
            std::sin(elevation),
            std::cos(elevation) * std::cos(azimuth)
        );

        const glm::vec3 eye = center + offsetDir * distance;

        m_Camera.position = eye;
        m_Camera.nearPlane = std::max(0.005f, distance - radius * 2.5f);
        m_Camera.farPlane = std::max(50.0f, distance + radius * 4.0f);
        m_Camera.UpdateProjection(size, size);
        m_Camera.SetView(glm::lookAt(eye, center, glm::vec3(0.0f, 1.0f, 0.0f)));
    }

    bool ThumbnailGenerator::RenderAndEnqueueReadback(const ThumbnailRequest &request, const Ref<Asset> &asset, FrameContext *frameContext, ReadbackSlot &slot)
    {
        IGN_PROFILE_FUNCTION();

        if (!EnsureRenderResources(request.size) || !EnsureReadbackResources() || !EnsureReadbackSlotBuffer(slot, request.size, request.size))
            return false;

        AABB targetBounds{};
        switch (request.type)
        {
            case AssetType::Material:
            {
                Ref<Material> material = asset->As<Material>();
                Ref<StaticMesh> sphereMesh = Renderer::GetDefaultMesh(EMeshType::UV_SPHERE);
                if (!material || !sphereMesh)
                    return false;

                if (glm::length(sphereMesh->localAABB.max - sphereMesh->localAABB.min) < 1e-4f)
                    sphereMesh->CalculateLocalAABB();

                targetBounds = sphereMesh->localAABB;
                m_SceneRenderer->SetPreviewSkeletalMesh(nullptr);
                m_SceneRenderer->SetPreviewStaticMesh(sphereMesh);
                m_SceneRenderer->SetPreviewMaterial(material);
                break;
            }
            case AssetType::StaticMesh:
            {
                Ref<StaticMesh> staticMesh = asset->As<StaticMesh>();
                if (!staticMesh || staticMesh->GetMeshInstances().empty())
                    return false;

                if (glm::length(staticMesh->localAABB.max - staticMesh->localAABB.min) < 1e-4f)
                    staticMesh->CalculateLocalAABB();

                targetBounds = staticMesh->localAABB;
                m_SceneRenderer->SetPreviewMaterial(nullptr);
                m_SceneRenderer->SetPreviewSkeletalMesh(nullptr);
                m_SceneRenderer->SetPreviewStaticMesh(staticMesh);
                break;
            }
            case AssetType::SkeletalMesh:
            {
                Ref<SkeletalMesh> skeletalMesh = asset->As<SkeletalMesh>();
                if (!skeletalMesh || skeletalMesh->GetMeshInstances().empty())
                    return false;

                if (glm::length(skeletalMesh->localAABB.max - skeletalMesh->localAABB.min) < 1e-4f)
                    skeletalMesh->CalculateLocalAABB();

                targetBounds = skeletalMesh->localAABB;
                m_SceneRenderer->SetPreviewMaterial(nullptr);
                m_SceneRenderer->SetPreviewStaticMesh(nullptr);
                m_SceneRenderer->SetPreviewSkeletalMesh(skeletalMesh);
                break;
            }
            default:
                return false;
        }

        SetupCameraForBounds(targetBounds, request.size);

        m_SceneRenderer->BeginFrame();
        m_SceneRenderer->Render(&m_Camera, frameContext, m_SceneRT, m_UiRT, m_CompositeRT);

        // Immediately release mesh/material references from the preview renderer so we don't pin unloaded assets.
        m_SceneRenderer->SetPreviewStaticMesh(nullptr);
        m_SceneRenderer->SetPreviewSkeletalMesh(nullptr);
        m_SceneRenderer->SetPreviewMaterial(nullptr);

        Ref<Texture> compositeColor = m_CompositeRT->GetColorAttachment(0);
        if (!compositeColor || !compositeColor->GetHandle())
            return false;

        auto *dm = DeviceManager::GetInstance();
        const auto &nri = dm->GetCoreInterface();
        nri::Queue *graphicsQueue = dm->GetGraphicsQueue();

        nri.ResetCommandAllocator(*slot.allocator);
        if (nri.BeginCommandBuffer(*slot.commandBuffer, BindlessSystem::GetDescriptorPool()) != nri::Result::SUCCESS)
            return false;

        nri::TextureBarrierDesc preBarrier{};
        preBarrier.texture = compositeColor->GetHandle();
        preBarrier.before = { compositeColor->GetCurrentAccess(), compositeColor->GetCurrentLayout(), compositeColor->GetCurrentStages() };
        preBarrier.after = { nri::AccessBits::COPY_SOURCE, nri::Layout::COPY_SOURCE, nri::StageBits::COPY };
        preBarrier.mipOffset = 0;
        preBarrier.mipNum = 1;
        preBarrier.layerOffset = 0;
        preBarrier.layerNum = 1;
        preBarrier.planes = nri::PlaneBits::ALL;

        nri::BarrierDesc barrierDesc{};
        barrierDesc.textures = &preBarrier;
        barrierDesc.textureNum = 1;
        nri.CmdBarrier(*slot.commandBuffer, barrierDesc);

        nri::TextureDataLayoutDesc dstLayout{};
        dstLayout.offset = 0;
        dstLayout.rowPitch = slot.rowPitch;
        dstLayout.slicePitch = static_cast<uint32_t>(slot.bufferSize);

        nri::TextureRegionDesc srcRegion{};
        srcRegion.x = 0;
        srcRegion.y = 0;
        srcRegion.z = 0;
        srcRegion.width = static_cast<nri::Dim_t>(slot.width);
        srcRegion.height = static_cast<nri::Dim_t>(slot.height);
        srcRegion.depth = 1;
        srcRegion.mipOffset = 0;
        srcRegion.layerOffset = 0;
        srcRegion.planes = nri::PlaneBits::ALL;

        nri.CmdReadbackTextureToBuffer(*slot.commandBuffer, *slot.buffer, dstLayout, *compositeColor->GetHandle(), srcRegion);

        nri::TextureBarrierDesc postBarrier{};
        postBarrier.texture = compositeColor->GetHandle();
        postBarrier.before = preBarrier.after;
        postBarrier.after = { nri::AccessBits::SHADER_RESOURCE, nri::Layout::SHADER_RESOURCE, nri::StageBits::ALL };
        postBarrier.mipOffset = 0;
        postBarrier.mipNum = 1;
        postBarrier.layerOffset = 0;
        postBarrier.layerNum = 1;
        postBarrier.planes = nri::PlaneBits::ALL;

        barrierDesc.textures = &postBarrier;
        nri.CmdBarrier(*slot.commandBuffer, barrierDesc);
        compositeColor->SetCurrentState(postBarrier.after.layout, postBarrier.after.access, postBarrier.after.stages);

        if (nri.EndCommandBuffer(*slot.commandBuffer) != nri::Result::SUCCESS)
            return false;

        const uint64_t signalValue = m_NextReadbackFenceValue++;
        nri::FenceSubmitDesc signalFence{};
        signalFence.fence = m_ReadbackFence;
        signalFence.value = signalValue;
        signalFence.stages = nri::StageBits::COPY;

        nri::QueueSubmitDesc submitDesc{};
        submitDesc.commandBuffers = &slot.commandBuffer;
        submitDesc.commandBufferNum = 1;
        submitDesc.signalFences = &signalFence;
        submitDesc.signalFenceNum = 1;

        nri::Result submitRes = nri::Result::FAILURE;
        {
            std::lock_guard<std::mutex> queueLock(GPUUploadSync::GetQueueMutex());
            submitRes = nri.QueueSubmit(*graphicsQueue, submitDesc);
        }

        if (submitRes != nri::Result::SUCCESS)
        {
            LOG_ERROR("[ThumbnailGenerator] Readback QueueSubmit failed with NRI result {}", static_cast<uint32_t>(submitRes));
            return false;
        }

        slot.fenceValue = signalValue;
        slot.inFlight = true;
        slot.request = request;
        return true;
    }

    void ThumbnailGenerator::PollCompletedReadbacks()
    {
        if (!m_ReadbackFence)
            return;

        auto *dm = DeviceManager::GetInstance();
        if (!dm || !dm->GetDevice())
            return;

        const auto &nri = dm->GetCoreInterface();
        const uint64_t completedFence = nri.GetFenceValue(*m_ReadbackFence);

        for (auto &slot : m_ReadbackSlots)
        {
            if (slot.inFlight && completedFence >= slot.fenceValue)
            {
                CompleteReadbackSlot(slot);
            }
        }
    }

    void ThumbnailGenerator::CompleteReadbackSlot(ReadbackSlot &slot)
    {
        IGN_PROFILE_FUNCTION();

        ThumbnailRequest req = std::move(slot.request);
        slot.request = {};
        slot.inFlight = false;

        {
            std::lock_guard<std::mutex> lock(m_QueueMutex);
            m_QueuedKeys.erase(MakeDedupeKey(req.assetPath, req.size));
        }

        if (m_Shutdown || req.generation != m_Generation.load(std::memory_order_acquire))
            return;

        auto *dm = DeviceManager::GetInstance();
        if (!dm || !dm->GetDevice() || !slot.buffer)
            return;

        const auto &nri = dm->GetCoreInterface();
        const size_t tightRowBytes = static_cast<size_t>(slot.width) * 4u;
        const size_t totalTightBytes = tightRowBytes * static_cast<size_t>(slot.height);
        std::vector<uint8_t> pixels(totalTightBytes);

        const auto *mapped = static_cast<const uint8_t *>(nri.MapBuffer(*slot.buffer, 0, slot.bufferSize));
        if (!mapped)
        {
            if (req.onComplete)
                req.onComplete(nullptr);
            return;
        }

        for (uint32_t row = 0; row < slot.height; ++row)
        {
            std::memcpy(pixels.data() + static_cast<size_t>(row) * tightRowBytes,
                mapped + static_cast<size_t>(row) * slot.rowPitch,
                tightRowBytes);
        }
        nri.UnmapBuffer(*slot.buffer);

        Ref<Texture> thumbnailTex = CreateThumbnailTextureFromPixels(
            pixels, slot.width, slot.height,
            std::format("[Thumbnail] {}", req.assetPath.filename().string()));

        if (req.onComplete)
        {
            req.onComplete(thumbnailTex);
        }

        // Persist to <project>/.cache/thumbnails/{key}.bin on a background worker thread.
        if (!req.cachePath.empty())
        {
            ThumbnailBinaryData cacheData{};
            cacheData.width = slot.width;
            cacheData.height = slot.height;
            cacheData.channels = 4;
            cacheData.sourceTimestamp = req.sourceTimestamp;
            cacheData.assetHandle = static_cast<uint64_t>(req.handle);
            cacheData.pixels = std::move(pixels);

            AssetWorker::SubmitJob([cacheData = std::move(cacheData), cachePath = req.cachePath]()
            {
                BinarySerializer::SerializeThumbnail(cacheData, cachePath);
            });
        }
    }

    Ref<Texture> ThumbnailGenerator::CreateThumbnailTextureFromPixels(const std::vector<uint8_t> &pixels, uint32_t width, uint32_t height, const std::string &debugName)
    {
        if (pixels.empty() || width == 0 || height == 0)
            return nullptr;

        TextureCreateInfo createInfo{};
        createInfo.format = nri::Format::RGBA8_UNORM;
        createInfo.keepInitialState = true;
        createInfo.keepCpuData = false;
        createInfo.deferGpuCreate = false;
        createInfo.bindless = false;
        createInfo.initialLayout = nri::Layout::SHADER_RESOURCE;
        createInfo.initialAccess = nri::AccessBits::SHADER_RESOURCE;
        createInfo.width = width;
        createInfo.height = height;
        createInfo.samplerAddressU = nri::AddressMode::CLAMP_TO_EDGE;
        createInfo.samplerAddressV = nri::AddressMode::CLAMP_TO_EDGE;
        createInfo.samplerAddressW = nri::AddressMode::CLAMP_TO_EDGE;
        createInfo.samplerLinearFiltering = true;

        Ref<Texture> tex = Texture::Create(pixels, createInfo, nullptr, debugName);
        if (tex)
        {
            tex->SetReadyFlag(true);
        }
        return tex;
    }
}

