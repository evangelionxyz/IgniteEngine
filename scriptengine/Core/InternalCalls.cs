// Copyright (c) 2026 Evangelion Manuhutu

using System;
using System.Runtime.InteropServices;

namespace Ignite.Core;

public static class InternalCalls
{
    private static bool s_Initialized;
    private static NativeAPI.Funcs.DebugLogFn s_DebugLog;
    private static NativeAPI.Funcs.ScenePickEntityAtFn s_ScenePickEntityAt;
    private static NativeAPI.Funcs.EntityHasComponentFn s_EntityHasComponent;
    private static NativeAPI.Funcs.EntityAddComponentFn s_EntityAddComponent;
    private static NativeAPI.Funcs.EntityFindEntityByNameFn s_EntityFindEntityByName;
    private static NativeAPI.Funcs.EntityFindChildEntityByNameFn s_EntityFindChildEntityByName;
    private static NativeAPI.Funcs.EntityIsParent s_EntityIsParent;
    private static NativeAPI.Funcs.EntityGetParent s_EntityGetParent;
    private static NativeAPI.Funcs.EntityInstantiateWithNameFn s_EntityInstantiateWithName;
    private static NativeAPI.Funcs.EntityInstantiateFn s_EntityInstantiate;
    private static NativeAPI.Funcs.EntityDestroyFn s_EntityDestroy;
    private static NativeAPI.Funcs.EntitySetVisibilityFn s_EntitySetVisibility;
    private static NativeAPI.Funcs.EntityGetVisibilityFn s_EntityGetVisibility;
    private static NativeAPI.Funcs.EntityGetNameFn s_EntityGetName;
    private static NativeAPI.Funcs.WidgetComponentHasButtonFn s_WidgetComponentHasButton;
    private static NativeAPI.Funcs.WidgetComponentButtonEventFn s_WidgetComponentAddButtonEventCallback;
    private static NativeAPI.Funcs.WidgetComponentButtonEventFn s_WidgetComponentRemoveButtonEventCallback;
    private static NativeAPI.Funcs.AudioSourceHasAudioFn s_AudioSourceHasAudio;
    private static NativeAPI.Funcs.AudioSourceActionFn s_AudioSourcePlay;
    private static NativeAPI.Funcs.AudioSourceActionFn s_AudioSourceStop;
    private static NativeAPI.Funcs.AudioSourceActionFn s_AudioSourcePause;
    private static NativeAPI.Funcs.AudioSourceActionFn s_AudioSourceResume;
    private static NativeAPI.Funcs.GetFloatFn s_AudioSourceGetVolume;
    private static NativeAPI.Funcs.SetFloatFn s_AudioSourceSetVolume;
    private static NativeAPI.Funcs.GetFloatFn s_AudioSourceGetPitch;
    private static NativeAPI.Funcs.SetFloatFn s_AudioSourceSetPitch;
    private static NativeAPI.Funcs.GetFloatFn s_AudioSourceGetPan;
    private static NativeAPI.Funcs.SetFloatFn s_AudioSourceSetPan;
    private static NativeAPI.Funcs.GetBoolFn s_AudioSourceGetPlayOnStart;
    private static NativeAPI.Funcs.SetBoolFn s_AudioSourceSetPlayOnStart;
    private static NativeAPI.Funcs.GetBoolFn s_AudioSourceGetLoop;
    private static NativeAPI.Funcs.SetBoolFn s_AudioSourceSetLoop;
    private static NativeAPI.Funcs.AudioSourceAddReverbDspFn s_AudioSourceAddReverbDsp;
    private static NativeAPI.Funcs.AudioSourceAddDistortionDspFn s_AudioSourceAddDistortionDsp;
    private static NativeAPI.Funcs.AudioSourceAddChorusDspFn s_AudioSourceAddChorusDsp;
    private static NativeAPI.Funcs.AudioSourceAddCompressorDspFn s_AudioSourceAddCompressorDsp;
    private static NativeAPI.Funcs.AudioSourceAddDelayDspFn s_AudioSourceAddDelayDsp;
    private static NativeAPI.Funcs.AudioSourceActionFn s_AudioSourceClearDsps;
    private static NativeAPI.Funcs.InputIsKeyPressedFn s_InputIsKeyPressed;
    private static NativeAPI.Funcs.InputIsModifierPressedFn s_InputIsModifierPressed;
    private static NativeAPI.Funcs.InputIsMouseButtonPressedFn s_InputIsMouseButtonPressed;
    private static NativeAPI.Funcs.InputGetMousePositionFn s_InputGetMousePosition;
    private static NativeAPI.Funcs.InputSetMouseToCenterFn s_InputSetMouseToCenter;
    private static NativeAPI.Funcs.InputSetCursorModeFn s_InputSetCursorMode;
    private static NativeAPI.Funcs.GetVector3Fn s_TransformGetForward;
    private static NativeAPI.Funcs.SetVector3Fn s_TransformSetForward;
    private static NativeAPI.Funcs.GetVector3Fn s_TransformGetRight;
    private static NativeAPI.Funcs.SetVector3Fn s_TransformSetRight;
    private static NativeAPI.Funcs.GetVector3Fn s_TransformGetUp;
    private static NativeAPI.Funcs.SetVector3Fn s_TransformSetUp;
    private static NativeAPI.Funcs.GetVector3Fn s_TransformGetTranslation;
    private static NativeAPI.Funcs.SetVector3Fn s_TransformSetTranslation;
    private static NativeAPI.Funcs.GetQuaternionFn s_TransformGetRotation;
    private static NativeAPI.Funcs.SetQuaternionFn s_TransformSetRotation;
    private static NativeAPI.Funcs.GetVector3Fn s_TransformGetEulerAngles;
    private static NativeAPI.Funcs.SetVector3Fn s_TransformSetEulerAngles;
    private static NativeAPI.Funcs.GetVector3Fn s_TransformGetScale;
    private static NativeAPI.Funcs.SetVector3Fn s_TransformSetScale;

