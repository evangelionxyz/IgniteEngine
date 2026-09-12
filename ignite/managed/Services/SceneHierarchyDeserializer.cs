using System;
using System.Collections.Generic;
using System.Numerics;
using System.Text.Json;
using Ignite.Managed.Models;

namespace Ignite.Managed.Services;

public static class SceneHierarchyDeserializer
{
    public static List<EntityModel> Deserialize(string? jsonString)
    {
        var result = new List<EntityModel>();
        if (string.IsNullOrWhiteSpace(jsonString) || jsonString.Trim() == "[]")
            return result;

        try
        {
            using var doc = JsonDocument.Parse(jsonString);
            if (doc.RootElement.ValueKind != JsonValueKind.Array)
                return result;

            foreach (var element in doc.RootElement.EnumerateArray())
            {
                var entity = ParseEntity(element);
                if (entity != null)
                    result.Add(entity);
            }
        }
        catch (Exception ex)
        {
            System.Diagnostics.Debug.WriteLine($"[SceneHierarchyDeserializer] Failed to parse JSON hierarchy: {ex.Message}");
        }

        return result;
    }

    private static EntityModel? ParseEntity(JsonElement element)
    {
        if (element.ValueKind != JsonValueKind.Object)
            return null;

        var entity = new EntityModel();

        // Parse UUID
        ulong uuid = 0;
        if (element.TryGetProperty("rawId", out var rawIdProp) && rawIdProp.ValueKind == JsonValueKind.Number)
        {
            uuid = rawIdProp.GetUInt64();
        }
        else if (element.TryGetProperty("id", out var idProp) && idProp.ValueKind == JsonValueKind.String)
        {
            if (ulong.TryParse(idProp.GetString(), out var parsedId))
                uuid = parsedId;
        }

        entity.Uuid = uuid;
        entity.Id = uuid != 0 ? EntityModel.GuidFromUInt64(uuid) : Guid.NewGuid();

        if (element.TryGetProperty("name", out var nameProp) && nameProp.ValueKind == JsonValueKind.String)
        {
            entity.Name = nameProp.GetString() ?? "Entity";
        }

        if (element.TryGetProperty("parentId", out var parentProp))
        {
            if (parentProp.ValueKind == JsonValueKind.String && ulong.TryParse(parentProp.GetString(), out var parentUuid) && parentUuid != 0)
            {
                entity.ParentId = EntityModel.GuidFromUInt64(parentUuid);
            }
            else if (parentProp.ValueKind == JsonValueKind.Number)
            {
                ulong parentUuidNum = parentProp.GetUInt64();
                if (parentUuidNum != 0)
                    entity.ParentId = EntityModel.GuidFromUInt64(parentUuidNum);
            }
        }

        if (element.TryGetProperty("isActive", out var activeProp) && (activeProp.ValueKind == JsonValueKind.True || activeProp.ValueKind == JsonValueKind.False))
        {
            entity.IsActive = activeProp.GetBoolean();
        }

        // Parse Components
        if (element.TryGetProperty("components", out var componentsProp) && componentsProp.ValueKind == JsonValueKind.Array)
        {
            foreach (var compElement in componentsProp.EnumerateArray())
            {
                var component = ParseComponent(compElement);
                if (component != null)
                    entity.Components.Add(component);
            }
        }

        // Parse Children recursively
        if (element.TryGetProperty("children", out var childrenProp) && childrenProp.ValueKind == JsonValueKind.Array)
        {
            foreach (var childElement in childrenProp.EnumerateArray())
            {
                var child = ParseEntity(childElement);
                if (child != null)
                    entity.Children.Add(child);
            }
        }

        return entity;
    }

