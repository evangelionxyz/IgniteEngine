// Copyright (c) 2026 Evangelion Manuhutu | IGNITE STUDIO

using System;
using System.Runtime.InteropServices;

namespace Ignite;

public static class InternalCalls
{
    [StructLayout(LayoutKind.Sequential)]
    private struct NativeVector2
    {
        public float X;
        public float Y;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct NativeVector3
    {
        public float X;
        public float Y;
        public float Z;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct NativeVector4
    {
        public float X;
        public float Y;
        public float Z;
        public float W;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct NativeQuaternion
    {
        public float X;
        public float Y;
        public float Z;
        public float W;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct NativeApi
    {
        public IntPtr Debug_Log;
        public IntPtr Entity_HasComponent;
        public IntPtr Entity_AddComponent;
        public IntPtr Entity_FindEntityByName;
        public IntPtr Entity_Instantiate;
        public IntPtr Entity_Destroy;
        public IntPtr Entity_SetVisibility;
        public IntPtr Entity_GetVisibility;

        public IntPtr Input_IsKeyPressed;
        public IntPtr Input_IsModifierPressed;
        public IntPtr Input_IsMouseButtonPressed;
        public IntPtr Input_GetMousePosition;
        public IntPtr Input_SetMouseToCenter;
        public IntPtr Input_SetCursorMode;

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
    }

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void DebugLogFn(IntPtr message);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    private delegate bool EntityHasComponentFn(ulong entityID, IntPtr componentTypeName);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void EntityAddComponentFn(ulong entityID, IntPtr componentTypeName);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate ulong EntityFindEntityByNameFn(IntPtr name);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate ulong EntityInstantiateFn(ulong entityID, NativeVector3 value);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void EntityDestroyFn(ulong entityID);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void EntitySetVisibilityFn(ulong entityID, [MarshalAs(UnmanagedType.I1)] bool value);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void EntityGetVisibilityFn(ulong entityID, [MarshalAs(UnmanagedType.I1)] out bool result);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    private delegate bool InputIsKeyPressedFn(uint keyCode);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    private delegate bool InputIsModifierPressedFn(ushort modCode);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    private delegate bool InputIsMouseButtonPressedFn(byte button);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void InputGetMousePositionFn(out NativeVector2 result);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void InputSetMouseToCenterFn();
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void InputSetCursorModeFn(int mode);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void TransformGetVec3Fn(ulong entityID, out NativeVector3 result);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void TransformSetVec3Fn(ulong entityID, NativeVector3 value);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void TransformGetQuatFn(ulong entityID, out NativeQuaternion result);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void TransformSetQuatFn(ulong entityID, NativeQuaternion value);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void Sprite2DSetColorFn(ulong entityID, NativeVector4 value);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void Sprite2DGetColorFn(ulong entityID, out NativeVector4 value);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void Sprite2DSetTilingFactorFn(ulong entityID, NativeVector2 value);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void Sprite2DGetTilingFactorFn(ulong entityID, out NativeVector2 value);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void Circle2DSetColorFn(ulong entityID, NativeVector4 value);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void Circle2DGetColorFn(ulong entityID, out NativeVector4 value);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void Rigidbody2DGetTypeFn(ulong entityID, out int result);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void Rigidbody2DSetTypeFn(ulong entityID, int value);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void Rigidbody2DGetVec2Fn(ulong entityID, out NativeVector2 result);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void Rigidbody2DSetVec2Fn(ulong entityID, NativeVector2 value);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void Rigidbody2DGetFloatFn(ulong entityID, out float result);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void Rigidbody2DSetFloatFn(ulong entityID, float value);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void Rigidbody2DGetBoolFn(ulong entityID, [MarshalAs(UnmanagedType.I1)] out bool result);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void Rigidbody2DSetBoolFn(ulong entityID, [MarshalAs(UnmanagedType.I1)] bool value);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void Rigidbody2DApplyForceFn(ulong entityID, NativeVector2 force, NativeVector2 point, [MarshalAs(UnmanagedType.I1)] bool value);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void Rigidbody2DApplyForceToCenterFn(ulong entityID, NativeVector2 force, [MarshalAs(UnmanagedType.I1)] bool value);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void Rigidbody2DApplyLinearImpulseFn(ulong entityID, NativeVector2 impulse, NativeVector2 point, [MarshalAs(UnmanagedType.I1)] bool value);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void Rigidbody2DApplyLinearImpulseToCenterFn(ulong entityID, NativeVector2 impulse, [MarshalAs(UnmanagedType.I1)] bool value);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void Rigidbody2DApplyAngularImpulseFn(ulong entityID, float impulse, [MarshalAs(UnmanagedType.I1)] bool value);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void Rigidbody2DApplyTorqueFn(ulong entityID, float torque, [MarshalAs(UnmanagedType.I1)] bool value);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void Rigidbody2DGetMassFn(ulong entityID, out float result);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void Rigidbody2DGetIsBulletFn(ulong entityID, [MarshalAs(UnmanagedType.I1)] out bool result);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void Rigidbody2DSetIsBulletFn(ulong entityID, [MarshalAs(UnmanagedType.I1)] bool value);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void BoxCollider2DGetVec2Fn(ulong entityID, out NativeVector2 result);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void BoxCollider2DSetVec2Fn(ulong entityID, NativeVector2 value);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void BoxCollider2DGetFloatFn(ulong entityID, out float result);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void BoxCollider2DSetFloatFn(ulong entityID, float value);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void BoxCollider2DGetBoolFn(ulong entityID, [MarshalAs(UnmanagedType.I1)] out bool result);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void BoxCollider2DSetBoolFn(ulong entityID, [MarshalAs(UnmanagedType.I1)] bool value);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void CircleCollider2DGetVec2Fn(ulong entityID, out NativeVector2 result);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void CircleCollider2DSetVec2Fn(ulong entityID, NativeVector2 value);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void CircleCollider2DGetFloatFn(ulong entityID, out float result);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void CircleCollider2DSetFloatFn(ulong entityID, float value);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void CircleCollider2DGetBoolFn(ulong entityID, [MarshalAs(UnmanagedType.I1)] out bool result);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void CircleCollider2DSetBoolFn(ulong entityID, [MarshalAs(UnmanagedType.I1)] bool value);

    private static bool s_Initialized;
    private static DebugLogFn s_DebugLog;
    private static EntityHasComponentFn s_EntityHasComponent;
    private static EntityAddComponentFn s_EntityAddComponent;
    private static EntityFindEntityByNameFn s_EntityFindEntityByName;
    private static EntityInstantiateFn s_EntityInstantiate;
    private static EntityDestroyFn s_EntityDestroy;
    private static EntitySetVisibilityFn s_EntitySetVisibility;
    private static EntityGetVisibilityFn s_EntityGetVisibility;
    private static InputIsKeyPressedFn s_InputIsKeyPressed;
    private static InputIsModifierPressedFn s_InputIsModifierPressed;
    private static InputIsMouseButtonPressedFn s_InputIsMouseButtonPressed;
    private static InputGetMousePositionFn s_InputGetMousePosition;
    private static InputSetMouseToCenterFn s_InputSetMouseToCenter;
    private static InputSetCursorModeFn s_InputSetCursorMode;
    private static TransformGetVec3Fn s_TransformGetForward;
    private static TransformSetVec3Fn s_TransformSetForward;
    private static TransformGetVec3Fn s_TransformGetRight;
    private static TransformSetVec3Fn s_TransformSetRight;
    private static TransformGetVec3Fn s_TransformGetUp;
    private static TransformSetVec3Fn s_TransformSetUp;
    private static TransformGetVec3Fn s_TransformGetTranslation;
    private static TransformSetVec3Fn s_TransformSetTranslation;
    private static TransformGetQuatFn s_TransformGetRotation;
    private static TransformSetQuatFn s_TransformSetRotation;
    private static TransformGetVec3Fn s_TransformGetEulerAngles;
    private static TransformSetVec3Fn s_TransformSetEulerAngles;
    private static TransformGetVec3Fn s_TransformGetScale;
    private static TransformSetVec3Fn s_TransformSetScale;

    private static Sprite2DSetColorFn s_Sprite2DSetColor;
    private static Sprite2DGetColorFn s_Sprite2DGetColor;
    private static Sprite2DSetTilingFactorFn s_Sprite2DSetTilingFactor;
    private static Sprite2DGetTilingFactorFn s_Sprite2DGetTilingFactor;

    private static Circle2DSetColorFn s_Circle2DSetColor;
    private static Circle2DGetColorFn s_Circle2DGetColor;

    private static Rigidbody2DGetTypeFn s_Rigidbody2DGetType;
    private static Rigidbody2DSetTypeFn s_Rigidbody2DSetType;
    private static Rigidbody2DGetVec2Fn s_Rigidbody2DGetLinearVelocity;
    private static Rigidbody2DSetVec2Fn s_Rigidbody2DSetLinearVelocity;
    private static Rigidbody2DGetFloatFn s_Rigidbody2DGetAngularVelocity;
    private static Rigidbody2DSetFloatFn s_Rigidbody2DSetAngularVelocity;
    private static Rigidbody2DGetFloatFn s_Rigidbody2DGetGravityScale;
    private static Rigidbody2DSetFloatFn s_Rigidbody2DSetGravityScale;
    private static Rigidbody2DGetFloatFn s_Rigidbody2DGetLinearDamping;
    private static Rigidbody2DSetFloatFn s_Rigidbody2DSetLinearDamping;
    private static Rigidbody2DGetFloatFn s_Rigidbody2DGetAngularDamping;
    private static Rigidbody2DSetFloatFn s_Rigidbody2DSetAngularDamping;
    private static Rigidbody2DGetBoolFn s_Rigidbody2DGetIsAwake;
    private static Rigidbody2DSetBoolFn s_Rigidbody2DSetIsAwake;
    private static Rigidbody2DGetBoolFn s_Rigidbody2DGetIsEnabled;
    private static Rigidbody2DSetBoolFn s_Rigidbody2DSetIsEnabled;
    private static Rigidbody2DGetBoolFn s_Rigidbody2DGetIsEnableSleep;
    private static Rigidbody2DSetBoolFn s_Rigidbody2DSetIsEnableSleep;
    private static Rigidbody2DApplyForceFn s_Rigidbody2DApplyForce;
    private static Rigidbody2DApplyForceToCenterFn s_Rigidbody2DApplyForceToCenter;
    private static Rigidbody2DApplyLinearImpulseFn s_Rigidbody2DApplyLinearImpulse;
    private static Rigidbody2DApplyLinearImpulseToCenterFn s_Rigidbody2DApplyLinearImpulseToCenter;
    private static Rigidbody2DApplyAngularImpulseFn s_Rigidbody2DApplyAngularImpulse;
    private static Rigidbody2DApplyTorqueFn s_Rigidbody2DApplyTorque;
    private static Rigidbody2DGetMassFn s_Rigidbody2DGetMass;
    private static Rigidbody2DGetIsBulletFn s_Rigidbody2DGetIsBullet;
    private static Rigidbody2DSetIsBulletFn s_Rigidbody2DSetIsBullet;
    private static BoxCollider2DGetVec2Fn s_BoxCollider2DGetSize;
    private static BoxCollider2DSetVec2Fn s_BoxCollider2DSetSize;
    private static BoxCollider2DGetVec2Fn s_BoxCollider2DGetOffset;
    private static BoxCollider2DSetVec2Fn s_BoxCollider2DSetOffset;
    private static BoxCollider2DGetFloatFn s_BoxCollider2DGetRestitution;
    private static BoxCollider2DSetFloatFn s_BoxCollider2DSetRestitution;
    private static BoxCollider2DGetFloatFn s_BoxCollider2DGetFriction;
    private static BoxCollider2DSetFloatFn s_BoxCollider2DSetFriction;
    private static BoxCollider2DGetFloatFn s_BoxCollider2DGetDensity;
    private static BoxCollider2DSetFloatFn s_BoxCollider2DSetDensity;
    private static BoxCollider2DGetBoolFn s_BoxCollider2DGetIsSensor;
    private static BoxCollider2DSetBoolFn s_BoxCollider2DSetIsSensor;

    private static CircleCollider2DGetVec2Fn s_CircleCollider2DGetCenter;
    private static CircleCollider2DSetVec2Fn s_CircleCollider2DSetCenter;
    private static CircleCollider2DGetFloatFn s_CircleCollider2DGetRadius;
    private static CircleCollider2DSetFloatFn s_CircleCollider2DSetRadius;
    private static CircleCollider2DGetFloatFn s_CircleCollider2DGetRestitution;
    private static CircleCollider2DSetFloatFn s_CircleCollider2DSetRestitution;
    private static CircleCollider2DGetFloatFn s_CircleCollider2DGetFriction;
    private static CircleCollider2DSetFloatFn s_CircleCollider2DSetFriction;
    private static CircleCollider2DGetFloatFn s_CircleCollider2DGetDensity;
    private static CircleCollider2DSetFloatFn s_CircleCollider2DSetDensity;
    private static CircleCollider2DGetBoolFn s_CircleCollider2DGetIsSensor;
    private static CircleCollider2DSetBoolFn s_CircleCollider2DSetIsSensor;

    public static void Initialize(ulong apiPtr)
    {
        if (apiPtr == 0)
            throw new ArgumentException("Invalid internal calls API pointer", nameof(apiPtr));

        NativeApi api = Marshal.PtrToStructure<NativeApi>((IntPtr)apiPtr);

        s_DebugLog = Marshal.GetDelegateForFunctionPointer<DebugLogFn>(api.Debug_Log);
        s_EntityHasComponent = Marshal.GetDelegateForFunctionPointer<EntityHasComponentFn>(api.Entity_HasComponent);
        s_EntityAddComponent = Marshal.GetDelegateForFunctionPointer<EntityAddComponentFn>(api.Entity_AddComponent);
        s_EntityFindEntityByName = Marshal.GetDelegateForFunctionPointer<EntityFindEntityByNameFn>(api.Entity_FindEntityByName);
        s_EntityInstantiate = Marshal.GetDelegateForFunctionPointer<EntityInstantiateFn>(api.Entity_Instantiate);
        s_EntityDestroy = Marshal.GetDelegateForFunctionPointer<EntityDestroyFn>(api.Entity_Destroy);
        s_EntitySetVisibility = Marshal.GetDelegateForFunctionPointer<EntitySetVisibilityFn>(api.Entity_SetVisibility);
        s_EntityGetVisibility = Marshal.GetDelegateForFunctionPointer<EntityGetVisibilityFn>(api.Entity_GetVisibility);
        s_InputIsKeyPressed = Marshal.GetDelegateForFunctionPointer<InputIsKeyPressedFn>(api.Input_IsKeyPressed);
        s_InputIsModifierPressed = Marshal.GetDelegateForFunctionPointer<InputIsModifierPressedFn>(api.Input_IsModifierPressed);
        s_InputIsMouseButtonPressed = Marshal.GetDelegateForFunctionPointer<InputIsMouseButtonPressedFn>(api.Input_IsMouseButtonPressed);
        s_InputGetMousePosition = Marshal.GetDelegateForFunctionPointer<InputGetMousePositionFn>(api.Input_GetMousePosition);
        s_InputSetMouseToCenter = Marshal.GetDelegateForFunctionPointer<InputSetMouseToCenterFn>(api.Input_SetMouseToCenter);
        s_InputSetCursorMode = Marshal.GetDelegateForFunctionPointer<InputSetCursorModeFn>(api.Input_SetCursorMode);
        
        s_TransformGetForward = Marshal.GetDelegateForFunctionPointer<TransformGetVec3Fn>(api.TransformComponent_GetForward);
        s_TransformSetForward = Marshal.GetDelegateForFunctionPointer<TransformSetVec3Fn>(api.TransformComponent_SetForward);
        s_TransformGetRight = Marshal.GetDelegateForFunctionPointer<TransformGetVec3Fn>(api.TransformComponent_GetRight);
        s_TransformSetRight = Marshal.GetDelegateForFunctionPointer<TransformSetVec3Fn>(api.TransformComponent_SetRight);
        s_TransformGetUp = Marshal.GetDelegateForFunctionPointer<TransformGetVec3Fn>(api.TransformComponent_GetUp);
        s_TransformSetUp = Marshal.GetDelegateForFunctionPointer<TransformSetVec3Fn>(api.TransformComponent_SetUp);
        s_TransformGetTranslation = Marshal.GetDelegateForFunctionPointer<TransformGetVec3Fn>(api.TransformComponent_GetTranslation);
        s_TransformSetTranslation = Marshal.GetDelegateForFunctionPointer<TransformSetVec3Fn>(api.TransformComponent_SetTranslation);
        s_TransformGetRotation = Marshal.GetDelegateForFunctionPointer<TransformGetQuatFn>(api.TransformComponent_GetRotation);
        s_TransformSetRotation = Marshal.GetDelegateForFunctionPointer<TransformSetQuatFn>(api.TransformComponent_SetRotation);
        s_TransformGetEulerAngles = Marshal.GetDelegateForFunctionPointer<TransformGetVec3Fn>(api.TransformComponent_GetEulerAngles);
        s_TransformSetEulerAngles = Marshal.GetDelegateForFunctionPointer<TransformSetVec3Fn>(api.TransformComponent_SetEulerAngles);
        s_TransformGetScale = Marshal.GetDelegateForFunctionPointer<TransformGetVec3Fn>(api.TransformComponent_GetScale);
        s_TransformSetScale = Marshal.GetDelegateForFunctionPointer<TransformSetVec3Fn>(api.TransformComponent_SetScale);

        s_Sprite2DSetColor = Marshal.GetDelegateForFunctionPointer<Sprite2DSetColorFn>(api.Sprite2DComponent_SetColor);
        s_Sprite2DGetColor = Marshal.GetDelegateForFunctionPointer<Sprite2DGetColorFn>(api.Sprite2DComponent_GetColor);
        s_Sprite2DSetTilingFactor = Marshal.GetDelegateForFunctionPointer<Sprite2DSetTilingFactorFn>(api.Sprite2DComponent_SetTilingFactor);
        s_Sprite2DGetTilingFactor = Marshal.GetDelegateForFunctionPointer<Sprite2DGetTilingFactorFn>(api.Sprite2DComponent_GetTilingFactor);

        s_Circle2DSetColor = Marshal.GetDelegateForFunctionPointer<Circle2DSetColorFn>(api.Circle2DComponent_SetColor);
        s_Circle2DGetColor = Marshal.GetDelegateForFunctionPointer<Circle2DGetColorFn>(api.Circle2DComponent_GetColor);

        s_Rigidbody2DGetType = Marshal.GetDelegateForFunctionPointer<Rigidbody2DGetTypeFn>(api.Rigidbody2DComponent_GetType);
        s_Rigidbody2DSetType = Marshal.GetDelegateForFunctionPointer<Rigidbody2DSetTypeFn>(api.Rigidbody2DComponent_SetType);
        s_Rigidbody2DGetLinearVelocity = Marshal.GetDelegateForFunctionPointer<Rigidbody2DGetVec2Fn>(api.Rigidbody2DComponent_GetLinearVelocity);
        s_Rigidbody2DSetLinearVelocity = Marshal.GetDelegateForFunctionPointer<Rigidbody2DSetVec2Fn>(api.Rigidbody2DComponent_SetLinearVelocity);
        s_Rigidbody2DGetAngularVelocity = Marshal.GetDelegateForFunctionPointer<Rigidbody2DGetFloatFn>(api.Rigidbody2DComponent_GetAngularVelocity);
        s_Rigidbody2DSetAngularVelocity = Marshal.GetDelegateForFunctionPointer<Rigidbody2DSetFloatFn>(api.Rigidbody2DComponent_SetAngularVelocity);
        s_Rigidbody2DGetGravityScale = Marshal.GetDelegateForFunctionPointer<Rigidbody2DGetFloatFn>(api.Rigidbody2DComponent_GetGravityScale);
        s_Rigidbody2DSetGravityScale = Marshal.GetDelegateForFunctionPointer<Rigidbody2DSetFloatFn>(api.Rigidbody2DComponent_SetGravityScale);
        s_Rigidbody2DGetLinearDamping = Marshal.GetDelegateForFunctionPointer<Rigidbody2DGetFloatFn>(api.Rigidbody2DComponent_GetLinearDamping);
        s_Rigidbody2DSetLinearDamping = Marshal.GetDelegateForFunctionPointer<Rigidbody2DSetFloatFn>(api.Rigidbody2DComponent_SetLinearDamping);
        s_Rigidbody2DGetAngularDamping = Marshal.GetDelegateForFunctionPointer<Rigidbody2DGetFloatFn>(api.Rigidbody2DComponent_GetAngularDamping);
        s_Rigidbody2DSetAngularDamping = Marshal.GetDelegateForFunctionPointer<Rigidbody2DSetFloatFn>(api.Rigidbody2DComponent_SetAngularDamping);
        s_Rigidbody2DGetIsAwake = Marshal.GetDelegateForFunctionPointer<Rigidbody2DGetBoolFn>(api.Rigidbody2DComponent_GetIsAwake);
        s_Rigidbody2DSetIsAwake = Marshal.GetDelegateForFunctionPointer<Rigidbody2DSetBoolFn>(api.Rigidbody2DComponent_SetIsAwake);
        s_Rigidbody2DGetIsEnabled = Marshal.GetDelegateForFunctionPointer<Rigidbody2DGetBoolFn>(api.Rigidbody2DComponent_GetIsEnabled);
        s_Rigidbody2DSetIsEnabled = Marshal.GetDelegateForFunctionPointer<Rigidbody2DSetBoolFn>(api.Rigidbody2DComponent_SetIsEnabled);
        s_Rigidbody2DGetIsEnableSleep = Marshal.GetDelegateForFunctionPointer<Rigidbody2DGetBoolFn>(api.Rigidbody2DComponent_GetIsEnableSleep);
        s_Rigidbody2DSetIsEnableSleep = Marshal.GetDelegateForFunctionPointer<Rigidbody2DSetBoolFn>(api.Rigidbody2DComponent_SetIsEnableSleep);
        s_Rigidbody2DApplyForce = Marshal.GetDelegateForFunctionPointer<Rigidbody2DApplyForceFn>(api.Rigidbody2DComponent_ApplyForce);
        s_Rigidbody2DApplyForceToCenter = Marshal.GetDelegateForFunctionPointer<Rigidbody2DApplyForceToCenterFn>(api.Rigidbody2DComponent_ApplyForceToCenter);
        s_Rigidbody2DApplyLinearImpulse = Marshal.GetDelegateForFunctionPointer<Rigidbody2DApplyLinearImpulseFn>(api.Rigidbody2DComponent_ApplyLinearImpulse);
        s_Rigidbody2DApplyLinearImpulseToCenter = Marshal.GetDelegateForFunctionPointer<Rigidbody2DApplyLinearImpulseToCenterFn>(api.Rigidbody2DComponent_ApplyLinearImpulseToCenter);
        s_Rigidbody2DApplyAngularImpulse = Marshal.GetDelegateForFunctionPointer<Rigidbody2DApplyAngularImpulseFn>(api.Rigidbody2DComponent_ApplyAngularImpulse);
        s_Rigidbody2DApplyTorque = Marshal.GetDelegateForFunctionPointer<Rigidbody2DApplyTorqueFn>(api.Rigidbody2DComponent_ApplyTorque);
        s_Rigidbody2DGetMass = Marshal.GetDelegateForFunctionPointer<Rigidbody2DGetMassFn>(api.Rigidbody2DComponent_GetMass);
        s_Rigidbody2DGetIsBullet = Marshal.GetDelegateForFunctionPointer<Rigidbody2DGetIsBulletFn>(api.Rigidbody2DComponent_GetIsBullet);
        s_Rigidbody2DSetIsBullet = Marshal.GetDelegateForFunctionPointer<Rigidbody2DSetIsBulletFn>(api.Rigidbody2DComponent_SetIsBullet);
        
        s_BoxCollider2DGetSize = Marshal.GetDelegateForFunctionPointer<BoxCollider2DGetVec2Fn>(api.BoxCollider2DComponent_GetSize);
        s_BoxCollider2DSetSize = Marshal.GetDelegateForFunctionPointer<BoxCollider2DSetVec2Fn>(api.BoxCollider2DComponent_SetSize);
        s_BoxCollider2DGetOffset = Marshal.GetDelegateForFunctionPointer<BoxCollider2DGetVec2Fn>(api.BoxCollider2DComponent_GetOffset);
        s_BoxCollider2DSetOffset = Marshal.GetDelegateForFunctionPointer<BoxCollider2DSetVec2Fn>(api.BoxCollider2DComponent_SetOffset);
        s_BoxCollider2DGetRestitution = Marshal.GetDelegateForFunctionPointer<BoxCollider2DGetFloatFn>(api.BoxCollider2DComponent_GetRestitution);
        s_BoxCollider2DSetRestitution = Marshal.GetDelegateForFunctionPointer<BoxCollider2DSetFloatFn>(api.BoxCollider2DComponent_SetRestitution);
        s_BoxCollider2DGetFriction = Marshal.GetDelegateForFunctionPointer<BoxCollider2DGetFloatFn>(api.BoxCollider2DComponent_GetFriction);
        s_BoxCollider2DSetFriction = Marshal.GetDelegateForFunctionPointer<BoxCollider2DSetFloatFn>(api.BoxCollider2DComponent_SetFriction);
        s_BoxCollider2DGetDensity = Marshal.GetDelegateForFunctionPointer<BoxCollider2DGetFloatFn>(api.BoxCollider2DComponent_GetDensity);
        s_BoxCollider2DSetDensity = Marshal.GetDelegateForFunctionPointer<BoxCollider2DSetFloatFn>(api.BoxCollider2DComponent_SetDensity);
        s_BoxCollider2DGetIsSensor = Marshal.GetDelegateForFunctionPointer<BoxCollider2DGetBoolFn>(api.BoxCollider2DComponent_GetIsSensor);
        s_BoxCollider2DSetIsSensor = Marshal.GetDelegateForFunctionPointer<BoxCollider2DSetBoolFn>(api.BoxCollider2DComponent_SetIsSensor);

        s_CircleCollider2DGetCenter = Marshal.GetDelegateForFunctionPointer<CircleCollider2DGetVec2Fn>(api.CircleCollider2DComponent_GetCenter);
        s_CircleCollider2DSetCenter = Marshal.GetDelegateForFunctionPointer<CircleCollider2DSetVec2Fn>(api.CircleCollider2DComponent_SetCenter);
        s_CircleCollider2DGetRadius = Marshal.GetDelegateForFunctionPointer<CircleCollider2DGetFloatFn>(api.CircleCollider2DComponent_GetRadius);
        s_CircleCollider2DSetRadius = Marshal.GetDelegateForFunctionPointer<CircleCollider2DSetFloatFn>(api.CircleCollider2DComponent_SetRadius);
        s_CircleCollider2DGetRestitution = Marshal.GetDelegateForFunctionPointer<CircleCollider2DGetFloatFn>(api.CircleCollider2DComponent_GetRestitution);
        s_CircleCollider2DSetRestitution = Marshal.GetDelegateForFunctionPointer<CircleCollider2DSetFloatFn>(api.CircleCollider2DComponent_SetRestitution);
        s_CircleCollider2DGetFriction = Marshal.GetDelegateForFunctionPointer<CircleCollider2DGetFloatFn>(api.CircleCollider2DComponent_GetFriction);
        s_CircleCollider2DSetFriction = Marshal.GetDelegateForFunctionPointer<CircleCollider2DSetFloatFn>(api.CircleCollider2DComponent_SetFriction);
        s_CircleCollider2DGetDensity = Marshal.GetDelegateForFunctionPointer<CircleCollider2DGetFloatFn>(api.CircleCollider2DComponent_GetDensity);
        s_CircleCollider2DSetDensity = Marshal.GetDelegateForFunctionPointer<CircleCollider2DSetFloatFn>(api.CircleCollider2DComponent_SetDensity);
        s_CircleCollider2DGetIsSensor = Marshal.GetDelegateForFunctionPointer<CircleCollider2DGetBoolFn>(api.CircleCollider2DComponent_GetIsSensor);
        s_CircleCollider2DSetIsSensor = Marshal.GetDelegateForFunctionPointer<CircleCollider2DSetBoolFn>(api.CircleCollider2DComponent_SetIsSensor);

        s_Initialized = true;
    }

    private static void EnsureInitialized()
    {
        if (!s_Initialized)
            throw new InvalidOperationException("InternalCalls bridge is not initialized.");
    }

    private static NativeVector2 ToNative(Vector2 value) => new NativeVector2 { X = value.X, Y = value.Y };
    private static NativeVector3 ToNative(Vector3 value) => new NativeVector3 { X = value.X, Y = value.Y, Z = value.Z };
    private static NativeVector4 ToNative(Vector4 value) => new NativeVector4 { X = value.X, Y = value.Y, Z = value.Z, W = value.W };
    
    private static Vector2 ToManaged(NativeVector2 value) => new Vector2(value.X, value.Y);
    private static Vector3 ToManaged(NativeVector3 value) => new Vector3(value.X, value.Y, value.Z);
    private static Vector4 ToManaged(NativeVector4 value) => new Vector4(value.X, value.Y, value.Z, value.W);
    
    private static NativeQuaternion ToNative(Quaternion value) => new NativeQuaternion { X = value.X, Y = value.Y, Z = value.Z, W = value.W };
    private static Quaternion ToManaged(NativeQuaternion value) => new Quaternion(value.X, value.Y, value.Z, value.W);

    private static IntPtr StringToUtf8(string value) => Marshal.StringToCoTaskMemUTF8(value ?? string.Empty);

    internal static void Debug_Log(string message)
    {
        EnsureInitialized();

        IntPtr ptr = StringToUtf8(message);
        try
        {
            s_DebugLog(ptr);
        }
        finally
        {
            Marshal.FreeCoTaskMem(ptr);
        }
    }

    internal static bool Entity_HasComponent(ulong entityID, Type componentType)
    {
        EnsureInitialized();

        string typeName = componentType?.FullName ?? componentType?.Name ?? string.Empty;
        IntPtr ptr = StringToUtf8(typeName);
        try
        {
            return s_EntityHasComponent(entityID, ptr);
        }
        finally
        {
            Marshal.FreeCoTaskMem(ptr);
        }
    }

    internal static void Entity_AddComponent(ulong entityID, Type componentType)
    {
        EnsureInitialized();

        string typeName = componentType?.FullName ?? componentType?.Name ?? string.Empty;
        IntPtr ptr = StringToUtf8(typeName);
        try
        {
            s_EntityAddComponent(entityID, ptr);
        }
        finally
        {
            Marshal.FreeCoTaskMem(ptr);
        }
    }

    internal static ulong Entity_FindEntityByName(string name)
    {
        EnsureInitialized();

        IntPtr ptr = StringToUtf8(name);
        try
        {
            return s_EntityFindEntityByName(ptr);
        }
        finally
        {
            Marshal.FreeCoTaskMem(ptr);
        }
    }

    internal static ulong Entity_Instantiate(ulong entityID, Vector3 value)
    {
        EnsureInitialized();
        return s_EntityInstantiate(entityID, ToNative(value));
    }

    internal static void Entity_Destroy(ulong entityID)
    {
        EnsureInitialized();
        s_EntityDestroy(entityID);
    }

    internal static void Entity_SetVisibility(ulong entityID, bool value)
    {
        EnsureInitialized();
        s_EntitySetVisibility(entityID, value);
    }

    internal static void Entity_GetVisibility(ulong entityID, out bool result)
    {
        EnsureInitialized();
        s_EntityGetVisibility(entityID, out result);
    }

    internal static bool Input_IsKeyPressed(uint keyCode)
    {
        EnsureInitialized();
        return s_InputIsKeyPressed(keyCode);
    }

    internal static bool Input_IsModifierPressed(ushort modCode)
    {
        EnsureInitialized();
        return s_InputIsModifierPressed(modCode);
    }

    internal static bool Input_IsMouseButtonPressed(byte button)
    {
        EnsureInitialized();
        return s_InputIsMouseButtonPressed(button);
    }

    internal static void Input_GetMousePosition(out Vector2 result)
    {
        EnsureInitialized();
        s_InputGetMousePosition(out NativeVector2 native);
        result = ToManaged(native);
    }

    internal static void Input_SetMouseToCenter()
    {
        EnsureInitialized();
        s_InputSetMouseToCenter();
    }

    internal static void Input_SetCursorMode(int mode)
    {
        EnsureInitialized();
        s_InputSetCursorMode(mode);
    }

    internal static object GetScriptInstance(ulong entityID)
    {
        _ = entityID;
        return null;
    }

    internal static void TransformComponent_GetForward(ulong entityID, out Vector3 result)
    {
        EnsureInitialized();
        s_TransformGetForward(entityID, out NativeVector3 native);
        result = ToManaged(native);
    }

    internal static void TransformComponent_SetForward(ulong entityID, Vector3 result)
    {
        EnsureInitialized();
        s_TransformSetForward(entityID, ToNative(result));
    }

    internal static void TransformComponent_GetRight(ulong entityID, out Vector3 result)
    {
        EnsureInitialized();
        s_TransformGetRight(entityID, out NativeVector3 native);
        result = ToManaged(native);
    }

    internal static void TransformComponent_SetRight(ulong entityID, Vector3 result)
    {
        EnsureInitialized();
        s_TransformSetRight(entityID, ToNative(result));
    }

    internal static void TransformComponent_GetUp(ulong entityID, out Vector3 result)
    {
        EnsureInitialized();
        s_TransformGetUp(entityID, out NativeVector3 native);
        result = ToManaged(native);
    }

    internal static void TransformComponent_SetUp(ulong entityID, Vector3 result)
    {
        EnsureInitialized();
        s_TransformSetUp(entityID, ToNative(result));
    }

    internal static void TransformComponent_GetTranslation(ulong entityID, out Vector3 result)
    {
        EnsureInitialized();
        s_TransformGetTranslation(entityID, out NativeVector3 native);
        result = ToManaged(native);
    }

    internal static void TransformComponent_SetTranslation(ulong entityID, Vector3 value)
    {
        EnsureInitialized();
        s_TransformSetTranslation(entityID, ToNative(value));
    }

    internal static void TransformComponent_GetRotation(ulong entityID, out Quaternion result)
    {
        EnsureInitialized();
        s_TransformGetRotation(entityID, out NativeQuaternion native);
        result = ToManaged(native);
    }

    internal static void TransformComponent_SetRotation(ulong entityID, Quaternion value)
    {
        EnsureInitialized();
        s_TransformSetRotation(entityID, ToNative(value ?? Quaternion.Identity));
    }

    internal static void TransformComponent_GetEulerAngles(ulong entityID, out Vector3 result)
    {
        EnsureInitialized();
        s_TransformGetEulerAngles(entityID, out NativeVector3 native);
        result = ToManaged(native);
    }

    internal static void TransformComponent_SetEulerAngles(ulong entityID, Vector3 value)
    {
        EnsureInitialized();
        s_TransformSetEulerAngles(entityID, ToNative(value));
    }

    internal static void TransformComponent_GetScale(ulong entityID, out Vector3 result)
    {
        EnsureInitialized();
        s_TransformGetScale(entityID, out NativeVector3 native);
        result = ToManaged(native);
    }

    internal static void TransformComponent_SetScale(ulong entityID, Vector3 value)
    {
        EnsureInitialized();
        s_TransformSetScale(entityID, ToNative(value));
    }


    internal static void Sprite2DComponent_SetColor(ulong entityID, Vector4 value)
    {
        EnsureInitialized();
        s_Sprite2DSetColor(entityID, ToNative(value));
    }

    internal static void Sprite2DComponent_GetColor(ulong entityID, out Vector4 result)
    {
        EnsureInitialized();
        s_Sprite2DGetColor(entityID, out NativeVector4 native);
        result = ToManaged(native);
    }

    internal static void Sprite2DComponent_SetTilingFactor(ulong entityID, Vector2 value)
    {
        EnsureInitialized();
        s_Sprite2DSetTilingFactor(entityID, ToNative(value));
    }

    internal static void Sprite2DComponent_GetTilingFactor(ulong entityID, out Vector2 result)
    {
        EnsureInitialized();
        s_Sprite2DGetTilingFactor(entityID, out NativeVector2 native);
        result = ToManaged(native);
    }

    internal static void Circle2DComponent_SetColor(ulong entityID, Vector4 value)
    {
        EnsureInitialized();
        s_Circle2DSetColor(entityID, ToNative(value));
    }

    internal static void Circle2DComponent_GetColor(ulong entityID, out Vector4 result)
    {
        EnsureInitialized();
        s_Circle2DGetColor(entityID, out NativeVector4 native);
        result = ToManaged(native);
    }

    internal static void Rigidbody2DComponent_GetType(ulong entityID, out int result)
    {
        EnsureInitialized();
        s_Rigidbody2DGetType(entityID, out result);
    }

    internal static void Rigidbody2DComponent_SetType(ulong entityID, int value)
    {
        EnsureInitialized();
        s_Rigidbody2DSetType(entityID, value);
    }

    internal static void Rigidbody2DComponent_GetLinearVelocity(ulong entityID, out Vector2 result)
    {
        EnsureInitialized();
        s_Rigidbody2DGetLinearVelocity(entityID, out NativeVector2 native);
        result = ToManaged(native);
    }

    internal static void Rigidbody2DComponent_SetLinearVelocity(ulong entityID, Vector2 value)
    {
        EnsureInitialized();
        s_Rigidbody2DSetLinearVelocity(entityID, ToNative(value));
    }

    internal static void Rigidbody2DComponent_GetAngularVelocity(ulong entityID, out float result)
    {
        EnsureInitialized();
        s_Rigidbody2DGetAngularVelocity(entityID, out result);
    }

    internal static void Rigidbody2DComponent_SetAngularVelocity(ulong entityID, float value)
    {
        EnsureInitialized();
        s_Rigidbody2DSetAngularVelocity(entityID, value);
    }

    internal static void Rigidbody2DComponent_GetGravityScale(ulong entityID, out float result)
    {
        EnsureInitialized();
        s_Rigidbody2DGetGravityScale(entityID, out result);
    }

    internal static void Rigidbody2DComponent_SetGravityScale(ulong entityID, float value)
    {
        EnsureInitialized();
        s_Rigidbody2DSetGravityScale(entityID, value);
    }

    internal static void Rigidbody2DComponent_GetLinearDamping(ulong entityID, out float result)
    {
        EnsureInitialized();
        s_Rigidbody2DGetLinearDamping(entityID, out result);
    }

    internal static void Rigidbody2DComponent_SetLinearDamping(ulong entityID, float value)
    {
        EnsureInitialized();
        s_Rigidbody2DSetLinearDamping(entityID, value);
    }

    internal static void Rigidbody2DComponent_GetAngularDamping(ulong entityID, out float result)
    {
        EnsureInitialized();
        s_Rigidbody2DGetAngularDamping(entityID, out result);
    }

    internal static void Rigidbody2DComponent_SetAngularDamping(ulong entityID, float value)
    {
        EnsureInitialized();
        s_Rigidbody2DSetAngularDamping(entityID, value);
    }

    internal static void Rigidbody2DComponent_GetIsAwake(ulong entityID, out bool result)
    {
        EnsureInitialized();
        s_Rigidbody2DGetIsAwake(entityID, out result);
    }

    internal static void Rigidbody2DComponent_SetIsAwake(ulong entityID, bool value)
    {
        EnsureInitialized();
        s_Rigidbody2DSetIsAwake(entityID, value);
    }

    internal static void Rigidbody2DComponent_GetIsEnabled(ulong entityID, out bool result)
    {
        EnsureInitialized();
        s_Rigidbody2DGetIsEnabled(entityID, out result);
    }

    internal static void Rigidbody2DComponent_SetIsEnabled(ulong entityID, bool value)
    {
        EnsureInitialized();
        s_Rigidbody2DSetIsEnabled(entityID, value);
    }

    internal static void Rigidbody2DComponent_GetIsEnableSleep(ulong entityID, out bool result)
    {
        EnsureInitialized();
        s_Rigidbody2DGetIsEnableSleep(entityID, out result);
    }

    internal static void Rigidbody2DComponent_SetIsEnableSleep(ulong entityID, bool value)
    {
        EnsureInitialized();
        s_Rigidbody2DSetIsEnableSleep(entityID, value);
    }

    internal static void Rigidbody2DComponent_ApplyForce(ulong entityID, Vector2 force, Vector2 point, bool wake)
    {
        EnsureInitialized();
        s_Rigidbody2DApplyForce(entityID, ToNative(force), ToNative(point), wake);
    }

    internal static void Rigidbody2DComponent_ApplyForceToCenter(ulong entityID, Vector2 force, bool wake)
    {
        EnsureInitialized();
        s_Rigidbody2DApplyForceToCenter(entityID, ToNative(force), wake);
    }

    internal static void Rigidbody2DComponent_ApplyLinearImpulse(ulong entityID, Vector2 impulse, Vector2 point, bool wake)
    {
        EnsureInitialized();
        s_Rigidbody2DApplyLinearImpulse(entityID, ToNative(impulse), ToNative(point), wake);
    }

    internal static void Rigidbody2DComponent_ApplyLinearImpulseToCenter(ulong entityID, Vector2 impulse, bool wake)
    {
        EnsureInitialized();
        s_Rigidbody2DApplyLinearImpulseToCenter(entityID, ToNative(impulse), wake);
    }

    internal static void Rigidbody2DComponent_ApplyAngularImpulse(ulong entityID, float impulse, bool wake)
    {
        EnsureInitialized();
        s_Rigidbody2DApplyAngularImpulse(entityID, impulse, wake);
    }

    internal static void Rigidbody2DComponent_ApplyTorque(ulong entityID, float torque, bool wake)
    {
        EnsureInitialized();
        s_Rigidbody2DApplyTorque(entityID, torque, wake);
    }

    internal static void Rigidbody2DComponent_GetMass(ulong entityID, out float result)
    {
        EnsureInitialized();
        s_Rigidbody2DGetMass(entityID, out result);
    }

    internal static void Rigidbody2DComponent_GetIsBullet(ulong entityID, out bool result)
    {
        EnsureInitialized();
        s_Rigidbody2DGetIsBullet(entityID, out result);
    }

    internal static void Rigidbody2DComponent_SetIsBullet(ulong entityID, bool value)
    {
        EnsureInitialized();
        s_Rigidbody2DSetIsBullet(entityID, value);
    }

    internal static void BoxCollider2DComponent_GetSize(ulong entityID, out Vector2 result)
    {
        EnsureInitialized();
        s_BoxCollider2DGetSize(entityID, out NativeVector2 native);
        result = ToManaged(native);
    }

    internal static void BoxCollider2DComponent_SetSize(ulong entityID, Vector2 value)
    {
        EnsureInitialized();
        s_BoxCollider2DSetSize(entityID, ToNative(value));
    }

    internal static void BoxCollider2DComponent_GetOffset(ulong entityID, out Vector2 result)
    {
        EnsureInitialized();
        s_BoxCollider2DGetOffset(entityID, out NativeVector2 native);
        result = ToManaged(native);
    }

    internal static void BoxCollider2DComponent_SetOffset(ulong entityID, Vector2 value)
    {
        EnsureInitialized();
        s_BoxCollider2DSetOffset(entityID, ToNative(value));
    }

    internal static void BoxCollider2DComponent_GetRestitution(ulong entityID, out float result)
    {
        EnsureInitialized();
        s_BoxCollider2DGetRestitution(entityID, out result);
    }

    internal static void BoxCollider2DComponent_SetRestitution(ulong entityID, float value)
    {
        EnsureInitialized();
        s_BoxCollider2DSetRestitution(entityID, value);
    }

    internal static void BoxCollider2DComponent_GetFriction(ulong entityID, out float result)
    {
        EnsureInitialized();
        s_BoxCollider2DGetFriction(entityID, out result);
    }

    internal static void BoxCollider2DComponent_SetFriction(ulong entityID, float value)
    {
        EnsureInitialized();
        s_BoxCollider2DSetFriction(entityID, value);
    }

    internal static void BoxCollider2DComponent_GetDensity(ulong entityID, out float result)
    {
        EnsureInitialized();
        s_BoxCollider2DGetDensity(entityID, out result);
    }

    internal static void BoxCollider2DComponent_SetDensity(ulong entityID, float value)
    {
        EnsureInitialized();
        s_BoxCollider2DSetDensity(entityID, value);
    }

    internal static void BoxCollider2DComponent_GetIsSensor(ulong entityID, out bool result)
    {
        EnsureInitialized();
        s_BoxCollider2DGetIsSensor(entityID, out result);
    }

    internal static void BoxCollider2DComponent_SetIsSensor(ulong entityID, bool value)
    {
        EnsureInitialized();
        s_BoxCollider2DSetIsSensor(entityID, value);
    }

    internal static void CircleCollider2DComponent_GetCenter(ulong entityID, out Vector2 result)
    {
        EnsureInitialized();
        s_CircleCollider2DGetCenter(entityID, out NativeVector2 native);
        result = ToManaged(native);
    }

    internal static void CircleCollider2DComponent_SetCenter(ulong entityID, Vector2 value)
    {
        EnsureInitialized();
        s_CircleCollider2DSetCenter(entityID, ToNative(value));
    }

    internal static void CircleCollider2DComponent_GetRadius(ulong entityID, out float result)
    {
        EnsureInitialized();
        s_CircleCollider2DGetRadius(entityID, out float native);
        result = native;
    }

    internal static void CircleCollider2DComponent_SetRadius(ulong entityID, float value)
    {
        EnsureInitialized();
        s_CircleCollider2DSetRadius(entityID, value);
    }

    internal static void CircleCollider2DComponent_GetRestitution(ulong entityID, out float result)
    {
        EnsureInitialized();
        s_CircleCollider2DGetRestitution(entityID, out result);
    }

    internal static void CircleCollider2DComponent_SetRestitution(ulong entityID, float value)
    {
        EnsureInitialized();
        s_CircleCollider2DSetRestitution(entityID, value);
    }

    internal static void CircleCollider2DComponent_GetFriction(ulong entityID, out float result)
    {
        EnsureInitialized();
        s_CircleCollider2DGetFriction(entityID, out result);
    }

    internal static void CircleCollider2DComponent_SetFriction(ulong entityID, float value)
    {
        EnsureInitialized();
        s_CircleCollider2DSetFriction(entityID, value);
    }

    internal static void CircleCollider2DComponent_GetDensity(ulong entityID, out float result)
    {
        EnsureInitialized();
        s_CircleCollider2DGetDensity(entityID, out result);
    }

    internal static void CircleCollider2DComponent_SetDensity(ulong entityID, float value)
    {
        EnsureInitialized();
        s_CircleCollider2DSetDensity(entityID, value);
    }

    internal static void CircleCollider2DComponent_GetIsSensor(ulong entityID, out bool result)
    {
        EnsureInitialized();
        s_CircleCollider2DGetIsSensor(entityID, out result);
    }

    internal static void CircleCollider2DComponent_SetIsSensor(ulong entityID, bool value)
    {
        EnsureInitialized();
        s_CircleCollider2DSetIsSensor(entityID, value);
    }
}
