using System;

namespace IgniteEditor.Models;

public enum LogLevel
{
    Trace, Debug, Info, Warning, Error, Critical
}

public record LogEntry(DateTime Timestamp, LogLevel Level, string Message);
