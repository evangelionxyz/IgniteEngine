using System;
using Ignite;
using Ignite.Core.Component;
using static Ignite.Mathf;

namespace PlatformerDemo.FPS
{
    public class FPSPlayerController : Entity
    {
        public float MoveSpeed = 9.2f;
        public float SprintSpeed = 14.2f;
        public float JumpSpeed = 8.8f;
        public float Gravity = 22.0f;
        public float MouseSensitivity = 0.14f;
        public float FireIntervalRifle = 0.105f;
        public float FireIntervalPlasma = 0.24f;
        public float MaxRange = 120.0f;

        public float BaseFov = 90.0f;
        public float SprintFov = 103.5f;
        public float AirBoostFov = 107.0f;

        private const ulong MeshHandleAssaultRifle = 9500000000000000001UL;
        private const ulong MeshHandlePlasmaCarbine = 9500000000000000002UL;

        private CharacterControllerComponent _cc;
        private NetworkIdentityComponent _netId;
        private Entity _cameraEntity;
        private CameraComponent _cameraComp;
        private Entity _weaponRig;
        private Entity _weaponMuzzleGlow;
        private Entity _muzzleFlashEntity;
        private Entity _operativeBody;
        private AnimatorComponent _animator;

        private Entity _sfxRifle;
        private Entity _sfxPlasma;
        private Entity _sfxHitTarget;
        private Entity _sfxHitCrit;
        private Entity _sfxHitMetal;
        private Entity _sfxJumpPad;
        private Entity _sfxReload;
        private Entity _sfxBarrelExplode;

        private Entity _impactSparkEntity;
        private Entity _impactPlasmaEntity;
        private Entity _impactLightEntity;
        private PointLightComponent _impactLight;

        // Pool of 3D fast-moving weapon projectiles
        private const int MaxProjectiles = 6;
        private readonly Entity[] _projectileEntities = new Entity[MaxProjectiles];
        private readonly Vector3[] _projectileVelocities = new Vector3[MaxProjectiles];
        private readonly Vector3[] _projectileTargets = new Vector3[MaxProjectiles];
        private readonly float[] _projectileTimers = new float[MaxProjectiles];
        private readonly Quaternion[] _projectileRotations = new Quaternion[MaxProjectiles];
        private int _nextProjectileIndex;

        // Pool of 3D world-space floating hit reaction text entities
        private const int MaxHitPopups = 8;
        private readonly Entity[] _hitPopupEntities = new Entity[MaxHitPopups];
        private readonly TextComponent[] _hitPopupTexts = new TextComponent[MaxHitPopups];
        private readonly Vector3[] _hitPopupVelocities = new Vector3[MaxHitPopups];
        private readonly Vector4[] _hitPopupBaseColors = new Vector4[MaxHitPopups];
        private readonly float[] _hitPopupTimers = new float[MaxHitPopups];
        private readonly float[] _hitPopupMaxLifetimes = new float[MaxHitPopups];
        private readonly float[] _hitPopupBaseScales = new float[MaxHitPopups];
        private int _nextHitPopupIndex;

        // Interactive Explosive Barrels state
        private const int MaxBarrels = 3;
        private readonly Entity[] _barrelEntities = new Entity[MaxBarrels];
        private readonly Vector3[] _barrelSpawnPositions = new Vector3[MaxBarrels];
        private readonly int[] _barrelDamageHits = new Vector3IntDummy().Values; // replaced below
        private readonly int[] _barrelHitCounts = new int[MaxBarrels];
        private readonly float[] _barrelRespawnTimers = new float[MaxBarrels];

        // Dynamic physics props for blast wave impulse
        private readonly Entity[] _blastProps = new Entity[12];
        private int _blastPropCount;

        private float _yawDeg;
        private float _pitchDeg;
        private float _verticalVelocity;
        private float _planarSpeed;
        private bool _wasGrounded = true;

        private float _fireCooldown;
        private float _recoilKick;
        private float _recoilPitchDeg;
        private float _weaponBobPhase;
        private float _weaponSwitchAnim;
        private float _reloadTimer;
        private float _reloadDuration = 1.15f;
        private float _dashCooldown;
        private float _jumpPadCooldown;
        private float _impactLightTimer;

        // Procedural Camera Shake & Dynamic FOV
        private float _cameraTrauma;
        private float _shakeTime;
        private float _currentFov = 90.0f;
        private float _fovKick;

        private int _weaponSlot = 1;
        private bool _prevTabDown;
        private bool _prevKey1Down;
        private bool _prevKey2Down;
        private bool _prevReloadDown;
        private bool _prevJumpDown;
        private bool _prevDashDown;

        private struct Vector3IntDummy
        {
            public int[] Values => new int[3];
        }

