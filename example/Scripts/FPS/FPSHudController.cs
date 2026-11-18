using System;
using Ignite;
using Ignite.Core.Component;
using static Ignite.Mathf;

namespace PlatformerDemo.FPS
{
    public class FPSHudController : Entity
    {
        public string ServerAddress = "127.0.0.1";
        public int ServerPort = 8192;

        private WidgetComponent _widget;

        // Labels
        private WidgetLabel _statusTitle;
        private WidgetLabel _scoreLabel;
        private WidgetLabel _controlsHint;
        private WidgetLabel _feedToastLabel;
        private WidgetLabel _speedLabel;
        private WidgetLabel _healthLabel;
        private WidgetLabel _shieldSubLabel;
        private WidgetLabel _weaponLabel;
        private WidgetLabel _ammoLabel;
        private WidgetLabel _centerHitBanner;

        // Moving / Animated Panels & Bars
        private WidgetPanel _topLeftAccentBar;
        private WidgetPanel _speedBarFill;
        private WidgetPanel _healthBarFill;
        private WidgetPanel _shieldBarFill;
        private WidgetPanel _bottomRightWeaponCard;
        private WidgetPanel _weaponAccentBar;
        private WidgetPanel _ammoBarFill;

        // Dynamic Crosshair & Reload Modules
        private WidgetPanel _crosshairTop;
        private WidgetPanel _crosshairBottom;
        private WidgetPanel _crosshairLeft;
        private WidgetPanel _crosshairRight;
        private WidgetPanel _crosshairDot;
        private WidgetPanel _hitMarkerRing;
        private WidgetPanel _reloadBarContainer;
        private WidgetPanel _reloadBarFill;

        // Tactical Pause Modal
        private WidgetPanel _pauseOverlay;
        private WidgetPanel _pauseHeaderAccent;
        private WidgetButton _pauseMenuBtn;
        private WidgetButton _resumeBtn;
        private WidgetButton _switchWeaponBtn;
        private WidgetButton _reconnectBtn;
        private WidgetButton _resetArenaBtn;
        private WidgetButton _mainMenuBtn;
        private WidgetButton _quitBtn;

        // Audio entities for button states (hover enter/exit, click start, click end)
        private Entity _sfxHover;
        private Entity _sfxClickStart;
        private Entity _sfxClickEnd;

        private float _animTime;
        private float _smoothSpeedBarWidth = 20f;
        private float _smoothHealthBarWidth = 316f;
        private float _smoothShieldBarWidth = 316f;
        private float _smoothAmmoBarWidth = 332f;
        private float _smoothCrosshairSpread = 11f;

