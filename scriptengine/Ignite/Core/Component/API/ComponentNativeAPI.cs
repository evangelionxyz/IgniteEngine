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
        public IntPtr Scene_PhysicsRaycast;
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
    }

    public struct Funcs
    {
        // ===========================
        // Entity funcs
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void SceneGetScreenToWorldRayFn(float x, float y, out NativeObject.Vector3 outOrigin, out NativeObject.Vector3 outDirection);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate ulong SceneRaycastFn(ref NativeObject.Vector3 origin, ref NativeObject.Vector3 direction);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public unsafe delegate ulong ScenePhysicsRaycastFn(ref NativeObject.Vector3 origin, ref NativeObject.Vector3 direction, float maxDistance, NativeObject.Vector3* outHitPoint, NativeObject.Vector3* outHitNormal);

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
    }
}
