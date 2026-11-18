-- ============================================================================
-- Ignite Engine - Lua 5.4 HUD Telemetry Bridge
-- Defers label & bar updates to C# FPSHudController so live combat telemetry,
-- dynamic crosshair bloom, velocity bars, and hitfeeds are never overwritten.
-- ============================================================================

local FpsHudBridge = {
    elapsed = 0.0,
}

function FpsHudBridge.on_create(entity)
    FpsHudBridge.elapsed = 0.0
end

function FpsHudBridge.on_update(entity, dt)
    FpsHudBridge.elapsed = FpsHudBridge.elapsed + dt
end

return FpsHudBridge
