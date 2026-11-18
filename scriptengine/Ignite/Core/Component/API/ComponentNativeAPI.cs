using System;
using System.Runtime.InteropServices;

namespace Ignite.Core.Component;

public static class ComponentNativeAPI
{
    [StructLayout(LayoutKind.Sequential)]
    public struct API
    {
        public IntPtr Scene_GetScreenToWorldRay;
        public IntPtr Scene_Raycast;
        public IntPtr Scene_GetPrimaryCamera;
        public IntPtr Entity_HasComponent;
        public IntPtr Entity_AddComponent;
        public IntPtr Entity_FindEntityByName;
        public IntPtr Entity_FindChildEntityByName;
        public IntPtr Entity_IsParent;
        public IntPtr Entity_GetParent;
        public IntPtr Entity_InstantiateWithName;
        public IntPtr Entity_Instantiate;
        public IntPtr Entity_Destroy;
        public IntPtr Entity_SetVisibility;
        public IntPtr Entity_GetVisibility;
        public IntPtr Entity_GetName;
        public IntPtr WidgetComponent_HasButton;
        public IntPtr WidgetComponent_AddButtonEventCallback;
        public IntPtr WidgetComponent_RemoveButtonEventCallback;

        // Label
        public IntPtr WidgetComponent_HasLabel;
        public IntPtr WidgetComponent_GetLabelText;
        public IntPtr WidgetComponent_SetLabelText;
        public IntPtr WidgetComponent_GetLabelColor;
        public IntPtr WidgetComponent_SetLabelColor;
        public IntPtr WidgetComponent_GetLabelFontSize;
        public IntPtr WidgetComponent_SetLabelFontSize;
        // Image
        public IntPtr WidgetComponent_HasImage;
        public IntPtr WidgetComponent_GetImageHandle;
        public IntPtr WidgetComponent_SetImageHandle;
        public IntPtr AudioSourceComponent_HasAudio;
        public IntPtr AudioSourceComponent_Play;
        public IntPtr AudioSourceComponent_Stop;
        public IntPtr AudioSourceComponent_Pause;
        public IntPtr AudioSourceComponent_Resume;
        public IntPtr AudioSourceComponent_GetVolume;
        public IntPtr AudioSourceComponent_SetVolume;
        public IntPtr AudioSourceComponent_GetPitch;
        public IntPtr AudioSourceComponent_SetPitch;
        public IntPtr AudioSourceComponent_GetPan;
        public IntPtr AudioSourceComponent_SetPan;
        public IntPtr AudioSourceComponent_GetPlayOnStart;
        public IntPtr AudioSourceComponent_SetPlayOnStart;
        public IntPtr AudioSourceComponent_GetLoop;
        public IntPtr AudioSourceComponent_SetLoop;
        public IntPtr AudioSourceComponent_AddReverbDSP;
        public IntPtr AudioSourceComponent_AddDistortionDSP;
        public IntPtr AudioSourceComponent_AddChorusDSP;
        public IntPtr AudioSourceComponent_AddCompressorDSP;
        public IntPtr AudioSourceComponent_AddDelayDSP;
        public IntPtr AudioSourceComponent_ClearDSPs;

        public IntPtr TransformComponent_GetForward;
        public IntPtr TransformComponent_SetForward;
        public IntPtr TransformComponent_GetRight;
        public IntPtr TransformComponent_SetRight;
        public IntPtr TransformComponent_GetUp;
        public IntPtr TransformComponent_SetUp;
        public IntPtr TransformComponent_GetTranslation;
        public IntPtr TransformComponent_SetTranslation;
        public IntPtr TransformComponent_GetRotation;
        public IntPtr TransformComponent_SetRotation;
        public IntPtr TransformComponent_GetEulerAngles;
        public IntPtr TransformComponent_SetEulerAngles;
        public IntPtr TransformComponent_GetScale;
        public IntPtr TransformComponent_SetScale;

        public IntPtr Sprite2DComponent_SetColor;
        public IntPtr Sprite2DComponent_GetColor;
        public IntPtr Sprite2DComponent_SetTilingFactor;
        public IntPtr Sprite2DComponent_GetTilingFactor;

        public IntPtr Circle2DComponent_SetColor;
        public IntPtr Circle2DComponent_GetColor;

