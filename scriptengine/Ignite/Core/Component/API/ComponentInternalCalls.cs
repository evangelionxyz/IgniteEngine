// Copyright (c) 2026 Evangelion Manuhutu

using System;
using System.Runtime.InteropServices;

namespace Ignite.Core.Component;

public static class ComponentInternalCalls
{
    private static bool s_Initialized;
    private static ComponentNativeAPI.Funcs.SceneGetScreenToWorldRayFn? s_SceneGetScreenToWorldRay;
    private static ComponentNativeAPI.Funcs.SceneRaycastFn? s_SceneRaycast;
    private static ComponentNativeAPI.Funcs.ScenePhysicsRaycastFn? s_ScenePhysicsRaycast;
    private static ComponentNativeAPI.Funcs.SceneGetPrimaryCameraFn? s_SceneGetPrimaryCamera;
    private static ComponentNativeAPI.Funcs.EntityHasComponentFn? s_EntityHasComponent;
    private static ComponentNativeAPI.Funcs.EntityAddComponentFn? s_EntityAddComponent;
    private static ComponentNativeAPI.Funcs.EntityFindEntityByNameFn? s_EntityFindEntityByName;
    private static ComponentNativeAPI.Funcs.EntityFindChildEntityByNameFn? s_EntityFindChildEntityByName;
    private static ComponentNativeAPI.Funcs.EntityIsParentFn? s_EntityIsParent;
    private static ComponentNativeAPI.Funcs.EntityGetParentFn? s_EntityGetParent;
    private static ComponentNativeAPI.Funcs.EntityInstantiateWithNameFn? s_EntityInstantiateWithName;
    private static ComponentNativeAPI.Funcs.EntityInstantiateFn? s_EntityInstantiate;
    private static ComponentNativeAPI.Funcs.EntityDestroyFn? s_EntityDestroy;
    private static ComponentNativeAPI.Funcs.EntitySetVisibilityFn? s_EntitySetVisibility;
    private static ComponentNativeAPI.Funcs.EntityGetVisibilityFn? s_EntityGetVisibility;
    private static ComponentNativeAPI.Funcs.EntityGetNameFn? s_EntityGetName;
    private static ComponentNativeAPI.Funcs.WidgetComponentHasButtonFn? s_WidgetComponentHasButton;
    private static ComponentNativeAPI.Funcs.WidgetComponentButtonEventFn? s_WidgetComponentAddButtonEventCallback;
    private static ComponentNativeAPI.Funcs.WidgetComponentHasNamedItemFn? s_WidgetComponentHasLabel;
    private static ComponentNativeAPI.Funcs.WidgetComponentGetStringByNameFn? s_WidgetComponentGetLabelText;
    private static ComponentNativeAPI.Funcs.WidgetComponentSetStringByNameFn? s_WidgetComponentSetLabelText;
    private static ComponentNativeAPI.Funcs.WidgetComponentGetVec4ByNameFn? s_WidgetComponentGetLabelColor;
    private static ComponentNativeAPI.Funcs.WidgetComponentSetVec4ByNameFn? s_WidgetComponentSetLabelColor;
    private static ComponentNativeAPI.Funcs.WidgetComponentGetFloatByNameFn? s_WidgetComponentGetLabelFontSize;
    private static ComponentNativeAPI.Funcs.WidgetComponentSetFloatByNameFn? s_WidgetComponentSetLabelFontSize;
    private static ComponentNativeAPI.Funcs.WidgetComponentHasNamedItemFn? s_WidgetComponentHasImage;
    private static ComponentNativeAPI.Funcs.WidgetComponentGetU64ByNameFn? s_WidgetComponentGetImageHandle;
    private static ComponentNativeAPI.Funcs.WidgetComponentSetU64ByNameFn? s_WidgetComponentSetImageHandle;
    private static ComponentNativeAPI.Funcs.WidgetComponentButtonEventFn? s_WidgetComponentRemoveButtonEventCallback;
    private static ComponentNativeAPI.Funcs.AudioSourceHasAudioFn? s_AudioSourceHasAudio;
    private static ComponentNativeAPI.Funcs.AudioSourceActionFn? s_AudioSourcePlay;
    private static ComponentNativeAPI.Funcs.AudioSourceActionFn? s_AudioSourceStop;
    private static ComponentNativeAPI.Funcs.AudioSourceActionFn? s_AudioSourcePause;
    private static ComponentNativeAPI.Funcs.AudioSourceActionFn? s_AudioSourceResume;
    private static CoreNativeAPI.Funcs.GetFloatFn? s_AudioSourceGetVolume;
    private static CoreNativeAPI.Funcs.SetFloatFn? s_AudioSourceSetVolume;
    private static CoreNativeAPI.Funcs.GetFloatFn? s_AudioSourceGetPitch;
    private static CoreNativeAPI.Funcs.SetFloatFn? s_AudioSourceSetPitch;
    private static CoreNativeAPI.Funcs.GetFloatFn? s_AudioSourceGetPan;
    private static CoreNativeAPI.Funcs.SetFloatFn? s_AudioSourceSetPan;
    private static CoreNativeAPI.Funcs.GetBoolFn? s_AudioSourceGetPlayOnStart;
    private static CoreNativeAPI.Funcs.SetBoolFn? s_AudioSourceSetPlayOnStart;
    private static CoreNativeAPI.Funcs.GetBoolFn? s_AudioSourceGetLoop;
    private static CoreNativeAPI.Funcs.SetBoolFn? s_AudioSourceSetLoop;
    private static ComponentNativeAPI.Funcs.AudioSourceAddReverbDspFn? s_AudioSourceAddReverbDsp;
    private static ComponentNativeAPI.Funcs.AudioSourceAddDistortionDspFn? s_AudioSourceAddDistortionDsp;
    private static ComponentNativeAPI.Funcs.AudioSourceAddChorusDspFn? s_AudioSourceAddChorusDsp;
    private static ComponentNativeAPI.Funcs.AudioSourceAddCompressorDspFn? s_AudioSourceAddCompressorDsp;
    private static ComponentNativeAPI.Funcs.AudioSourceAddDelayDspFn? s_AudioSourceAddDelayDsp;
    private static ComponentNativeAPI.Funcs.AudioSourceActionFn? s_AudioSourceClearDsps;
    private static CoreNativeAPI.Funcs.GetVector3Fn? s_TransformGetForward;
    private static CoreNativeAPI.Funcs.SetVector3Fn? s_TransformSetForward;
    private static CoreNativeAPI.Funcs.GetVector3Fn? s_TransformGetRight;
    private static CoreNativeAPI.Funcs.SetVector3Fn? s_TransformSetRight;
    private static CoreNativeAPI.Funcs.GetVector3Fn? s_TransformGetUp;
    private static CoreNativeAPI.Funcs.SetVector3Fn? s_TransformSetUp;
    private static CoreNativeAPI.Funcs.GetVector3Fn? s_TransformGetTranslation;
    private static CoreNativeAPI.Funcs.SetVector3Fn? s_TransformSetTranslation;
    private static CoreNativeAPI.Funcs.GetQuaternionFn? s_TransformGetRotation;
    private static CoreNativeAPI.Funcs.SetQuaternionFn? s_TransformSetRotation;
    private static CoreNativeAPI.Funcs.GetVector3Fn? s_TransformGetEulerAngles;
    private static CoreNativeAPI.Funcs.SetVector3Fn? s_TransformSetEulerAngles;
    private static CoreNativeAPI.Funcs.GetVector3Fn? s_TransformGetScale;
    private static CoreNativeAPI.Funcs.SetVector3Fn? s_TransformSetScale;

    private static CoreNativeAPI.Funcs.SetVector4Fn? s_Sprite2DSetColor;
    private static CoreNativeAPI.Funcs.GetVector4Fn? s_Sprite2DGetColor;
    private static CoreNativeAPI.Funcs.SetVector2Fn? s_Sprite2DSetTilingFactor;
    private static CoreNativeAPI.Funcs.GetVector2Fn? s_Sprite2DGetTilingFactor;

    private static CoreNativeAPI.Funcs.SetVector4Fn? s_Circle2DSetColor;
    private static CoreNativeAPI.Funcs.GetVector4Fn? s_Circle2DGetColor;

    private static CoreNativeAPI.Funcs.GetIntFn? s_Rigidbody2DGetType;
    private static CoreNativeAPI.Funcs.SetIntFn? s_Rigidbody2DSetType;
    private static CoreNativeAPI.Funcs.GetVector2Fn? s_Rigidbody2DGetLinearVelocity;
    private static CoreNativeAPI.Funcs.SetVector2Fn? s_Rigidbody2DSetLinearVelocity;
    private static CoreNativeAPI.Funcs.GetFloatFn? s_Rigidbody2DGetAngularVelocity;
    private static CoreNativeAPI.Funcs.SetFloatFn? s_Rigidbody2DSetAngularVelocity;
    private static CoreNativeAPI.Funcs.GetFloatFn? s_Rigidbody2DGetGravityScale;
    private static CoreNativeAPI.Funcs.SetFloatFn? s_Rigidbody2DSetGravityScale;
    private static CoreNativeAPI.Funcs.GetFloatFn? s_Rigidbody2DGetLinearDamping;
    private static CoreNativeAPI.Funcs.SetFloatFn? s_Rigidbody2DSetLinearDamping;
    private static CoreNativeAPI.Funcs.GetFloatFn? s_Rigidbody2DGetAngularDamping;
    private static CoreNativeAPI.Funcs.SetFloatFn? s_Rigidbody2DSetAngularDamping;
    private static CoreNativeAPI.Funcs.GetBoolFn? s_Rigidbody2DGetIsAwake;
    private static CoreNativeAPI.Funcs.SetBoolFn? s_Rigidbody2DSetIsAwake;
    private static CoreNativeAPI.Funcs.GetBoolFn? s_Rigidbody2DGetIsEnabled;
    private static CoreNativeAPI.Funcs.SetBoolFn? s_Rigidbody2DSetIsEnabled;
    private static CoreNativeAPI.Funcs.GetBoolFn? s_Rigidbody2DGetIsEnableSleep;
    private static CoreNativeAPI.Funcs.SetBoolFn? s_Rigidbody2DSetIsEnableSleep;
    private static ComponentNativeAPI.Funcs.Rigidbody2DApplyForceFn? s_Rigidbody2DApplyForce;
    private static ComponentNativeAPI.Funcs.Rigidbody2DApplyForceToCenterFn? s_Rigidbody2DApplyForceToCenter;
    private static ComponentNativeAPI.Funcs.Rigidbody2DApplyLinearImpulseFn? s_Rigidbody2DApplyLinearImpulse;
    private static ComponentNativeAPI.Funcs.Rigidbody2DApplyLinearImpulseToCenterFn? s_Rigidbody2DApplyLinearImpulseToCenter;
    private static ComponentNativeAPI.Funcs.Rigidbody2DApplyAngularImpulseFn? s_Rigidbody2DApplyAngularImpulse;
    private static ComponentNativeAPI.Funcs.Rigidbody2DApplyTorqueFn? s_Rigidbody2DApplyTorque;
    private static CoreNativeAPI.Funcs.GetFloatFn? s_Rigidbody2DGetMass;
    private static CoreNativeAPI.Funcs.GetBoolFn? s_Rigidbody2DGetIsBullet;
    private static CoreNativeAPI.Funcs.SetBoolFn? s_Rigidbody2DSetIsBullet;
    private static CoreNativeAPI.Funcs.GetVector2Fn? s_BoxCollider2DGetSize;
    private static CoreNativeAPI.Funcs.SetVector2Fn? s_BoxCollider2DSetSize;
    private static CoreNativeAPI.Funcs.GetVector2Fn? s_BoxCollider2DGetOffset;
    private static CoreNativeAPI.Funcs.SetVector2Fn? s_BoxCollider2DSetOffset;
    private static CoreNativeAPI.Funcs.GetFloatFn? s_BoxCollider2DGetRestitution;
    private static CoreNativeAPI.Funcs.SetFloatFn? s_BoxCollider2DSetRestitution;
    private static CoreNativeAPI.Funcs.GetFloatFn? s_BoxCollider2DGetFriction;
    private static CoreNativeAPI.Funcs.SetFloatFn? s_BoxCollider2DSetFriction;
    private static CoreNativeAPI.Funcs.GetFloatFn? s_BoxCollider2DGetDensity;
    private static CoreNativeAPI.Funcs.SetFloatFn? s_BoxCollider2DSetDensity;
    private static CoreNativeAPI.Funcs.GetBoolFn? s_BoxCollider2DGetIsSensor;
    private static CoreNativeAPI.Funcs.SetBoolFn? s_BoxCollider2DSetIsSensor;