        public override void OnCreate()
        {
            _cc = GetComponent<CharacterControllerComponent>();
            _netId = GetComponent<NetworkIdentityComponent>();

            _cameraEntity = FindChild("PlayerCamera") ?? Entity.FindEntity("PlayerCamera");
            if (_cameraEntity != null)
                _cameraComp = _cameraEntity.GetComponent<CameraComponent>();

            _weaponRig = (_cameraEntity != null ? _cameraEntity.FindChild("WeaponRig") : null)
                         ?? FindChild("WeaponRig")
                         ?? Entity.FindEntity("WeaponRig");
            _weaponMuzzleGlow = (_weaponRig != null ? _weaponRig.FindChild("WeaponMuzzleGlow") : null)
                                ?? Entity.FindEntity("WeaponMuzzleGlow");
            _muzzleFlashEntity = (_weaponRig != null ? _weaponRig.FindChild("MuzzleFlashParticles") : null)
                                 ?? Entity.FindEntity("MuzzleFlashParticles");
            _operativeBody = FindChild("OperativeBody") ?? Entity.FindEntity("OperativeBody");
            if (_operativeBody != null && _operativeBody.HasComponent<AnimatorComponent>())
                _animator = _operativeBody.GetComponent<AnimatorComponent>();

            _sfxRifle = Entity.FindEntity("SFX_RifleShot");
            _sfxPlasma = Entity.FindEntity("SFX_PulseShot");
            _sfxHitTarget = Entity.FindEntity("SFX_HitTarget");
            _sfxHitCrit = Entity.FindEntity("SFX_HitCrit");
            _sfxHitMetal = Entity.FindEntity("SFX_HitMetal");
            _sfxJumpPad = Entity.FindEntity("SFX_JumpPad");
            _sfxReload = Entity.FindEntity("SFX_WeaponReload");
            _sfxBarrelExplode = Entity.FindEntity("SFX_BarrelExplode");

            _impactSparkEntity = Entity.FindEntity("ImpactSpark");
            _impactPlasmaEntity = Entity.FindEntity("ImpactPlasma");
            _impactLightEntity = Entity.FindEntity("ImpactFlashLight");
            if (_impactLightEntity != null)
                _impactLight = _impactLightEntity.GetComponent<PointLightComponent>();

            for (int i = 0; i < MaxProjectiles; i++)
            {
                string name = $"WeaponProjectile_0{i + 1}";
                _projectileEntities[i] = Entity.FindEntity(name);
                if (_projectileEntities[i] != null)
                    _projectileEntities[i].Visible = false;
            }

            for (int i = 0; i < MaxHitPopups; i++)
            {
                string name = $"WorldHitText_0{i + 1}";
                Entity e = Entity.FindEntity(name);
                if (e != null)
                {
                    _hitPopupEntities[i] = e;
                    _hitPopupTexts[i] = e.GetComponent<TextComponent>();
                    e.Visible = false;
                }
            }

            for (int i = 0; i < MaxBarrels; i++)
            {
                string name = $"ExplosiveBarrel_0{i + 1}";
                Entity b = Entity.FindEntity(name);
                if (b != null)
                {
                    _barrelEntities[i] = b;
                    _barrelSpawnPositions[i] = b.Translation;
                }
            }

            string[] propNames = {
                "PhysicsCrate_01", "PhysicsCrate_02", "PhysicsCrate_03",
                "PhysicsCrate_04", "PhysicsCrate_05", "PhysicsCrate_06",
                "PhysicsSphere_01", "PhysicsSphere_02", "PhysicsSphere_03",
                "Target_01", "Target_02", "Target_03"
            };
            _blastPropCount = 0;
            foreach (string pn in propNames)
            {
                if (_blastPropCount >= _blastProps.Length) break;
                Entity pe = Entity.FindEntity(pn);
                if (pe != null)
                    _blastProps[_blastPropCount++] = pe;
            }

            if (IsLocalControlled())
            {
                FPSGameSession.ResetMatch();
                Input.SetCursorMode(CursorMode.Disabled);
                EquipWeaponSlot(1, playSound: false);

                if (_operativeBody != null)
                    _operativeBody.Visible = false;
            }
            else
            {
                if (_operativeBody != null)
                    _operativeBody.Visible = true;
            }
        }

        private bool IsLocalControlled()
        {
            if (_netId == null || (!Net.IsConnected && !Net.IsServerRunning))
                return true;
            return _netId.IsLocalPlayer || _netId.OwnerClient == 0 || _netId.OwnerClient == Net.GetClientId();
        }