        public IntPtr Rigidbody2DComponent_GetType;
        public IntPtr Rigidbody2DComponent_SetType;
        public IntPtr Rigidbody2DComponent_GetLinearVelocity;
        public IntPtr Rigidbody2DComponent_SetLinearVelocity;
        public IntPtr Rigidbody2DComponent_GetAngularVelocity;
        public IntPtr Rigidbody2DComponent_SetAngularVelocity;
        public IntPtr Rigidbody2DComponent_GetGravityScale;
        public IntPtr Rigidbody2DComponent_SetGravityScale;
        public IntPtr Rigidbody2DComponent_GetLinearDamping;
        public IntPtr Rigidbody2DComponent_SetLinearDamping;
        public IntPtr Rigidbody2DComponent_GetAngularDamping;
        public IntPtr Rigidbody2DComponent_SetAngularDamping;
        public IntPtr Rigidbody2DComponent_GetIsAwake;
        public IntPtr Rigidbody2DComponent_SetIsAwake;
        public IntPtr Rigidbody2DComponent_GetIsEnabled;
        public IntPtr Rigidbody2DComponent_SetIsEnabled;
        public IntPtr Rigidbody2DComponent_GetIsEnableSleep;
        public IntPtr Rigidbody2DComponent_SetIsEnableSleep;
        public IntPtr Rigidbody2DComponent_ApplyForce;
        public IntPtr Rigidbody2DComponent_ApplyForceToCenter;
        public IntPtr Rigidbody2DComponent_ApplyLinearImpulse;
        public IntPtr Rigidbody2DComponent_ApplyLinearImpulseToCenter;
        public IntPtr Rigidbody2DComponent_ApplyAngularImpulse;
        public IntPtr Rigidbody2DComponent_ApplyTorque;
        public IntPtr Rigidbody2DComponent_GetMass;
        public IntPtr Rigidbody2DComponent_GetIsBullet;
        public IntPtr Rigidbody2DComponent_SetIsBullet;

        public IntPtr BoxCollider2DComponent_GetSize;
        public IntPtr BoxCollider2DComponent_SetSize;
        public IntPtr BoxCollider2DComponent_GetOffset;
        public IntPtr BoxCollider2DComponent_SetOffset;
        public IntPtr BoxCollider2DComponent_GetRestitution;
        public IntPtr BoxCollider2DComponent_SetRestitution;
        public IntPtr BoxCollider2DComponent_GetFriction;
        public IntPtr BoxCollider2DComponent_SetFriction;
        public IntPtr BoxCollider2DComponent_GetDensity;
        public IntPtr BoxCollider2DComponent_SetDensity;
        public IntPtr BoxCollider2DComponent_GetIsSensor;
        public IntPtr BoxCollider2DComponent_SetIsSensor;

        public IntPtr CircleCollider2DComponent_GetCenter;
        public IntPtr CircleCollider2DComponent_SetCenter;
        public IntPtr CircleCollider2DComponent_GetRadius;
        public IntPtr CircleCollider2DComponent_SetRadius;
        public IntPtr CircleCollider2DComponent_GetRestitution;
        public IntPtr CircleCollider2DComponent_SetRestitution;
        public IntPtr CircleCollider2DComponent_GetFriction;
        public IntPtr CircleCollider2DComponent_SetFriction;
        public IntPtr CircleCollider2DComponent_GetDensity;
        public IntPtr CircleCollider2DComponent_SetDensity;
        public IntPtr CircleCollider2DComponent_GetIsSensor;
        public IntPtr CircleCollider2DComponent_SetIsSensor;

        public IntPtr TextComponent_SetText;
        public IntPtr TextComponent_GetText;
        public IntPtr TextComponent_SetColor;
        public IntPtr TextComponent_GetColor;
        public IntPtr TextComponent_SetKerning;
        public IntPtr TextComponent_GetKerning;
        public IntPtr TextComponent_SetLineSpacing;
        public IntPtr TextComponent_GetLineSpacing;

