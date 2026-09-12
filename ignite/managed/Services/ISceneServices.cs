using System;
using Ignite.Managed.Models;
using System.Collections.Generic;

namespace Ignite.Managed.Services;

public interface ISceneService
{
    IReadOnlyList<EntityModel> GetRootEntities();
    EntityModel? GetEntity(Guid id);
    EntityModel CreateEntity(string name, Guid? parentId = null);
    void DeleteEntity(Guid id);
    void ReparentEntity(Guid entityId, Guid? newParentId);
    void RenameEntity(Guid id, string newName);
    EntityModel DuplicateEntity(Guid id);
}