        public override void OnUpdate(float dt)
        {
            _shakeTime += dt;
            UpdateActiveProjectiles(dt);
            UpdateWorldHitPopups(dt);
            UpdateExplosiveBarrels(dt);

            if (!IsLocalControlled())
            {
                if (_operativeBody != null && !_operativeBody.Visible)
                    _operativeBody.Visible = true;
                UpdateCharacterAnimator();
                return;
            }

            if (_operativeBody != null && _operativeBody.Visible)
                _operativeBody.Visible = false;

            if (FPSGameSession.HitMarkerTimer > 0f)
                FPSGameSession.HitMarkerTimer = MathF.Max(0f, FPSGameSession.HitMarkerTimer - dt);

            if (_impactLightTimer > 0f)
            {
                _impactLightTimer = MathF.Max(0f, _impactLightTimer - dt);
                if (_impactLight != null)
                    _impactLight.Intensity = _impactLightTimer * 55.0f;
            }

            // Edge-triggered key detection
            bool tabDown = Input.IsKeyPressed(KeyCode.Tab) || Input.IsKeyPressed(KeyCode.M);
            if (tabDown && !_prevTabDown)
            {
                FPSGameSession.IsPauseMenuOpen = !FPSGameSession.IsPauseMenuOpen;
                Input.SetCursorMode(FPSGameSession.IsPauseMenuOpen ? CursorMode.Normal : CursorMode.Disabled);
            }
            _prevTabDown = tabDown;

            if (FPSGameSession.IsPauseMenuOpen)
            {
                UpdateCameraShakeAndFov(dt, false, false);
                ApplyLookTransforms();
                UpdateWeaponViewmodel(dt, false);
                return;
            }

            // 1. Mouse Look
            Vector2 mouseDelta = Input.MouseDelta;
            _yawDeg -= mouseDelta.X * MouseSensitivity;
            _pitchDeg = Clamp(_pitchDeg - mouseDelta.Y * MouseSensitivity, -82.0f, 82.0f);

            // 2. Weapon Switching (1 = VX-47 Assault Rifle, 2 = Hyperion Plasma Carbine)
            bool key1Down = Input.IsKeyPressed(KeyCode.D1);
            bool key2Down = Input.IsKeyPressed(KeyCode.D2);
            if (key1Down && !_prevKey1Down && _weaponSlot != 1)
            {
                EquipWeaponSlot(1, playSound: true);
            }
            else if (key2Down && !_prevKey2Down && _weaponSlot != 2)
            {
                EquipWeaponSlot(2, playSound: true);
            }
            _prevKey1Down = key1Down;
            _prevKey2Down = key2Down;

            if (_weaponSlot != FPSGameSession.CurrentWeaponSlot)
            {
                EquipWeaponSlot(FPSGameSession.CurrentWeaponSlot, playSound: true);
            }

            // 3. Reloading (R or automatic when empty)
            bool reloadDown = Input.IsKeyPressed(KeyCode.R);
            if (FPSGameSession.IsReloading)
            {
                _reloadTimer += dt;
                FPSGameSession.ReloadProgress = Clamp(_reloadTimer / _reloadDuration, 0f, 1f);
                if (_reloadTimer >= _reloadDuration)
                {
                    FPSGameSession.IsReloading = false;
                    FPSGameSession.ReloadProgress = 0f;
                    FPSGameSession.CurrentAmmo = FPSGameSession.MaxAmmo;
                    FPSGameSession.EventFeedText = $"{FPSGameSession.CurrentWeaponName} RELOADED [{FPSGameSession.MaxAmmo}/{FPSGameSession.MaxAmmo}]";
                }
            }
            else if (reloadDown && !_prevReloadDown && FPSGameSession.CurrentAmmo < FPSGameSession.MaxAmmo)
            {
                StartReload();
            }
            _prevReloadDown = reloadDown;

            // 4. Fast Competitive Movement + Exo-Dash + Jump Pad
            bool isSprinting = false;
            bool isGrounded = false;
            UpdateLocomotion(dt, out isSprinting, out isGrounded);

            // 5. Dynamic Sprint FOV & Trauma Camera Shake
            UpdateCameraShakeAndFov(dt, isSprinting, !isGrounded);

            // 6. Apply exact world/local transforms BEFORE firing so camera position & forward are 100% up-to-date
            ApplyLookTransforms();

            // 7. Weapon Firing (LMB)
            _fireCooldown = MathF.Max(0f, _fireCooldown - dt);
            _recoilKick = MathF.Max(0f, _recoilKick - dt * 6.8f);
            _recoilPitchDeg = MathF.Max(0f, _recoilPitchDeg - dt * 15.0f);

            if (!FPSGameSession.IsReloading && Input.IsMouseButtonPressed(MouseCode.Left) && _fireCooldown <= 0f)
            {
                FireWeapon();
            }

            Entity testTrigger = Entity.FindEntity("AgentTestShot");
            if (testTrigger != null)
            {
                Entity target = Entity.FindEntity("Target_02") ?? Entity.FindEntity("ExplosiveBarrel_03");
                if (target != null && _cameraEntity != null)
                {
                    Vector3 toTgt = (target.Translation - _cameraEntity.Translation).normalized;
                    _yawDeg = MathF.Atan2(-toTgt.X, -toTgt.Z) * (180.0f / MathF.PI);
                    _pitchDeg = MathF.Asin(Clamp(toTgt.Y, -1.0f, 1.0f)) * (180.0f / MathF.PI);
                    ApplyLookTransforms();
                }
                FireWeapon();
                testTrigger.Destroy();
            }

            // 8. Update 3D Weapon Viewmodel & Crosshair Bloom
            UpdateWeaponViewmodel(dt, isSprinting);

            float speedBloom = Clamp(_planarSpeed / SprintSpeed, 0f, 1.2f) * 8.0f;
            float airBloom = isGrounded ? 0f : 7.5f;
            float recoilBloom = _recoilKick * 16.0f;
            FPSGameSession.CrosshairBloom = speedBloom + airBloom + recoilBloom;
        }

        public void EquipWeaponSlot(int slot, bool playSound)
        {
            _weaponSlot = (slot == 2) ? 2 : 1;
            _weaponSwitchAnim = 1.0f;
            FPSGameSession.IsReloading = false;
            FPSGameSession.ReloadProgress = 0f;
            FPSGameSession.CurrentWeaponSlot = _weaponSlot;

            if (_weaponSlot == 1)
            {
                FPSGameSession.CurrentWeaponName = "VX-47 ASSAULT RIFLE";
                FPSGameSession.MaxAmmo = 30;
                FPSGameSession.CurrentAmmo = 30;
                _reloadDuration = 1.10f;
            }
            else
            {
                FPSGameSession.CurrentWeaponName = "HYPERION PLASMA CARBINE";
                FPSGameSession.MaxAmmo = 18;
                FPSGameSession.CurrentAmmo = 18;
                _reloadDuration = 1.30f;
            }

            if (_weaponRig != null)
            {
                var sm = _weaponRig.GetComponent<StaticMeshComponent>();
                if (sm != null)
                {
                    sm.MeshHandle = (_weaponSlot == 1) ? MeshHandleAssaultRifle : MeshHandlePlasmaCarbine;
                }
            }

            if (playSound)
            {
                PlayAudioEntity(_sfxReload, 1.18f);
                FPSGameSession.EventFeedText = $"EQUIPPED {FPSGameSession.CurrentWeaponName}";
            }
        }

        private void StartReload()
        {
            if (FPSGameSession.IsReloading || FPSGameSession.CurrentAmmo >= FPSGameSession.MaxAmmo)
                return;

            FPSGameSession.IsReloading = true;
            FPSGameSession.ReloadProgress = 0f;
            _reloadTimer = 0f;
            PlayAudioEntity(_sfxReload, _weaponSlot == 1 ? 1.0f : 0.92f);
            FPSGameSession.EventFeedText = $"RELOADING {FPSGameSession.CurrentWeaponName}...";
        }