        public override void OnCreate()
        {
            _widget = GetComponent<WidgetComponent>();
            _sfxHover = Entity.FindEntity("SFX_UIHover");
            _sfxClickStart = Entity.FindEntity("SFX_UIClick");
            _sfxClickEnd = Entity.FindEntity("SFX_UIClickEnd") ?? _sfxClickStart;

            if (_widget == null)
                return;

            _statusTitle = _widget.GetLabel("StatusTitle");
            _scoreLabel = _widget.GetLabel("ScoreLabel");
            _controlsHint = _widget.GetLabel("ControlsHint");
            _feedToastLabel = _widget.GetLabel("FeedToastLabel");
            _speedLabel = _widget.GetLabel("SpeedLabel");
            _healthLabel = _widget.GetLabel("HealthLabel");
            _shieldSubLabel = _widget.GetLabel("ShieldSubLabel");
            _weaponLabel = _widget.GetLabel("WeaponLabel");
            _ammoLabel = _widget.GetLabel("AmmoLabel");
            _centerHitBanner = _widget.GetLabel("CenterHitBanner");

            _topLeftAccentBar = _widget.GetPanel("TopLeftAccentBar");
            _speedBarFill = _widget.GetPanel("SpeedBarFill");
            _healthBarFill = _widget.GetPanel("HealthBarFill");
            _shieldBarFill = _widget.GetPanel("ShieldBarFill");
            _bottomRightWeaponCard = _widget.GetPanel("BottomRightWeaponCard");
            _weaponAccentBar = _widget.GetPanel("WeaponAccentBar");
            _ammoBarFill = _widget.GetPanel("AmmoBarFill");

            _crosshairTop = _widget.GetPanel("CrosshairTop");
            _crosshairBottom = _widget.GetPanel("CrosshairBottom");
            _crosshairLeft = _widget.GetPanel("CrosshairLeft");
            _crosshairRight = _widget.GetPanel("CrosshairRight");
            _crosshairDot = _widget.GetPanel("CrosshairCenterDot");
            _hitMarkerRing = _widget.GetPanel("HitMarkerRing");
            _reloadBarContainer = _widget.GetPanel("ReloadBarContainer");
            _reloadBarFill = _widget.GetPanel("ReloadBarFill");

            _pauseOverlay = _widget.GetPanel("PauseOverlay");
            _pauseHeaderAccent = _widget.GetPanel("PauseHeaderAccent");

            _pauseMenuBtn = _widget.GetButton("PauseMenuButton");
            _resumeBtn = _widget.GetButton("ResumeButton");
            _switchWeaponBtn = _widget.GetButton("SwitchWeaponButton");
            _reconnectBtn = _widget.GetButton("ReconnectButton");
            _resetArenaBtn = _widget.GetButton("ResetArenaButton");
            _mainMenuBtn = _widget.GetButton("MainMenuButton");
            _quitBtn = _widget.GetButton("QuitButton");

            BindButtonStates(_pauseMenuBtn);
            if (_pauseMenuBtn != null) _pauseMenuBtn.OnClickEvent += OnPauseMenuClicked;

            BindButtonStates(_resumeBtn);
            if (_resumeBtn != null) _resumeBtn.OnClickEvent += OnResumeClicked;

            BindButtonStates(_switchWeaponBtn);
            if (_switchWeaponBtn != null) _switchWeaponBtn.OnClickEvent += OnSwitchWeaponClicked;

            BindButtonStates(_reconnectBtn);
            if (_reconnectBtn != null) _reconnectBtn.OnClickEvent += OnReconnectClicked;

            BindButtonStates(_resetArenaBtn);
            if (_resetArenaBtn != null) _resetArenaBtn.OnClickEvent += OnResetArenaClicked;

            BindButtonStates(_mainMenuBtn);
            if (_mainMenuBtn != null) _mainMenuBtn.OnClickEvent += OnMainMenuClicked;

            BindButtonStates(_quitBtn);
            if (_quitBtn != null) _quitBtn.OnClickEvent += OnQuitClicked;

            SyncPauseModalState();
        }

        private void BindButtonStates(WidgetButton btn)
        {
            if (btn == null)
                return;
            btn.OnHoverEnterEvent += OnButtonHoverEnter;
            btn.OnHoverExitEvent += OnButtonHoverExit;
            btn.OnPressedEvent += OnButtonPressed;
            btn.OnReleasedEvent += OnButtonReleased;
        }

