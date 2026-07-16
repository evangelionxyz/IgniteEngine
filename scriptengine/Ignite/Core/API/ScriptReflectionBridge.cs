// Copyright (c) 2026 Evangelion Manuhutu

using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Runtime.Loader;
using System.Text;

namespace Ignite.Core;

internal static class ScriptReflectionBridge
{
    internal static string GetDerivedTypes(string assemblyName, string baseTypeFullName)
    {
        if (string.IsNullOrWhiteSpace(assemblyName) || string.IsNullOrWhiteSpace(baseTypeFullName))
            return string.Empty;

        Assembly? assembly = ResolveAssembly(assemblyName);
        Type? baseType = assembly != null
            ? ResolveTypeInContext(assembly, baseTypeFullName)
            : ResolveType(baseTypeFullName);
        if (assembly == null || baseType == null)
            return string.Empty;

        var derivedTypes = assembly.GetTypes()
            .Where(type => type.IsClass && !type.IsAbstract && baseType.IsAssignableFrom(type))
            .Select(type => type.FullName)
            .Where(name => !string.IsNullOrWhiteSpace(name));

        return string.Join('|', derivedTypes!);
    }

    internal static string GetTypeFields(string typeName, string serializeFieldAttributeTypeName)
    {
        if (string.IsNullOrWhiteSpace(typeName))
            return string.Empty;

        Type? type = ResolveTypePreferScriptingContext(typeName);
        Type? serializeFieldAttributeType = ResolveTypePreferScriptingContext(serializeFieldAttributeTypeName);
        if (type == null)
            return string.Empty;

        var builder = new StringBuilder();
        var fields = type.GetFields(BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic);

        foreach (var field in fields)
        {
            bool isPublic = field.IsPublic;
            bool hasSerializeField = serializeFieldAttributeType != null && field.IsDefined(serializeFieldAttributeType, inherit: true);

            // Only expose public fields or fields explicitly marked with [SerializeField].
            // Private runtime-only fields (e.g. internal state, component wrappers) are skipped.
            if (!isPublic && !hasSerializeField)
                continue;

            if (builder.Length > 0)
                builder.Append('|');

            builder.Append(field.Name);
            builder.Append('~');
            builder.Append(GetNormalizedTypeName(field.FieldType));
            builder.Append('~');
            builder.Append(isPublic ? '1' : '0');
            builder.Append('~');
            builder.Append(hasSerializeField ? '1' : '0');

            if (field.FieldType.IsEnum)
            {
                builder.Append("~1~");
                builder.Append(string.Join(',', Enum.GetNames(field.FieldType)));
                builder.Append('~');
                builder.Append(string.Join(',', Enum.GetValues(field.FieldType).Cast<object>().Select(value => Convert.ToInt32(value))));
            }
        }

        return builder.ToString();
    }

    /// <summary>
    /// Returns a stable, compact type name for use as a field type key on the C++ side.
    /// 
    /// Rules:
    ///   - List&lt;T&gt;  → "List&lt;ElementFullName&gt;"  (normalized so C++ can map it directly)
    ///   - Everything else → raw FullName  (already covered by the C++ s_ScriptFieldTypeMap)
    /// </summary>
    private static string GetNormalizedTypeName(Type type)
    {
        // List<T>: emit "List<ElementFullName>" so C++ can look it up in s_ScriptFieldTypeMap.
        if (type.IsGenericType && type.GetGenericTypeDefinition() == typeof(System.Collections.Generic.List<>))
        {
            Type elementType = type.GetGenericArguments()[0];
            string elementName = elementType.FullName ?? elementType.Name;
            return $"List<{elementName}>";
        }

        return type.FullName ?? type.Name;
    }

    internal static string GetCreateAssetMenuData(string assemblyName, string baseTypeFullName)
    {
        if (string.IsNullOrWhiteSpace(assemblyName) || string.IsNullOrWhiteSpace(baseTypeFullName))
            return string.Empty;

        Assembly? assembly = ResolveAssembly(assemblyName);
        Type? baseType = assembly != null
            ? ResolveTypeInContext(assembly, baseTypeFullName)
            : ResolveType(baseTypeFullName);
        Type? createAssetMenuType = assembly != null
            ? ResolveTypeInContext(assembly, "Ignite.CreateAssetMenu")
            : ResolveType("Ignite.CreateAssetMenu");
        if (assembly == null || baseType == null || createAssetMenuType == null)
            return string.Empty;

        var builder = new StringBuilder();
        var types = assembly.GetTypes()
            .Where(type => type.IsClass && !type.IsAbstract && baseType.IsAssignableFrom(type));

        foreach (var type in types)
        {
            var attribute = type.GetCustomAttributes(createAssetMenuType, inherit: true).FirstOrDefault();
            if (attribute == null)
                continue;

            string fileName = type.Name;
            string menuName = string.Empty;

            var fileNameField = createAssetMenuType.GetField("FileName", BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic);
            var menuNameField = createAssetMenuType.GetField("MenuName", BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic);

            if (fileNameField?.GetValue(attribute) is string resolvedFileName && !string.IsNullOrWhiteSpace(resolvedFileName))
                fileName = resolvedFileName;

            if (menuNameField?.GetValue(attribute) is string resolvedMenuName)
                menuName = resolvedMenuName;

            if (builder.Length > 0)
                builder.Append('|');

            builder.Append(type.FullName);
            builder.Append('~');
            builder.Append(fileName);
            builder.Append('~');
            builder.Append(menuName);
        }

        return builder.ToString();
    }