        private void UpdateLocomotion(float dt, out bool isSprinting, out bool isGrounded)
        {
            float moveX = 0f;
            float moveZ = 0f;
            if (Input.IsKeyPressed(KeyCode.W) || Input.IsKeyPressed(KeyCode.Up)) moveZ += 1f;
            if (Input.IsKeyPressed(KeyCode.S) || Input.IsKeyPressed(KeyCode.Down)) moveZ -= 1f;
            if (Input.IsKeyPressed(KeyCode.D) || Input.IsKeyPressed(KeyCode.Right)) moveX += 1f;
            if (Input.IsKeyPressed(KeyCode.A) || Input.IsKeyPressed(KeyCode.Left)) moveX -= 1f;

            float yawRad = _yawDeg * (MathF.PI / 180.0f);
            Vector3 forward = new Vector3(-MathF.Sin(yawRad), 0f, -MathF.Cos(yawRad));
            Vector3 right = new Vector3(MathF.Cos(yawRad), 0f, -MathF.Sin(yawRad));

            Vector3 wishDir = forward * moveZ + right * moveX;
            if (wishDir.sqrMagnitude > 0.001f)
                wishDir = wishDir.normalized;

            bool wantsSprint = Input.IsKeyPressed(KeyCode.LeftShift) && wishDir.sqrMagnitude > 0.01f;
            isSprinting = wantsSprint && FPSGameSession.ExoThrusterCharge > 5.0f;

            // Exo-Thruster Dash / Slide burst on LeftControl or C
            _dashCooldown = MathF.Max(0f, _dashCooldown - dt);
            bool dashDown = Input.IsKeyPressed(KeyCode.LeftControl) || Input.IsKeyPressed(KeyCode.C);
            bool dashed = false;
            if (dashDown && !_prevDashDown &&
                _dashCooldown <= 0f && FPSGameSession.ExoThrusterCharge >= 28.0f && wishDir.sqrMagnitude > 0.01f)
            {
                _dashCooldown = 0.85f;
                FPSGameSession.ExoThrusterCharge = MathF.Max(0f, FPSGameSession.ExoThrusterCharge - 28.0f);
                dashed = true;
                AddCameraTrauma(0.28f);
                _fovKick = 5.5f;
                PlayAudioEntity(_sfxJumpPad, 1.35f);
                FPSGameSession.EventFeedText = "EXO-THRUSTER DASH ENGAGED!";
            }
            _prevDashDown = dashDown;

            if (isSprinting)
                FPSGameSession.ExoThrusterCharge = MathF.Max(0f, FPSGameSession.ExoThrusterCharge - dt * 14.0f);
            else
                FPSGameSession.ExoThrusterCharge = MathF.Min(100.0f, FPSGameSession.ExoThrusterCharge + dt * 22.0f);

            float speed = isSprinting ? SprintSpeed : MoveSpeed;
            if (_dashCooldown > 0.55f || dashed)
                speed = SprintSpeed * 1.48f;

            isGrounded = _cc != null && (_cc.IsOnGround || _cc.IsSupported);
            if (isGrounded && !_wasGrounded && _verticalVelocity < -6.0f)
            {
                AddCameraTrauma(Clamp(-_verticalVelocity / 24.0f, 0.12f, 0.38f));
            }
            _wasGrounded = isGrounded;

            if (isGrounded && _verticalVelocity < 0f)
                _verticalVelocity = -1.5f;

            bool jumpDown = Input.IsKeyPressed(KeyCode.Space);
            bool jumpPressed = jumpDown && !_prevJumpDown;
            _prevJumpDown = jumpDown;

            if (isGrounded && jumpPressed)
            {
                _verticalVelocity = JumpSpeed;
                isGrounded = false;
                AddCameraTrauma(0.12f);
            }
            else
            {
                _verticalVelocity -= Gravity * dt;
            }

            // Center Arena Jump Pad (at X=0, Z=0, radius 2.2m)
            _jumpPadCooldown = MathF.Max(0f, _jumpPadCooldown - dt);
            var tr = GetComponent<TransformComponent>();
            if (tr != null && _jumpPadCooldown <= 0f)
            {
                Vector3 pos = tr.Translation;
                float distToPadSq = pos.X * pos.X + pos.Z * pos.Z;
                if (distToPadSq < 4.5f && pos.Y < 2.1f)
                {
                    _verticalVelocity = 16.5f;
                    _jumpPadCooldown = 0.65f;
                    isGrounded = false;
                    AddCameraTrauma(0.36f);
                    _fovKick = 7.0f;
                    PlayAudioEntity(_sfxJumpPad, 1.0f);
                    FPSGameSession.EventFeedText = "JUMP PAD LAUNCH // AERIAL OVERDRIVE!";
                }
            }

            Vector3 velocity = new Vector3(wishDir.X * speed, _verticalVelocity, wishDir.Z * speed);
            _planarSpeed = MathF.Sqrt(velocity.X * velocity.X + velocity.Z * velocity.Z);

            FPSGameSession.CurrentMoveSpeed = _planarSpeed;
            FPSGameSession.IsSprinting = isSprinting;
            FPSGameSession.IsAirborne = !isGrounded;

            if (_cc != null)
            {
                _cc.LinearVelocity = velocity;
                _cc.Move(velocity * dt, dt);
            }
            else if (tr != null)
            {
                tr.Translation += velocity * dt;
            }

            if (Net.IsConnected)
            {
                NetInputButtons buttons = NetInputButtons.None;
                if (jumpPressed) buttons |= NetInputButtons.Jump;
                if (isSprinting) buttons |= NetInputButtons.Sprint;
                if (Input.IsMouseButtonPressed(MouseCode.Left)) buttons |= NetInputButtons.Fire;
                Net.SendInput(new Vector2(moveX, moveZ), buttons, _yawDeg);
            }

            if (_netId != null)
            {
                _netId.ReplicatedMovementSpeed = _planarSpeed;
                _netId.ReplicatedIsGrounded = isGrounded;
            }
        }

        public void AddCameraTrauma(float amount)
        {
            _cameraTrauma = Clamp(_cameraTrauma + amount, 0f, 1.0f);
        }