    private static NativeAPI.Funcs.SetVector4Fn s_Sprite2DSetColor;
    private static NativeAPI.Funcs.GetVector4Fn s_Sprite2DGetColor;
    private static NativeAPI.Funcs.SetVector2Fn s_Sprite2DSetTilingFactor;
    private static NativeAPI.Funcs.GetVector2Fn s_Sprite2DGetTilingFactor;

    private static NativeAPI.Funcs.SetVector4Fn s_Circle2DSetColor;
    private static NativeAPI.Funcs.GetVector4Fn s_Circle2DGetColor;

    private static NativeAPI.Funcs.GetIntFn s_Rigidbody2DGetType;
    private static NativeAPI.Funcs.SetIntFn s_Rigidbody2DSetType;
    private static NativeAPI.Funcs.GetVector2Fn s_Rigidbody2DGetLinearVelocity;
    private static NativeAPI.Funcs.SetVector2Fn s_Rigidbody2DSetLinearVelocity;
    private static NativeAPI.Funcs.GetFloatFn s_Rigidbody2DGetAngularVelocity;
    private static NativeAPI.Funcs.SetFloatFn s_Rigidbody2DSetAngularVelocity;
    private static NativeAPI.Funcs.GetFloatFn s_Rigidbody2DGetGravityScale;
    private static NativeAPI.Funcs.SetFloatFn s_Rigidbody2DSetGravityScale;
    private static NativeAPI.Funcs.GetFloatFn s_Rigidbody2DGetLinearDamping;
    private static NativeAPI.Funcs.SetFloatFn s_Rigidbody2DSetLinearDamping;
    private static NativeAPI.Funcs.GetFloatFn s_Rigidbody2DGetAngularDamping;
    private static NativeAPI.Funcs.SetFloatFn s_Rigidbody2DSetAngularDamping;
    private static NativeAPI.Funcs.GetBoolFn s_Rigidbody2DGetIsAwake;
    private static NativeAPI.Funcs.SetBoolFn s_Rigidbody2DSetIsAwake;
    private static NativeAPI.Funcs.GetBoolFn s_Rigidbody2DGetIsEnabled;
    private static NativeAPI.Funcs.SetBoolFn s_Rigidbody2DSetIsEnabled;
    private static NativeAPI.Funcs.GetBoolFn s_Rigidbody2DGetIsEnableSleep;
    private static NativeAPI.Funcs.SetBoolFn s_Rigidbody2DSetIsEnableSleep;
    private static NativeAPI.Funcs.Rigidbody2DApplyForceFn s_Rigidbody2DApplyForce;
    private static NativeAPI.Funcs.Rigidbody2DApplyForceToCenterFn s_Rigidbody2DApplyForceToCenter;
    private static NativeAPI.Funcs.Rigidbody2DApplyLinearImpulseFn s_Rigidbody2DApplyLinearImpulse;
    private static NativeAPI.Funcs.Rigidbody2DApplyLinearImpulseToCenterFn s_Rigidbody2DApplyLinearImpulseToCenter;
    private static NativeAPI.Funcs.Rigidbody2DApplyAngularImpulseFn s_Rigidbody2DApplyAngularImpulse;
    private static NativeAPI.Funcs.Rigidbody2DApplyTorqueFn s_Rigidbody2DApplyTorque;
    private static NativeAPI.Funcs.GetFloatFn s_Rigidbody2DGetMass;
    private static NativeAPI.Funcs.GetBoolFn s_Rigidbody2DGetIsBullet;
    private static NativeAPI.Funcs.SetBoolFn s_Rigidbody2DSetIsBullet;
    private static NativeAPI.Funcs.GetVector2Fn s_BoxCollider2DGetSize;
    private static NativeAPI.Funcs.SetVector2Fn s_BoxCollider2DSetSize;
    private static NativeAPI.Funcs.GetVector2Fn s_BoxCollider2DGetOffset;
    private static NativeAPI.Funcs.SetVector2Fn s_BoxCollider2DSetOffset;
    private static NativeAPI.Funcs.GetFloatFn s_BoxCollider2DGetRestitution;
    private static NativeAPI.Funcs.SetFloatFn s_BoxCollider2DSetRestitution;
    private static NativeAPI.Funcs.GetFloatFn s_BoxCollider2DGetFriction;
    private static NativeAPI.Funcs.SetFloatFn s_BoxCollider2DSetFriction;
    private static NativeAPI.Funcs.GetFloatFn s_BoxCollider2DGetDensity;
    private static NativeAPI.Funcs.SetFloatFn s_BoxCollider2DSetDensity;
    private static NativeAPI.Funcs.GetBoolFn s_BoxCollider2DGetIsSensor;
    private static NativeAPI.Funcs.SetBoolFn s_BoxCollider2DSetIsSensor;

