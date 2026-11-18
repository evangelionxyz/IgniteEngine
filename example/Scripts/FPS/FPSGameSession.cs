using System;
using Ignite;

namespace PlatformerDemo.FPS
{
    public enum FpsConnectionIntent
    {
        SoloOffline = 0,
        ConnectDedicated = 1,
        HostListen = 2
    }

    /// <summary>
    /// Shared tactical session state bridging C# gameplay scripts, HUD widgets,
    /// world-space hit reactions, and Lua target scripts.
    /// </summary>
    public static class FPSGameSession
    {
        public static FpsConnectionIntent Intent { get; set; } = FpsConnectionIntent.ConnectDedicated;
        public static string ServerAddress { get; set; } = "127.0.0.1";
        public static ushort ServerPort { get; set; } = 8192;
        public static string LastEventBanner
        {
            get => EventFeedText;
            set => EventFeedText = value ?? string.Empty;
        }

        public static int Score { get; set; }
        public static int HitCount { get; set; }
        public static int ShotsFired { get; set; }
        public static int TargetsRemaining { get; set; } = 6;
        public static int TotalTargets { get; set; } = 6;
        public static int CurrentStreak { get; set; }
        public static int BestStreak { get; set; }

        public static int CurrentHealth { get; set; } = 100;
        public static int MaxHealth { get; set; } = 100;
        public static float ExoThrusterCharge { get; set; } = 100.0f;

        public static int CurrentWeaponSlot { get; set; } = 1;
        public static string CurrentWeaponName { get; set; } = "VX-47 ASSAULT RIFLE";
        public static int CurrentAmmo { get; set; } = 30;
        public static int MaxAmmo { get; set; } = 30;
        public static bool IsReloading { get; set; }
        public static float ReloadProgress { get; set; }

        public static float CurrentMoveSpeed { get; set; }
        public static bool IsSprinting { get; set; }
        public static bool IsAirborne { get; set; }
        public static float CurrentFov { get; set; } = 90.0f;
        public static float CrosshairBloom { get; set; }

        public static bool IsPauseMenuOpen { get; set; }
        public static float HitMarkerTimer { get; set; }
        public static bool LastHitWasCrit { get; set; }
        public static int LastHitDamage { get; set; }
        public static int LastHitScore { get; set; }
        public static string LastHitTarget { get; set; } = "";
        public static string EventFeedText { get; set; } = "ARENA LIVE // ENGAGE DRONES & HAZARD BARRELS";

        public static void ResetMatch()
        {
            Score = 0;
            HitCount = 0;
            ShotsFired = 0;
            TargetsRemaining = TotalTargets;
            CurrentStreak = 0;
            BestStreak = 0;
            CurrentHealth = MaxHealth;
            ExoThrusterCharge = 100.0f;
            CurrentWeaponSlot = 1;
            CurrentWeaponName = "VX-47 ASSAULT RIFLE";
            CurrentAmmo = 30;
            MaxAmmo = 30;
            IsReloading = false;
            ReloadProgress = 0f;
            CurrentMoveSpeed = 0f;
            IsSprinting = false;
            IsAirborne = false;
            CurrentFov = 90.0f;
            CrosshairBloom = 0f;
            IsPauseMenuOpen = false;
            HitMarkerTimer = 0f;
            LastHitWasCrit = false;
            LastHitDamage = 0;
            LastHitScore = 0;
            LastHitTarget = "";
            EventFeedText = "ARENA LIVE // ENGAGE DRONES & HAZARD BARRELS";
        }

        public static void RecordHit(string targetName, int damage, int points, bool isCrit = false)
        {
            HitCount++;
            CurrentStreak++;
            if (CurrentStreak > BestStreak)
                BestStreak = CurrentStreak;

            int streakBonus = (CurrentStreak >= 3) ? (CurrentStreak - 2) * 25 : 0;
            int totalPoints = points + streakBonus;
            Score += totalPoints;

            HitMarkerTimer = isCrit ? 0.36f : 0.24f;
            LastHitWasCrit = isCrit;
            LastHitDamage = damage;
            LastHitScore = totalPoints;
            LastHitTarget = targetName;

            string critTag = isCrit ? "CRIT! " : "";
            string streakTag = (CurrentStreak >= 2) ? $" [STREAK x{CurrentStreak}]" : "";
            EventFeedText = $"{critTag}HIT {targetName} (-{damage} HP)  +{totalPoints} PTS{streakTag}";
        }
    }
}