        public override void OnUpdate(float dt)
        {
            if (_widget == null)
                return;

            _animTime += dt;
            SyncPauseModalState();

            // 1. Top-Left Network & Score Telemetry
            if (_statusTitle != null)
            {
                string netStr = Net.IsConnected
                    ? $"ONLINE // DEDICATED ARENA  |  ID #{Net.GetClientId()}  |  RTT {Net.GetStats().RttMs} ms"
                    : (Net.IsServerRunning
                        ? $"DEDICATED SERVER HOST (:{ServerPort})  |  CLIENTS: {Net.GetClientCount()}"
                        : $"LOCAL TACTICAL ARENA  |  FOV {FPSGameSession.CurrentFov:F0} DEG");
                _statusTitle.Text = netStr;
            }

            if (_scoreLabel != null)
            {
                int acc = (FPSGameSession.ShotsFired > 0)
                    ? (int)MathF.Min(100f, (FPSGameSession.HitCount * 100f) / FPSGameSession.ShotsFired)
                    : 100;
                _scoreLabel.Text = $"SCORE {FPSGameSession.Score:D5}  |  HITS {FPSGameSession.HitCount} ({acc}%)  |  STREAK x{FPSGameSession.CurrentStreak}";
            }

            if (_feedToastLabel != null)
            {
                _feedToastLabel.Text = FPSGameSession.EventFeedText;
                float pulse = 0.82f + 0.18f * MathF.Sin(_animTime * 5.0f);
                _feedToastLabel.Color = (FPSGameSession.HitMarkerTimer > 0f)
                    ? new Vector4(1.0f, 0.88f, 0.30f, 1.0f)
                    : new Vector4(0.38f * pulse, 0.98f * pulse, 0.72f * pulse, 0.96f);
            }

            if (_topLeftAccentBar != null)
            {
                float glow = 0.80f + 0.20f * MathF.Sin(_animTime * 4.0f);
                _topLeftAccentBar.BackgroundColor = new Vector4(0.18f, 0.92f * glow, 1.0f, 0.98f);
            }

            // 2. Top-Center Velocity & Sprint Bar (Moving Part)
            float speed = FPSGameSession.CurrentMoveSpeed;
            if (_speedLabel != null)
            {
                string stateTag = FPSGameSession.IsAirborne
                    ? "[AIRBORNE]"
                    : (FPSGameSession.IsSprinting ? "[SPRINT OVERDRIVE]" : "[TACTICAL CRUISE]");
                _speedLabel.Text = $"VELOCITY  {speed:00.0} m/s  {stateTag}";
                _speedLabel.Color = FPSGameSession.IsSprinting
                    ? new Vector4(1.0f, 0.86f, 0.28f, 1.0f)
                    : new Vector4(0.84f, 0.96f, 1.0f, 0.98f);
            }

            if (_speedBarFill != null)
            {
                float normSpeed = Clamp(speed / 15.0f, 0.04f, 1.0f);
                float targetWidth = 270.0f * normSpeed;
                _smoothSpeedBarWidth += (targetWidth - _smoothSpeedBarWidth) * MathF.Min(1.0f, dt * 14.0f);
                _speedBarFill.Size = new Vector2(_smoothSpeedBarWidth, 10.0f);
                _speedBarFill.BackgroundColor = FPSGameSession.IsSprinting
                    ? new Vector4(1.0f, 0.82f, 0.22f, 0.98f)
                    : new Vector4(0.20f, 0.90f, 1.0f, 0.96f);
            }

            // 3. Center Dynamic Crosshair Spread, Hitmarker & Reload Bar (Moving Parts)
            float targetSpread = 10.0f + FPSGameSession.CrosshairBloom;
            _smoothCrosshairSpread += (targetSpread - _smoothCrosshairSpread) * MathF.Min(1.0f, dt * 18.0f);
            float sp = MathF.Round(_smoothCrosshairSpread);

            bool isPlasma = (FPSGameSession.CurrentWeaponSlot == 2);
            Vector4 chColor = isPlasma
                ? new Vector4(0.25f, 0.96f, 1.0f, 0.96f)
                : new Vector4(0.28f, 1.0f, 0.68f, 0.96f);

            if (_crosshairTop != null)
            {
                _crosshairTop.Position = new Vector2(0f, -sp);
                _crosshairTop.BackgroundColor = chColor;
            }
            if (_crosshairBottom != null)
            {
                _crosshairBottom.Position = new Vector2(0f, sp);
                _crosshairBottom.BackgroundColor = chColor;
            }
            if (_crosshairLeft != null)
            {
                _crosshairLeft.Position = new Vector2(-sp, 0f);
                _crosshairLeft.BackgroundColor = chColor;
            }
            if (_crosshairRight != null)
            {
                _crosshairRight.Position = new Vector2(sp, 0f);
                _crosshairRight.BackgroundColor = chColor;
            }
            if (_crosshairDot != null)
            {
                _crosshairDot.BackgroundColor = chColor;
            }

            if (_hitMarkerRing != null)
            {
                bool showHit = FPSGameSession.HitMarkerTimer > 0f;
                _hitMarkerRing.Visible = showHit;
                if (showHit)
                {
                    float ringSize = 24.0f + (1.0f - Clamp(FPSGameSession.HitMarkerTimer / 0.32f, 0f, 1f)) * 16.0f;
                    _hitMarkerRing.Size = new Vector2(ringSize, ringSize);
                    _hitMarkerRing.BorderColor = FPSGameSession.LastHitWasCrit
                        ? new Vector4(1.0f, 0.28f, 0.20f, 1.0f)
                        : new Vector4(1.0f, 0.88f, 0.25f, 1.0f);
                }
            }

            if (_centerHitBanner != null)
            {
                bool showBanner = FPSGameSession.HitMarkerTimer > 0f;
                _centerHitBanner.Visible = showBanner;
                if (showBanner)
                {
                    string prefix = FPSGameSession.LastHitWasCrit ? "CRITICAL HIT! " : "TARGET HIT ";
                    _centerHitBanner.Text = $"{prefix}-{FPSGameSession.LastHitDamage} HP  (+{FPSGameSession.LastHitScore})";
                    _centerHitBanner.Color = FPSGameSession.LastHitWasCrit
                        ? new Vector4(1.0f, 0.35f, 0.22f, 1.0f)
                        : new Vector4(1.0f, 0.88f, 0.28f, 1.0f);
                }
            }

            if (_reloadBarContainer != null && _reloadBarFill != null)
            {
                _reloadBarContainer.Visible = FPSGameSession.IsReloading;
                if (FPSGameSession.IsReloading)
                {
                    float fillW = MathF.Max(6.0f, 172.0f * Clamp(FPSGameSession.ReloadProgress, 0f, 1f));
                    _reloadBarFill.Size = new Vector2(fillW, 16.0f);
                }
            }

            // 4. Bottom-Left Vitality & Exo-Thruster Bars
            if (_healthLabel != null)
            {
                _healthLabel.Text = $"KINETIC ARMOR // HP  {FPSGameSession.CurrentHealth}";
            }
            if (_healthBarFill != null)
            {
                float hpRatio = Clamp((float)FPSGameSession.CurrentHealth / Math.Max(1, FPSGameSession.MaxHealth), 0f, 1f);
                float targetHpW = 316.0f * hpRatio;
                _smoothHealthBarWidth += (targetHpW - _smoothHealthBarWidth) * MathF.Min(1.0f, dt * 12.0f);
                _healthBarFill.Size = new Vector2(_smoothHealthBarWidth, 18.0f);
            }
            if (_shieldSubLabel != null)
            {
                _shieldSubLabel.Text = $"EXO-THRUSTER CHARGE  {FPSGameSession.ExoThrusterCharge:F0}%  [SHIFT / CTRL DASH]";
            }
            if (_shieldBarFill != null)
            {
                float targetShieldW = 316.0f * Clamp(FPSGameSession.ExoThrusterCharge / 100.0f, 0.02f, 1.0f);
                _smoothShieldBarWidth += (targetShieldW - _smoothShieldBarWidth) * MathF.Min(1.0f, dt * 14.0f);
                _shieldBarFill.Size = new Vector2(_smoothShieldBarWidth, 8.0f);
            }

            // 5. Bottom-Right Weapon & Magazine Module
            Vector4 weaponTheme = isPlasma
                ? new Vector4(0.22f, 0.92f, 1.0f, 0.96f)
                : new Vector4(1.0f, 0.76f, 0.22f, 0.96f);

            if (_bottomRightWeaponCard != null)
                _bottomRightWeaponCard.BorderColor = weaponTheme;
            if (_weaponAccentBar != null)
                _weaponAccentBar.BackgroundColor = weaponTheme;

            if (_weaponLabel != null)
            {
                _weaponLabel.Text = $"{FPSGameSession.CurrentWeaponName}  [{FPSGameSession.CurrentWeaponSlot}/2]";
                _weaponLabel.Color = weaponTheme;
            }
            if (_ammoLabel != null)
            {
                if (FPSGameSession.IsReloading)
                {
                    int pct = (int)(FPSGameSession.ReloadProgress * 100.0f);
                    _ammoLabel.Text = $"RELOADING... {pct}%";
                    _ammoLabel.Color = new Vector4(1.0f, 0.84f, 0.28f, 1.0f);
                }
                else
                {
                    _ammoLabel.Text = $"{FPSGameSession.CurrentAmmo:D2} / {FPSGameSession.MaxAmmo:D2}";
                    bool lowAmmo = FPSGameSession.CurrentAmmo <= (FPSGameSession.MaxAmmo / 4);
                    _ammoLabel.Color = lowAmmo
                        ? new Vector4(1.0f, 0.34f, 0.26f, 1.0f)
                        : new Vector4(0.96f, 0.99f, 1.0f, 1.0f);
                }
            }
            if (_ammoBarFill != null)
            {
                float ammoRatio = FPSGameSession.IsReloading
                    ? Clamp(FPSGameSession.ReloadProgress, 0.02f, 1.0f)
                    : Clamp((float)FPSGameSession.CurrentAmmo / Math.Max(1, FPSGameSession.MaxAmmo), 0.0f, 1.0f);
                float targetAmmoW = 332.0f * ammoRatio;
                _smoothAmmoBarWidth += (targetAmmoW - _smoothAmmoBarWidth) * MathF.Min(1.0f, dt * 16.0f);
                _ammoBarFill.Size = new Vector2(MathF.Max(2.0f, _smoothAmmoBarWidth), 14.0f);
                _ammoBarFill.BackgroundColor = weaponTheme;
            }
        }