    private static NativeAPI.Funcs.GetVector2Fn s_CircleCollider2DGetCenter;
    private static NativeAPI.Funcs.SetVector2Fn s_CircleCollider2DSetCenter;
    private static NativeAPI.Funcs.GetFloatFn s_CircleCollider2DGetRadius;
    private static NativeAPI.Funcs.SetFloatFn s_CircleCollider2DSetRadius;
    private static NativeAPI.Funcs.GetFloatFn s_CircleCollider2DGetRestitution;
    private static NativeAPI.Funcs.SetFloatFn s_CircleCollider2DSetRestitution;
    private static NativeAPI.Funcs.GetFloatFn s_CircleCollider2DGetFriction;
    private static NativeAPI.Funcs.SetFloatFn s_CircleCollider2DSetFriction;
    private static NativeAPI.Funcs.GetFloatFn s_CircleCollider2DGetDensity;
    private static NativeAPI.Funcs.SetFloatFn s_CircleCollider2DSetDensity;
    private static NativeAPI.Funcs.GetBoolFn s_CircleCollider2DGetIsSensor;
    private static NativeAPI.Funcs.SetBoolFn s_CircleCollider2DSetIsSensor;

    private static NativeAPI.Funcs.SetStringFn s_TextComponentSetText;
    private static NativeAPI.Funcs.GetStringFn s_TextComponentGetText;
    private static NativeAPI.Funcs.SetVector4Fn s_TextComponentSetColor;
    private static NativeAPI.Funcs.GetVector4Fn s_TextComponentGetColor;
    private static NativeAPI.Funcs.SetFloatFn s_TextComponentSetKerning;
    private static NativeAPI.Funcs.GetFloatFn s_TextComponentGetKerning;
    private static NativeAPI.Funcs.SetFloatFn s_TextComponentSetLineSpacing;
    private static NativeAPI.Funcs.GetFloatFn s_TextComponentGetLineSpacing;
    private static NativeAPI.Funcs.AssetManagerQueryFn s_AssetManagerIsAssetHandleValid;
    private static NativeAPI.Funcs.AssetManagerQueryFn s_AssetManagerIsAssetLoaded;
    private static NativeAPI.Funcs.AssetManagerLoadFn s_AssetManagerLoadAssetAsync;
    private static NativeAPI.Funcs.AssetManagerLoadFn s_AssetManagerLoadAssetImmediate;

