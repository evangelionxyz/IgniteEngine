using System;
using System.Collections.Generic;
using System.Text;

namespace Ignite.Managed.Models;

public class EntityModel
{
    public Guid Id { get; set; } = Guid.NewGuid();
    public string Name { get; set; } = "Entity";
    public Guid? ParentId { get; set; }
    public bool IsActive { get; set; } = true;
    public List<ComponentModel> Components { get; set; } = new();
    public List<EntityModel> Children { get; set; } = new();
}