        private void UpdateCameraShakeAndFov(float dt, bool isSprinting, bool isAirborne)
        {
            _cameraTrauma = MathF.Max(0f, _cameraTrauma - dt * 2.4f);
            _fovKick = MathF.Max(0f, _fovKick - dt * 16.0f);

            float targetFov = BaseFov;
            if (isAirborne && _planarSpeed > 8.5f)
                targetFov = AirBoostFov;
            else if (isSprinting && _planarSpeed > 5.0f)
                targetFov = SprintFov;
            else if (_planarSpeed > 2.0f)
                targetFov = BaseFov + (_planarSpeed / SprintSpeed) * 4.5f;

            targetFov += _fovKick - _recoilKick * 2.2f;
            _currentFov += (targetFov - _currentFov) * MathF.Min(1.0f, dt * 11.0f);
            FPSGameSession.CurrentFov = _currentFov;

            if (_cameraComp != null)
            {
                _cameraComp.Fov = _currentFov;
            }
        }

        private Vector3 GetLookForwardVector()
        {
            float shakeIntensity = _cameraTrauma * _cameraTrauma;
            float shakeYawDeg = MathF.Sin(_shakeTime * 43.0f) * 1.4f * shakeIntensity;
            float shakePitchDeg = MathF.Cos(_shakeTime * 51.0f) * 1.6f * shakeIntensity;

            float yawRad = (_yawDeg + shakeYawDeg) * (MathF.PI / 180.0f);
            float pitchRad = Clamp(_pitchDeg + _recoilPitchDeg + shakePitchDeg, -85.0f, 85.0f) * (MathF.PI / 180.0f);

            float cosPitch = MathF.Cos(pitchRad);
            return new Vector3(
                -MathF.Sin(yawRad) * cosPitch,
                MathF.Sin(pitchRad),
                -MathF.Cos(yawRad) * cosPitch
            ).normalized;
        }

        private void ApplyLookTransforms()
        {
            float shakeIntensity = _cameraTrauma * _cameraTrauma;
            float shakeYawDeg = MathF.Sin(_shakeTime * 43.0f) * 1.4f * shakeIntensity;
            float shakePitchDeg = MathF.Cos(_shakeTime * 51.0f) * 1.6f * shakeIntensity;

            float yawRad = (_yawDeg + shakeYawDeg) * (MathF.PI / 180.0f);
            float pitchRad = Clamp(_pitchDeg + _recoilPitchDeg + shakePitchDeg, -85.0f, 85.0f) * (MathF.PI / 180.0f);

            var rootTr = GetComponent<TransformComponent>();
            if (rootTr != null)
            {
                rootTr.Rotation = Quaternion.FromEuler(0f, yawRad, 0f);
            }

            if (_cameraEntity != null)
            {
                var camTr = _cameraEntity.GetComponent<TransformComponent>();
                if (camTr != null)
                {
                    float bobY = (_planarSpeed > 0.5f && _wasGrounded)
                        ? MathF.Sin(_weaponBobPhase * 2.0f) * 0.028f * Clamp(_planarSpeed / SprintSpeed, 0f, 1.2f)
                        : 0f;
                    float shakeX = MathF.Sin(_shakeTime * 59.0f) * 0.045f * shakeIntensity;
                    float shakeY = MathF.Cos(_shakeTime * 67.0f) * 0.045f * shakeIntensity;

                    camTr.Translation = new Vector3(shakeX, 1.88f + bobY + shakeY, 0.0f);
                    camTr.Rotation = Quaternion.FromEuler(pitchRad, 0f, 0f);
                }
            }
        }

        private void UpdateWeaponViewmodel(float dt, bool isSprinting)
        {
            if (_weaponRig == null)
                return;

            var wTr = _weaponRig.GetComponent<TransformComponent>();
            if (wTr == null)
                return;

            _weaponSwitchAnim = MathF.Max(0f, _weaponSwitchAnim - dt * 4.5f);

            float speedFactor = Clamp(_planarSpeed / SprintSpeed, 0f, 1.35f);
            _weaponBobPhase += dt * (3.5f + speedFactor * 8.5f);

            float bobX = MathF.Cos(_weaponBobPhase) * 0.016f * speedFactor;
            float bobY = MathF.Abs(MathF.Sin(_weaponBobPhase * 2.0f)) * -0.018f * speedFactor;
            float kickZ = _recoilKick * 0.095f;

            float reloadDipY = 0f;
            float reloadPitch = 0f;
            if (FPSGameSession.IsReloading)
            {
                float rCurve = MathF.Sin(FPSGameSession.ReloadProgress * MathF.PI);
                reloadDipY = -0.14f * rCurve;
                reloadPitch = -0.45f * rCurve;
            }
            float switchDipY = -0.18f * _weaponSwitchAnim;

            float sprintTiltY = isSprinting ? 0.14f : 0.0f;
            float sprintPitch = isSprinting ? -0.12f : 0.0f;

            wTr.Translation = new Vector3(
                0.28f + bobX,
                -0.24f + bobY + reloadDipY + switchDipY,
                -0.46f + kickZ
            );

            wTr.Rotation = Quaternion.FromEuler(
                _recoilKick * 0.22f + reloadPitch + sprintPitch,
                sprintTiltY,
                0f
            );
        }

        private void UpdateCharacterAnimator()
        {
            if (_animator == null)
                return;

            float speed = (_netId != null) ? _netId.ReplicatedMovementSpeed : _planarSpeed;
            bool grounded = (_netId != null) ? _netId.ReplicatedIsGrounded : _wasGrounded;
            _animator.SetFloat("Speed", speed);
            _animator.SetBool("IsGrounded", grounded);
        }