    private static ComponentModel? ParseComponent(JsonElement compElement)
    {
        if (!compElement.TryGetProperty("type", out var typeProp) || typeProp.ValueKind != JsonValueKind.String)
            return null;

        string typeStr = typeProp.GetString() ?? string.Empty;
        string displayName = compElement.TryGetProperty("displayName", out var nameProp) && nameProp.ValueKind == JsonValueKind.String
            ? nameProp.GetString() ?? typeStr
            : typeStr;

        if (!Enum.TryParse<ComponentType>(typeStr, ignoreCase: true, out var compType))
        {
            return new ComponentModel
            {
                Type = ComponentType.Transform,
                DisplayName = displayName,
                IsEnabled = true
            };
        }

        var compModel = new ComponentModel
        {
            Type = compType,
            DisplayName = displayName,
            IsEnabled = true
        };

        switch (compType)
        {
            case ComponentType.Transform:
            {
                var transformData = new TransformData();
                if (compElement.TryGetProperty("position", out var posProp) && posProp.ValueKind == JsonValueKind.Array)
                {
                    float x = 0, y = 0, z = 0;
                    int i = 0;
                    foreach (var val in posProp.EnumerateArray())
                    {
                        if (i == 0) x = (float)val.GetDouble();
                        else if (i == 1) y = (float)val.GetDouble();
                        else if (i == 2) z = (float)val.GetDouble();
                        i++;
                    }
                    transformData.Position = new Vector3(x, y, z);
                }
                if (compElement.TryGetProperty("rotation", out var rotProp) && rotProp.ValueKind == JsonValueKind.Array)
                {
                    float x = 0, y = 0, z = 0;
                    int i = 0;
                    foreach (var val in rotProp.EnumerateArray())
                    {
                        if (i == 0) x = (float)val.GetDouble();
                        else if (i == 1) y = (float)val.GetDouble();
                        else if (i == 2) z = (float)val.GetDouble();
                        i++;
                    }
                    transformData.Rotation = new Vector3(x, y, z);
                }
                if (compElement.TryGetProperty("scale", out var sclProp) && sclProp.ValueKind == JsonValueKind.Array)
                {
                    float x = 1, y = 1, z = 1;
                    int i = 0;
                    foreach (var val in sclProp.EnumerateArray())
                    {
                        if (i == 0) x = (float)val.GetDouble();
                        else if (i == 1) y = (float)val.GetDouble();
                        else if (i == 2) z = (float)val.GetDouble();
                        i++;
                    }
                    transformData.Scale = new Vector3(x, y, z);
                }
                compModel.Data = transformData;
                break;
            }

            case ComponentType.Camera:
            {
                var camData = new CameraData();
                if (compElement.TryGetProperty("isPerspective", out var pProp))
                    camData.IsPerspective = pProp.GetBoolean();
                if (compElement.TryGetProperty("fov", out var fovProp))
                    camData.FieldOfView = (float)fovProp.GetDouble();
                if (compElement.TryGetProperty("near", out var nearProp))
                    camData.NearClip = (float)nearProp.GetDouble();
                if (compElement.TryGetProperty("far", out var farProp))
                    camData.FarClip = (float)farProp.GetDouble();
                if (compElement.TryGetProperty("orthoSize", out var orthoProp))
                    camData.OrthographicSize = (float)orthoProp.GetDouble();
                compModel.Data = camData;
                break;
            }

            case ComponentType.DirectionalLight:
            {
                var d = new DirectionalLightData();
                d.Color = ParseVector4(compElement, "color", Vector4.One);
                if (compElement.TryGetProperty("intensity", out var p)) d.Intensity = (float)p.GetDouble();
                if (compElement.TryGetProperty("shadowDistance", out var sd)) d.ShadowDistance = (float)sd.GetDouble();
                if (compElement.TryGetProperty("castShadows", out var cs)) d.CastShadows = cs.GetBoolean();
                compModel.Data = d;
                break;
            }

            case ComponentType.PointLight:
            {
                var d = new PointLightData();
                d.Color = ParseVector4(compElement, "color", Vector4.One);
                if (compElement.TryGetProperty("intensity", out var p)) d.Intensity = (float)p.GetDouble();
                if (compElement.TryGetProperty("range", out var r)) d.Range = (float)r.GetDouble();
                if (compElement.TryGetProperty("enabled", out var en)) d.Enabled = en.GetBoolean();
                compModel.Data = d;
                break;
            }

            case ComponentType.SpotLight:
            {
                var d = new SpotLightData();
                d.Color = ParseVector4(compElement, "color", Vector4.One);
                if (compElement.TryGetProperty("intensity", out var p)) d.Intensity = (float)p.GetDouble();
                if (compElement.TryGetProperty("range", out var r)) d.Range = (float)r.GetDouble();
                if (compElement.TryGetProperty("innerCone", out var ic)) d.InnerCone = (float)ic.GetDouble();
                if (compElement.TryGetProperty("outerCone", out var oc)) d.OuterCone = (float)oc.GetDouble();
                if (compElement.TryGetProperty("enabled", out var en)) d.Enabled = en.GetBoolean();
                compModel.Data = d;
                break;
            }

            case ComponentType.PointLight2D:
            {
                var d = new PointLight2DData();
                d.Color = ParseVector4(compElement, "color", Vector4.One);
                if (compElement.TryGetProperty("radius", out var rad)) d.Radius = (float)rad.GetDouble();
                if (compElement.TryGetProperty("intensity", out var p)) d.Intensity = (float)p.GetDouble();
                if (compElement.TryGetProperty("enabled", out var en)) d.Enabled = en.GetBoolean();
                compModel.Data = d;
                break;
            }

            case ComponentType.Sprite2D:
            {
                var d = new SpriteData();
                d.Color = ParseVector4(compElement, "color", Vector4.One);
                d.Tiling = ParseVector2(compElement, "tiling", Vector2.One);
                if (compElement.TryGetProperty("flipX", out var fx)) d.FlipX = fx.GetBoolean();
                if (compElement.TryGetProperty("flipY", out var fy)) d.FlipY = fy.GetBoolean();
                compModel.Data = d;
                break;
            }

            case ComponentType.Circle2D:
            {
                var d = new Circle2DData();
                d.Color = ParseVector4(compElement, "color", Vector4.One);
                if (compElement.TryGetProperty("thickness", out var th)) d.Thickness = (float)th.GetDouble();
                if (compElement.TryGetProperty("fade", out var fd)) d.Fade = (float)fd.GetDouble();
                compModel.Data = d;
                break;
            }

            case ComponentType.Rigidbody:
            {
                var d = new RigidbodyData();
                if (compElement.TryGetProperty("bodyType", out var bt)) d.BodyType = bt.GetInt32();
                if (compElement.TryGetProperty("mass", out var massProp)) d.Mass = (float)massProp.GetDouble();
                if (compElement.TryGetProperty("linearDamping", out var ld)) d.LinearDamping = (float)ld.GetDouble();
                if (compElement.TryGetProperty("angularDamping", out var ad)) d.AngularDamping = (float)ad.GetDouble();
                if (compElement.TryGetProperty("friction", out var fr)) d.Friction = (float)fr.GetDouble();
                if (compElement.TryGetProperty("restitution", out var re)) d.Restitution = (float)re.GetDouble();
                if (compElement.TryGetProperty("useGravity", out var ug)) d.UseGravity = ug.GetBoolean();
                compModel.Data = d;
                break;
            }

            case ComponentType.Rigidbody2D:
            {
                var d = new Rigidbody2DData();
                if (compElement.TryGetProperty("bodyType", out var bt)) d.BodyType = bt.GetInt32();
                if (compElement.TryGetProperty("gravityScale", out var gs)) d.GravityScale = (float)gs.GetDouble();
                if (compElement.TryGetProperty("linearDamping", out var ld)) d.LinearDamping = (float)ld.GetDouble();
                if (compElement.TryGetProperty("angularDamping", out var ad)) d.AngularDamping = (float)ad.GetDouble();
                if (compElement.TryGetProperty("fixedRotation", out var fr)) d.FixedRotation = fr.GetBoolean();
                if (compElement.TryGetProperty("isAwake", out var aw)) d.IsAwake = aw.GetBoolean();
                if (compElement.TryGetProperty("isEnabled", out var en)) d.IsEnabled = en.GetBoolean();
                compModel.Data = d;
                break;
            }

            case ComponentType.BoxCollider:
            {
                var d = new BoxColliderData();
                d.Center = ParseVector3(compElement, "center", Vector3.Zero);
                d.Size = ParseVector3(compElement, "size", Vector3.One);
                compModel.Data = d;
                break;
            }

            case ComponentType.SphereCollider:
            {
                var d = new SphereColliderData();
                d.Center = ParseVector3(compElement, "center", Vector3.Zero);
                if (compElement.TryGetProperty("radius", out var r)) d.Radius = (float)r.GetDouble();
                compModel.Data = d;
                break;
            }

            case ComponentType.CapsuleCollider:
            {
                var d = new CapsuleColliderData();
                d.Center = ParseVector3(compElement, "center", Vector3.Zero);
                if (compElement.TryGetProperty("radius", out var r)) d.Radius = (float)r.GetDouble();
                if (compElement.TryGetProperty("height", out var h)) d.Height = (float)h.GetDouble();
                compModel.Data = d;
                break;
            }

            case ComponentType.BoxCollider2D:
            {
                var d = new BoxCollider2DData();
                d.Offset = ParseVector2(compElement, "offset", Vector2.Zero);
                d.Size = ParseVector2(compElement, "size", new Vector2(0.5f, 0.5f));
                if (compElement.TryGetProperty("density", out var de)) d.Density = (float)de.GetDouble();
                if (compElement.TryGetProperty("friction", out var fr)) d.Friction = (float)fr.GetDouble();
                if (compElement.TryGetProperty("restitution", out var re)) d.Restitution = (float)re.GetDouble();
                if (compElement.TryGetProperty("isSensor", out var se)) d.IsSensor = se.GetBoolean();
                compModel.Data = d;
                break;
            }

            case ComponentType.CircleCollider2D:
            {
                var d = new CircleCollider2DData();
                d.Center = ParseVector2(compElement, "center", Vector2.Zero);
                if (compElement.TryGetProperty("radius", out var r)) d.Radius = (float)r.GetDouble();
                if (compElement.TryGetProperty("density", out var de)) d.Density = (float)de.GetDouble();
                if (compElement.TryGetProperty("friction", out var fr)) d.Friction = (float)fr.GetDouble();
                if (compElement.TryGetProperty("restitution", out var re)) d.Restitution = (float)re.GetDouble();
                if (compElement.TryGetProperty("isSensor", out var se)) d.IsSensor = se.GetBoolean();
                compModel.Data = d;
                break;
            }

            case ComponentType.CharacterController:
            {
                var d = new CharacterControllerData();
                if (compElement.TryGetProperty("radius", out var r)) d.Radius = (float)r.GetDouble();
                if (compElement.TryGetProperty("height", out var h)) d.Height = (float)h.GetDouble();
                if (compElement.TryGetProperty("maxStepHeight", out var sh)) d.MaxStepHeight = (float)sh.GetDouble();
                if (compElement.TryGetProperty("maxSlopeAngle", out var sa)) d.MaxSlopeAngle = (float)sa.GetDouble();
                if (compElement.TryGetProperty("mass", out var m)) d.Mass = (float)m.GetDouble();
                if (compElement.TryGetProperty("friction", out var fr)) d.Friction = (float)fr.GetDouble();
                compModel.Data = d;
                break;
            }

            case ComponentType.AudioSource:
            {
                var d = new AudioSourceData();
                if (compElement.TryGetProperty("volume", out var v)) d.Volume = (float)v.GetDouble();
                if (compElement.TryGetProperty("pitch", out var pi)) d.Pitch = (float)pi.GetDouble();
                if (compElement.TryGetProperty("pan", out var pa)) d.Pan = (float)pa.GetDouble();
                if (compElement.TryGetProperty("playOnStart", out var ps)) d.PlayOnStart = ps.GetBoolean();
                if (compElement.TryGetProperty("loop", out var lp)) d.Loop = lp.GetBoolean();
                compModel.Data = d;
                break;
            }

            case ComponentType.Text:
            {
                var d = new TextData();
                if (compElement.TryGetProperty("text", out var t)) d.Text = t.GetString() ?? "Empty";
                d.Color = ParseVector4(compElement, "color", Vector4.One);
                if (compElement.TryGetProperty("kerning", out var k)) d.Kerning = (float)k.GetDouble();
                if (compElement.TryGetProperty("lineSpacing", out var ls)) d.LineSpacing = (float)ls.GetDouble();
                if (compElement.TryGetProperty("screenSpace", out var ss)) d.ScreenSpace = ss.GetBoolean();
                compModel.Data = d;
                break;
            }

            case ComponentType.WorldEnvironment:
            {
                var d = new WorldEnvironmentData();
                if (compElement.TryGetProperty("exposure", out var ex)) d.Exposure = (float)ex.GetDouble();
                if (compElement.TryGetProperty("gamma", out var ga)) d.Gamma = (float)ga.GetDouble();
                if (compElement.TryGetProperty("ambient", out var am)) d.Ambient = (float)am.GetDouble();
                if (compElement.TryGetProperty("fogDensity", out var fd)) d.FogDensity = (float)fd.GetDouble();
                d.FogColor = ParseVector4(compElement, "fogColor", new Vector4(0.5f, 0.6f, 0.7f, 1.0f));
                if (compElement.TryGetProperty("fogStart", out var fs)) d.FogStart = (float)fs.GetDouble();
                if (compElement.TryGetProperty("fogEnd", out var fe)) d.FogEnd = (float)fe.GetDouble();
                compModel.Data = d;
                break;
            }

            case ComponentType.Script:
            {
                var d = new ScriptData();
                if (compElement.TryGetProperty("className", out var cn)) d.ClassName = cn.GetString() ?? "";
                compModel.Data = d;
                break;
            }
        }

        return compModel;
    }