    private static CoreNativeAPI.Funcs.GetVector2Fn? s_CircleCollider2DGetCenter;
    private static CoreNativeAPI.Funcs.SetVector2Fn? s_CircleCollider2DSetCenter;
    private static CoreNativeAPI.Funcs.GetFloatFn? s_CircleCollider2DGetRadius;
    private static CoreNativeAPI.Funcs.SetFloatFn? s_CircleCollider2DSetRadius;
    private static CoreNativeAPI.Funcs.GetFloatFn? s_CircleCollider2DGetRestitution;
    private static CoreNativeAPI.Funcs.SetFloatFn? s_CircleCollider2DSetRestitution;
    private static CoreNativeAPI.Funcs.GetFloatFn? s_CircleCollider2DGetFriction;
    private static CoreNativeAPI.Funcs.SetFloatFn? s_CircleCollider2DSetFriction;
    private static CoreNativeAPI.Funcs.GetFloatFn? s_CircleCollider2DGetDensity;
    private static CoreNativeAPI.Funcs.SetFloatFn? s_CircleCollider2DSetDensity;
    private static CoreNativeAPI.Funcs.GetBoolFn? s_CircleCollider2DGetIsSensor;
    private static CoreNativeAPI.Funcs.SetBoolFn? s_CircleCollider2DSetIsSensor;

    private static CoreNativeAPI.Funcs.SetStringFn? s_TextComponentSetText;
    private static CoreNativeAPI.Funcs.GetStringFn? s_TextComponentGetText;
    private static CoreNativeAPI.Funcs.SetVector4Fn? s_TextComponentSetColor;
    private static CoreNativeAPI.Funcs.GetVector4Fn? s_TextComponentGetColor;
    private static CoreNativeAPI.Funcs.SetFloatFn? s_TextComponentSetKerning;
    private static CoreNativeAPI.Funcs.GetFloatFn? s_TextComponentGetKerning;
    private static CoreNativeAPI.Funcs.SetFloatFn? s_TextComponentSetLineSpacing;
    private static CoreNativeAPI.Funcs.GetFloatFn? s_TextComponentGetLineSpacing;

    // RigidbodyComponent (3D)
    private static CoreNativeAPI.Funcs.GetIntFn? s_RigidbodyGetType;
    private static CoreNativeAPI.Funcs.SetIntFn? s_RigidbodySetType;
    private static CoreNativeAPI.Funcs.GetIntFn? s_RigidbodyGetMotionQuality;
    private static CoreNativeAPI.Funcs.GetBoolFn? s_RigidbodyGetUseGravity;
    private static CoreNativeAPI.Funcs.SetBoolFn? s_RigidbodySetUseGravity;
    private static CoreNativeAPI.Funcs.GetFloatFn? s_RigidbodyGetMass;
    private static CoreNativeAPI.Funcs.GetFloatFn? s_RigidbodyGetGravityFactor;
    private static CoreNativeAPI.Funcs.SetFloatFn? s_RigidbodySetGravityFactor;
    private static ComponentNativeAPI.Funcs.GetVector3VoidFn? s_RigidbodyGetLinearVelocity;
    private static ComponentNativeAPI.Funcs.SetVector3VoidFn? s_RigidbodySetLinearVelocity;
    private static ComponentNativeAPI.Funcs.GetVector3VoidFn? s_RigidbodyGetAngularVelocity;
    private static ComponentNativeAPI.Funcs.GetVector3VoidFn? s_RigidbodyGetPosition;
    private static ComponentNativeAPI.Funcs.SetVector3VoidFn? s_RigidbodySetPosition;
    private static ComponentNativeAPI.Funcs.GetQuaternionVoidFn? s_RigidbodyGetRotation;
    private static ComponentNativeAPI.Funcs.SetQuaternionVoidFn? s_RigidbodySetRotation;
    private static ComponentNativeAPI.Funcs.GetVector3VoidFn? s_RigidbodyGetCenterOfMass;
    private static CoreNativeAPI.Funcs.GetBoolFn? s_RigidbodyIsActive;
    private static ComponentNativeAPI.Funcs.RigidbodyApplyForceFn? s_RigidbodyApplyForce;
    private static ComponentNativeAPI.Funcs.RigidbodyApplyForceToCenterFn? s_RigidbodyApplyForceToCenter;
    private static ComponentNativeAPI.Funcs.RigidbodyApplyTorqueFn? s_RigidbodyApplyTorque;
    private static ComponentNativeAPI.Funcs.RigidbodyApplyLinearImpulseFn? s_RigidbodyApplyLinearImpulse;
    private static ComponentNativeAPI.Funcs.RigidbodyApplyLinearImpulseToCenterFn? s_RigidbodyApplyLinearImpulseToCenter;
    private static ComponentNativeAPI.Funcs.RigidbodyApplyAngularImpulseFn? s_RigidbodyApplyAngularImpulse;
    private static ComponentNativeAPI.Funcs.RigidbodyActionFn? s_RigidbodyActivate;
    private static ComponentNativeAPI.Funcs.RigidbodyActionFn? s_RigidbodyDeactivate;
    private static ComponentNativeAPI.Funcs.RigidbodyMoveKinematicFn? s_RigidbodyMoveKinematic;
    private static CoreNativeAPI.Funcs.SetFloatFn? s_RigidbodySetMass;
    private static CoreNativeAPI.Funcs.GetFloatFn? s_RigidbodyGetLinearDamping;
    private static CoreNativeAPI.Funcs.SetFloatFn? s_RigidbodySetLinearDamping;
    private static CoreNativeAPI.Funcs.GetFloatFn? s_RigidbodyGetAngularDamping;
    private static CoreNativeAPI.Funcs.SetFloatFn? s_RigidbodySetAngularDamping;
    private static CoreNativeAPI.Funcs.GetFloatFn? s_RigidbodyGetFriction;
    private static CoreNativeAPI.Funcs.SetFloatFn? s_RigidbodySetFriction;
    private static CoreNativeAPI.Funcs.GetFloatFn? s_RigidbodyGetRestitution;
    private static CoreNativeAPI.Funcs.SetFloatFn? s_RigidbodySetRestitution;
    private static CoreNativeAPI.Funcs.GetFloatFn? s_RigidbodyGetMaxLinearVelocity;
    private static CoreNativeAPI.Funcs.SetFloatFn? s_RigidbodySetMaxLinearVelocity;
    private static CoreNativeAPI.Funcs.GetFloatFn? s_RigidbodyGetMaxAngularVelocity;
    private static CoreNativeAPI.Funcs.SetFloatFn? s_RigidbodySetMaxAngularVelocity;
    private static CoreNativeAPI.Funcs.GetBoolFn? s_RigidbodyGetApplyGyroscopicForce;
    private static CoreNativeAPI.Funcs.SetBoolFn? s_RigidbodySetApplyGyroscopicForce;
    private static ComponentNativeAPI.Funcs.SetVector3VoidFn? s_RigidbodySetAngularVelocity;

    // BoxColliderComponent
    private static ComponentNativeAPI.Funcs.GetVector3VoidFn? s_BoxColliderGetCenter;
    private static ComponentNativeAPI.Funcs.SetVector3VoidFn? s_BoxColliderSetCenter;
    private static ComponentNativeAPI.Funcs.GetVector3VoidFn? s_BoxColliderGetScale;
    private static ComponentNativeAPI.Funcs.SetVector3VoidFn? s_BoxColliderSetScale;

    // SphereColliderComponent
    private static ComponentNativeAPI.Funcs.GetVector3VoidFn? s_SphereColliderGetCenter;
    private static ComponentNativeAPI.Funcs.SetVector3VoidFn? s_SphereColliderSetCenter;
    private static CoreNativeAPI.Funcs.GetFloatFn? s_SphereColliderGetRadius;
    private static CoreNativeAPI.Funcs.SetFloatFn? s_SphereColliderSetRadius;

    // CapsuleColliderComponent
    private static ComponentNativeAPI.Funcs.GetVector3VoidFn? s_CapsuleColliderGetCenter;
    private static ComponentNativeAPI.Funcs.SetVector3VoidFn? s_CapsuleColliderSetCenter;
    private static CoreNativeAPI.Funcs.GetFloatFn? s_CapsuleColliderGetRadius;
    private static CoreNativeAPI.Funcs.SetFloatFn? s_CapsuleColliderSetRadius;
    private static CoreNativeAPI.Funcs.GetFloatFn? s_CapsuleColliderGetHeight;
    private static CoreNativeAPI.Funcs.SetFloatFn? s_CapsuleColliderSetHeight;