    public static void Initialize(ulong apiPtr)
    {
        if (apiPtr == 0)
            throw new ArgumentException("Invalid internal calls API pointer", nameof(apiPtr));

        NativeAPI.API api = Marshal.PtrToStructure<NativeAPI.API>((IntPtr)apiPtr);

        s_DebugLog = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.DebugLogFn>(api.Debug_Log);
        s_ScenePickEntityAt = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.ScenePickEntityAtFn>(api.Scene_PickEntityAt);
        s_EntityHasComponent = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.EntityHasComponentFn>(api.Entity_HasComponent);
        s_EntityAddComponent = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.EntityAddComponentFn>(api.Entity_AddComponent);
        s_EntityFindEntityByName = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.EntityFindEntityByNameFn>(api.Entity_FindEntityByName);
        s_EntityFindChildEntityByName = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.EntityFindChildEntityByNameFn>(api.Entity_FindChildEntityByName);
        s_EntityIsParent = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.EntityIsParent>(api.Entity_IsParent);
        s_EntityGetParent = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.EntityGetParent>(api.Entity_GetParent);
        s_EntityInstantiateWithName = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.EntityInstantiateWithNameFn>(api.Entity_InstantiateWithName);
        s_EntityInstantiate = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.EntityInstantiateFn>(api.Entity_Instantiate);
        s_EntityDestroy = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.EntityDestroyFn>(api.Entity_Destroy);
        s_EntitySetVisibility = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.EntitySetVisibilityFn>(api.Entity_SetVisibility);
        s_EntityGetVisibility = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.EntityGetVisibilityFn>(api.Entity_GetVisibility);
        s_EntityGetName = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.EntityGetNameFn>(api.Entity_GetName);
        s_WidgetComponentHasButton = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.WidgetComponentHasButtonFn>(api.WidgetComponent_HasButton);
        s_WidgetComponentAddButtonEventCallback = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.WidgetComponentButtonEventFn>(api.WidgetComponent_AddButtonEventCallback);
        s_WidgetComponentRemoveButtonEventCallback = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.WidgetComponentButtonEventFn>(api.WidgetComponent_RemoveButtonEventCallback);
        s_AudioSourceHasAudio = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.AudioSourceHasAudioFn>(api.AudioSourceComponent_HasAudio);
        s_AudioSourcePlay = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.AudioSourceActionFn>(api.AudioSourceComponent_Play);
        s_AudioSourceStop = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.AudioSourceActionFn>(api.AudioSourceComponent_Stop);
        s_AudioSourcePause = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.AudioSourceActionFn>(api.AudioSourceComponent_Pause);
        s_AudioSourceResume = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.AudioSourceActionFn>(api.AudioSourceComponent_Resume);
        s_AudioSourceGetVolume = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.GetFloatFn>(api.AudioSourceComponent_GetVolume);
        s_AudioSourceSetVolume = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.SetFloatFn>(api.AudioSourceComponent_SetVolume);
        s_AudioSourceGetPitch = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.GetFloatFn>(api.AudioSourceComponent_GetPitch);
        s_AudioSourceSetPitch = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.SetFloatFn>(api.AudioSourceComponent_SetPitch);
        s_AudioSourceGetPan = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.GetFloatFn>(api.AudioSourceComponent_GetPan);
        s_AudioSourceSetPan = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.SetFloatFn>(api.AudioSourceComponent_SetPan);
        s_AudioSourceGetPlayOnStart = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.GetBoolFn>(api.AudioSourceComponent_GetPlayOnStart);
        s_AudioSourceSetPlayOnStart = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.SetBoolFn>(api.AudioSourceComponent_SetPlayOnStart);
        s_AudioSourceGetLoop = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.GetBoolFn>(api.AudioSourceComponent_GetLoop);
        s_AudioSourceSetLoop = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.SetBoolFn>(api.AudioSourceComponent_SetLoop);
        s_AudioSourceAddReverbDsp = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.AudioSourceAddReverbDspFn>(api.AudioSourceComponent_AddReverbDSP);
        s_AudioSourceAddDistortionDsp = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.AudioSourceAddDistortionDspFn>(api.AudioSourceComponent_AddDistortionDSP);
        s_AudioSourceAddChorusDsp = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.AudioSourceAddChorusDspFn>(api.AudioSourceComponent_AddChorusDSP);
        s_AudioSourceAddCompressorDsp = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.AudioSourceAddCompressorDspFn>(api.AudioSourceComponent_AddCompressorDSP);
        s_AudioSourceAddDelayDsp = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.AudioSourceAddDelayDspFn>(api.AudioSourceComponent_AddDelayDSP);
        s_AudioSourceClearDsps = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.AudioSourceActionFn>(api.AudioSourceComponent_ClearDSPs);
        s_InputIsKeyPressed = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.InputIsKeyPressedFn>(api.Input_IsKeyPressed);
        s_InputIsModifierPressed = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.InputIsModifierPressedFn>(api.Input_IsModifierPressed);
        s_InputIsMouseButtonPressed = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.InputIsMouseButtonPressedFn>(api.Input_IsMouseButtonPressed);
        s_InputGetMousePosition = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.InputGetMousePositionFn>(api.Input_GetMousePosition);
        s_InputSetMouseToCenter = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.InputSetMouseToCenterFn>(api.Input_SetMouseToCenter);
        s_InputSetCursorMode = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.InputSetCursorModeFn>(api.Input_SetCursorMode);
        
        s_TransformGetForward = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.GetVector3Fn>(api.TransformComponent_GetForward);
        s_TransformSetForward = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.SetVector3Fn>(api.TransformComponent_SetForward);
        s_TransformGetRight = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.GetVector3Fn>(api.TransformComponent_GetRight);
        s_TransformSetRight = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.SetVector3Fn>(api.TransformComponent_SetRight);
        s_TransformGetUp = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.GetVector3Fn>(api.TransformComponent_GetUp);
        s_TransformSetUp = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.SetVector3Fn>(api.TransformComponent_SetUp);
        s_TransformGetTranslation = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.GetVector3Fn>(api.TransformComponent_GetTranslation);
        s_TransformSetTranslation = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.SetVector3Fn>(api.TransformComponent_SetTranslation);
        s_TransformGetRotation = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.GetQuaternionFn>(api.TransformComponent_GetRotation);
        s_TransformSetRotation = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.SetQuaternionFn>(api.TransformComponent_SetRotation);
        s_TransformGetEulerAngles = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.GetVector3Fn>(api.TransformComponent_GetEulerAngles);
        s_TransformSetEulerAngles = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.SetVector3Fn>(api.TransformComponent_SetEulerAngles);
        s_TransformGetScale = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.GetVector3Fn>(api.TransformComponent_GetScale);
        s_TransformSetScale = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.SetVector3Fn>(api.TransformComponent_SetScale);

        s_Sprite2DSetColor = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.SetVector4Fn>(api.Sprite2DComponent_SetColor);
        s_Sprite2DGetColor = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.GetVector4Fn>(api.Sprite2DComponent_GetColor);
        s_Sprite2DSetTilingFactor = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.SetVector2Fn>(api.Sprite2DComponent_SetTilingFactor);
        s_Sprite2DGetTilingFactor = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.GetVector2Fn>(api.Sprite2DComponent_GetTilingFactor);

        s_Circle2DSetColor = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.SetVector4Fn>(api.Circle2DComponent_SetColor);
        s_Circle2DGetColor = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.GetVector4Fn>(api.Circle2DComponent_GetColor);

        s_Rigidbody2DGetType = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.GetIntFn>(api.Rigidbody2DComponent_GetType);
        s_Rigidbody2DSetType = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.SetIntFn>(api.Rigidbody2DComponent_SetType);
        s_Rigidbody2DGetLinearVelocity = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.GetVector2Fn>(api.Rigidbody2DComponent_GetLinearVelocity);
        s_Rigidbody2DSetLinearVelocity = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.SetVector2Fn>(api.Rigidbody2DComponent_SetLinearVelocity);
        s_Rigidbody2DGetAngularVelocity = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.GetFloatFn>(api.Rigidbody2DComponent_GetAngularVelocity);
        s_Rigidbody2DSetAngularVelocity = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.SetFloatFn>(api.Rigidbody2DComponent_SetAngularVelocity);
        s_Rigidbody2DGetGravityScale = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.GetFloatFn>(api.Rigidbody2DComponent_GetGravityScale);
        s_Rigidbody2DSetGravityScale = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.SetFloatFn>(api.Rigidbody2DComponent_SetGravityScale);
        s_Rigidbody2DGetLinearDamping = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.GetFloatFn>(api.Rigidbody2DComponent_GetLinearDamping);
        s_Rigidbody2DSetLinearDamping = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.SetFloatFn>(api.Rigidbody2DComponent_SetLinearDamping);
        s_Rigidbody2DGetAngularDamping = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.GetFloatFn>(api.Rigidbody2DComponent_GetAngularDamping);
        s_Rigidbody2DSetAngularDamping = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.SetFloatFn>(api.Rigidbody2DComponent_SetAngularDamping);
        s_Rigidbody2DGetIsAwake = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.GetBoolFn>(api.Rigidbody2DComponent_GetIsAwake);
        s_Rigidbody2DSetIsAwake = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.SetBoolFn>(api.Rigidbody2DComponent_SetIsAwake);
        s_Rigidbody2DGetIsEnabled = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.GetBoolFn>(api.Rigidbody2DComponent_GetIsEnabled);
        s_Rigidbody2DSetIsEnabled = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.SetBoolFn>(api.Rigidbody2DComponent_SetIsEnabled);
        s_Rigidbody2DGetIsEnableSleep = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.GetBoolFn>(api.Rigidbody2DComponent_GetIsEnableSleep);
        s_Rigidbody2DSetIsEnableSleep = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.SetBoolFn>(api.Rigidbody2DComponent_SetIsEnableSleep);
        s_Rigidbody2DApplyForce = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.Rigidbody2DApplyForceFn>(api.Rigidbody2DComponent_ApplyForce);
        s_Rigidbody2DApplyForceToCenter = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.Rigidbody2DApplyForceToCenterFn>(api.Rigidbody2DComponent_ApplyForceToCenter);
        s_Rigidbody2DApplyLinearImpulse = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.Rigidbody2DApplyLinearImpulseFn>(api.Rigidbody2DComponent_ApplyLinearImpulse);
        s_Rigidbody2DApplyLinearImpulseToCenter = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.Rigidbody2DApplyLinearImpulseToCenterFn>(api.Rigidbody2DComponent_ApplyLinearImpulseToCenter);
        s_Rigidbody2DApplyAngularImpulse = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.Rigidbody2DApplyAngularImpulseFn>(api.Rigidbody2DComponent_ApplyAngularImpulse);
        s_Rigidbody2DApplyTorque = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.Rigidbody2DApplyTorqueFn>(api.Rigidbody2DComponent_ApplyTorque);
        s_Rigidbody2DGetMass = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.GetFloatFn>(api.Rigidbody2DComponent_GetMass);
        s_Rigidbody2DGetIsBullet = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.GetBoolFn>(api.Rigidbody2DComponent_GetIsBullet);
        s_Rigidbody2DSetIsBullet = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.SetBoolFn>(api.Rigidbody2DComponent_SetIsBullet);
        
        s_BoxCollider2DGetSize = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.GetVector2Fn>(api.BoxCollider2DComponent_GetSize);
        s_BoxCollider2DSetSize = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.SetVector2Fn>(api.BoxCollider2DComponent_SetSize);
        s_BoxCollider2DGetOffset = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.GetVector2Fn>(api.BoxCollider2DComponent_GetOffset);
        s_BoxCollider2DSetOffset = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.SetVector2Fn>(api.BoxCollider2DComponent_SetOffset);
        s_BoxCollider2DGetRestitution = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.GetFloatFn>(api.BoxCollider2DComponent_GetRestitution);
        s_BoxCollider2DSetRestitution = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.SetFloatFn>(api.BoxCollider2DComponent_SetRestitution);
        s_BoxCollider2DGetFriction = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.GetFloatFn>(api.BoxCollider2DComponent_GetFriction);
        s_BoxCollider2DSetFriction = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.SetFloatFn>(api.BoxCollider2DComponent_SetFriction);
        s_BoxCollider2DGetDensity = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.GetFloatFn>(api.BoxCollider2DComponent_GetDensity);
        s_BoxCollider2DSetDensity = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.SetFloatFn>(api.BoxCollider2DComponent_SetDensity);
        s_BoxCollider2DGetIsSensor = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.GetBoolFn>(api.BoxCollider2DComponent_GetIsSensor);
        s_BoxCollider2DSetIsSensor = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.SetBoolFn>(api.BoxCollider2DComponent_SetIsSensor);

        s_CircleCollider2DGetCenter = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.GetVector2Fn>(api.CircleCollider2DComponent_GetCenter);
        s_CircleCollider2DSetCenter = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.SetVector2Fn>(api.CircleCollider2DComponent_SetCenter);
        s_CircleCollider2DGetRadius = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.GetFloatFn>(api.CircleCollider2DComponent_GetRadius);
        s_CircleCollider2DSetRadius = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.SetFloatFn>(api.CircleCollider2DComponent_SetRadius);
        s_CircleCollider2DGetRestitution = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.GetFloatFn>(api.CircleCollider2DComponent_GetRestitution);
        s_CircleCollider2DSetRestitution = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.SetFloatFn>(api.CircleCollider2DComponent_SetRestitution);
        s_CircleCollider2DGetFriction = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.GetFloatFn>(api.CircleCollider2DComponent_GetFriction);
        s_CircleCollider2DSetFriction = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.SetFloatFn>(api.CircleCollider2DComponent_SetFriction);
        s_CircleCollider2DGetDensity = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.GetFloatFn>(api.CircleCollider2DComponent_GetDensity);
        s_CircleCollider2DSetDensity = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.SetFloatFn>(api.CircleCollider2DComponent_SetDensity);
        s_CircleCollider2DGetIsSensor = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.GetBoolFn>(api.CircleCollider2DComponent_GetIsSensor);
        s_CircleCollider2DSetIsSensor = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.SetBoolFn>(api.CircleCollider2DComponent_SetIsSensor);

        s_TextComponentSetText = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.SetStringFn>(api.TextComponent_SetText);
        s_TextComponentGetText = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.GetStringFn>(api.TextComponent_GetText);
        s_TextComponentSetColor = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.SetVector4Fn>(api.TextComponent_SetColor);
        s_TextComponentGetColor = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.GetVector4Fn>(api.TextComponent_GetColor);
        s_TextComponentSetKerning = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.SetFloatFn>(api.TextComponent_SetKerning);
        s_TextComponentGetKerning = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.GetFloatFn>(api.TextComponent_GetKerning);
        s_TextComponentSetLineSpacing = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.SetFloatFn>(api.TextComponent_SetLineSpacing);
        s_TextComponentGetLineSpacing = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.GetFloatFn>(api.TextComponent_GetLineSpacing);
        s_AssetManagerIsAssetHandleValid = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.AssetManagerQueryFn>(api.AssetManager_IsAssetHandleValid);
        s_AssetManagerIsAssetLoaded = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.AssetManagerQueryFn>(api.AssetManager_IsAssetLoaded);
        s_AssetManagerLoadAssetAsync = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.AssetManagerLoadFn>(api.AssetManager_LoadAssetAsync);
        s_AssetManagerLoadAssetImmediate = Marshal.GetDelegateForFunctionPointer<NativeAPI.Funcs.AssetManagerLoadFn>(api.AssetManager_LoadAssetImmediate);

        s_Initialized = true;
    }

