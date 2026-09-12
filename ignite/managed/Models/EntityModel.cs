using System;
using System.Collections.Generic;
using System.Text;

namespace Ignite.Managed.Models;

public class EntityModel
{
    public Guid Id { get; set; } = Guid.NewGuid();
    public ulong Uuid { get; set; }
    public string Name { get; set; } = "Entity";
    public Guid? ParentId { get; set; }
    public bool IsActive { get; set; } = true;
    public List<ComponentModel> Components { get; set; } = new();
    public List<EntityModel> Children { get; set; } = new();

    public static Guid GuidFromUInt64(ulong id)
    {
        return new Guid((uint)(id >> 32), (ushort)(id >> 16), (ushort)id, 0x49, 0x47, 0x4E, 0x54, 0x5F, 0x53, 0x43, 0x4E);
    }
}
