using System;
using Ignite;
using Ignite.Core.Component;
using static Ignite.Mathf;

namespace PlatformerDemo.FPS
{
    public class FPSMenuController : Entity
    {
        public string ArenaScenePath = "Assets/Scenes/FPSArena.ixscene";
        public string ServerAddress = "127.0.0.1";
        public int ServerPort = 8192;

        private WidgetComponent _widget;
        private WidgetLabel _statusLabel;
        private WidgetPanel _pulseFill;

        private WidgetButton _enterArenaBtn;
        private WidgetButton _startLocalBtn;
        private WidgetButton _inspectWeaponsBtn;
        private WidgetButton _quitBtn;

        private Entity _heroHelmet;
        private Entity _orbRough;
        private Entity _orbCopper;
        private Entity _orbGold;

        private Entity _sfxHover;
        private Entity _sfxClickStart;
        private Entity _sfxClickEnd;

        private float _elapsed;
        private int _loadoutPreviewIndex;
        private bool _prevDeployKeyDown;

        public override void OnCreate()
        {
            Input.SetCursorMode(CursorMode.Normal);

            _widget = GetComponent<WidgetComponent>();
            _heroHelmet = Entity.FindEntity("Menu_HeroHelmet");
            _orbRough = Entity.FindEntity("Menu_OrbRough");
            _orbCopper = Entity.FindEntity("Menu_OrbCopper");
            _orbGold = Entity.FindEntity("Menu_OrbGold");

            _sfxHover = Entity.FindEntity("SFX_UIHover");
            _sfxClickStart = Entity.FindEntity("SFX_UIClick");
            _sfxClickEnd = Entity.FindEntity("SFX_UIClickEnd") ?? _sfxClickStart;

            if (_widget == null)
                return;

            _statusLabel = _widget.GetLabel("MenuStatusLabel");
            _pulseFill = _widget.GetPanel("MenuPulseFill");

            _enterArenaBtn = _widget.GetButton("EnterArenaButton");
            _startLocalBtn = _widget.GetButton("StartLocalMatchButton");
            _inspectWeaponsBtn = _widget.GetButton("InspectWeaponsButton");
            _quitBtn = _widget.GetButton("QuitGameButton");

            BindButtonStates(_enterArenaBtn);
            if (_enterArenaBtn != null) _enterArenaBtn.OnClickEvent += OnEnterArenaClicked;

            BindButtonStates(_startLocalBtn);
            if (_startLocalBtn != null) _startLocalBtn.OnClickEvent += OnStartLocalMatchClicked;

            BindButtonStates(_inspectWeaponsBtn);
            if (_inspectWeaponsBtn != null) _inspectWeaponsBtn.OnClickEvent += OnInspectWeaponsClicked;

            BindButtonStates(_quitBtn);
            if (_quitBtn != null) _quitBtn.OnClickEvent += OnQuitClicked;
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
            _elapsed += dt;

            // Animate 3D menu showcase centerpiece and orbital PBR spheres
            if (_heroHelmet != null)
            {
                _heroHelmet.Translation = new Vector3(2.1f, 1.85f + MathF.Sin(_elapsed * 1.6f) * 0.08f, -3.3f);
                _heroHelmet.Rotation = Quaternion.FromEuler(0.08f, _elapsed * 0.55f, 0f);
            }

            AnimateOrbitOrb(_orbRough, _elapsed, 0.0f, 1.75f);
            AnimateOrbitOrb(_orbCopper, _elapsed, 2.094f, 1.75f);
            AnimateOrbitOrb(_orbGold, _elapsed, 4.188f, 1.75f);

            // Animate scanning bar inside MainMenuCard
            if (_pulseFill != null)
            {
                float wave = 0.5f + 0.5f * MathF.Sin(_elapsed * 2.6f);
                float xPos = wave * (452.0f - 160.0f);
                _pulseFill.Position = new Vector2(xPos, 0f);
                _pulseFill.Size = new Vector2(160.0f, 6.0f);
            }

            bool deployDown = Input.IsKeyPressed(KeyCode.Return) || Input.IsKeyPressed(KeyCode.Space);
            if (deployDown && !_prevDeployKeyDown)
            {
                OnEnterArenaClicked();
            }
            _prevDeployKeyDown = deployDown;
        }

        private static void AnimateOrbitOrb(Entity orb, float time, float phaseOffset, float radius)
        {
            if (orb == null)
                return;

            float angle = time * 0.72f + phaseOffset;
            orb.Translation = new Vector3(
                2.1f + MathF.Cos(angle) * radius,
                0.75f + MathF.Sin(time * 2.0f + phaseOffset) * 0.14f,
                -3.3f + MathF.Sin(angle) * radius
            );
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

        public void OnEnterArenaClicked()
        {
            if (_statusLabel != null)
                _statusLabel.Text = $"CONNECTING TO {ServerAddress}:{ServerPort} & LOADING ARENA...";

            FPSGameSession.ResetMatch();
            FPSGameSession.Intent = FpsConnectionIntent.ConnectDedicated;
            FPSGameSession.ServerAddress = ServerAddress;
            FPSGameSession.ServerPort = (ushort)ServerPort;
            Net.Connect(ServerAddress, (ushort)ServerPort, "VanguardOperative");
            Scene.Load(ArenaScenePath);
        }

        public void OnStartLocalMatchClicked()
        {
            if (_statusLabel != null)
                _statusLabel.Text = "LAUNCHING SOLO COMPETITIVE ARENA & PBR LAB...";

            FPSGameSession.ResetMatch();
            FPSGameSession.Intent = FpsConnectionIntent.SoloOffline;
            Scene.Load(ArenaScenePath);
        }

        public void OnInspectWeaponsClicked()
        {
            _loadoutPreviewIndex = (_loadoutPreviewIndex + 1) % 3;
            if (_statusLabel != null)
            {
                if (_loadoutPreviewIndex == 1)
                    _statusLabel.Text = "LOADOUT 01: VX-47 ASSAULT RIFLE (850 RPM, HIGH-VELOCITY TRACER)";
                else if (_loadoutPreviewIndex == 2)
                    _statusLabel.Text = "LOADOUT 02: HYPERION PLASMA CARBINE (PLASMA BOLT + BLAST DETONATOR)";
                else
                    _statusLabel.Text = $"STATUS: READY TO DEPLOY -> {ServerAddress}:{ServerPort}";
            }

            if (_heroHelmet != null)
            {
                var sm = _heroHelmet.GetComponent<StaticMeshComponent>();
                if (sm != null)
                {
                    if (_loadoutPreviewIndex == 1)
                        sm.MeshHandle = 9500000000000000001UL; // VX47_AssaultRifle
                    else if (_loadoutPreviewIndex == 2)
                        sm.MeshHandle = 9500000000000000002UL; // PlasmaCarbine
                    else
                        sm.MeshHandle = 12091667214104364287UL; // DamagedHelmet
                }
            }
        }

        public void OnQuitClicked()
        {
            if (_statusLabel != null)
                _statusLabel.Text = "EXITING...";
            Net.Disconnect();
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