        private void FireWeapon()
        {
            if (FPSGameSession.CurrentAmmo <= 0)
            {
                StartReload();
                return;
            }

            FPSGameSession.CurrentAmmo--;
            FPSGameSession.ShotsFired++;

            bool isPlasma = (_weaponSlot == 2);
            _fireCooldown = isPlasma ? FireIntervalPlasma : FireIntervalRifle;
            _recoilKick = isPlasma ? 0.92f : 0.56f;
            _recoilPitchDeg = MathF.Min(_recoilPitchDeg + (isPlasma ? 2.1f : 1.15f), 8.5f);
            AddCameraTrauma(isPlasma ? 0.26f : 0.15f);
            _fovKick = isPlasma ? -1.8f : -1.1f;

            PlayAudioEntity(isPlasma ? _sfxPlasma : _sfxRifle, isPlasma ? 1.0f : (0.97f + (FPSGameSession.ShotsFired % 5) * 0.015f));

            if (_muzzleFlashEntity != null)
            {
                var muzzlePs = _muzzleFlashEntity.GetComponent<ParticleSystemComponent>();
                if (muzzlePs != null)
                {
                    muzzlePs.StartColor = isPlasma
                        ? new Vector4(0.25f, 0.96f, 1.0f, 1.0f)
                        : new Vector4(1.0f, 0.90f, 0.45f, 1.0f);
                    muzzlePs.EmitBurst(isPlasma ? 32u : 22u);
                }
            }

            // Compute exact eye origin (TransformComponent.Translation already returns world translation!)
            Vector3 eyePos = Translation + new Vector3(0f, 1.88f, 0f);
            if (_cameraEntity != null)
            {
                eyePos = _cameraEntity.Translation;
            }

            Vector3 rayDir = GetLookForwardVector();
            float yawRad = _yawDeg * (MathF.PI / 180.0f);
            Vector3 camRight = new Vector3(MathF.Cos(yawRad), 0f, -MathF.Sin(yawRad));
            Vector3 camUp = Vector3.Cross(camRight, rayDir).normalized;

            Vector3 muzzlePos = eyePos + camRight * 0.26f - camUp * 0.18f + rayDir * 0.72f;
            if (_weaponMuzzleGlow != null && (_weaponMuzzleGlow.Translation - eyePos).sqrMagnitude < 4.0f)
            {
                muzzlePos = _weaponMuzzleGlow.Translation;
            }

            // Start raycast 0.65m in front of camera eye so it clears the player's own 0.42m capsule
            Vector3 rayOrigin = eyePos + rayDir * 0.65f;
            bool didHit = Physics.Raycast(new Ray(rayOrigin, rayDir), MaxRange, out RaycastHit hit);

            Vector3 targetPoint = (didHit && hit.IsHit) ? hit.Point : (eyePos + rayDir * 65.0f);

            // Spawn visible 3D high-speed weapon projectile moving from muzzle toward targetPoint!
            SpawnWeaponProjectile(muzzlePos, targetPoint, isPlasma);

            if (didHit && hit.Entity != null)
            {
                SpawnImpactEffects(hit.Point, isPlasma);
                HandleHitEntity(hit.Entity, hit.Point, rayDir, isPlasma);
            }
            else
            {
                FPSGameSession.CurrentStreak = 0;
            }

            if (FPSGameSession.CurrentAmmo <= 0)
            {
                StartReload();
            }
        }

        private void SpawnWeaponProjectile(Vector3 startPos, Vector3 targetPos, bool isPlasma)
        {
            int slot = _nextProjectileIndex;
            _nextProjectileIndex = (_nextProjectileIndex + 1) % MaxProjectiles;

            Entity proj = _projectileEntities[slot];
            if (proj == null)
                return;

            Vector3 toTarget = targetPos - startPos;
            float dist = toTarget.magnitude;
            Vector3 flightDir = (dist > 0.05f) ? (toTarget / dist) : GetLookForwardVector();

            float speed = isPlasma ? 82.0f : 115.0f;
            float flightTime = Clamp(dist / speed, 0.03f, 0.85f);

            float flightYaw = MathF.Atan2(-flightDir.X, -flightDir.Z);
            float flightPitch = MathF.Asin(Clamp(flightDir.Y, -1.0f, 1.0f));
            Quaternion flightRot = Quaternion.FromEuler(flightPitch, flightYaw, 0f);

            var tr = proj.GetComponent<TransformComponent>();
            if (tr != null)
            {
                tr.Translation = startPos;
                tr.Rotation = flightRot;
                tr.Scale = isPlasma
                    ? new Vector3(0.42f, 0.42f, 0.95f)
                    : new Vector3(0.24f, 0.24f, 0.78f);
            }

            var pl = proj.GetComponent<PointLightComponent>();
            if (pl != null)
            {
                pl.Color = isPlasma
                    ? new Vector4(0.20f, 0.95f, 1.0f, 1.0f)
                    : new Vector4(1.0f, 0.82f, 0.30f, 1.0f);
                pl.Intensity = isPlasma ? 8.5f : 5.5f;
            }

            _projectileVelocities[slot] = flightDir * speed;
            _projectileTargets[slot] = targetPos;
            _projectileTimers[slot] = flightTime;
            _projectileRotations[slot] = flightRot;
            proj.Visible = true;
        }

        private void UpdateActiveProjectiles(float dt)
        {
            for (int i = 0; i < MaxProjectiles; i++)
            {
                if (_projectileTimers[i] <= 0f)
                    continue;

                _projectileTimers[i] -= dt;
                Entity proj = _projectileEntities[i];
                if (proj == null)
                    continue;

                if (_projectileTimers[i] <= 0f)
                {
                    proj.Visible = false;
                    continue;
                }

                var tr = proj.GetComponent<TransformComponent>();
                if (tr != null)
                {
                    tr.Translation += _projectileVelocities[i] * dt;
                    tr.Rotation = _projectileRotations[i];
                }
            }
        }