    public static void Initialize(ulong apiPtr)
    {
        if (apiPtr == 0)
            throw new ArgumentException("Invalid internal calls API pointer", nameof(apiPtr));

        ComponentNativeAPI.API api = Marshal.PtrToStructure<ComponentNativeAPI.API>((IntPtr)apiPtr);
        s_SceneGetScreenToWorldRay = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.SceneGetScreenToWorldRayFn>(api.Scene_GetScreenToWorldRay);
        s_SceneRaycast = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.SceneRaycastFn>(api.Scene_Raycast);
        s_ScenePhysicsRaycast = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.ScenePhysicsRaycastFn>(api.Scene_PhysicsRaycast);
        s_SceneGetPrimaryCamera = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.SceneGetPrimaryCameraFn>(api.Scene_GetPrimaryCamera);
        s_EntityHasComponent = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.EntityHasComponentFn>(api.Entity_HasComponent);
        s_EntityAddComponent = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.EntityAddComponentFn>(api.Entity_AddComponent);
        s_EntityFindEntityByName = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.EntityFindEntityByNameFn>(api.Entity_FindEntityByName);
        s_EntityFindChildEntityByName = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.EntityFindChildEntityByNameFn>(api.Entity_FindChildEntityByName);
        s_EntityIsParent = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.EntityIsParentFn>(api.Entity_IsParent);
        s_EntityGetParent = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.EntityGetParentFn>(api.Entity_GetParent);
        s_EntityInstantiateWithName = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.EntityInstantiateWithNameFn>(api.Entity_InstantiateWithName);
        s_EntityInstantiate = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.EntityInstantiateFn>(api.Entity_Instantiate);
        s_EntityDestroy = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.EntityDestroyFn>(api.Entity_Destroy);
        s_EntitySetVisibility = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.EntitySetVisibilityFn>(api.Entity_SetVisibility);
        s_EntityGetVisibility = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.EntityGetVisibilityFn>(api.Entity_GetVisibility);
        s_EntityGetName = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.EntityGetNameFn>(api.Entity_GetName);
        s_WidgetComponentHasButton = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.WidgetComponentHasButtonFn>(api.WidgetComponent_HasButton);
        s_WidgetComponentAddButtonEventCallback = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.WidgetComponentButtonEventFn>(api.WidgetComponent_AddButtonEventCallback);
        s_WidgetComponentHasLabel = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.WidgetComponentHasNamedItemFn>(api.WidgetComponent_HasLabel);
        s_WidgetComponentGetLabelText = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.WidgetComponentGetStringByNameFn>(api.WidgetComponent_GetLabelText);
        s_WidgetComponentSetLabelText = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.WidgetComponentSetStringByNameFn>(api.WidgetComponent_SetLabelText);
        s_WidgetComponentGetLabelColor = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.WidgetComponentGetVec4ByNameFn>(api.WidgetComponent_GetLabelColor);
        s_WidgetComponentSetLabelColor = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.WidgetComponentSetVec4ByNameFn>(api.WidgetComponent_SetLabelColor);
        s_WidgetComponentGetLabelFontSize = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.WidgetComponentGetFloatByNameFn>(api.WidgetComponent_GetLabelFontSize);
        s_WidgetComponentSetLabelFontSize = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.WidgetComponentSetFloatByNameFn>(api.WidgetComponent_SetLabelFontSize);
        s_WidgetComponentHasImage = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.WidgetComponentHasNamedItemFn>(api.WidgetComponent_HasImage);
        s_WidgetComponentGetImageHandle = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.WidgetComponentGetU64ByNameFn>(api.WidgetComponent_GetImageHandle);
        s_WidgetComponentSetImageHandle = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.WidgetComponentSetU64ByNameFn>(api.WidgetComponent_SetImageHandle); s_WidgetComponentRemoveButtonEventCallback = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.WidgetComponentButtonEventFn>(api.WidgetComponent_RemoveButtonEventCallback);
        s_AudioSourceHasAudio = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.AudioSourceHasAudioFn>(api.AudioSourceComponent_HasAudio);
        s_AudioSourcePlay = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.AudioSourceActionFn>(api.AudioSourceComponent_Play);
        s_AudioSourceStop = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.AudioSourceActionFn>(api.AudioSourceComponent_Stop);
        s_AudioSourcePause = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.AudioSourceActionFn>(api.AudioSourceComponent_Pause);
        s_AudioSourceResume = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.AudioSourceActionFn>(api.AudioSourceComponent_Resume);
        s_AudioSourceGetVolume = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.GetFloatFn>(api.AudioSourceComponent_GetVolume);
        s_AudioSourceSetVolume = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.SetFloatFn>(api.AudioSourceComponent_SetVolume);
        s_AudioSourceGetPitch = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.GetFloatFn>(api.AudioSourceComponent_GetPitch);
        s_AudioSourceSetPitch = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.SetFloatFn>(api.AudioSourceComponent_SetPitch);
        s_AudioSourceGetPan = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.GetFloatFn>(api.AudioSourceComponent_GetPan);
        s_AudioSourceSetPan = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.SetFloatFn>(api.AudioSourceComponent_SetPan);
        s_AudioSourceGetPlayOnStart = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.GetBoolFn>(api.AudioSourceComponent_GetPlayOnStart);
        s_AudioSourceSetPlayOnStart = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.SetBoolFn>(api.AudioSourceComponent_SetPlayOnStart);
        s_AudioSourceGetLoop = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.GetBoolFn>(api.AudioSourceComponent_GetLoop);
        s_AudioSourceSetLoop = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.SetBoolFn>(api.AudioSourceComponent_SetLoop);
        s_AudioSourceAddReverbDsp = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.AudioSourceAddReverbDspFn>(api.AudioSourceComponent_AddReverbDSP);
        s_AudioSourceAddDistortionDsp = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.AudioSourceAddDistortionDspFn>(api.AudioSourceComponent_AddDistortionDSP);
        s_AudioSourceAddChorusDsp = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.AudioSourceAddChorusDspFn>(api.AudioSourceComponent_AddChorusDSP);
        s_AudioSourceAddCompressorDsp = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.AudioSourceAddCompressorDspFn>(api.AudioSourceComponent_AddCompressorDSP);
        s_AudioSourceAddDelayDsp = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.AudioSourceAddDelayDspFn>(api.AudioSourceComponent_AddDelayDSP);
        s_AudioSourceClearDsps = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.AudioSourceActionFn>(api.AudioSourceComponent_ClearDSPs);

        s_TransformGetForward = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.GetVector3Fn>(api.TransformComponent_GetForward);
        s_TransformSetForward = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.SetVector3Fn>(api.TransformComponent_SetForward);
        s_TransformGetRight = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.GetVector3Fn>(api.TransformComponent_GetRight);
        s_TransformSetRight = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.SetVector3Fn>(api.TransformComponent_SetRight);
        s_TransformGetUp = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.GetVector3Fn>(api.TransformComponent_GetUp);
        s_TransformSetUp = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.SetVector3Fn>(api.TransformComponent_SetUp);
        s_TransformGetTranslation = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.GetVector3Fn>(api.TransformComponent_GetTranslation);
        s_TransformSetTranslation = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.SetVector3Fn>(api.TransformComponent_SetTranslation);
        s_TransformGetRotation = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.GetQuaternionFn>(api.TransformComponent_GetRotation);
        s_TransformSetRotation = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.SetQuaternionFn>(api.TransformComponent_SetRotation);
        s_TransformGetEulerAngles = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.GetVector3Fn>(api.TransformComponent_GetEulerAngles);
        s_TransformSetEulerAngles = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.SetVector3Fn>(api.TransformComponent_SetEulerAngles);
        s_TransformGetScale = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.GetVector3Fn>(api.TransformComponent_GetScale);
        s_TransformSetScale = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.SetVector3Fn>(api.TransformComponent_SetScale);

        s_Sprite2DSetColor = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.SetVector4Fn>(api.Sprite2DComponent_SetColor);
        s_Sprite2DGetColor = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.GetVector4Fn>(api.Sprite2DComponent_GetColor);
        s_Sprite2DSetTilingFactor = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.SetVector2Fn>(api.Sprite2DComponent_SetTilingFactor);
        s_Sprite2DGetTilingFactor = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.GetVector2Fn>(api.Sprite2DComponent_GetTilingFactor);

        s_Circle2DSetColor = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.SetVector4Fn>(api.Circle2DComponent_SetColor);
        s_Circle2DGetColor = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.GetVector4Fn>(api.Circle2DComponent_GetColor);

        s_Rigidbody2DGetType = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.GetIntFn>(api.Rigidbody2DComponent_GetType);
        s_Rigidbody2DSetType = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.SetIntFn>(api.Rigidbody2DComponent_SetType);
        s_Rigidbody2DGetLinearVelocity = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.GetVector2Fn>(api.Rigidbody2DComponent_GetLinearVelocity);
        s_Rigidbody2DSetLinearVelocity = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.SetVector2Fn>(api.Rigidbody2DComponent_SetLinearVelocity);
        s_Rigidbody2DGetAngularVelocity = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.GetFloatFn>(api.Rigidbody2DComponent_GetAngularVelocity);
        s_Rigidbody2DSetAngularVelocity = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.SetFloatFn>(api.Rigidbody2DComponent_SetAngularVelocity);
        s_Rigidbody2DGetGravityScale = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.GetFloatFn>(api.Rigidbody2DComponent_GetGravityScale);
        s_Rigidbody2DSetGravityScale = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.SetFloatFn>(api.Rigidbody2DComponent_SetGravityScale);
        s_Rigidbody2DGetLinearDamping = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.GetFloatFn>(api.Rigidbody2DComponent_GetLinearDamping);
        s_Rigidbody2DSetLinearDamping = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.SetFloatFn>(api.Rigidbody2DComponent_SetLinearDamping);
        s_Rigidbody2DGetAngularDamping = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.GetFloatFn>(api.Rigidbody2DComponent_GetAngularDamping);
        s_Rigidbody2DSetAngularDamping = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.SetFloatFn>(api.Rigidbody2DComponent_SetAngularDamping);
        s_Rigidbody2DGetIsAwake = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.GetBoolFn>(api.Rigidbody2DComponent_GetIsAwake);
        s_Rigidbody2DSetIsAwake = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.SetBoolFn>(api.Rigidbody2DComponent_SetIsAwake);
        s_Rigidbody2DGetIsEnabled = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.GetBoolFn>(api.Rigidbody2DComponent_GetIsEnabled);
        s_Rigidbody2DSetIsEnabled = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.SetBoolFn>(api.Rigidbody2DComponent_SetIsEnabled);
        s_Rigidbody2DGetIsEnableSleep = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.GetBoolFn>(api.Rigidbody2DComponent_GetIsEnableSleep);
        s_Rigidbody2DSetIsEnableSleep = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.SetBoolFn>(api.Rigidbody2DComponent_SetIsEnableSleep);
        s_Rigidbody2DApplyForce = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.Rigidbody2DApplyForceFn>(api.Rigidbody2DComponent_ApplyForce);
        s_Rigidbody2DApplyForceToCenter = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.Rigidbody2DApplyForceToCenterFn>(api.Rigidbody2DComponent_ApplyForceToCenter);
        s_Rigidbody2DApplyLinearImpulse = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.Rigidbody2DApplyLinearImpulseFn>(api.Rigidbody2DComponent_ApplyLinearImpulse);
        s_Rigidbody2DApplyLinearImpulseToCenter = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.Rigidbody2DApplyLinearImpulseToCenterFn>(api.Rigidbody2DComponent_ApplyLinearImpulseToCenter);
        s_Rigidbody2DApplyAngularImpulse = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.Rigidbody2DApplyAngularImpulseFn>(api.Rigidbody2DComponent_ApplyAngularImpulse);
        s_Rigidbody2DApplyTorque = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.Rigidbody2DApplyTorqueFn>(api.Rigidbody2DComponent_ApplyTorque);
        s_Rigidbody2DGetMass = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.GetFloatFn>(api.Rigidbody2DComponent_GetMass);
        s_Rigidbody2DGetIsBullet = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.GetBoolFn>(api.Rigidbody2DComponent_GetIsBullet);
        s_Rigidbody2DSetIsBullet = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.SetBoolFn>(api.Rigidbody2DComponent_SetIsBullet);

        s_BoxCollider2DGetSize = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.GetVector2Fn>(api.BoxCollider2DComponent_GetSize);
        s_BoxCollider2DSetSize = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.SetVector2Fn>(api.BoxCollider2DComponent_SetSize);
        s_BoxCollider2DGetOffset = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.GetVector2Fn>(api.BoxCollider2DComponent_GetOffset);
        s_BoxCollider2DSetOffset = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.SetVector2Fn>(api.BoxCollider2DComponent_SetOffset);
        s_BoxCollider2DGetRestitution = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.GetFloatFn>(api.BoxCollider2DComponent_GetRestitution);
        s_BoxCollider2DSetRestitution = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.SetFloatFn>(api.BoxCollider2DComponent_SetRestitution);
        s_BoxCollider2DGetFriction = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.GetFloatFn>(api.BoxCollider2DComponent_GetFriction);
        s_BoxCollider2DSetFriction = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.SetFloatFn>(api.BoxCollider2DComponent_SetFriction);
        s_BoxCollider2DGetDensity = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.GetFloatFn>(api.BoxCollider2DComponent_GetDensity);
        s_BoxCollider2DSetDensity = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.SetFloatFn>(api.BoxCollider2DComponent_SetDensity);
        s_BoxCollider2DGetIsSensor = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.GetBoolFn>(api.BoxCollider2DComponent_GetIsSensor);
        s_BoxCollider2DSetIsSensor = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.SetBoolFn>(api.BoxCollider2DComponent_SetIsSensor);

        s_CircleCollider2DGetCenter = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.GetVector2Fn>(api.CircleCollider2DComponent_GetCenter);
        s_CircleCollider2DSetCenter = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.SetVector2Fn>(api.CircleCollider2DComponent_SetCenter);
        s_CircleCollider2DGetRadius = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.GetFloatFn>(api.CircleCollider2DComponent_GetRadius);
        s_CircleCollider2DSetRadius = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.SetFloatFn>(api.CircleCollider2DComponent_SetRadius);
        s_CircleCollider2DGetRestitution = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.GetFloatFn>(api.CircleCollider2DComponent_GetRestitution);
        s_CircleCollider2DSetRestitution = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.SetFloatFn>(api.CircleCollider2DComponent_SetRestitution);
        s_CircleCollider2DGetFriction = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.GetFloatFn>(api.CircleCollider2DComponent_GetFriction);
        s_CircleCollider2DSetFriction = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.SetFloatFn>(api.CircleCollider2DComponent_SetFriction);
        s_CircleCollider2DGetDensity = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.GetFloatFn>(api.CircleCollider2DComponent_GetDensity);
        s_CircleCollider2DSetDensity = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.SetFloatFn>(api.CircleCollider2DComponent_SetDensity);
        s_CircleCollider2DGetIsSensor = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.GetBoolFn>(api.CircleCollider2DComponent_GetIsSensor);
        s_CircleCollider2DSetIsSensor = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.SetBoolFn>(api.CircleCollider2DComponent_SetIsSensor);

        s_TextComponentSetText = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.SetStringFn>(api.TextComponent_SetText);
        s_TextComponentGetText = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.GetStringFn>(api.TextComponent_GetText);
        s_TextComponentSetColor = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.SetVector4Fn>(api.TextComponent_SetColor);
        s_TextComponentGetColor = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.GetVector4Fn>(api.TextComponent_GetColor);
        s_TextComponentSetKerning = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.SetFloatFn>(api.TextComponent_SetKerning);
        s_TextComponentGetKerning = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.GetFloatFn>(api.TextComponent_GetKerning);
        s_TextComponentSetLineSpacing = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.SetFloatFn>(api.TextComponent_SetLineSpacing);
        s_TextComponentGetLineSpacing = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.GetFloatFn>(api.TextComponent_GetLineSpacing);

        // RigidbodyComponent (3D)
        s_RigidbodyGetType = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.GetIntFn>(api.RigidbodyComponent_GetType);
        s_RigidbodySetType = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.SetIntFn>(api.RigidbodyComponent_SetType);
        s_RigidbodyGetMotionQuality = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.GetIntFn>(api.RigidbodyComponent_GetMotionQuality);
        s_RigidbodyGetUseGravity = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.GetBoolFn>(api.RigidbodyComponent_GetUseGravity);
        s_RigidbodySetUseGravity = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.SetBoolFn>(api.RigidbodyComponent_SetUseGravity);
        s_RigidbodyGetMass = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.GetFloatFn>(api.RigidbodyComponent_GetMass);
        s_RigidbodyGetGravityFactor = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.GetFloatFn>(api.RigidbodyComponent_GetGravityFactor);
        s_RigidbodySetGravityFactor = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.SetFloatFn>(api.RigidbodyComponent_SetGravityFactor);
        s_RigidbodyGetLinearVelocity = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.GetVector3VoidFn>(api.RigidbodyComponent_GetLinearVelocity);
        s_RigidbodySetLinearVelocity = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.SetVector3VoidFn>(api.RigidbodyComponent_SetLinearVelocity);
        s_RigidbodyGetAngularVelocity = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.GetVector3VoidFn>(api.RigidbodyComponent_GetAngularVelocity);
        s_RigidbodyGetPosition = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.GetVector3VoidFn>(api.RigidbodyComponent_GetPosition);
        s_RigidbodySetPosition = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.SetVector3VoidFn>(api.RigidbodyComponent_SetPosition);
        s_RigidbodyGetRotation = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.GetQuaternionVoidFn>(api.RigidbodyComponent_GetRotation);
        s_RigidbodySetRotation = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.SetQuaternionVoidFn>(api.RigidbodyComponent_SetRotation);
        s_RigidbodyGetCenterOfMass = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.GetVector3VoidFn>(api.RigidbodyComponent_GetCenterOfMass);
        s_RigidbodyIsActive = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.GetBoolFn>(api.RigidbodyComponent_IsActive);
        s_RigidbodyApplyForce = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.RigidbodyApplyForceFn>(api.RigidbodyComponent_ApplyForce);
        s_RigidbodyApplyForceToCenter = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.RigidbodyApplyForceToCenterFn>(api.RigidbodyComponent_ApplyForceToCenter);
        s_RigidbodyApplyTorque = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.RigidbodyApplyTorqueFn>(api.RigidbodyComponent_ApplyTorque);
        s_RigidbodyApplyLinearImpulse = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.RigidbodyApplyLinearImpulseFn>(api.RigidbodyComponent_ApplyLinearImpulse);
        s_RigidbodyApplyLinearImpulseToCenter = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.RigidbodyApplyLinearImpulseToCenterFn>(api.RigidbodyComponent_ApplyLinearImpulseToCenter);
        s_RigidbodyApplyAngularImpulse = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.RigidbodyApplyAngularImpulseFn>(api.RigidbodyComponent_ApplyAngularImpulse);
        s_RigidbodyActivate = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.RigidbodyActionFn>(api.RigidbodyComponent_Activate);
        s_RigidbodyDeactivate = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.RigidbodyActionFn>(api.RigidbodyComponent_Deactivate);
        s_RigidbodyMoveKinematic = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.RigidbodyMoveKinematicFn>(api.RigidbodyComponent_MoveKinematic);
        s_RigidbodySetMass = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.SetFloatFn>(api.RigidbodyComponent_SetMass);
        s_RigidbodyGetLinearDamping = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.GetFloatFn>(api.RigidbodyComponent_GetLinearDamping);
        s_RigidbodySetLinearDamping = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.SetFloatFn>(api.RigidbodyComponent_SetLinearDamping);
        s_RigidbodyGetAngularDamping = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.GetFloatFn>(api.RigidbodyComponent_GetAngularDamping);
        s_RigidbodySetAngularDamping = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.SetFloatFn>(api.RigidbodyComponent_SetAngularDamping);
        s_RigidbodyGetFriction = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.GetFloatFn>(api.RigidbodyComponent_GetFriction);
        s_RigidbodySetFriction = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.SetFloatFn>(api.RigidbodyComponent_SetFriction);
        s_RigidbodyGetRestitution = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.GetFloatFn>(api.RigidbodyComponent_GetRestitution);
        s_RigidbodySetRestitution = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.SetFloatFn>(api.RigidbodyComponent_SetRestitution);
        s_RigidbodyGetMaxLinearVelocity = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.GetFloatFn>(api.RigidbodyComponent_GetMaxLinearVelocity);
        s_RigidbodySetMaxLinearVelocity = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.SetFloatFn>(api.RigidbodyComponent_SetMaxLinearVelocity);
        s_RigidbodyGetMaxAngularVelocity = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.GetFloatFn>(api.RigidbodyComponent_GetMaxAngularVelocity);
        s_RigidbodySetMaxAngularVelocity = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.SetFloatFn>(api.RigidbodyComponent_SetMaxAngularVelocity);
        s_RigidbodyGetApplyGyroscopicForce = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.GetBoolFn>(api.RigidbodyComponent_GetApplyGyroscopicForce);
        s_RigidbodySetApplyGyroscopicForce = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.SetBoolFn>(api.RigidbodyComponent_SetApplyGyroscopicForce);
        s_RigidbodySetAngularVelocity = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.SetVector3VoidFn>(api.RigidbodyComponent_SetAngularVelocity);

        // BoxColliderComponent
        s_BoxColliderGetCenter = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.GetVector3VoidFn>(api.BoxColliderComponent_GetCenter);
        s_BoxColliderSetCenter = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.SetVector3VoidFn>(api.BoxColliderComponent_SetCenter);
        s_BoxColliderGetScale = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.GetVector3VoidFn>(api.BoxColliderComponent_GetScale);
        s_BoxColliderSetScale = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.SetVector3VoidFn>(api.BoxColliderComponent_SetScale);

        // SphereColliderComponent
        s_SphereColliderGetCenter = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.GetVector3VoidFn>(api.SphereColliderComponent_GetCenter);
        s_SphereColliderSetCenter = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.SetVector3VoidFn>(api.SphereColliderComponent_SetCenter);
        s_SphereColliderGetRadius = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.GetFloatFn>(api.SphereColliderComponent_GetRadius);
        s_SphereColliderSetRadius = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.SetFloatFn>(api.SphereColliderComponent_SetRadius);

        // CapsuleColliderComponent
        s_CapsuleColliderGetCenter = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.GetVector3VoidFn>(api.CapsuleColliderComponent_GetCenter);
        s_CapsuleColliderSetCenter = Marshal.GetDelegateForFunctionPointer<ComponentNativeAPI.Funcs.SetVector3VoidFn>(api.CapsuleColliderComponent_SetCenter);
        s_CapsuleColliderGetRadius = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.GetFloatFn>(api.CapsuleColliderComponent_GetRadius);
        s_CapsuleColliderSetRadius = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.SetFloatFn>(api.CapsuleColliderComponent_SetRadius);
        s_CapsuleColliderGetHeight = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.GetFloatFn>(api.CapsuleColliderComponent_GetHeight);
        s_CapsuleColliderSetHeight = Marshal.GetDelegateForFunctionPointer<CoreNativeAPI.Funcs.SetFloatFn>(api.CapsuleColliderComponent_SetHeight);

        s_Initialized = true;
    }