        // RigidbodyComponent (3D)
        public IntPtr RigidbodyComponent_GetType;
        public IntPtr RigidbodyComponent_SetType;
        public IntPtr RigidbodyComponent_GetMotionQuality;
        public IntPtr RigidbodyComponent_GetUseGravity;
        public IntPtr RigidbodyComponent_SetUseGravity;
        public IntPtr RigidbodyComponent_GetMass;
        public IntPtr RigidbodyComponent_GetGravityFactor;
        public IntPtr RigidbodyComponent_SetGravityFactor;
        public IntPtr RigidbodyComponent_GetLinearVelocity;
        public IntPtr RigidbodyComponent_SetLinearVelocity;
        public IntPtr RigidbodyComponent_GetAngularVelocity;
        public IntPtr RigidbodyComponent_GetPosition;
        public IntPtr RigidbodyComponent_SetPosition;
        public IntPtr RigidbodyComponent_GetRotation;
        public IntPtr RigidbodyComponent_SetRotation;
        public IntPtr RigidbodyComponent_GetCenterOfMass;
        public IntPtr RigidbodyComponent_IsActive;
        public IntPtr RigidbodyComponent_ApplyForce;
        public IntPtr RigidbodyComponent_ApplyForceToCenter;
        public IntPtr RigidbodyComponent_ApplyTorque;
        public IntPtr RigidbodyComponent_ApplyLinearImpulse;
        public IntPtr RigidbodyComponent_ApplyLinearImpulseToCenter;
        public IntPtr RigidbodyComponent_ApplyAngularImpulse;
        public IntPtr RigidbodyComponent_Activate;
        public IntPtr RigidbodyComponent_Deactivate;
        public IntPtr RigidbodyComponent_MoveKinematic;
        public IntPtr RigidbodyComponent_SetMass;
        public IntPtr RigidbodyComponent_GetLinearDamping;
        public IntPtr RigidbodyComponent_SetLinearDamping;
        public IntPtr RigidbodyComponent_GetAngularDamping;
        public IntPtr RigidbodyComponent_SetAngularDamping;
        public IntPtr RigidbodyComponent_GetFriction;
        public IntPtr RigidbodyComponent_SetFriction;
        public IntPtr RigidbodyComponent_GetRestitution;
        public IntPtr RigidbodyComponent_SetRestitution;
        public IntPtr RigidbodyComponent_GetMaxLinearVelocity;
        public IntPtr RigidbodyComponent_SetMaxLinearVelocity;
        public IntPtr RigidbodyComponent_GetMaxAngularVelocity;
        public IntPtr RigidbodyComponent_SetMaxAngularVelocity;
        public IntPtr RigidbodyComponent_GetApplyGyroscopicForce;
        public IntPtr RigidbodyComponent_SetApplyGyroscopicForce;
        public IntPtr RigidbodyComponent_SetAngularVelocity;
        public IntPtr RigidbodyComponent_GetLayer;
        public IntPtr RigidbodyComponent_SetLayer;

        // BoxColliderComponent
        public IntPtr BoxColliderComponent_GetCenter;
        public IntPtr BoxColliderComponent_SetCenter;
        public IntPtr BoxColliderComponent_GetScale;
        public IntPtr BoxColliderComponent_SetScale;

        // SphereColliderComponent
        public IntPtr SphereColliderComponent_GetCenter;
        public IntPtr SphereColliderComponent_SetCenter;
        public IntPtr SphereColliderComponent_GetRadius;
        public IntPtr SphereColliderComponent_SetRadius;

        // CapsuleColliderComponent
        public IntPtr CapsuleColliderComponent_GetCenter;
        public IntPtr CapsuleColliderComponent_SetCenter;
        public IntPtr CapsuleColliderComponent_GetRadius;
        public IntPtr CapsuleColliderComponent_SetRadius;
        public IntPtr CapsuleColliderComponent_GetHeight;
        public IntPtr CapsuleColliderComponent_SetHeight;