    private static void EnsureInitialized()
    {
        if (!s_Initialized)
            throw new InvalidOperationException("InternalCalls bridge is not initialized.");
    }

    private static NativeAPI.NativeVector2 ToNative(Vector2 value) => new NativeAPI.NativeVector2 { X = value.X, Y = value.Y };
    private static NativeAPI.NativeVector3 ToNative(Vector3 value) => new NativeAPI.NativeVector3 { X = value.X, Y = value.Y, Z = value.Z };
    private static NativeAPI.NativeVector4 ToNative(Vector4 value) => new NativeAPI.NativeVector4 { X = value.X, Y = value.Y, Z = value.Z, W = value.W };
    
    private static Vector2 ToManaged(NativeAPI.NativeVector2 value) => new Vector2(value.X, value.Y);
    private static Vector3 ToManaged(NativeAPI.NativeVector3 value) => new Vector3(value.X, value.Y, value.Z);
    private static Vector4 ToManaged(NativeAPI.NativeVector4 value) => new Vector4(value.X, value.Y, value.Z, value.W);
    
    private static NativeAPI.NativeQuaternion ToNative(Quaternion value) => new NativeAPI.NativeQuaternion { X = value.X, Y = value.Y, Z = value.Z, W = value.W };
    private static Quaternion ToManaged(NativeAPI.NativeQuaternion value) => new Quaternion(value.X, value.Y, value.Z, value.W);

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

