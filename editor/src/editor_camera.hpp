#pragma once

#include "ignite/scene/icamera.hpp"

namespace ignite
{
    class EditorCamera : public ICamera
    {
    public:
        EditorCamera() = default;
        EditorCamera(std::string name);

    private:
        std::string m_Name;
    };
}