    private static Assembly? ResolveAssembly(string assemblyName)
    {
        string expectedName = assemblyName.Trim();

        var selfContext = AssemblyLoadContext.GetLoadContext(typeof(ScriptReflectionBridge).Assembly);
        
        var alcList = new List<AssemblyLoadContext>(AssemblyLoadContext.All);
        alcList.Sort((a, b) =>
        {
            string nameA = a.Name ?? string.Empty;
            string nameB = b.Name ?? string.Empty;
            return string.Compare(nameB, nameA, StringComparison.OrdinalIgnoreCase);
        });

        // Search self context first
        if (selfContext != null)
        {
            var found = selfContext.Assemblies
                .FirstOrDefault(a => string.Equals(a.GetName().Name, expectedName, StringComparison.OrdinalIgnoreCase));
            if (found != null)
                return found;
        }

        // Search other sorted ALCs
        foreach (var alc in alcList)
        {
            if (alc == selfContext)
                continue;

            if (alc.Name == null || !alc.Name.StartsWith("Ignite.Scripting", StringComparison.OrdinalIgnoreCase))
                continue;

            var found = alc.Assemblies
                .FirstOrDefault(a => string.Equals(a.GetName().Name, expectedName, StringComparison.OrdinalIgnoreCase));
            if (found != null)
                return found;
        }

        // Fallback: search the whole domain (covers the case where the app assembly was
        // loaded into the default ALC or a different context for some reason).
        return AppDomain.CurrentDomain.GetAssemblies()
            .FirstOrDefault(a => string.Equals(a.GetName().Name, expectedName, StringComparison.OrdinalIgnoreCase));
    }

    private static Type? ResolveType(string typeName)
    {
        if (string.IsNullOrWhiteSpace(typeName))
            return null;

        string trimmedName = typeName.Trim();
        return Type.GetType(trimmedName, throwOnError: false)
            ?? AppDomain.CurrentDomain.GetAssemblies()
                .Select(assembly => assembly.GetType(trimmedName, throwOnError: false, ignoreCase: false))
                .FirstOrDefault(type => type != null);
    }

    private static Type? ResolveTypeInContext(Assembly contextAssembly, string typeName)
    {
        if (string.IsNullOrWhiteSpace(typeName))
            return null;

        string trimmedName = typeName.Trim();
        var loadContext = AssemblyLoadContext.GetLoadContext(contextAssembly);
        if (loadContext != null)
        {
            foreach (var assembly in loadContext.Assemblies)
            {
                var type = assembly.GetType(trimmedName, throwOnError: false, ignoreCase: false);
                if (type != null)
                    return type;
            }
        }

        return ResolveType(trimmedName);
    }

    private static Type? ResolveTypePreferScriptingContext(string typeName)
    {
        if (string.IsNullOrWhiteSpace(typeName))
            return null;

        string trimmedName = typeName.Trim();

        // 1. Get the current load context of the bridge (this ALC contains the refreshed core assemblies)
        var selfContext = AssemblyLoadContext.GetLoadContext(typeof(ScriptReflectionBridge).Assembly);
        
        // 2. Iterate all ALCs in the app, but prioritize the active one first.
        // We look for assemblies loaded in ALCs matching the "Ignite.Scripting" pattern.
        // To avoid picking up stale assemblies, we can order ALCs such that the highest reload counter
        // (the last one created) is checked first.
        var alcList = new List<AssemblyLoadContext>(AssemblyLoadContext.All);
        
        // Sort ALCs so that newer contexts (e.g. Ignite.Scripting.2) come before older ones (Ignite.Scripting.1)
        alcList.Sort((a, b) =>
        {
            string nameA = a.Name ?? string.Empty;
            string nameB = b.Name ?? string.Empty;
            return string.Compare(nameB, nameA, StringComparison.OrdinalIgnoreCase);
        });

        // Search in self context first if it's the active one
        if (selfContext != null)
        {
            foreach (var assembly in selfContext.Assemblies)
            {
                var type = assembly.GetType(trimmedName, throwOnError: false, ignoreCase: false);
                if (type != null)
                    return type;
            }
        }

        // Search sorted ALCs
        foreach (var alc in alcList)
        {
            if (alc == selfContext)
                continue;

            // Only consider the newer Ignite.Scripting ALCs
            if (alc.Name == null || !alc.Name.StartsWith("Ignite.Scripting", StringComparison.OrdinalIgnoreCase))
                continue;

            foreach (var assembly in alc.Assemblies)
            {
                var type = assembly.GetType(trimmedName, throwOnError: false, ignoreCase: false);
                if (type != null)
                    return type;
            }
        }

        // Fallback: search the whole domain (covers default ALC or other unusual layouts).
        return ResolveType(trimmedName);
    }
}