    internal static ulong Scene_PickEntityAt(float x, float y)
    {
        EnsureInitialized();
        return s_ScenePickEntityAt(x, y);
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

    internal static ulong Entity_FindEntity(string name)
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

    internal static ulong Entity_FindChildEntity(ulong entityID, string childName)
    {
        EnsureInitialized();

        IntPtr ptr = StringToUtf8(childName);
        try
        {
            return s_EntityFindChildEntityByName(entityID, ptr);
        }
        finally
        {
            Marshal.FreeCoTaskMem(ptr);
        }
    }

    internal static bool Entity_IsParent(ulong entityID, ulong parentEntityID)
    {
        EnsureInitialized();
        return s_EntityIsParent(entityID, parentEntityID);
    }

    internal static ulong Entity_GetParent(ulong entityID)
    {
        EnsureInitialized();
        return s_EntityGetParent(entityID);
    }

    internal static ulong Entity_Instantiate(string name, Vector3 value)
    {
        EnsureInitialized();
        IntPtr ptr = StringToUtf8(name);
        try
        {
            return s_EntityInstantiateWithName(ptr, ToNative(value));
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

    internal static string Entity_GetName(ulong entityID)
    {
        EnsureInitialized();
        IntPtr namePtr = s_EntityGetName(entityID);
        if (namePtr == IntPtr.Zero)
            return null;

        return Marshal.PtrToStringUTF8(namePtr);
    }

    internal static bool WidgetComponent_HasButton(ulong entityID, string buttonName)
    {
        EnsureInitialized();

        IntPtr buttonNamePtr = StringToUtf8(buttonName);
        try
        {
            return s_WidgetComponentHasButton(entityID, buttonNamePtr);
        }
        finally
        {
            Marshal.FreeCoTaskMem(buttonNamePtr);
        }
    }

    internal static bool WidgetComponent_AddButtonEventCallback(ulong entityID, string buttonName, int eventType, string methodName)
    {
        EnsureInitialized();

        IntPtr buttonNamePtr = StringToUtf8(buttonName);
        IntPtr methodNamePtr = StringToUtf8(methodName);
        try
        {
            return s_WidgetComponentAddButtonEventCallback(entityID, buttonNamePtr, eventType, methodNamePtr);
        }
        finally
        {
            Marshal.FreeCoTaskMem(buttonNamePtr);
            Marshal.FreeCoTaskMem(methodNamePtr);
        }
    }

    internal static bool WidgetComponent_RemoveButtonEventCallback(ulong entityID, string buttonName, int eventType, string methodName)
    {
        EnsureInitialized();

        IntPtr buttonNamePtr = StringToUtf8(buttonName);
        IntPtr methodNamePtr = StringToUtf8(methodName);
        try
        {
            return s_WidgetComponentRemoveButtonEventCallback(entityID, buttonNamePtr, eventType, methodNamePtr);
        }
        finally
        {
            Marshal.FreeCoTaskMem(buttonNamePtr);
            Marshal.FreeCoTaskMem(methodNamePtr);
        }
    }

    internal static bool AudioSourceComponent_HasAudio(ulong entityID)
    {
        EnsureInitialized();
        return s_AudioSourceHasAudio(entityID);
    }

    internal static void AudioSourceComponent_Play(ulong entityID)
    {
        EnsureInitialized();
        s_AudioSourcePlay(entityID);
    }

    internal static void AudioSourceComponent_Stop(ulong entityID)
    {
        EnsureInitialized();
        s_AudioSourceStop(entityID);
    }

    internal static void AudioSourceComponent_Pause(ulong entityID)
    {
        EnsureInitialized();
        s_AudioSourcePause(entityID);
    }

    internal static void AudioSourceComponent_Resume(ulong entityID)
    {
        EnsureInitialized();
        s_AudioSourceResume(entityID);
    }

    internal static void AudioSourceComponent_GetVolume(ulong entityID, out float result)
    {
        EnsureInitialized();
        s_AudioSourceGetVolume(entityID, out result);
    }

    internal static void AudioSourceComponent_SetVolume(ulong entityID, float value)
    {
        EnsureInitialized();
        s_AudioSourceSetVolume(entityID, value);
    }

    internal static void AudioSourceComponent_GetPitch(ulong entityID, out float result)
    {
        EnsureInitialized();
        s_AudioSourceGetPitch(entityID, out result);
    }

    internal static void AudioSourceComponent_SetPitch(ulong entityID, float value)
    {
        EnsureInitialized();
        s_AudioSourceSetPitch(entityID, value);
    }

    internal static void AudioSourceComponent_GetPan(ulong entityID, out float result)
    {
        EnsureInitialized();
        s_AudioSourceGetPan(entityID, out result);
    }

    internal static void AudioSourceComponent_SetPan(ulong entityID, float value)
    {
        EnsureInitialized();
        s_AudioSourceSetPan(entityID, value);
    }

    internal static void AudioSourceComponent_GetPlayOnStart(ulong entityID, out bool result)
    {
        EnsureInitialized();
        s_AudioSourceGetPlayOnStart(entityID, out result);
    }

    internal static void AudioSourceComponent_SetPlayOnStart(ulong entityID, bool value)
    {
        EnsureInitialized();
        s_AudioSourceSetPlayOnStart(entityID, value);
    }

    internal static void AudioSourceComponent_GetLoop(ulong entityID, out bool result)
    {
        EnsureInitialized();
        s_AudioSourceGetLoop(entityID, out result);
    }

    internal static void AudioSourceComponent_SetLoop(ulong entityID, bool value)
    {
        EnsureInitialized();
        s_AudioSourceSetLoop(entityID, value);
    }

    internal static bool AudioSourceComponent_AddReverbDSP(ulong entityID, float decayTime, float earlyDelay, float lateDelay, float highFrequencyReference, float diffusion, float density, float lowShelfGain, float highCut, float dryLevel, float wetLevel)
    {
        EnsureInitialized();
        return s_AudioSourceAddReverbDsp(entityID, decayTime, earlyDelay, lateDelay, highFrequencyReference, diffusion, density, lowShelfGain, highCut, dryLevel, wetLevel);
    }

    internal static bool AudioSourceComponent_AddDistortionDSP(ulong entityID, float distortionLevel)
    {
        EnsureInitialized();
        return s_AudioSourceAddDistortionDsp(entityID, distortionLevel);
    }

    internal static bool AudioSourceComponent_AddChorusDSP(ulong entityID, float mix, float rate, float depth)
    {
        EnsureInitialized();
        return s_AudioSourceAddChorusDsp(entityID, mix, rate, depth);
    }

    internal static bool AudioSourceComponent_AddCompressorDSP(ulong entityID, float threshold, float ratio, float release, float gainMakeup, bool useSidechain)
    {
        EnsureInitialized();
        return s_AudioSourceAddCompressorDsp(entityID, threshold, ratio, release, gainMakeup, useSidechain);
    }

    internal static bool AudioSourceComponent_AddDelayDSP(ulong entityID, float delayMs, float feedback)
    {
        EnsureInitialized();
        return s_AudioSourceAddDelayDsp(entityID, delayMs, feedback);
    }

    internal static void AudioSourceComponent_ClearDSPs(ulong entityID)
    {
        EnsureInitialized();
        s_AudioSourceClearDsps(entityID);
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
        s_InputGetMousePosition(out NativeAPI.NativeVector2 native);
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
        s_TransformGetForward(entityID, out NativeAPI.NativeVector3 native);
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
        s_TransformGetRight(entityID, out NativeAPI.NativeVector3 native);
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
        s_TransformGetUp(entityID, out NativeAPI.NativeVector3 native);
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
        s_TransformGetTranslation(entityID, out NativeAPI.NativeVector3 native);
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
        s_TransformGetRotation(entityID, out NativeAPI.NativeQuaternion native);
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
        s_TransformGetEulerAngles(entityID, out NativeAPI.NativeVector3 native);
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
        s_TransformGetScale(entityID, out NativeAPI.NativeVector3 native);
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
        s_Sprite2DGetColor(entityID, out NativeAPI.NativeVector4 native);
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
        s_Sprite2DGetTilingFactor(entityID, out NativeAPI.NativeVector2 native);
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
        s_Circle2DGetColor(entityID, out NativeAPI.NativeVector4 native);
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
        s_Rigidbody2DGetLinearVelocity(entityID, out NativeAPI.NativeVector2 native);
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
        s_BoxCollider2DGetSize(entityID, out NativeAPI.NativeVector2 native);
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
        s_BoxCollider2DGetOffset(entityID, out NativeAPI.NativeVector2 native);
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
        s_CircleCollider2DGetCenter(entityID, out NativeAPI.NativeVector2 native);
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

    internal static void TextComponent_SetText(ulong entityID, string value)
    {
        EnsureInitialized();
        IntPtr ptr = StringToUtf8(value);
        try
        {
            s_TextComponentSetText(entityID, ptr);
        }
        finally
        {
            Marshal.FreeCoTaskMem(ptr);
        }
    }

    internal static void TextComponent_GetText(ulong entityID, out string result)
    {
        EnsureInitialized();
        s_TextComponentGetText(entityID, out IntPtr ptr);
        try
        {
            result = Marshal.PtrToStringUTF8(ptr);
        }
        finally
        {
            if (ptr != IntPtr.Zero)
                Marshal.FreeCoTaskMem(ptr);
        }
    }

    internal static void TextComponent_SetColor(ulong entityID, Vector4 value)
    {
        EnsureInitialized();
        s_TextComponentSetColor(entityID, ToNative(value));
    }

    internal static void TextComponent_GetColor(ulong entityID, out Vector4 result)
    {
        EnsureInitialized();
        s_TextComponentGetColor(entityID, out NativeAPI.NativeVector4 outResult);
        result = ToManaged(outResult);
    }

    internal static void TextComponent_SetKerning(ulong entityID, float value)
    {
        EnsureInitialized();
        s_TextComponentSetKerning(entityID, value);
    }

    internal static void TextComponent_GetKerning(ulong entityID, out float result)
    {
        EnsureInitialized();
        s_TextComponentGetKerning(entityID, out result);
    }

    internal static void TextComponent_SetLineSpacing(ulong entityID, float value)
    {
        EnsureInitialized();
        s_TextComponentSetLineSpacing(entityID, value);
    }

    internal static void TextComponent_GetLineSpacing(ulong entityID, out float result)
    {
        EnsureInitialized();
        s_TextComponentGetLineSpacing(entityID, out result);
    }

    internal static bool AssetManager_IsAssetHandleValid(ulong handle)
    {
        EnsureInitialized();
        return s_AssetManagerIsAssetHandleValid(handle);
    }

    internal static bool AssetManager_IsAssetLoaded(ulong handle)
    {
        EnsureInitialized();
        return s_AssetManagerIsAssetLoaded(handle);
    }

    internal static void AssetManager_LoadAssetAsync(ulong handle)
    {
        EnsureInitialized();
        s_AssetManagerLoadAssetAsync(handle);
    }

    internal static void AssetManager_LoadAssetImmediate(ulong handle)
    {
        EnsureInitialized();
        s_AssetManagerLoadAssetImmediate(handle);
    }
}
