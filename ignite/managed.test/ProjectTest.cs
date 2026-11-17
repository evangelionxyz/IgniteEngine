// Copyright (c) 2026 Evangelion Manuhutu

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Reflection;
using NUnit.Framework;

namespace Ignite.Managed.Test;

internal static class ProjectTest
{
    public static int Main(string[] args)
    {
        string? filter = null;
        for (int i = 0; i < args.Length; ++i)
        {
            if (args[i] == "--filter" && i + 1 < args.Length)
            {
                filter = args[++i];
            }
            else if (!args[i].StartsWith('-'))
            {
                filter = args[i];
            }
        }

        Assembly asm = typeof(ProjectTest).Assembly;
        List<Type> fixtures = asm.GetTypes()
            .Where(t => t.IsClass && !t.IsAbstract && t.GetCustomAttribute<TestFixtureAttribute>() != null)
            .OrderBy(t => t.FullName, StringComparer.Ordinal)
            .ToList();

        int passed = 0;
        int failed = 0;
        int skipped = 0;
        var totalSw = Stopwatch.StartNew();

        foreach (Type fixtureType in fixtures)
        {
            object? instance = null;
            MethodInfo[] methods = fixtureType.GetMethods(BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic);
            MethodInfo? oneTimeSetUp = methods.FirstOrDefault(m => m.GetCustomAttribute<OneTimeSetUpAttribute>() != null);
            MethodInfo? setUp = methods.FirstOrDefault(m => m.GetCustomAttribute<SetUpAttribute>() != null);
            MethodInfo? tearDown = methods.FirstOrDefault(m => m.GetCustomAttribute<TearDownAttribute>() != null);
            MethodInfo? oneTimeTearDown = methods.FirstOrDefault(m => m.GetCustomAttribute<OneTimeTearDownAttribute>() != null);

            var testMethods = methods
                .Where(m => m.GetCustomAttribute<TestAttribute>() != null || m.GetCustomAttributes<TestCaseAttribute>().Any())
                .Where(m => string.IsNullOrEmpty(filter) ||
                            $"{fixtureType.Name}.{m.Name}".Contains(filter, StringComparison.OrdinalIgnoreCase))
                .OrderBy(m => m.Name, StringComparer.Ordinal)
                .ToList();

            if (testMethods.Count == 0)
                continue;

            Console.WriteLine($"[SUITE] {fixtureType.Name}");
            try
            {
                instance = Activator.CreateInstance(fixtureType);
                oneTimeSetUp?.Invoke(instance, null);
            }
            catch (Exception ex)
            {
                Exception root = UnwrapException(ex);
                if (IsIgnoreException(root))
                {
                    foreach (var tm in testMethods)
                    {
                        ++skipped;
                        Console.WriteLine($"  [SKIP] {fixtureType.Name}.{tm.Name}: {root.Message}");
                    }
                    continue;
                }

                foreach (var tm in testMethods)
                {
                    ++failed;
                    Console.WriteLine($"  [FAIL] {fixtureType.Name}.{tm.Name} (OneTimeSetUp): {root.Message}");
                }
                continue;
            }

            foreach (MethodInfo testMethod in testMethods)
            {
                IgnoreAttribute? ignoreAttr = testMethod.GetCustomAttribute<IgnoreAttribute>();
                if (ignoreAttr != null)
                {
                    ++skipped;
                    Console.WriteLine($"  [SKIP] {fixtureType.Name}.{testMethod.Name}");
                    continue;
                }

                var testCases = testMethod.GetCustomAttributes<TestCaseAttribute>().ToList();
                IEnumerable<object?[]?> invocations = testCases.Count > 0
                    ? testCases.Select(tc => (object?[]?)tc.Arguments)
                    : new object?[]?[] { null };

                foreach (object?[]? callArgs in invocations)
                {
                    string caseLabel = callArgs is { Length: > 0 }
                        ? $"{fixtureType.Name}.{testMethod.Name}({string.Join(", ", callArgs)})"
                        : $"{fixtureType.Name}.{testMethod.Name}";

                    var sw = Stopwatch.StartNew();
                    try
                    {
                        setUp?.Invoke(instance, null);
                        testMethod.Invoke(instance, callArgs);
                        tearDown?.Invoke(instance, null);
                        sw.Stop();
                        ++passed;
                        Console.WriteLine($"  [PASS] {caseLabel} ({sw.ElapsedMilliseconds} ms)");
                    }
                    catch (Exception ex)
                    {
                        try { tearDown?.Invoke(instance, null); } catch { }
                        sw.Stop();
                        Exception root = UnwrapException(ex);
                        if (IsIgnoreException(root))
                        {
                            ++skipped;
                            Console.WriteLine($"  [SKIP] {caseLabel}: {root.Message}");
                        }
                        else if (root is SuccessException)
                        {
                            ++passed;
                            Console.WriteLine($"  [PASS] {caseLabel} ({sw.ElapsedMilliseconds} ms)");
                        }
                        else
                        {
                            ++failed;
                            Console.WriteLine($"  [FAIL] {caseLabel} ({sw.ElapsedMilliseconds} ms)");
                            Console.WriteLine($"         {root.GetType().Name}: {root.Message}");
                            if (!string.IsNullOrEmpty(root.StackTrace))
                                Console.WriteLine(root.StackTrace);
                        }
                    }
                }
            }

            try
            {
                oneTimeTearDown?.Invoke(instance, null);
            }
            catch (Exception ex)
            {
                Exception root = UnwrapException(ex);
                ++failed;
                Console.WriteLine($"  [FAIL] {fixtureType.Name}.OneTimeTearDown: {root.Message}");
            }
        }

        totalSw.Stop();
        Console.WriteLine();
        Console.WriteLine($"=== Ignite.Managed.Test Summary: {passed} passed, {failed} failed, {skipped} skipped ({totalSw.ElapsedMilliseconds} ms) ===");
        return failed == 0 ? 0 : 1;
    }

    private static Exception UnwrapException(Exception ex)
    {
        while (ex is TargetInvocationException tie && tie.InnerException != null)
            ex = tie.InnerException;
        return ex;
    }

    private static bool IsIgnoreException(Exception ex)
        => ex is IgnoreException || ex.GetType().Name == "IgnoreException";
}
