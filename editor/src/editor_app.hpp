#pragma once

#include <functional>

namespace ignite
{
    void ConfigureEditorHost(void *nativeHostWindowHandle, std::function<void()> platformEventPump);
}