        private void HandleHitEntity(Entity hitEntity, Vector3 hitPoint, Vector3 rayDir, bool isPlasma)
        {
            string hitName = hitEntity.GetName() ?? "";

            // 1. Check if we hit an Interactive Explosive Barrel
            for (int i = 0; i < MaxBarrels; i++)
            {
                if (_barrelEntities[i] != null && _barrelEntities[i].ID == hitEntity.ID)
                {
                    int hitsNeeded = isPlasma ? 1 : 2;
                    _barrelHitCounts[i] += isPlasma ? 2 : 1;

                    if (hitEntity.HasComponent<RigidbodyComponent>())
                    {
                        var barrelRb = hitEntity.GetComponent<RigidbodyComponent>();
                        barrelRb.ApplyLinearImpulse(rayDir * 8.0f + new Vector3(0f, 2.5f, 0f), hitPoint);
                    }

                    if (_barrelHitCounts[i] >= hitsNeeded)
                    {
                        DetonateExplosiveBarrel(i, hitPoint);
                    }
                    else
                    {
                        PlayAudioEntity(_sfxHitMetal, 1.15f);
                        SpawnWorldHitPopup(hitPoint, "BARREL -65 HP [UNSTABLE!]", new Vector4(1.0f, 0.55f, 0.18f, 1.0f), 0.34f);
                        FPSGameSession.EventFeedText = $"HIT {hitName} — 1 MORE SHOT TO DETONATE!";
                    }
                    return;
                }
            }

            // 2. Check if we hit a Reactive Target Drone / Target
            if (hitName.StartsWith("Target", StringComparison.OrdinalIgnoreCase))
            {
                Vector3 center = hitEntity.Translation;
                float distFromCenter = (hitPoint - center).magnitude;
                bool isCrit = distFromCenter < 0.38f || isPlasma;

                int baseDamage = isPlasma ? 85 : 45;
                int damage = isCrit ? (int)(baseDamage * 1.6f) : baseDamage;
                int basePts = isPlasma ? 150 : 100;
                int points = isCrit ? basePts + 50 : basePts;

                FPSGameSession.RecordHit(hitName, damage, points, isCrit);
                PlayAudioEntity(isCrit ? (_sfxHitCrit ?? _sfxHitTarget) : _sfxHitTarget, isCrit ? 1.22f : 1.0f);
                AddCameraTrauma(isCrit ? 0.24f : 0.14f);

                if (hitEntity.HasComponent<RigidbodyComponent>())
                {
                    var rb = hitEntity.GetComponent<RigidbodyComponent>();
                    float impulseMag = isPlasma ? 16.0f : 10.5f;
                    rb.ApplyLinearImpulse(rayDir * impulseMag + new Vector3(0f, 3.8f, 0f), hitPoint);
                }

                string streakSuffix = (FPSGameSession.CurrentStreak >= 2) ? $" [x{FPSGameSession.CurrentStreak}]" : "";
                string popupText = isCrit
                    ? $"CRIT -{damage} HP  +{FPSGameSession.LastHitScore}{streakSuffix}"
                    : $"-{damage} HP  +{FPSGameSession.LastHitScore}{streakSuffix}";
                Vector4 popupColor = isCrit
                    ? new Vector4(1.0f, 0.32f, 0.22f, 1.0f)
                    : new Vector4(1.0f, 0.88f, 0.24f, 1.0f);

                SpawnWorldHitPopup(hitPoint, popupText, popupColor, isCrit ? 0.42f : 0.34f);
                return;
            }

            // 3. Dynamic Physics Prop (Crates, Spheres, Plinths, etc.)
            if (hitEntity.HasComponent<RigidbodyComponent>())
            {
                var propRb = hitEntity.GetComponent<RigidbodyComponent>();
                float impulse = isPlasma ? 15.0f : 9.5f;
                propRb.ApplyLinearImpulse(rayDir * impulse + new Vector3(0f, 2.2f, 0f), hitPoint);
                PlayAudioEntity(_sfxHitMetal, 1.05f);

                int dmg = isPlasma ? 60 : 30;
                FPSGameSession.Score += 25;
                FPSGameSession.HitMarkerTimer = 0.16f;
                FPSGameSession.EventFeedText = $"KINETIC IMPACT: {hitName} (-{dmg} HP)  +25 PTS";
                SpawnWorldHitPopup(hitPoint, $"-{dmg} HP  +25", new Vector4(0.35f, 0.92f, 1.0f, 1.0f), 0.30f);
            }
            else
            {
                PlayAudioEntity(_sfxHitMetal, 0.95f);
            }
        }

        private void DetonateExplosiveBarrel(int barrelIndex, Vector3 blastCenter)
        {
            Entity barrel = _barrelEntities[barrelIndex];
            if (barrel == null)
                return;

            _barrelHitCounts[barrelIndex] = 0;
            _barrelRespawnTimers[barrelIndex] = 6.5f;
            barrel.Visible = false;

            Vector3 hiddenPos = new Vector3(_barrelSpawnPositions[barrelIndex].X, -20.0f, _barrelSpawnPositions[barrelIndex].Z);
            barrel.Translation = hiddenPos;
            if (barrel.HasComponent<RigidbodyComponent>())
            {
                var bRb = barrel.GetComponent<RigidbodyComponent>();
                bRb.Position = hiddenPos;
                bRb.LinearVelocity = Vector3.Zero;
            }

            PlayAudioEntity(_sfxBarrelExplode ?? _sfxJumpPad, 0.85f);
            AddCameraTrauma(0.62f);
            _fovKick = 4.5f;

            SpawnImpactEffects(blastCenter, isPlasma: true, burstMultiplier: 3u);

            for (int i = 0; i < _blastPropCount; i++)
            {
                Entity prop = _blastProps[i];
                if (prop == null || !prop.Visible || !prop.HasComponent<RigidbodyComponent>())
                    continue;

                Vector3 offset = prop.Translation - blastCenter;
                float dist = offset.magnitude;
                if (dist < 11.0f && dist > 0.05f)
                {
                    float falloff = Clamp(1.0f - (dist / 11.0f), 0.15f, 1.0f);
                    Vector3 blastDir = (offset / dist + new Vector3(0f, 0.85f, 0f)).normalized;
                    var pRb = prop.GetComponent<RigidbodyComponent>();
                    pRb.ApplyLinearImpulse(blastDir * (26.0f * falloff), blastCenter);
                }
            }

            FPSGameSession.RecordHit("EXPLOSIVE BARREL", 250, 250, isCrit: true);
            SpawnWorldHitPopup(blastCenter + new Vector3(0f, 0.45f, 0f), "DETONATION! -250 HP  +250", new Vector4(0.25f, 0.98f, 1.0f, 1.0f), 0.48f);
        }

