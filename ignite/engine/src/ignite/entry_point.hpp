// Copyright (c) 2026 Evangelion Manuhutu
#pragma once
#ifndef IGN_ENTRY_POINT_HPP
#define IGN_ENTRY_POINT_HPP

#include "core/application.hpp"
#include "ignite/core/logger.hpp"

inline int Main(const int argc, char **argv)
{
    ignite::Logger::Init();
    ignite::Application *app = ignite::CreateApplication({argc, argv});
    app->Run();
    delete app;
    ignite::Logger::Shutdown();
    return 0;
}

int main(const int argc, char **argv)
{
    return Main(argc, argv);
}

#endif