    private static Vector4 ParseVector4(JsonElement el, string propName, Vector4 defaultVal)
    {
        if (el.TryGetProperty(propName, out var p) && p.ValueKind == JsonValueKind.Array)
        {
            float x = defaultVal.X, y = defaultVal.Y, z = defaultVal.Z, w = defaultVal.W;
            int i = 0;
            foreach (var item in p.EnumerateArray())
            {
                if (i == 0) x = (float)item.GetDouble();
                else if (i == 1) y = (float)item.GetDouble();
                else if (i == 2) z = (float)item.GetDouble();
                else if (i == 3) w = (float)item.GetDouble();
                i++;
            }
            return new Vector4(x, y, z, w);
        }
        return defaultVal;
    }

    private static Vector3 ParseVector3(JsonElement el, string propName, Vector3 defaultVal)
    {
        if (el.TryGetProperty(propName, out var p) && p.ValueKind == JsonValueKind.Array)
        {
            float x = defaultVal.X, y = defaultVal.Y, z = defaultVal.Z;
            int i = 0;
            foreach (var item in p.EnumerateArray())
            {
                if (i == 0) x = (float)item.GetDouble();
                else if (i == 1) y = (float)item.GetDouble();
                else if (i == 2) z = (float)item.GetDouble();
                i++;
            }
            return new Vector3(x, y, z);
        }
        return defaultVal;
    }

    private static Vector2 ParseVector2(JsonElement el, string propName, Vector2 defaultVal)
    {
        if (el.TryGetProperty(propName, out var p) && p.ValueKind == JsonValueKind.Array)
        {
            float x = defaultVal.X, y = defaultVal.Y;
            int i = 0;
            foreach (var item in p.EnumerateArray())
            {
                if (i == 0) x = (float)item.GetDouble();
                else if (i == 1) y = (float)item.GetDouble();
                i++;
            }
            return new Vector2(x, y);
        }
        return defaultVal;
    }
}