        // CharacterControllerComponent
        public IntPtr CharacterControllerComponent_GetCenter;
        public IntPtr CharacterControllerComponent_SetCenter;
        public IntPtr CharacterControllerComponent_GetRadius;
        public IntPtr CharacterControllerComponent_SetRadius;
        public IntPtr CharacterControllerComponent_GetHeight;
        public IntPtr CharacterControllerComponent_SetHeight;
        public IntPtr CharacterControllerComponent_GetMaxStepHeight;
        public IntPtr CharacterControllerComponent_SetMaxStepHeight;
        public IntPtr CharacterControllerComponent_GetMaxSlopeAngle;
        public IntPtr CharacterControllerComponent_SetMaxSlopeAngle;
        public IntPtr CharacterControllerComponent_GetMass;
        public IntPtr CharacterControllerComponent_SetMass;
        public IntPtr CharacterControllerComponent_GetFriction;
        public IntPtr CharacterControllerComponent_SetFriction;
        public IntPtr CharacterControllerComponent_GetGravityFactor;
        public IntPtr CharacterControllerComponent_SetGravityFactor;
        public IntPtr CharacterControllerComponent_GetUp;
        public IntPtr CharacterControllerComponent_SetUp;
        public IntPtr CharacterControllerComponent_GetLinearVelocity;
        public IntPtr CharacterControllerComponent_SetLinearVelocity;
        public IntPtr CharacterControllerComponent_IsOnGround;
        public IntPtr CharacterControllerComponent_IsSupported;
        public IntPtr CharacterControllerComponent_GetGroundNormal;
        public IntPtr CharacterControllerComponent_Move;
        public IntPtr CharacterControllerComponent_GetPosition;
        public IntPtr CharacterControllerComponent_SetPosition;
        public IntPtr CharacterControllerComponent_GetRotation;
        public IntPtr CharacterControllerComponent_SetRotation;

        // AnimatorComponent
        public IntPtr AnimatorComponent_SetFloat;
        public IntPtr AnimatorComponent_GetFloat;
        public IntPtr AnimatorComponent_SetInt;
        public IntPtr AnimatorComponent_GetInt;
        public IntPtr AnimatorComponent_SetBool;
        public IntPtr AnimatorComponent_GetBool;
        public IntPtr AnimatorComponent_SetString;
        public IntPtr AnimatorComponent_GetString;
        public IntPtr AnimatorComponent_SetState;
        public IntPtr AnimatorComponent_GetCurrentStateName;

        // AnimationMontage (present in native ComponentScriptGlueAPI before trailing extensions)
        public IntPtr AnimationMontage_GetNotifyCallbackCount;
        public IntPtr AnimationMontage_GetNotifyCallbackAt;
        public IntPtr AnimationMontage_AddNotifyCallback;
        public IntPtr AnimationMontage_RemoveNotifyCallback;

        // Entity removal
        public IntPtr Entity_RemoveComponent;

        // RenderingComponent
        public IntPtr RenderingComponent_GetVisible;
        public IntPtr RenderingComponent_SetVisible;

        // CameraComponent
        public IntPtr CameraComponent_GetPrimary;
        public IntPtr CameraComponent_SetPrimary;
        public IntPtr CameraComponent_GetProjectionType;
        public IntPtr CameraComponent_SetProjectionType;
        public IntPtr CameraComponent_GetFov;
        public IntPtr CameraComponent_SetFov;
        public IntPtr CameraComponent_GetOrthoSize;
        public IntPtr CameraComponent_SetOrthoSize;
        public IntPtr CameraComponent_GetNearClip;
        public IntPtr CameraComponent_SetNearClip;
        public IntPtr CameraComponent_GetFarClip;
        public IntPtr CameraComponent_SetFarClip;
        public IntPtr CameraComponent_GetAspectRatioPreset;
        public IntPtr CameraComponent_SetAspectRatioPreset;

        // DirectionalLightComponent
        public IntPtr DirectionalLightComponent_GetColor;
        public IntPtr DirectionalLightComponent_SetColor;
        public IntPtr DirectionalLightComponent_GetIntensity;
        public IntPtr DirectionalLightComponent_SetIntensity;
        public IntPtr DirectionalLightComponent_GetAngularRadius;
        public IntPtr DirectionalLightComponent_SetAngularRadius;
        public IntPtr DirectionalLightComponent_GetShadowStrength;
        public IntPtr DirectionalLightComponent_SetShadowStrength;
        public IntPtr DirectionalLightComponent_GetShadowDistance;
        public IntPtr DirectionalLightComponent_SetShadowDistance;
        public IntPtr DirectionalLightComponent_GetShadowResolution;
        public IntPtr DirectionalLightComponent_SetShadowResolution;
        public IntPtr DirectionalLightComponent_GetCascadeShadow;
        public IntPtr DirectionalLightComponent_SetCascadeShadow;
        public IntPtr DirectionalLightComponent_GetSoftShadows;
        public IntPtr DirectionalLightComponent_SetSoftShadows;

