#pragma once
#include "Platform/Window.h"
#include <memory>

namespace koi
{
    class Application
    {
    public:
        Window& CreateWindow(const WindowDesc& desc = {});
        bool Tick();

    private:
        std::vector<std::unique_ptr<Window>> m_windows;
    };
}
