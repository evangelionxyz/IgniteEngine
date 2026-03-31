// Copyright (c) 2026 Evangelion Manuhutu

#ifndef ANIMATION_2D_HPP
#define ANIMATION_2D_HPP

#include "ignite/asset/asset.hpp"
#include <glm/glm.hpp>

namespace ignite
{
	class Sprite2DAnimation : public Asset
	{
	public:
		struct Data
		{
			glm::vec2 uv0 = glm::vec2(0.0f);
			glm::vec2 uv1 = glm::vec2(1.0f);
		};

	public:
		Sprite2DAnimation() = default;

		void OnUpdate(float deltaTime);

		void SetLoop(bool enable) { m_IsLooping = enable; }
		bool IsLooping() const { return m_IsLooping; }

		float GetFrameTime() const { return m_FrameTime; }

		virtual bool Serialize(const std::filesystem::path &filepath);
		const Ref<Sprite2DAnimation> Deserialize(const std::filesystem::path &filepath);

		const std::vector<Data> &GetAnimations() { return m_Animations; }
		const Data GetCurrentAnimation();

	private:
		std::vector<Data> m_Animations;

		int m_CurrentFrame = 0;
		float m_FrameTime = 0.0f; // Frame duration in seconds
		float m_CurrentTime = 0.0f;
		bool m_IsLooping = false;
	};


	// Other animations
}

#endif