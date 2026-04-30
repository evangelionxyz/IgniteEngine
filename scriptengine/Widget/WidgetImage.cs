// Copyright (c) 2026 Evangelion Manuhutu

using Ignite.Core;
namespace Ignite;

public sealed class WidgetImage
{
    private readonly ulong m_EntityID;
    private readonly string m_ImageName;

    internal WidgetImage(ulong entityID, string imageName)
    {
        m_EntityID = entityID;
        m_ImageName = imageName;
    }

    public ulong ImageHandle
    {
        get => InternalCalls.WidgetComponent_GetImageHandle(m_EntityID, m_ImageName);
        set => InternalCalls.WidgetComponent_SetImageHandle(m_EntityID, m_ImageName, value);
    }
}