    private static void EnsureInitialized()
    {
        if (!s_Initialized)
            throw new InvalidOperationException("InternalCalls bridge is not initialized.");
    }

    internal static void Scene_GetScreenToWorldRay(float x, float y, out Mathf.Vector3 outOrigin, out Mathf.Vector3 outDirection)
    {
        EnsureInitialized();
        s_SceneGetScreenToWorldRay!(x, y, out NativeObject.Vector3 origin, out NativeObject.Vector3 dir);
        outOrigin = NativeObject.ToManaged(origin);
        outDirection = NativeObject.ToManaged(dir);
    }

    internal static ulong Scene_Raycast(Mathf.Vector3 origin, Mathf.Vector3 direction)
    {
        EnsureInitialized();
        NativeObject.Vector3 nOrigin = NativeObject.ToNative(origin);
        NativeObject.Vector3 nDirection = NativeObject.ToNative(direction);
        return s_SceneRaycast!(ref nOrigin, ref nDirection);
    }

    internal static unsafe ulong Scene_PhysicsRaycast(Mathf.Vector3 origin, Mathf.Vector3 direction, float maxDistance,
                                                        out Mathf.Vector3 hitPoint, out Mathf.Vector3 hitNormal)
    {
        EnsureInitialized();
        NativeObject.Vector3 nOrigin = NativeObject.ToNative(origin);
        NativeObject.Vector3 nDirection = NativeObject.ToNative(direction);
        NativeObject.Vector3 nHitPoint  = default;
        NativeObject.Vector3 nHitNormal = default;
        ulong id = s_ScenePhysicsRaycast!(
            ref nOrigin, ref nDirection,
            maxDistance, &nHitPoint, &nHitNormal);
        hitPoint  = NativeObject.ToManaged(nHitPoint);
        hitNormal = NativeObject.ToManaged(nHitNormal);
        return id;
    }

