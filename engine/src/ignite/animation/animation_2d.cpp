// Copyright (c) 2026 Evangelion Manuhutu

#include "animation_2d.hpp"

namespace ignite
{

	void Sprite2DAnimation::OnUpdate(float deltaTime)
	{
		m_CurrentTime += deltaTime;
		if (m_CurrentTime >= m_FrameTime)
		{
			m_CurrentFrame++;
			m_CurrentTime = 0.0f;
		}
	}

	const Sprite2DAnimation::Data Sprite2DAnimation::GetCurrentAnimation()
	{
		if (m_Animations.empty() || m_CurrentFrame >= m_Animations.size())
			return Data{};

		return m_Animations[m_CurrentFrame];
	}

}