        // PointLightComponent
        public IntPtr PointLightComponent_GetColor;
        public IntPtr PointLightComponent_SetColor;
        public IntPtr PointLightComponent_GetIntensity;
        public IntPtr PointLightComponent_SetIntensity;
        public IntPtr PointLightComponent_GetRange;
        public IntPtr PointLightComponent_SetRange;
        public IntPtr PointLightComponent_GetEnabled;
        public IntPtr PointLightComponent_SetEnabled;
        public IntPtr PointLightComponent_GetConstantAttenuation;
        public IntPtr PointLightComponent_SetConstantAttenuation;
        public IntPtr PointLightComponent_GetLinearAttenuation;
        public IntPtr PointLightComponent_SetLinearAttenuation;
        public IntPtr PointLightComponent_GetQuadraticAttenuation;
        public IntPtr PointLightComponent_SetQuadraticAttenuation;

        // SpotLightComponent
        public IntPtr SpotLightComponent_GetColor;
        public IntPtr SpotLightComponent_SetColor;
        public IntPtr SpotLightComponent_GetIntensity;
        public IntPtr SpotLightComponent_SetIntensity;
        public IntPtr SpotLightComponent_GetRange;
        public IntPtr SpotLightComponent_SetRange;
        public IntPtr SpotLightComponent_GetEnabled;
        public IntPtr SpotLightComponent_SetEnabled;
        public IntPtr SpotLightComponent_GetInnerConeAngle;
        public IntPtr SpotLightComponent_SetInnerConeAngle;
        public IntPtr SpotLightComponent_GetOuterConeAngle;
        public IntPtr SpotLightComponent_SetOuterConeAngle;
        public IntPtr SpotLightComponent_GetConstantAttenuation;
        public IntPtr SpotLightComponent_SetConstantAttenuation;
        public IntPtr SpotLightComponent_GetLinearAttenuation;
        public IntPtr SpotLightComponent_SetLinearAttenuation;
        public IntPtr SpotLightComponent_GetQuadraticAttenuation;
        public IntPtr SpotLightComponent_SetQuadraticAttenuation;

        // PointLight2DComponent
        public IntPtr PointLight2DComponent_GetColor;
        public IntPtr PointLight2DComponent_SetColor;
        public IntPtr PointLight2DComponent_GetRadius;
        public IntPtr PointLight2DComponent_SetRadius;
        public IntPtr PointLight2DComponent_GetIntensity;
        public IntPtr PointLight2DComponent_SetIntensity;
        public IntPtr PointLight2DComponent_GetEnabled;
        public IntPtr PointLight2DComponent_SetEnabled;

        // StaticMeshComponent
        public IntPtr StaticMeshComponent_GetMeshHandle;
        public IntPtr StaticMeshComponent_SetMeshHandle;
        public IntPtr StaticMeshComponent_SetOverrideMaterial;
        public IntPtr StaticMeshComponent_GetOverrideMaterial;

        // WorldEnvironmentComponent
        public IntPtr WorldEnvironmentComponent_GetExposure;
        public IntPtr WorldEnvironmentComponent_SetExposure;
        public IntPtr WorldEnvironmentComponent_GetGamma;
        public IntPtr WorldEnvironmentComponent_SetGamma;
        public IntPtr WorldEnvironmentComponent_GetAmbient;
        public IntPtr WorldEnvironmentComponent_SetAmbient;
        public IntPtr WorldEnvironmentComponent_GetEnableIBL;
        public IntPtr WorldEnvironmentComponent_SetEnableIBL;
        public IntPtr WorldEnvironmentComponent_GetFogDensity;
        public IntPtr WorldEnvironmentComponent_SetFogDensity;
        public IntPtr WorldEnvironmentComponent_GetFogColor;
        public IntPtr WorldEnvironmentComponent_SetFogColor;
        public IntPtr WorldEnvironmentComponent_GetFogStart;
        public IntPtr WorldEnvironmentComponent_SetFogStart;
        public IntPtr WorldEnvironmentComponent_GetFogEnd;
        public IntPtr WorldEnvironmentComponent_SetFogEnd;
        public IntPtr WorldEnvironmentComponent_MarkDirty;