        private void SyncPauseModalState()
        {
            if (_pauseOverlay != null)
                _pauseOverlay.Visible = FPSGameSession.IsPauseMenuOpen;
        }

        public void OnButtonHoverEnter()
        {
            PlayAudioEntity(_sfxHover, 1.15f);
        }

        public void OnButtonHoverExit()
        {
            PlayAudioEntity(_sfxHover, 0.78f);
        }

        public void OnButtonPressed()
        {
            PlayAudioEntity(_sfxClickStart, 0.92f);
        }

        public void OnButtonReleased()
        {
            PlayAudioEntity(_sfxClickEnd, 1.10f);
        }

        public void OnPauseMenuClicked()
        {
            FPSGameSession.IsPauseMenuOpen = !FPSGameSession.IsPauseMenuOpen;
            Input.SetCursorMode(FPSGameSession.IsPauseMenuOpen ? CursorMode.Normal : CursorMode.Disabled);
            SyncPauseModalState();
        }

        public void OnResumeClicked()
        {
            FPSGameSession.IsPauseMenuOpen = false;
            Input.SetCursorMode(CursorMode.Disabled);
            SyncPauseModalState();
        }

        public void OnSwitchWeaponClicked()
        {
            int nextSlot = (FPSGameSession.CurrentWeaponSlot == 1) ? 2 : 1;
            FPSGameSession.CurrentWeaponSlot = nextSlot;
            FPSGameSession.CurrentWeaponName = (nextSlot == 1) ? "VX-47 ASSAULT RIFLE" : "HYPERION PLASMA CARBINE";
            FPSGameSession.MaxAmmo = (nextSlot == 1) ? 30 : 18;
            FPSGameSession.CurrentAmmo = FPSGameSession.MaxAmmo;
            FPSGameSession.EventFeedText = $"SWITCHED LOADOUT -> {FPSGameSession.CurrentWeaponName}";
        }

