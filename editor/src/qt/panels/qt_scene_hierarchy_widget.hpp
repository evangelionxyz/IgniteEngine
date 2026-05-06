// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef QT_SCENE_HIERARCHY_WIDGET_HPP
#define QT_SCENE_HIERARCHY_WIDGET_HPP

#include <QWidget>

namespace ignite
{
    class QtSceneHierarchyWidget final : public QWidget
    {
    public:
        explicit QtSceneHierarchyWidget(QWidget *parent = nullptr);
        void RefreshHierarchy();
    };
}

#endif