        // ParticleSystemComponent
        public IntPtr ParticleSystemComponent_GetIsPlaying;
        public IntPtr ParticleSystemComponent_SetIsPlaying;
        public IntPtr ParticleSystemComponent_GetEmissionRate;
        public IntPtr ParticleSystemComponent_SetEmissionRate;
        public IntPtr ParticleSystemComponent_GetMaxParticles;
        public IntPtr ParticleSystemComponent_SetMaxParticles;
        public IntPtr ParticleSystemComponent_GetStartColor;
        public IntPtr ParticleSystemComponent_SetStartColor;
        public IntPtr ParticleSystemComponent_GetEndColor;
        public IntPtr ParticleSystemComponent_SetEndColor;
        public IntPtr ParticleSystemComponent_GetStartSize;
        public IntPtr ParticleSystemComponent_SetStartSize;
        public IntPtr ParticleSystemComponent_GetEndSize;
        public IntPtr ParticleSystemComponent_SetEndSize;
        public IntPtr ParticleSystemComponent_Play;
        public IntPtr ParticleSystemComponent_Stop;
        public IntPtr ParticleSystemComponent_Restart;
        public IntPtr ParticleSystemComponent_SetPreset;

        // NetworkIdentityComponent
        public IntPtr NetworkIdentityComponent_GetNetId;
        public IntPtr NetworkIdentityComponent_GetOwnerClient;
        public IntPtr NetworkIdentityComponent_SetOwnerClient;
        public IntPtr NetworkIdentityComponent_GetIsLocalPlayer;
        public IntPtr NetworkIdentityComponent_SetIsLocalPlayer;
        public IntPtr NetworkIdentityComponent_GetReplicate;
        public IntPtr NetworkIdentityComponent_SetReplicate;
        public IntPtr NetworkIdentityComponent_GetPrefab;
        public IntPtr NetworkIdentityComponent_SetPrefab;
        public IntPtr NetworkIdentityComponent_GetReplicatedMovementSpeed;
        public IntPtr NetworkIdentityComponent_SetReplicatedMovementSpeed;
        public IntPtr NetworkIdentityComponent_GetReplicatedIsGrounded;
        public IntPtr NetworkIdentityComponent_SetReplicatedIsGrounded;
        public IntPtr NetworkIdentityComponent_GetSyncComponentsMask;
        public IntPtr NetworkIdentityComponent_SetSyncComponentsMask;
        public IntPtr NetworkIdentityComponent_MarkDirty;

        // PlaneColliderComponent
        public IntPtr PlaneColliderComponent_GetCenter;
        public IntPtr PlaneColliderComponent_SetCenter;
        public IntPtr PlaneColliderComponent_GetScale;
        public IntPtr PlaneColliderComponent_SetScale;

        // TextComponent extensions
        public IntPtr TextComponent_GetFontSize;
        public IntPtr TextComponent_SetFontSize;
        public IntPtr TextComponent_GetScreenSpace;
        public IntPtr TextComponent_SetScreenSpace;
        public IntPtr TextComponent_GetBillboard;
        public IntPtr TextComponent_SetBillboard;
        public IntPtr TextComponent_GetCenterAlign;
        public IntPtr TextComponent_SetCenterAlign;

        // WidgetComponent extensions
        public IntPtr WidgetComponent_GetCanvasEnabled;
        public IntPtr WidgetComponent_SetCanvasEnabled;
        public IntPtr WidgetComponent_HasItem;
        public IntPtr WidgetComponent_GetItemVisible;
        public IntPtr WidgetComponent_SetItemVisible;
        public IntPtr WidgetComponent_GetItemBackgroundColor;
        public IntPtr WidgetComponent_SetItemBackgroundColor;
        public IntPtr WidgetComponent_GetItemBorderColor;
        public IntPtr WidgetComponent_SetItemBorderColor;
        public IntPtr WidgetComponent_GetItemOpacity;
        public IntPtr WidgetComponent_SetItemOpacity;
        public IntPtr WidgetComponent_GetItemSize;
        public IntPtr WidgetComponent_SetItemSize;
        public IntPtr WidgetComponent_GetItemPosition;
        public IntPtr WidgetComponent_SetItemPosition;
        public IntPtr WidgetComponent_GetButtonText;
        public IntPtr WidgetComponent_SetButtonText;
        public IntPtr WidgetComponent_GetButtonInteractable;
        public IntPtr WidgetComponent_SetButtonInteractable;
        public IntPtr WidgetComponent_IsButtonHovered;
        public IntPtr WidgetComponent_SetButtonColors;
    }