        private void UpdateExplosiveBarrels(float dt)
        {
            for (int i = 0; i < MaxBarrels; i++)
            {
                if (_barrelRespawnTimers[i] <= 0f)
                    continue;

                _barrelRespawnTimers[i] -= dt;
                if (_barrelRespawnTimers[i] <= 0f && _barrelEntities[i] != null)
                {
                    Entity barrel = _barrelEntities[i];
                    _barrelHitCounts[i] = 0;
                    barrel.Translation = _barrelSpawnPositions[i];
                    barrel.Rotation = Quaternion.Identity;
                    if (barrel.HasComponent<RigidbodyComponent>())
                    {
                        var bRb = barrel.GetComponent<RigidbodyComponent>();
                        bRb.Position = _barrelSpawnPositions[i];
                        bRb.Rotation = Quaternion.Identity;
                        bRb.LinearVelocity = Vector3.Zero;
                    }
                    barrel.Visible = true;
                }
            }
        }

        private void SpawnWorldHitPopup(Vector3 worldPos, string text, Vector4 color, float baseScale)
        {
            int idx = _nextHitPopupIndex;
            _nextHitPopupIndex = (_nextHitPopupIndex + 1) % MaxHitPopups;

            Entity popup = _hitPopupEntities[idx];
            TextComponent tc = _hitPopupTexts[idx];
            if (popup == null || tc == null)
                return;

            float jitterX = ((idx % 3) - 1) * 0.18f;
            var tr = popup.GetComponent<TransformComponent>();
            if (tr != null)
            {
                tr.Translation = worldPos + new Vector3(jitterX, 0.45f, 0f);
                tr.Scale = new Vector3(baseScale * 1.35f, baseScale * 1.35f, baseScale * 1.35f);
            }

            tc.TextString = text;
            tc.Color = color;
            tc.Billboard = true;
            tc.CenterAlign = true;
            tc.ScreenSpace = false;

            _hitPopupVelocities[idx] = new Vector3(jitterX * 0.45f, 1.95f, 0f);
            _hitPopupBaseColors[idx] = color;
            _hitPopupMaxLifetimes[idx] = 1.05f;
            _hitPopupTimers[idx] = 1.05f;
            _hitPopupBaseScales[idx] = baseScale;
            popup.Visible = true;
        }

        private void UpdateWorldHitPopups(float dt)
        {
            for (int i = 0; i < MaxHitPopups; i++)
            {
                if (_hitPopupTimers[i] <= 0f)
                    continue;

                _hitPopupTimers[i] -= dt;
                Entity popup = _hitPopupEntities[i];
                TextComponent tc = _hitPopupTexts[i];
                if (popup == null || tc == null)
                    continue;

                if (_hitPopupTimers[i] <= 0f)
                {
                    popup.Visible = false;
                    continue;
                }

                float t = 1.0f - (_hitPopupTimers[i] / _hitPopupMaxLifetimes[i]);
                var tr = popup.GetComponent<TransformComponent>();
                if (tr != null)
                {
                    tr.Translation += _hitPopupVelocities[i] * dt;
                    _hitPopupVelocities[i] = new Vector3(_hitPopupVelocities[i].X * 0.96f, _hitPopupVelocities[i].Y * 0.97f, 0f);

                    float popMult = (t < 0.18f) ? (1.38f - (t / 0.18f) * 0.38f) : (1.0f + t * 0.12f);
                    float s = _hitPopupBaseScales[i] * popMult;
                    tr.Scale = new Vector3(s, s, s);
                }

                float alpha = (t > 0.55f) ? Clamp(1.0f - (t - 0.55f) / 0.45f, 0f, 1f) : 1.0f;
                Vector4 baseCol = _hitPopupBaseColors[i];
                tc.Color = new Vector4(baseCol.X, baseCol.Y, baseCol.Z, alpha);
            }
        }

        private void SpawnImpactEffects(Vector3 hitPoint, bool isPlasma, uint burstMultiplier = 1u)
        {
            if (_impactSparkEntity != null)
            {
                _impactSparkEntity.Translation = hitPoint;
                var ps = _impactSparkEntity.GetComponent<ParticleSystemComponent>();
                if (ps != null)
                {
                    ps.StartColor = isPlasma
                        ? new Vector4(0.25f, 0.96f, 1.0f, 1.0f)
                        : new Vector4(1.0f, 0.85f, 0.35f, 1.0f);
                    ps.EmitBurst(36u * burstMultiplier);
                }
            }

            if (_impactPlasmaEntity != null && (isPlasma || burstMultiplier > 1u))
            {
                _impactPlasmaEntity.Translation = hitPoint;
                var ps = _impactPlasmaEntity.GetComponent<ParticleSystemComponent>();
                if (ps != null)
                {
                    ps.EmitBurst(28u * burstMultiplier);
                }
            }

            if (_impactLightEntity != null && _impactLight != null)
            {
                _impactLightEntity.Translation = hitPoint + new Vector3(0f, 0.25f, 0f);
                _impactLight.Color = isPlasma
                    ? new Vector4(0.22f, 0.94f, 1.0f, 1.0f)
                    : new Vector4(1.0f, 0.78f, 0.32f, 1.0f);
                _impactLight.Intensity = 24.0f * burstMultiplier;
                _impactLightTimer = 0.16f;
            }
        }

        public override void OnDestroy()
        {
            if (IsLocalControlled())
            {
                Input.SetCursorMode(CursorMode.Normal);
            }
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