        public void OnReconnectClicked()
        {
            Net.Disconnect();
            bool ok = Net.Connect(ServerAddress, (ushort)ServerPort, "VanguardOperative");
            FPSGameSession.EventFeedText = ok
                ? $"CONNECTING TO DEDICATED SERVER {ServerAddress}:{ServerPort}..."
                : $"CONNECTION FAILED ({ServerAddress}:{ServerPort})";
        }

        public void OnResetArenaClicked()
        {
            FPSGameSession.ResetMatch();
            FPSGameSession.IsPauseMenuOpen = false;
            Input.SetCursorMode(CursorMode.Disabled);
            SyncPauseModalState();
        }

        public void OnMainMenuClicked()
        {
            FPSGameSession.IsPauseMenuOpen = false;
            Input.SetCursorMode(CursorMode.Normal);
            Scene.Load("Assets/Scenes/FPSMenu.ixscene");
        }

        public void OnQuitClicked()
        {
            Net.Disconnect();
            FPSGameSession.IsPauseMenuOpen = false;
            Input.SetCursorMode(CursorMode.Normal);
            Scene.Stop();
        }

        private static void PlayAudioEntity(Entity audioEntity, float pitch = 1.0f)
        {
            if (audioEntity == null)
                return;

            var audio = audioEntity.GetComponent<AudioSourceComponent>();
            if (audio != null && audio.HasAudio)
            {
                audio.Stop();
                audio.Pitch = pitch;
                audio.Play();
            }
        }
    }
}