    public struct Funcs
    {
        // ===========================
        // Entity funcs
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void SceneGetScreenToWorldRayFn(float x, float y, out NativeObject.Vector3 outOrigin, out NativeObject.Vector3 outDirection);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public unsafe delegate ulong SceneRaycastFn(ref NativeObject.Vector3 origin, ref NativeObject.Vector3 direction, float maxDistance, uint layerMask, NativeObject.Vector3* outHitPoint, NativeObject.Vector3* outHitNormal);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate ulong SceneGetPrimaryCameraFn();

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate ulong EntityInstantiateWithNameFn(IntPtr name, ref NativeObject.Vector3 value);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate ulong EntityInstantiateFn(ulong entityID, ref NativeObject.Vector3 value);

        [return: MarshalAs(UnmanagedType.I1)]
        public delegate bool EntityHasComponentFn(ulong entityID, IntPtr componentTypeName);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void EntityAddComponentFn(ulong entityID, IntPtr componentTypeName);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate ulong EntityFindEntityByNameFn(IntPtr name);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate ulong EntityFindChildEntityByNameFn(ulong entityID, IntPtr childName);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate bool EntityIsParentFn(ulong entityID, ulong parentEntityID);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate ulong EntityGetParentFn(ulong entityID);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void EntityDestroyFn(ulong entityID);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void EntitySetVisibilityFn(ulong entityID, [MarshalAs(UnmanagedType.I1)] bool value);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void EntityGetVisibilityFn(ulong entityID, [MarshalAs(UnmanagedType.I1)] out bool result);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate IntPtr EntityGetNameFn(ulong entityID);

        // =================================
        // Custom functions
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void Rigidbody2DApplyForceFn(ulong entityID, NativeObject.Vector2 force, NativeObject.Vector2 point, [MarshalAs(UnmanagedType.I1)] bool value);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void Rigidbody2DApplyForceToCenterFn(ulong entityID, NativeObject.Vector2 force, [MarshalAs(UnmanagedType.I1)] bool value);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void Rigidbody2DApplyLinearImpulseFn(ulong entityID, NativeObject.Vector2 impulse, NativeObject.Vector2 point, [MarshalAs(UnmanagedType.I1)] bool value);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void Rigidbody2DApplyLinearImpulseToCenterFn(ulong entityID, NativeObject.Vector2 impulse, [MarshalAs(UnmanagedType.I1)] bool value);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void Rigidbody2DApplyAngularImpulseFn(ulong entityID, float impulse, [MarshalAs(UnmanagedType.I1)] bool value);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void Rigidbody2DApplyTorqueFn(ulong entityID, float torque, [MarshalAs(UnmanagedType.I1)] bool value);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void AudioSourceActionFn(ulong entityID);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public delegate bool AudioSourceAddReverbDspFn(ulong entityID, float decayTime, float earlyDelay, float lateDelay, float highFrequencyReference, float diffusion, float density, float lowShelfGain, float highCut, float dryLevel, float wetLevel);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public delegate bool AudioSourceAddDistortionDspFn(ulong entityID, float distortionLevel);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public delegate bool AudioSourceAddChorusDspFn(ulong entityID, float mix, float rate, float depth);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public delegate bool AudioSourceAddCompressorDspFn(ulong entityID, float threshold, float ratio, float release, float gainMakeup, [MarshalAs(UnmanagedType.I1)] bool useSidechain);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public delegate bool AudioSourceAddDelayDspFn(ulong entityID, float delayMs, float feedback);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public delegate bool AudioSourceHasAudioFn(ulong entityID);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public delegate bool WidgetComponentHasButtonFn(ulong entityID, IntPtr buttonName);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public delegate bool WidgetComponentButtonEventFn(ulong entityID, IntPtr buttonName, int eventType, IntPtr methodName);


        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public delegate bool WidgetComponentHasNamedItemFn(ulong entityID, IntPtr name);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public delegate void WidgetComponentGetStringByNameFn(ulong entityID, IntPtr name, out IntPtr result);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public delegate bool WidgetComponentSetStringByNameF(ulong entityID, IntPtr name);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void WidgetComponentGetVec4ByNameFn(ulong entityID, IntPtr name, out NativeObject.Vector4 result);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void WidgetComponentSetVec4ByNameFn(ulong entityID, IntPtr name, ref NativeObject.Vector4 value);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void WidgetComponentSetStringByNameFn(ulong entityID, IntPtr name, IntPtr value);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void WidgetComponentGetFloatByNameFn(ulong entityID, IntPtr name, out float result);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void WidgetComponentSetFloatByNameFn(ulong entityID, IntPtr name, float value);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void WidgetComponentGetU64ByNameFn(ulong entityID, IntPtr name, out ulong result);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void WidgetComponentSetU64ByNameFn(ulong entityID, IntPtr name, ulong value);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void WidgetComponentGetBoolByNameFn(ulong entityID, IntPtr name, [MarshalAs(UnmanagedType.I1)] out bool result);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void WidgetComponentSetBoolByNameFn(ulong entityID, IntPtr name, [MarshalAs(UnmanagedType.I1)] bool value);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void WidgetComponentGetVec2ByNameFn(ulong entityID, IntPtr name, out NativeObject.Vector2 result);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void WidgetComponentSetVec2ByNameFn(ulong entityID, IntPtr name, ref NativeObject.Vector2 value);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void WidgetComponentSetButtonColorsFn(ulong entityID, IntPtr name, ref NativeObject.Vector4 normal, ref NativeObject.Vector4 hover, ref NativeObject.Vector4 pressed);

