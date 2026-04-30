using Ignite.Core;

namespace Ignite;

public static class Debug
{
    public enum LogLevel : byte
    {
        Trace = 0,
        Debug,
        Info,
        Warn,
        Error,
    }

    public static void Log(string message, LogLevel level = LogLevel.Trace)
    {
        CoreInternalCalls.Debug_Log(message, level);
    }
}
