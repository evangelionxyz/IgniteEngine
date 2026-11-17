// Copyright (c) 2026 Evangelion Manuhutu

using System;
using System.Collections.Generic;
using System.Numerics;
using Ignite.Managed.Models;
using Ignite.Managed.Services;
using NUnit.Framework;

namespace Ignite.Managed.Test;

[TestFixture]
public class ManagedModelAndDeserializerTests
{
    [Test]
    public void EmptyAndMalformedHierarchyJsonDoesNotThrow()
    {
        Assert.That(SceneHierarchyDeserializer.Deserialize(null), Is.Empty);
        Assert.That(SceneHierarchyDeserializer.Deserialize(""), Is.Empty);
        Assert.That(SceneHierarchyDeserializer.Deserialize("[]"), Is.Empty);
        Assert.That(SceneHierarchyDeserializer.Deserialize("{not valid json"), Is.Empty);
        Assert.That(SceneHierarchyDeserializer.Deserialize("{}"), Is.Empty);
    }

    [Test]
    public void DeserializesHierarchyTreeAndComponentsAccurately()
    {
        const string json = """
        [
          {
            "rawId": 1001,
            "name": "PlayerRobot",
            "isActive": true,
            "components": [
              {
                "type": "Transform",
                "displayName": "Transform",
                "position": [1.5, 2.5, -3.5],
                "rotation": [0.0, 90.0, 0.0],
                "scale": [1.0, 1.0, 1.0]
              },
              {
                "type": "PointLight",
                "displayName": "Point Light",
                "color": [0.2, 0.8, 1.0, 1.0],
                "intensity": 4.2,
                "range": 12.0,
                "enabled": true
              }
            ],
            "children": [
              {
                "id": "2002",
                "parentId": 1001,
                "name": "PlayerCamera",
                "isActive": false,
                "components": [
                  {
                    "type": "Camera",
                    "displayName": "Camera",
                    "isPerspective": true,
                    "fov": 65.0,
                    "nearClip": 0.1,
                    "farClip": 500.0
                  }
                ],
                "children": []
              }
            ]
          }
        ]
        """;

        List<EntityModel> entities = SceneHierarchyDeserializer.Deserialize(json);
        Assert.That(entities.Count, Is.EqualTo(1));

        EntityModel root = entities[0];
        Assert.That(root.Uuid, Is.EqualTo(1001ul));
        Assert.That(root.Guid, Is.EqualTo(EntityModel.GuidFromUInt64(1001ul)));
        Assert.That(root.Name, Is.EqualTo("PlayerRobot"));
        Assert.That(root.IsActive, Is.True);
        Assert.That(root.Components.Count, Is.EqualTo(2));

        Assert.That(root.Components[0].Type, Is.EqualTo(ComponentType.Transform));
        var trData = root.Components[0].Data as TransformData;
        Assert.That(trData, Is.Not.Null);
        Assert.That(trData!.Position, Is.EqualTo(new Vector3(1.5f, 2.5f, -3.5f)));
        Assert.That(trData.Rotation, Is.EqualTo(new Vector3(0.0f, 90.0f, 0.0f)));

        Assert.That(root.Components[1].Type, Is.EqualTo(ComponentType.PointLight));
        var plData = root.Components[1].Data as PointLightData;
        Assert.That(plData, Is.Not.Null);
        Assert.That(plData!.Range, Is.EqualTo(12.0f).Within(1e-4f));

        Assert.That(root.Children.Count, Is.EqualTo(1));
        EntityModel child = root.Children[0];
        Assert.That(child.Uuid, Is.EqualTo(2002ul));
        Assert.That(child.ParentId, Is.EqualTo(EntityModel.GuidFromUInt64(1001ul)));
        Assert.That(child.Name, Is.EqualTo("PlayerCamera"));
        Assert.That(child.IsActive, Is.False);
        Assert.That(child.Components.Count, Is.EqualTo(1));
        Assert.That(child.Components[0].Type, Is.EqualTo(ComponentType.Camera));
    }

    [Test]
    public void LogEntryRecordStoresTimestampAndSeverity()
    {
        var now = DateTime.UtcNow;
        var entry = new LogEntry(now, LogLevel.Warning, "Disk near limit");
        Assert.That(entry.Timestamp, Is.EqualTo(now));
        Assert.That(entry.Level, Is.EqualTo(LogLevel.Warning));
        Assert.That(entry.Message, Is.EqualTo("Disk near limit"));
    }
}