        // ==========================================
        // RigidbodyComponent (3D) and Collider Delegates
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void GetVector3VoidFn(ulong entityID, out NativeObject.Vector3 result);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void SetVector3VoidFn(ulong entityID, ref NativeObject.Vector3 value);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void GetQuaternionVoidFn(ulong entityID, out NativeObject.Quaternion result);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void SetQuaternionVoidFn(ulong entityID, ref NativeObject.Quaternion value);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void RigidbodyApplyForceFn(ulong entityID, ref NativeObject.Vector3 force, ref NativeObject.Vector3 point);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void RigidbodyApplyForceToCenterFn(ulong entityID, ref NativeObject.Vector3 force);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void RigidbodyApplyTorqueFn(ulong entityID, ref NativeObject.Vector3 torque);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void RigidbodyApplyLinearImpulseFn(ulong entityID, ref NativeObject.Vector3 impulse, ref NativeObject.Vector3 point);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void RigidbodyApplyLinearImpulseToCenterFn(ulong entityID, ref NativeObject.Vector3 impulse);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void RigidbodyApplyAngularImpulseFn(ulong entityID, ref NativeObject.Vector3 impulse);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void RigidbodyActionFn(ulong entityID);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void RigidbodyMoveKinematicFn(ulong entityID, ref NativeObject.Vector3 targetPosition, ref NativeObject.Vector3 targetRotation, float deltaTime);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void CharacterControllerMoveFn(ulong entityID, ref NativeObject.Vector3 displacement, float deltaTime);

        // AnimatorComponent delegates
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void AnimatorSetFloatFn(ulong entityID, IntPtr paramName, float value);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void AnimatorGetFloatFn(ulong entityID, IntPtr paramName, out float result);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void AnimatorSetIntFn(ulong entityID, IntPtr paramName, int value);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void AnimatorGetIntFn(ulong entityID, IntPtr paramName, out int result);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void AnimatorSetBoolFn(ulong entityID, IntPtr paramName, bool value);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void AnimatorGetBoolFn(ulong entityID, IntPtr paramName, out bool result);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void AnimatorSetStringFn(ulong entityID, IntPtr paramName, IntPtr value);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void AnimatorGetStringFn(ulong entityID, IntPtr paramName, out IntPtr result);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void AnimatorSetStateFn(ulong entityID, IntPtr stateName);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void AnimatorGetCurrentStateNameFn(ulong entityID, out IntPtr result);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void GetUInt64Fn(ulong entityID, out ulong result);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void SetUInt64Fn(ulong entityID, ulong value);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void StaticMeshSetOverrideMaterialFn(ulong entityID, int slot, ulong materialHandle);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void StaticMeshGetOverrideMaterialFn(ulong entityID, int slot, out ulong result);
    }
}
