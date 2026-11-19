// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_THUMBNAIL_GENERATOR_HPP
#define IGN_THUMBNAIL_GENERATOR_HPP

#include "ignite/core/base.hpp"
#include "ignite/core/types.hpp"
#include "ignite/asset/asset.hpp"
#include "ignite/graphics/texture.hpp"
#include "ignite/graphics/render_target.hpp"
#include "ignite/graphics/renderer/asset_scene_renderer.hpp"
#include "ignite/scene/icamera.hpp"

#include <NRI.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <functional>
#include <mutex>
#include <unordered_set>
#include <vector>

namespace ignite
{
    class Project;
    struct FrameContext;

    struct ThumbnailRequest
    {
        AssetHandle handle = AssetHandle(0);
        AssetType type = AssetType::Invalid;
        std::filesystem::path assetPath;
        std::filesystem::path cachePath;
        uint32_t size = 72;
        int64_t sourceTimestamp = 0;
        uint64_t generation = 0;
        std::function<void(Ref<Texture>)> onComplete;
        uint32_t waitFrames = 0;
    };

    class IGN_API ThumbnailGenerator
    {
    public:
        ThumbnailGenerator();
        ~ThumbnailGenerator();

        ThumbnailGenerator(const ThumbnailGenerator &) = delete;
        ThumbnailGenerator &operator=(const ThumbnailGenerator &) = delete;

        static ThumbnailGenerator &Get();

        static bool IsSupportedAssetType(AssetType type);
        static std::filesystem::path GetCacheDirectory(const Project *project);
        static std::filesystem::path GetCacheFilepath(const Project *project, AssetHandle handle, const std::filesystem::path &assetPath);
        static int64_t GetFileTimestamp(const std::filesystem::path &filepath);

        // Requests a thumbnail asynchronously:
        //  1. Checks the binary disk cache (<project>/.cache/thumbnails/{key}.bin) on AssetWorker.
        //  2. On cache hit, uploads the tiny RGBA buffer on the render thread and invokes onComplete.
        //  3. On cache miss, enqueues an amortized GPU scene render processed in ProcessFrame().
        void RequestThumbnail(ThumbnailRequest request);

        // Must be called once per frame on the render thread (or main thread in headless tests).
        // Polls non-blocking async readbacks and renders at most one ready thumbnail per interval.
        void ProcessFrame(FrameContext *frameContext);

        // Waits for all in-flight GPU readbacks and processes their completion callbacks.
        // Intended for deterministic tests or shutdown.
        bool FlushPendingReadbacks(uint32_t timeoutMs = 5000);

        // Deletes the on-disk .bin cache file and drops pending requests for this asset.
        void InvalidateThumbnail(const Project *project, AssetHandle handle, const std::filesystem::path &assetPath);

        // Clears queued render requests and bumps generation so in-flight requests are ignored.
        void ClearPending();
        uint64_t IncrementGeneration();
        uint64_t GetGeneration() const { return m_Generation.load(std::memory_order_acquire); }

        // Frame pacing: render at most 1 thumbnail every N frames (default 2) to prevent FPS drops.
        void SetRenderIntervalFrames(uint32_t intervalFrames) { m_RenderIntervalFrames = std::max(1u, intervalFrames); }
        uint32_t GetRenderIntervalFrames() const { return m_RenderIntervalFrames; }

        bool HasPendingWork() const;
        size_t GetPendingRenderCount() const;

        // Releases all GPU resources (render targets, readback buffers, fence, scene renderer).
        // Safe to call multiple times or before graphics device destruction.
        void Shutdown();

    private:
        static constexpr size_t kReadbackSlotCount = 3;
        static constexpr uint32_t kMaxAssetWaitFrames = 300;
        static constexpr uint32_t kMaxDependencyWaitFrames = 120;

        struct ReadbackSlot
        {
            nri::CommandAllocator *allocator = nullptr;
            nri::CommandBuffer *commandBuffer = nullptr;
            nri::Buffer *buffer = nullptr;
            uint64_t bufferSize = 0;
            uint32_t width = 0;
            uint32_t height = 0;
            uint32_t rowPitch = 0;
            uint64_t fenceValue = 0;
            bool inFlight = false;
            ThumbnailRequest request;
        };

        enum class ReadinessResult : uint8_t
        {
            Ready,
            Waiting,
            Failed
        };

        bool EnsureRenderResources(uint32_t size);
        bool EnsureReadbackResources();
        bool EnsureReadbackSlotBuffer(ReadbackSlot &slot, uint32_t width, uint32_t height);
        void DestroyReadbackResources();

        ReadinessResult CheckRequestReadiness(ThumbnailRequest &request, Ref<Asset> &outAsset);
        void SetupCameraForBounds(const AABB &bounds, uint32_t size);
        bool RenderAndEnqueueReadback(const ThumbnailRequest &request, const Ref<Asset> &asset, FrameContext *frameContext, ReadbackSlot &slot);
        void PollCompletedReadbacks();
        void CompleteReadbackSlot(ReadbackSlot &slot);
        static Ref<Texture> CreateThumbnailTextureFromPixels(const std::vector<uint8_t> &pixels, uint32_t width, uint32_t height, const std::string &debugName);
        static std::string MakeDedupeKey(const std::filesystem::path &assetPath, uint32_t size);

        Ref<AssetSceneRenderer> m_SceneRenderer;
        Ref<RenderTarget> m_SceneRT;
        Ref<RenderTarget> m_UiRT;
        Ref<RenderTarget> m_CompositeRT;
        ICamera m_Camera;
        uint32_t m_CurrentRTSize = 0;

        nri::Fence *m_ReadbackFence = nullptr;
        uint64_t m_NextReadbackFenceValue = 1;
        std::array<ReadbackSlot, kReadbackSlotCount> m_ReadbackSlots;

        mutable std::mutex m_QueueMutex;
        std::deque<ThumbnailRequest> m_PendingRenders;
        std::unordered_set<std::string> m_QueuedKeys;

        std::atomic<uint64_t> m_Generation{ 1 };
        uint64_t m_FrameCounter = 0;
        uint32_t m_RenderIntervalFrames = 2;
        bool m_Shutdown = false;
    };
}

#endif // IGN_THUMBNAIL_GENERATOR_HPP