    internal static ulong Scene_GetPrimaryCamera()
    {
        EnsureInitialized();
        return s_SceneGetPrimaryCamera!();
    }

    internal static bool Entity_HasComponent(ulong entityID, Type componentType)
    {
        EnsureInitialized();

        string typeName = componentType?.FullName ?? componentType?.Name ?? string.Empty;
        IntPtr ptr = NativeObject.StringToUtf8(typeName);
        try
        {
            return s_EntityHasComponent!(entityID, ptr);
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
        IntPtr ptr = NativeObject.StringToUtf8(typeName);
        try
        {
            s_EntityAddComponent!(entityID, ptr);
        }
        finally
        {
            Marshal.FreeCoTaskMem(ptr);
        }
    }

    internal static ulong Entity_FindEntity(string name)
    {
        EnsureInitialized();

        IntPtr ptr = NativeObject.StringToUtf8(name);
        try
        {
            return s_EntityFindEntityByName!(ptr);
        }
        finally
        {
            Marshal.FreeCoTaskMem(ptr);
        }
    }

    internal static ulong Entity_FindChildEntity(ulong entityID, string childName)
    {
        EnsureInitialized();

        IntPtr ptr = NativeObject.StringToUtf8(childName);
        try
        {
            return s_EntityFindChildEntityByName!(entityID, ptr);
        }
        finally
        {
            Marshal.FreeCoTaskMem(ptr);
        }
    }

    internal static bool Entity_IsParent(ulong entityID, ulong parentEntityID)
    {
        EnsureInitialized();
        return s_EntityIsParent!(entityID, parentEntityID);
    }

    internal static ulong Entity_GetParent(ulong entityID)
    {
        EnsureInitialized();
        return s_EntityGetParent!(entityID);
    }

    internal static ulong Entity_Instantiate(string name, Mathf.Vector3 value)
    {
        EnsureInitialized();
        IntPtr ptr = NativeObject.StringToUtf8(name);
        try
        {
            NativeObject.Vector3 nativeVal = NativeObject.ToNative(value);
            return s_EntityInstantiateWithName!(ptr, ref nativeVal);
        }
        finally
        {
            Marshal.FreeCoTaskMem(ptr);
        }
    }

    internal static ulong Entity_Instantiate(ulong entityID, Mathf.Vector3 value)
    {
        EnsureInitialized();
        NativeObject.Vector3 nativeVal = NativeObject.ToNative(value);
        return s_EntityInstantiate!(entityID, ref nativeVal);
    }

    internal static void Entity_Destroy(ulong entityID)
    {
        EnsureInitialized();
        s_EntityDestroy!(entityID);
    }

    internal static void Entity_SetVisibility(ulong entityID, bool value)
    {
        EnsureInitialized();
        s_EntitySetVisibility!(entityID, value);
    }

    internal static void Entity_GetVisibility(ulong entityID, out bool result)
    {
        EnsureInitialized();
        s_EntityGetVisibility!(entityID, out result);
    }

    internal static string Entity_GetName(ulong entityID)
    {
        EnsureInitialized();
        IntPtr namePtr = s_EntityGetName!(entityID);
        return NativeObject.Utf8ToString(namePtr) ?? string.Empty;
    }

    internal static bool WidgetComponent_HasButton(ulong entityID, string buttonName)
    {
        EnsureInitialized();

        IntPtr buttonNamePtr = NativeObject.StringToUtf8(buttonName);
        try
        {
            return s_WidgetComponentHasButton!(entityID, buttonNamePtr);
        }
        finally
        {
            Marshal.FreeCoTaskMem(buttonNamePtr);
        }
    }

    internal static bool WidgetComponent_AddButtonEventCallback(ulong entityID, string buttonName, int eventType, string methodName)
    {
        EnsureInitialized();

        IntPtr buttonNamePtr = NativeObject.StringToUtf8(buttonName);
        IntPtr methodNamePtr = NativeObject.StringToUtf8(methodName);
        try
        {
            return s_WidgetComponentAddButtonEventCallback!(entityID, buttonNamePtr, eventType, methodNamePtr);
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

        IntPtr buttonNamePtr = NativeObject.StringToUtf8(buttonName);
        IntPtr methodNamePtr = NativeObject.StringToUtf8(methodName);
        try
        {
            return s_WidgetComponentRemoveButtonEventCallback!(entityID, buttonNamePtr, eventType, methodNamePtr);
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
        return s_AudioSourceHasAudio!(entityID);
    }

    internal static void AudioSourceComponent_Play(ulong entityID)
    {
        EnsureInitialized();
        s_AudioSourcePlay!(entityID);
    }

    internal static void AudioSourceComponent_Stop(ulong entityID)
    {
        EnsureInitialized();
        s_AudioSourceStop!(entityID);
    }

    internal static void AudioSourceComponent_Pause(ulong entityID)
    {
        EnsureInitialized();
        s_AudioSourcePause!(entityID);
    }

    internal static void AudioSourceComponent_Resume(ulong entityID)
    {
        EnsureInitialized();
        s_AudioSourceResume!(entityID);
    }

    internal static void AudioSourceComponent_GetVolume(ulong entityID, out float result)
    {
        EnsureInitialized();
        s_AudioSourceGetVolume!(entityID, out result);
    }

    internal static void AudioSourceComponent_SetVolume(ulong entityID, float value)
    {
        EnsureInitialized();
        s_AudioSourceSetVolume!(entityID, value);
    }

    internal static void AudioSourceComponent_GetPitch(ulong entityID, out float result)
    {
        EnsureInitialized();
        s_AudioSourceGetPitch!(entityID, out result);
    }

    internal static void AudioSourceComponent_SetPitch(ulong entityID, float value)
    {
        EnsureInitialized();
        s_AudioSourceSetPitch!(entityID, value);
    }

    internal static void AudioSourceComponent_GetPan(ulong entityID, out float result)
    {
        EnsureInitialized();
        s_AudioSourceGetPan!(entityID, out result);
    }

    internal static void AudioSourceComponent_SetPan(ulong entityID, float value)
    {
        EnsureInitialized();
        s_AudioSourceSetPan!(entityID, value);
    }

    internal static void AudioSourceComponent_GetPlayOnStart(ulong entityID, out bool result)
    {
        EnsureInitialized();
        s_AudioSourceGetPlayOnStart!(entityID, out result);
    }

    internal static void AudioSourceComponent_SetPlayOnStart(ulong entityID, bool value)
    {
        EnsureInitialized();
        s_AudioSourceSetPlayOnStart!(entityID, value);
    }

    internal static void AudioSourceComponent_GetLoop(ulong entityID, out bool result)
    {
        EnsureInitialized();
        s_AudioSourceGetLoop!(entityID, out result);
    }

    internal static void AudioSourceComponent_SetLoop(ulong entityID, bool value)
    {
        EnsureInitialized();
        s_AudioSourceSetLoop!(entityID, value);
    }

    internal static bool AudioSourceComponent_AddReverbDSP(ulong entityID, float decayTime, float earlyDelay, float lateDelay, float highFrequencyReference, float diffusion, float density, float lowShelfGain, float highCut, float dryLevel, float wetLevel)
    {
        EnsureInitialized();
        return s_AudioSourceAddReverbDsp!(entityID, decayTime, earlyDelay, lateDelay, highFrequencyReference, diffusion, density, lowShelfGain, highCut, dryLevel, wetLevel);
    }

    internal static bool AudioSourceComponent_AddDistortionDSP(ulong entityID, float distortionLevel)
    {
        EnsureInitialized();
        return s_AudioSourceAddDistortionDsp!(entityID, distortionLevel);
    }

    internal static bool AudioSourceComponent_AddChorusDSP(ulong entityID, float mix, float rate, float depth)
    {
        EnsureInitialized();
        return s_AudioSourceAddChorusDsp!(entityID, mix, rate, depth);
    }

    internal static bool AudioSourceComponent_AddCompressorDSP(ulong entityID, float threshold, float ratio, float release, float gainMakeup, bool useSidechain)
    {
        EnsureInitialized();
        return s_AudioSourceAddCompressorDsp!(entityID, threshold, ratio, release, gainMakeup, useSidechain);
    }

    internal static bool AudioSourceComponent_AddDelayDSP(ulong entityID, float delayMs, float feedback)
    {
        EnsureInitialized();
        return s_AudioSourceAddDelayDsp!(entityID, delayMs, feedback);
    }

    internal static void AudioSourceComponent_ClearDSPs(ulong entityID)
    {
        EnsureInitialized();
        s_AudioSourceClearDsps!(entityID);
    }

    internal static object? GetScriptInstance(ulong entityID)
    {
        _ = entityID;
        return null;
    }

    internal static void TransformComponent_GetForward(ulong entityID, out Mathf.Vector3 result)
    {
        EnsureInitialized();
        s_TransformGetForward!(entityID, out NativeObject.Vector3 native);
        result = NativeObject.ToManaged(native);
    }

    internal static void TransformComponent_SetForward(ulong entityID, Mathf.Vector3 result)
    {
        EnsureInitialized();
        NativeObject.Vector3 nativeVal = NativeObject.ToNative(result);
        s_TransformSetForward!(entityID, ref nativeVal);
    }

    internal static void TransformComponent_GetRight(ulong entityID, out Mathf.Vector3 result)
    {
        EnsureInitialized();
        s_TransformGetRight!(entityID, out NativeObject.Vector3 native);
        result = NativeObject.ToManaged(native);
    }

    internal static void TransformComponent_SetRight(ulong entityID, Mathf.Vector3 result)
    {
        EnsureInitialized();
        NativeObject.Vector3 nativeVal = NativeObject.ToNative(result);
        s_TransformSetRight!(entityID, ref nativeVal);
    }

    internal static void TransformComponent_GetUp(ulong entityID, out Mathf.Vector3 result)
    {
        EnsureInitialized();
        s_TransformGetUp!(entityID, out NativeObject.Vector3 native);
        result = NativeObject.ToManaged(native);
    }

    internal static void TransformComponent_SetUp(ulong entityID, Mathf.Vector3 result)
    {
        EnsureInitialized();
        NativeObject.Vector3 nativeVal = NativeObject.ToNative(result);
        s_TransformSetUp!(entityID, ref nativeVal);
    }

    internal static void TransformComponent_GetTranslation(ulong entityID, out Mathf.Vector3 result)
    {
        EnsureInitialized();
        s_TransformGetTranslation!(entityID, out NativeObject.Vector3 native);
        result = NativeObject.ToManaged(native);
    }

    internal static void TransformComponent_SetTranslation(ulong entityID, Mathf.Vector3 value)
    {
        EnsureInitialized();
        NativeObject.Vector3 nativeVal = NativeObject.ToNative(value);
        s_TransformSetTranslation!(entityID, ref nativeVal);
    }

    internal static void TransformComponent_GetRotation(ulong entityID, out Mathf.Quaternion result)
    {
        EnsureInitialized();
        s_TransformGetRotation!(entityID, out NativeObject.Quaternion native);
        result = NativeObject.ToManaged(native);
    }

    internal static void TransformComponent_SetRotation(ulong entityID, Mathf.Quaternion value)
    {
        EnsureInitialized();
        NativeObject.Quaternion nativeVal = NativeObject.ToNative(value);
        s_TransformSetRotation!(entityID, ref nativeVal);
    }

    internal static void TransformComponent_GetEulerAngles(ulong entityID, out Mathf.Vector3 result)
    {
        EnsureInitialized();
        s_TransformGetEulerAngles!(entityID, out NativeObject.Vector3 native);
        result = NativeObject.ToManaged(native);
    }

    internal static void TransformComponent_SetEulerAngles(ulong entityID, Mathf.Vector3 value)
    {
        EnsureInitialized();
        NativeObject.Vector3 nativeVal = NativeObject.ToNative(value);
        s_TransformSetEulerAngles!(entityID, ref nativeVal);
    }

    internal static void TransformComponent_GetScale(ulong entityID, out Mathf.Vector3 result)
    {
        EnsureInitialized();
        s_TransformGetScale!(entityID, out NativeObject.Vector3 native);
        result = NativeObject.ToManaged(native);
    }

    internal static void TransformComponent_SetScale(ulong entityID, Mathf.Vector3 value)
    {
        EnsureInitialized();
        NativeObject.Vector3 nativeVal = NativeObject.ToNative(value);
        s_TransformSetScale!(entityID, ref nativeVal);
    }


    internal static void Sprite2DComponent_SetColor(ulong entityID, Mathf.Vector4 value)
    {
        EnsureInitialized();
        NativeObject.Vector4 nativeVal = NativeObject.ToNative(value);
        s_Sprite2DSetColor!(entityID, ref nativeVal);
    }

    internal static void Sprite2DComponent_GetColor(ulong entityID, out Mathf.Vector4 result)
    {
        EnsureInitialized();
        s_Sprite2DGetColor!(entityID, out NativeObject.Vector4 native);
        result = NativeObject.ToManaged(native);
    }

    internal static void Sprite2DComponent_SetTilingFactor(ulong entityID, Mathf.Vector2 value)
    {
        EnsureInitialized();
        NativeObject.Vector2 nativeVal = NativeObject.ToNative(value);
        s_Sprite2DSetTilingFactor!(entityID, ref nativeVal);
    }

    internal static void Sprite2DComponent_GetTilingFactor(ulong entityID, out Mathf.Vector2 result)
    {
        EnsureInitialized();
        s_Sprite2DGetTilingFactor!(entityID, out NativeObject.Vector2 native);
        result = NativeObject.ToManaged(native);
    }

    internal static void Circle2DComponent_SetColor(ulong entityID, Mathf.Vector4 value)
    {
        EnsureInitialized();
        NativeObject.Vector4 nativeVal = NativeObject.ToNative(value);
        s_Circle2DSetColor!(entityID, ref nativeVal);
    }

    internal static void Circle2DComponent_GetColor(ulong entityID, out Mathf.Vector4 result)
    {
        EnsureInitialized();
        s_Circle2DGetColor!(entityID, out NativeObject.Vector4 native);
        result = NativeObject.ToManaged(native);
    }

    internal static void Rigidbody2DComponent_GetType(ulong entityID, out int result)
    {
        EnsureInitialized();
        s_Rigidbody2DGetType!(entityID, out result);
    }

    internal static void Rigidbody2DComponent_SetType(ulong entityID, int value)
    {
        EnsureInitialized();
        s_Rigidbody2DSetType!(entityID, value);
    }

    internal static void Rigidbody2DComponent_GetLinearVelocity(ulong entityID, out Mathf.Vector2 result)
    {
        EnsureInitialized();
        s_Rigidbody2DGetLinearVelocity!(entityID, out NativeObject.Vector2 native);
        result = NativeObject.ToManaged(native);
    }

    internal static void Rigidbody2DComponent_SetLinearVelocity(ulong entityID, Mathf.Vector2 value)
    {
        EnsureInitialized();
        NativeObject.Vector2 nativeVal = NativeObject.ToNative(value);
        s_Rigidbody2DSetLinearVelocity!(entityID, ref nativeVal);
    }

    internal static void Rigidbody2DComponent_GetAngularVelocity(ulong entityID, out float result)
    {
        EnsureInitialized();
        s_Rigidbody2DGetAngularVelocity!(entityID, out result);
    }

    internal static void Rigidbody2DComponent_SetAngularVelocity(ulong entityID, float value)
    {
        EnsureInitialized();
        s_Rigidbody2DSetAngularVelocity!(entityID, value);
    }

    internal static void Rigidbody2DComponent_GetGravityScale(ulong entityID, out float result)
    {
        EnsureInitialized();
        s_Rigidbody2DGetGravityScale!(entityID, out result);
    }

    internal static void Rigidbody2DComponent_SetGravityScale(ulong entityID, float value)
    {
        EnsureInitialized();
        s_Rigidbody2DSetGravityScale!(entityID, value);
    }

    internal static void Rigidbody2DComponent_GetLinearDamping(ulong entityID, out float result)
    {
        EnsureInitialized();
        s_Rigidbody2DGetLinearDamping!(entityID, out result);
    }

    internal static void Rigidbody2DComponent_SetLinearDamping(ulong entityID, float value)
    {
        EnsureInitialized();
        s_Rigidbody2DSetLinearDamping!(entityID, value);
    }

    internal static void Rigidbody2DComponent_GetAngularDamping(ulong entityID, out float result)
    {
        EnsureInitialized();
        s_Rigidbody2DGetAngularDamping!(entityID, out result);
    }

    internal static void Rigidbody2DComponent_SetAngularDamping(ulong entityID, float value)
    {
        EnsureInitialized();
        s_Rigidbody2DSetAngularDamping!(entityID, value);
    }

    internal static void Rigidbody2DComponent_GetIsAwake(ulong entityID, out bool result)
    {
        EnsureInitialized();
        s_Rigidbody2DGetIsAwake!(entityID, out result);
    }

    internal static void Rigidbody2DComponent_SetIsAwake(ulong entityID, bool value)
    {
        EnsureInitialized();
        s_Rigidbody2DSetIsAwake!(entityID, value);
    }

    internal static void Rigidbody2DComponent_GetIsEnabled(ulong entityID, out bool result)
    {
        EnsureInitialized();
        s_Rigidbody2DGetIsEnabled!(entityID, out result);
    }

    internal static void Rigidbody2DComponent_SetIsEnabled(ulong entityID, bool value)
    {
        EnsureInitialized();
        s_Rigidbody2DSetIsEnabled!(entityID, value);
    }

    internal static void Rigidbody2DComponent_GetIsEnableSleep(ulong entityID, out bool result)
    {
        EnsureInitialized();
        s_Rigidbody2DGetIsEnableSleep!(entityID, out result);
    }

    internal static void Rigidbody2DComponent_SetIsEnableSleep(ulong entityID, bool value)
    {
        EnsureInitialized();
        s_Rigidbody2DSetIsEnableSleep!(entityID, value);
    }

    internal static void Rigidbody2DComponent_ApplyForce(ulong entityID, Mathf.Vector2 force, Mathf.Vector2 point, bool wake)
    {
        EnsureInitialized();
        s_Rigidbody2DApplyForce!(entityID, NativeObject.ToNative(force), NativeObject.ToNative(point), wake);
    }

    internal static void Rigidbody2DComponent_ApplyForceToCenter(ulong entityID, Mathf.Vector2 force, bool wake)
    {
        EnsureInitialized();
        s_Rigidbody2DApplyForceToCenter!(entityID, NativeObject.ToNative(force), wake);
    }

    internal static void Rigidbody2DComponent_ApplyLinearImpulse(ulong entityID, Mathf.Vector2 impulse, Mathf.Vector2 point, bool wake)
    {
        EnsureInitialized();
        s_Rigidbody2DApplyLinearImpulse!(entityID, NativeObject.ToNative(impulse), NativeObject.ToNative(point), wake);
    }

    internal static void Rigidbody2DComponent_ApplyLinearImpulseToCenter(ulong entityID, Mathf.Vector2 impulse, bool wake)
    {
        EnsureInitialized();
        s_Rigidbody2DApplyLinearImpulseToCenter!(entityID, NativeObject.ToNative(impulse), wake);
    }

    internal static void Rigidbody2DComponent_ApplyAngularImpulse(ulong entityID, float impulse, bool wake)
    {
        EnsureInitialized();
        s_Rigidbody2DApplyAngularImpulse!(entityID, impulse, wake);
    }

    internal static void Rigidbody2DComponent_ApplyTorque(ulong entityID, float torque, bool wake)
    {
        EnsureInitialized();
        s_Rigidbody2DApplyTorque!(entityID, torque, wake);
    }

    internal static void Rigidbody2DComponent_GetMass(ulong entityID, out float result)
    {
        EnsureInitialized();
        s_Rigidbody2DGetMass!(entityID, out result);
    }

    internal static void Rigidbody2DComponent_GetIsBullet(ulong entityID, out bool result)
    {
        EnsureInitialized();
        s_Rigidbody2DGetIsBullet!(entityID, out result);
    }

    internal static void Rigidbody2DComponent_SetIsBullet(ulong entityID, bool value)
    {
        EnsureInitialized();
        s_Rigidbody2DSetIsBullet!(entityID, value);
    }

    internal static void BoxCollider2DComponent_GetSize(ulong entityID, out Mathf.Vector2 result)
    {
        EnsureInitialized();
        s_BoxCollider2DGetSize!(entityID, out NativeObject.Vector2 native);
        result = NativeObject.ToManaged(native);
    }

    internal static void BoxCollider2DComponent_SetSize(ulong entityID, Mathf.Vector2 value)
    {
        EnsureInitialized();
        var v = NativeObject.ToNative(value);
        s_BoxCollider2DSetSize!(entityID, ref v);
    }

    internal static void BoxCollider2DComponent_GetOffset(ulong entityID, out Mathf.Vector2 result)
    {
        EnsureInitialized();
        s_BoxCollider2DGetOffset!(entityID, out NativeObject.Vector2 native);
        result = NativeObject.ToManaged(native);
    }

    internal static void BoxCollider2DComponent_SetOffset(ulong entityID, Mathf.Vector2 value)
    {
        EnsureInitialized();
        var v = NativeObject.ToNative(value);
        s_BoxCollider2DSetOffset!(entityID, ref v);
    }

    internal static void BoxCollider2DComponent_GetRestitution(ulong entityID, out float result)
    {
        EnsureInitialized();
        s_BoxCollider2DGetRestitution!(entityID, out result);
    }

    internal static void BoxCollider2DComponent_SetRestitution(ulong entityID, float value)
    {
        EnsureInitialized();
        s_BoxCollider2DSetRestitution!(entityID, value);
    }

    internal static void BoxCollider2DComponent_GetFriction(ulong entityID, out float result)
    {
        EnsureInitialized();
        s_BoxCollider2DGetFriction!(entityID, out result);
    }

    internal static void BoxCollider2DComponent_SetFriction(ulong entityID, float value)
    {
        EnsureInitialized();
        s_BoxCollider2DSetFriction!(entityID, value);
    }

    internal static void BoxCollider2DComponent_GetDensity(ulong entityID, out float result)
    {
        EnsureInitialized();
        s_BoxCollider2DGetDensity!(entityID, out result);
    }

    internal static void BoxCollider2DComponent_SetDensity(ulong entityID, float value)
    {
        EnsureInitialized();
        s_BoxCollider2DSetDensity!(entityID, value);
    }

    internal static void BoxCollider2DComponent_GetIsSensor(ulong entityID, out bool result)
    {
        EnsureInitialized();
        s_BoxCollider2DGetIsSensor!(entityID, out result);
    }

    internal static void BoxCollider2DComponent_SetIsSensor(ulong entityID, bool value)
    {
        EnsureInitialized();
        s_BoxCollider2DSetIsSensor!(entityID, value);
    }

    internal static void CircleCollider2DComponent_GetCenter(ulong entityID, out Mathf.Vector2 result)
    {
        EnsureInitialized();
        s_CircleCollider2DGetCenter!(entityID, out NativeObject.Vector2 native);
        result = NativeObject.ToManaged(native);
    }

    internal static void CircleCollider2DComponent_SetCenter(ulong entityID, Mathf.Vector2 value)
    {
        EnsureInitialized();
        var v = NativeObject.ToNative(value);
        s_CircleCollider2DSetCenter!(entityID, ref v);
    }

    internal static void CircleCollider2DComponent_GetRadius(ulong entityID, out float result)
    {
        EnsureInitialized();
        s_CircleCollider2DGetRadius!(entityID, out float native);
        result = native;
    }

    internal static void CircleCollider2DComponent_SetRadius(ulong entityID, float value)
    {
        EnsureInitialized();
        s_CircleCollider2DSetRadius!(entityID, value);
    }

    internal static void CircleCollider2DComponent_GetRestitution(ulong entityID, out float result)
    {
        EnsureInitialized();
        s_CircleCollider2DGetRestitution!(entityID, out result);
    }

    internal static void CircleCollider2DComponent_SetRestitution(ulong entityID, float value)
    {
        EnsureInitialized();
        s_CircleCollider2DSetRestitution!(entityID, value);
    }

    internal static void CircleCollider2DComponent_GetFriction(ulong entityID, out float result)
    {
        EnsureInitialized();
        s_CircleCollider2DGetFriction!(entityID, out result);
    }

    internal static void CircleCollider2DComponent_SetFriction(ulong entityID, float value)
    {
        EnsureInitialized();
        s_CircleCollider2DSetFriction!(entityID, value);
    }

    internal static void CircleCollider2DComponent_GetDensity(ulong entityID, out float result)
    {
        EnsureInitialized();
        s_CircleCollider2DGetDensity!(entityID, out result);
    }

    internal static void CircleCollider2DComponent_SetDensity(ulong entityID, float value)
    {
        EnsureInitialized();
        s_CircleCollider2DSetDensity!(entityID, value);
    }

    internal static void CircleCollider2DComponent_GetIsSensor(ulong entityID, out bool result)
    {
        EnsureInitialized();
        s_CircleCollider2DGetIsSensor!(entityID, out result);
    }

    internal static void CircleCollider2DComponent_SetIsSensor(ulong entityID, bool value)
    {
        EnsureInitialized();
        s_CircleCollider2DSetIsSensor!(entityID, value);
    }

    internal static void TextComponent_SetText(ulong entityID, string value)
    {
        EnsureInitialized();
        IntPtr ptr = NativeObject.StringToUtf8(value);
        try
        {
            s_TextComponentSetText!(entityID, ptr);
        }
        finally
        {
            Marshal.FreeCoTaskMem(ptr);
        }
    }

    internal static void TextComponent_GetText(ulong entityID, out string result)
    {
        EnsureInitialized();
        s_TextComponentGetText!(entityID, out IntPtr ptr);
        try
        {
            result = Marshal.PtrToStringUTF8(ptr) ?? "<invalid>";
        }
        finally
        {
            if (ptr != IntPtr.Zero)
                Marshal.FreeCoTaskMem(ptr);
        }
    }

    internal static void TextComponent_SetColor(ulong entityID, Mathf.Vector4 value)
    {
        EnsureInitialized();
        var v = NativeObject.ToNative(value);
        s_TextComponentSetColor!(entityID, ref v);
    }

    internal static void TextComponent_GetColor(ulong entityID, out Mathf.Vector4 result)
    {
        EnsureInitialized();
        s_TextComponentGetColor!(entityID, out NativeObject.Vector4 outResult);
        result = NativeObject.ToManaged(outResult);
    }

    internal static void TextComponent_SetKerning(ulong entityID, float value)
    {
        EnsureInitialized();
        s_TextComponentSetKerning!(entityID, value);
    }

    internal static void TextComponent_GetKerning(ulong entityID, out float result)
    {
        EnsureInitialized();
        s_TextComponentGetKerning!(entityID, out result);
    }

    internal static void TextComponent_SetLineSpacing(ulong entityID, float value)
    {
        EnsureInitialized();
        s_TextComponentSetLineSpacing!(entityID, value);
    }

    internal static void TextComponent_GetLineSpacing(ulong entityID, out float result)
    {
        EnsureInitialized();
        s_TextComponentGetLineSpacing!(entityID, out result);
    }

    // =========================================================================
    // Widget Label
    // =========================================================================

    internal static bool WidgetComponent_HasLabel(ulong entityID, string labelName)
    {
        EnsureInitialized();
        IntPtr ptr = NativeObject.StringToUtf8(labelName);
        try { return s_WidgetComponentHasLabel!(entityID, ptr); }
        finally { Marshal.FreeCoTaskMem(ptr); }
    }

    internal static string WidgetComponent_GetLabelText(ulong entityID, string labelName)
    {
        EnsureInitialized();
        IntPtr namePtr = NativeObject.StringToUtf8(labelName);
        try
        {
            s_WidgetComponentGetLabelText!(entityID, namePtr, out IntPtr result);
            return NativeObject.Utf8ToString(result)!;
        }
        finally { Marshal.FreeCoTaskMem(namePtr); }
    }

    internal static void WidgetComponent_SetLabelText(ulong entityID, string labelName, string text)
    {
        EnsureInitialized();
        IntPtr namePtr = NativeObject.StringToUtf8(labelName);
        IntPtr textPtr = NativeObject.StringToUtf8(text);
        try { s_WidgetComponentSetLabelText!(entityID, namePtr, textPtr); }
        finally { Marshal.FreeCoTaskMem(namePtr); Marshal.FreeCoTaskMem(textPtr); }
    }

    internal static void WidgetComponent_GetLabelColor(ulong entityID, string labelName, out Mathf.Vector4 result)
    {
        EnsureInitialized();
        IntPtr ptr = NativeObject.StringToUtf8(labelName);
        try { s_WidgetComponentGetLabelColor!(entityID, ptr, out NativeObject.Vector4 native); result = NativeObject.ToManaged(native); }
        finally { Marshal.FreeCoTaskMem(ptr); }
    }

    internal static void WidgetComponent_SetLabelColor(ulong entityID, string labelName, Mathf.Vector4 color)
    {
        EnsureInitialized();
        IntPtr ptr = NativeObject.StringToUtf8(labelName);
        NativeObject.Vector4 native = NativeObject.ToNative(color);
        try { s_WidgetComponentSetLabelColor!(entityID, ptr, ref native); }
        finally { Marshal.FreeCoTaskMem(ptr); }
    }

    internal static void WidgetComponent_GetLabelFontSize(ulong entityID, string labelName, out float result)
    {
        EnsureInitialized();
        IntPtr ptr = NativeObject.StringToUtf8(labelName);
        try { s_WidgetComponentGetLabelFontSize!(entityID, ptr, out result); }
        finally { Marshal.FreeCoTaskMem(ptr); }
    }

    internal static void WidgetComponent_SetLabelFontSize(ulong entityID, string labelName, float size)
    {
        EnsureInitialized();
        IntPtr ptr = NativeObject.StringToUtf8(labelName);
        try { s_WidgetComponentSetLabelFontSize!(entityID, ptr, size); }
        finally { Marshal.FreeCoTaskMem(ptr); }
    }

    // =========================================================================
    // Widget Image
    // =========================================================================

    internal static bool WidgetComponent_HasImage(ulong entityID, string imageName)
    {
        EnsureInitialized();
        IntPtr ptr = NativeObject.StringToUtf8(imageName);
        try { return s_WidgetComponentHasImage!(entityID, ptr); }
        finally { Marshal.FreeCoTaskMem(ptr); }
    }

    internal static ulong WidgetComponent_GetImageHandle(ulong entityID, string imageName)
    {
        EnsureInitialized();
        IntPtr ptr = NativeObject.StringToUtf8(imageName);
        try { s_WidgetComponentGetImageHandle!(entityID, ptr, out ulong result); return result; }
        finally { Marshal.FreeCoTaskMem(ptr); }
    }

    internal static void WidgetComponent_SetImageHandle(ulong entityID, string imageName, ulong handle)
    {
        EnsureInitialized();
        IntPtr ptr = NativeObject.StringToUtf8(imageName);
        try { s_WidgetComponentSetImageHandle!(entityID, ptr, handle); }
        finally { Marshal.FreeCoTaskMem(ptr); }
    }

    // RigidbodyComponent (3D) wrappers
    internal static void RigidbodyComponent_GetType(ulong entityID, out int result)
    {
        EnsureInitialized();
        s_RigidbodyGetType!(entityID, out result);
    }

    internal static void RigidbodyComponent_SetType(ulong entityID, int value)
    {
        EnsureInitialized();
        s_RigidbodySetType!(entityID, value);
    }

    internal static void RigidbodyComponent_GetMotionQuality(ulong entityID, out int result)
    {
        EnsureInitialized();
        s_RigidbodyGetMotionQuality!(entityID, out result);
    }

    internal static void RigidbodyComponent_GetUseGravity(ulong entityID, out bool result)
    {
        EnsureInitialized();
        s_RigidbodyGetUseGravity!(entityID, out result);
    }

    internal static void RigidbodyComponent_SetUseGravity(ulong entityID, bool value)
    {
        EnsureInitialized();
        s_RigidbodySetUseGravity!(entityID, value);
    }

    internal static void RigidbodyComponent_GetMass(ulong entityID, out float result)
    {
        EnsureInitialized();
        s_RigidbodyGetMass!(entityID, out result);
    }

    internal static void RigidbodyComponent_GetGravityFactor(ulong entityID, out float result)
    {
        EnsureInitialized();
        s_RigidbodyGetGravityFactor!(entityID, out result);
    }

    internal static void RigidbodyComponent_SetGravityFactor(ulong entityID, float value)
    {
        EnsureInitialized();
        s_RigidbodySetGravityFactor!(entityID, value);
    }

    internal static void RigidbodyComponent_GetLinearVelocity(ulong entityID, out Mathf.Vector3 result)
    {
        EnsureInitialized();
        s_RigidbodyGetLinearVelocity!(entityID, out NativeObject.Vector3 native);
        result = NativeObject.ToManaged(native);
    }

    internal static void RigidbodyComponent_SetLinearVelocity(ulong entityID, Mathf.Vector3 value)
    {
        EnsureInitialized();
        NativeObject.Vector3 nativeVal = NativeObject.ToNative(value);
        s_RigidbodySetLinearVelocity!(entityID, ref nativeVal);
    }

    internal static void RigidbodyComponent_GetAngularVelocity(ulong entityID, out Mathf.Vector3 result)
    {
        EnsureInitialized();
        s_RigidbodyGetAngularVelocity!(entityID, out NativeObject.Vector3 native);
        result = NativeObject.ToManaged(native);
    }

    internal static void RigidbodyComponent_GetPosition(ulong entityID, out Mathf.Vector3 result)
    {
        EnsureInitialized();
        s_RigidbodyGetPosition!(entityID, out NativeObject.Vector3 native);
        result = NativeObject.ToManaged(native);
    }

    internal static void RigidbodyComponent_SetPosition(ulong entityID, Mathf.Vector3 value)
    {
        EnsureInitialized();
        NativeObject.Vector3 nativeVal = NativeObject.ToNative(value);
        s_RigidbodySetPosition!(entityID, ref nativeVal);
    }

    internal static void RigidbodyComponent_GetRotation(ulong entityID, out Mathf.Quaternion result)
    {
        EnsureInitialized();
        s_RigidbodyGetRotation!(entityID, out NativeObject.Quaternion native);
        result = NativeObject.ToManaged(native);
    }

    internal static void RigidbodyComponent_SetRotation(ulong entityID, Mathf.Quaternion value)
    {
        EnsureInitialized();
        NativeObject.Quaternion nativeVal = NativeObject.ToNative(value);
        s_RigidbodySetRotation!(entityID, ref nativeVal);
    }

    internal static void RigidbodyComponent_GetCenterOfMass(ulong entityID, out Mathf.Vector3 result)
    {
        EnsureInitialized();
        s_RigidbodyGetCenterOfMass!(entityID, out NativeObject.Vector3 native);
        result = NativeObject.ToManaged(native);
    }

    internal static void RigidbodyComponent_IsActive(ulong entityID, out bool result)
    {
        EnsureInitialized();
        s_RigidbodyIsActive!(entityID, out result);
    }

    internal static void RigidbodyComponent_ApplyForce(ulong entityID, Mathf.Vector3 force, Mathf.Vector3 point)
    {
        EnsureInitialized();
        NativeObject.Vector3 nativeForce = NativeObject.ToNative(force);
        NativeObject.Vector3 nativePoint = NativeObject.ToNative(point);
        s_RigidbodyApplyForce!(entityID, ref nativeForce, ref nativePoint);
    }

    internal static void RigidbodyComponent_ApplyForceToCenter(ulong entityID, Mathf.Vector3 force)
    {
        EnsureInitialized();
        NativeObject.Vector3 nativeVal = NativeObject.ToNative(force);
        s_RigidbodyApplyForceToCenter!(entityID, ref nativeVal);
    }

    internal static void RigidbodyComponent_ApplyTorque(ulong entityID, Mathf.Vector3 torque)
    {
        EnsureInitialized();
        NativeObject.Vector3 nativeVal = NativeObject.ToNative(torque);
        s_RigidbodyApplyTorque!(entityID, ref nativeVal);
    }

    internal static void RigidbodyComponent_ApplyLinearImpulse(ulong entityID, Mathf.Vector3 impulse, Mathf.Vector3 point)
    {
        EnsureInitialized();
        NativeObject.Vector3 nativeImpulse = NativeObject.ToNative(impulse);
        NativeObject.Vector3 nativePoint = NativeObject.ToNative(point);
        s_RigidbodyApplyLinearImpulse!(entityID, ref nativeImpulse, ref nativePoint);
    }

    internal static void RigidbodyComponent_ApplyLinearImpulseToCenter(ulong entityID, Mathf.Vector3 impulse)
    {
        EnsureInitialized();
        NativeObject.Vector3 nativeVal = NativeObject.ToNative(impulse);
        s_RigidbodyApplyLinearImpulseToCenter!(entityID, ref nativeVal);
    }

    internal static void RigidbodyComponent_ApplyAngularImpulse(ulong entityID, Mathf.Vector3 impulse)
    {
        EnsureInitialized();
        NativeObject.Vector3 nativeVal = NativeObject.ToNative(impulse);
        s_RigidbodyApplyAngularImpulse!(entityID, ref nativeVal);
    }

    internal static void RigidbodyComponent_Activate(ulong entityID)
    {
        EnsureInitialized();
        s_RigidbodyActivate!(entityID);
    }

    internal static void RigidbodyComponent_Deactivate(ulong entityID)
    {
        EnsureInitialized();
        s_RigidbodyDeactivate!(entityID);
    }

    internal static void RigidbodyComponent_MoveKinematic(ulong entityID, Mathf.Vector3 targetPosition, Mathf.Vector3 targetRotation, float deltaTime)
    {
        EnsureInitialized();
        NativeObject.Vector3 nativePos = NativeObject.ToNative(targetPosition);
        NativeObject.Vector3 nativeRot = NativeObject.ToNative(targetRotation);
        s_RigidbodyMoveKinematic!(entityID, ref nativePos, ref nativeRot, deltaTime);
    }

    // BoxColliderComponent wrappers
    internal static void BoxColliderComponent_GetCenter(ulong entityID, out Mathf.Vector3 result)
    {
        EnsureInitialized();
        s_BoxColliderGetCenter!(entityID, out NativeObject.Vector3 native);
        result = NativeObject.ToManaged(native);
    }

    internal static void BoxColliderComponent_SetCenter(ulong entityID, Mathf.Vector3 value)
    {
        EnsureInitialized();
        var v = NativeObject.ToNative(value);
        s_BoxColliderSetCenter!(entityID, ref v);
    }

    internal static void BoxColliderComponent_GetScale(ulong entityID, out Mathf.Vector3 result)
    {
        EnsureInitialized();
        s_BoxColliderGetScale!(entityID, out NativeObject.Vector3 native);
        result = NativeObject.ToManaged(native);
    }

    internal static void BoxColliderComponent_SetScale(ulong entityID, Mathf.Vector3 value)
    {
        EnsureInitialized();
        var v = NativeObject.ToNative(value);
        s_BoxColliderSetScale!(entityID, ref v);
    }


    // SphereColliderComponent wrappers
    internal static void SphereColliderComponent_GetCenter(ulong entityID, out Mathf.Vector3 result)
    {
        EnsureInitialized();
        s_SphereColliderGetCenter!(entityID, out NativeObject.Vector3 native);
        result = NativeObject.ToManaged(native);
    }

    internal static void SphereColliderComponent_SetCenter(ulong entityID, Mathf.Vector3 value)
    {
        EnsureInitialized();
        var v = NativeObject.ToNative(value);
        s_SphereColliderSetCenter!(entityID, ref v);
    }

    internal static void SphereColliderComponent_GetRadius(ulong entityID, out float result)
    {
        EnsureInitialized();
        s_SphereColliderGetRadius!(entityID, out result);
    }

    internal static void SphereColliderComponent_SetRadius(ulong entityID, float value)
    {
        EnsureInitialized();
        s_SphereColliderSetRadius!(entityID, value);
    }

    // CapsuleColliderComponent wrappers
    internal static void CapsuleColliderComponent_GetCenter(ulong entityID, out Mathf.Vector3 result)
    {
        EnsureInitialized();
        s_CapsuleColliderGetCenter!(entityID, out NativeObject.Vector3 native);
        result = NativeObject.ToManaged(native);
    }

    internal static void CapsuleColliderComponent_SetCenter(ulong entityID, Mathf.Vector3 value)
    {
        EnsureInitialized();
        var v = NativeObject.ToNative(value);
        s_CapsuleColliderSetCenter!(entityID, ref v);
    }

    internal static void CapsuleColliderComponent_GetRadius(ulong entityID, out float result)
    {
        EnsureInitialized();
        s_CapsuleColliderGetRadius!(entityID, out result);
    }

    internal static void CapsuleColliderComponent_SetRadius(ulong entityID, float value)
    {
        EnsureInitialized();
        s_CapsuleColliderSetRadius!(entityID, value);
    }

    internal static void CapsuleColliderComponent_GetHeight(ulong entityID, out float result)
    {
        EnsureInitialized();
        s_CapsuleColliderGetHeight!(entityID, out result);
    }

    internal static void CapsuleColliderComponent_SetHeight(ulong entityID, float value)
    {
        EnsureInitialized();
        s_CapsuleColliderSetHeight!(entityID, value);
    }

    internal static void RigidbodyComponent_SetMass(ulong entityID, float value)
    {
        EnsureInitialized();
        s_RigidbodySetMass!(entityID, value);
    }

    internal static void RigidbodyComponent_GetLinearDamping(ulong entityID, out float result)
    {
        EnsureInitialized();
        s_RigidbodyGetLinearDamping!(entityID, out result);
    }

    internal static void RigidbodyComponent_SetLinearDamping(ulong entityID, float value)
    {
        EnsureInitialized();
        s_RigidbodySetLinearDamping!(entityID, value);
    }

    internal static void RigidbodyComponent_GetAngularDamping(ulong entityID, out float result)
    {
        EnsureInitialized();
        s_RigidbodyGetAngularDamping!(entityID, out result);
    }

    internal static void RigidbodyComponent_SetAngularDamping(ulong entityID, float value)
    {
        EnsureInitialized();
        s_RigidbodySetAngularDamping!(entityID, value);
    }

    internal static void RigidbodyComponent_GetFriction(ulong entityID, out float result)
    {
        EnsureInitialized();
        s_RigidbodyGetFriction!(entityID, out result);
    }

    internal static void RigidbodyComponent_SetFriction(ulong entityID, float value)
    {
        EnsureInitialized();
        s_RigidbodySetFriction!(entityID, value);
    }

    internal static void RigidbodyComponent_GetRestitution(ulong entityID, out float result)
    {
        EnsureInitialized();
        s_RigidbodyGetRestitution!(entityID, out result);
    }

    internal static void RigidbodyComponent_SetRestitution(ulong entityID, float value)
    {
        EnsureInitialized();
        s_RigidbodySetRestitution!(entityID, value);
    }

    internal static void RigidbodyComponent_GetMaxLinearVelocity(ulong entityID, out float result)
    {
        EnsureInitialized();
        s_RigidbodyGetMaxLinearVelocity!(entityID, out result);
    }

    internal static void RigidbodyComponent_SetMaxLinearVelocity(ulong entityID, float value)
    {
        EnsureInitialized();
        s_RigidbodySetMaxLinearVelocity!(entityID, value);
    }

    internal static void RigidbodyComponent_GetMaxAngularVelocity(ulong entityID, out float result)
    {
        EnsureInitialized();
        s_RigidbodyGetMaxAngularVelocity!(entityID, out result);
    }

    internal static void RigidbodyComponent_SetMaxAngularVelocity(ulong entityID, float value)
    {
        EnsureInitialized();
        s_RigidbodySetMaxAngularVelocity!(entityID, value);
    }

    internal static void RigidbodyComponent_GetApplyGyroscopicForce(ulong entityID, out bool result)
    {
        EnsureInitialized();
        s_RigidbodyGetApplyGyroscopicForce!(entityID, out result);
    }

    internal static void RigidbodyComponent_SetApplyGyroscopicForce(ulong entityID, bool value)
    {
        EnsureInitialized();
        s_RigidbodySetApplyGyroscopicForce!(entityID, value);
    }

    internal static void RigidbodyComponent_SetAngularVelocity(ulong entityID, Mathf.Vector3 value)
    {
        EnsureInitialized();
        NativeObject.Vector3 nativeVal = NativeObject.ToNative(value);
        s_RigidbodySetAngularVelocity!(entityID, ref nativeVal);
    }
}
