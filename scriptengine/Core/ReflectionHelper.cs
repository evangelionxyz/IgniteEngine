using System;
using System.Linq;
using System.Reflection;

namespace Ignite;

public struct FieldMetadata
{
    public string Name;
    public string TypeName;
    public string FullTypeName;
    public bool IsPublic;
    public bool IsStatic;
}

public struct TypeMetadata
{
    public string FullName;
    public string Namespace;
    public string ClassName;
    public string AssemblyName;
    public FieldMetadata[] Fields;
}

public static class ReflectionHelper
{
    public static TypeMetadata GetTypeMetadata(string typeName, string assemblyPath)
    {
        try
        {
            var assembly = Assembly.LoadFrom(assemblyPath);
            var type = assembly.GetType(typeName);

            if (type == null)
            {
                Console.WriteLine($"[ReflectionHelper] Type {typeName} not found in {assemblyPath}");
                return default;
            }

            var fields = type.GetFields(BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance)
                .Where(f => !f.IsSpecialName && !f.Name.StartsWith("<"))
                .Select(f => new FieldMetadata
                {
                    Name = f.Name,
                    TypeName = f.FieldType.Name,
                    FullTypeName = f.FieldType.FullName ?? f.FieldType.Name,
                    IsPublic = f.IsPublic,
                    IsStatic = f.IsStatic
                })
                .ToArray();

            return new TypeMetadata
            {
                FullName = type.FullName ?? type.Name,
                Namespace = type.Namespace ?? string.Empty,
                ClassName = type.Name,
                AssemblyName = assembly.GetName().Name ?? string.Empty,
                Fields = fields
            };
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[ReflectionHelper] Error getting metadata for {typeName}: {ex.Message}");
            return default;
        }
    }

    public static string[] GetPublicFields(string typeName, string assemblyPath)
    {
        try
        {
            var assembly = Assembly.LoadFrom(assemblyPath);
            var type = assembly.GetType(typeName);

            if (type == null)
            {
                Console.WriteLine($"[ReflectionHelper] Type {typeName} not found in {assemblyPath}");
                return Array.Empty<string>();
            }

            return type.GetFields(BindingFlags.Public | BindingFlags.Instance)
                .Where(f => !f.IsSpecialName && !f.Name.StartsWith("<"))
                .Select(f => $"{f.Name}:{f.FieldType.FullName ?? f.FieldType.Name}")
                .ToArray();
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[ReflectionHelper] Error getting fields for {typeName}: {ex.Message}");
            return Array.Empty<string>();
        }
    }

    public static bool HasMethod(string typeName, string methodName, string assemblyPath)
    {
        try
        {
            var assembly = Assembly.LoadFrom(assemblyPath);
            var type = assembly.GetType(typeName);

            if (type == null)
                return false;

            var method = type.GetMethod(methodName, BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance);
            return method != null;
        }
        catch
        {
            return false;
        }
    }

    public static string[] GetMethodNames(string typeName, string assemblyPath)
    {
        try
        {
            var assembly = Assembly.LoadFrom(assemblyPath);
            var type = assembly.GetType(typeName);

            if (type == null)
                return Array.Empty<string>();

            return type.GetMethods(BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance | BindingFlags.DeclaredOnly)
                .Where(m => !m.IsSpecialName)
                .Select(m => m.Name)
                .ToArray();
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[ReflectionHelper] Error getting methods for {typeName}: {ex.Message}");
            return Array.Empty<string>();
        }
    }
}