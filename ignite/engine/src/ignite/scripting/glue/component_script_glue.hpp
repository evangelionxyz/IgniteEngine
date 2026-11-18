// Copyright (c) 2026 Evangelion Manuhutu

#ifndef IGN_COMPONENT_SCRIPT_GLUE_HPP
#define IGN_COMPONENT_SCRIPT_GLUE_HPP

#include <cstdint>
#include <glm/glm.hpp>

namespace ignite
{
    struct ComponentScriptGlueAPI
    {
        void (*Scene_GetScreenToWorldRay)(float x, float y, glm::vec3 *outOrigin, glm::vec3 *outDirection);
        uint64_t (*Scene_Raycast)(const glm::vec3 *origin, const glm::vec3 *direction, float maxDistance, uint32_t layerMask, glm::vec3 *outHitPoint, glm::vec3 *outHitNormal);
        uint64_t (*Scene_GetPrimaryCamera)();

        bool (*Entity_HasComponent)(uint64_t entityID, const char *componentTypeName);
        void (*Entity_AddComponent)(uint64_t entityID, const char *componentTypeName);
        uint64_t (*Entity_FindEntityByName)(const char *name);
        uint64_t (*Entity_FindChildByName)(uint64_t entityID, const char *childName);
        bool (*Entity_IsParent)(uint64_t entityID, uint64_t parentEntityID);
        uint64_t (*Entity_GetParent)(uint64_t entityID);
        uint64_t (*Entity_InstantiateWithName)(const char *name, const glm::vec3 *value);
        uint64_t (*Entity_Instantiate)(uint64_t entityID, const glm::vec3 *value);
        void (*Entity_Destroy)(uint64_t entityID);
        void (*Entity_SetVisibility)(uint64_t entityID, bool value);
        void (*Entity_GetVisibility)(uint64_t entityID, bool *result);
        const char *(*Entity_GetName)(uint64_t entityID);

        bool (*WidgetComponent_HasButton)(uint64_t entityID, const char *buttonName);
        bool (*WidgetComponent_AddButtonEventCallback)(uint64_t entityID, const char *buttonName, int32_t eventType, const char *methodName);
        bool (*WidgetComponent_RemoveButtonEventCallback)(uint64_t entityID, const char *buttonName, int32_t eventType, const char *methodName);

        // Label
        bool (*WidgetComponent_HasLabel)(uint64_t entityID, const char *labelName);
        void (*WidgetComponent_GetLabelText)(uint64_t entityID, const char *labelName, const char **result);
        void (*WidgetComponent_SetLabelText)(uint64_t entityID, const char *labelName, const char *text);
        void (*WidgetComponent_GetLabelColor)(uint64_t entityID, const char *labelName, glm::vec4 *result);
        void (*WidgetComponent_SetLabelColor)(uint64_t entityID, const char *labelName, glm::vec4 *color);
        void (*WidgetComponent_GetLabelFontSize)(uint64_t entityID, const char *labelName, float *result);
        void (*WidgetComponent_SetLabelFontSize)(uint64_t entityID, const char *labelName, float size);

        // Image
        bool (*WidgetComponent_HasImage)(uint64_t entityID, const char *imageName);
        void (*WidgetComponent_GetImageHandle)(uint64_t entityID, const char *imageName, uint64_t *result);
        void (*WidgetComponent_SetImageHandle)(uint64_t entityID, const char *imageName, uint64_t handle);

        bool (*AudioSourceComponent_HasAudio)(uint64_t entityID);
        void (*AudioSourceComponent_Play)(uint64_t entityID);
        void (*AudioSourceComponent_Stop)(uint64_t entityID);
        void (*AudioSourceComponent_Pause)(uint64_t entityID);
        void (*AudioSourceComponent_Resume)(uint64_t entityID);
        void (*AudioSourceComponent_GetVolume)(uint64_t entityID, float *result);
        void (*AudioSourceComponent_SetVolume)(uint64_t entityID, float value);
        void (*AudioSourceComponent_GetPitch)(uint64_t entityID, float *result);
        void (*AudioSourceComponent_SetPitch)(uint64_t entityID, float value);
        void (*AudioSourceComponent_GetPan)(uint64_t entityID, float *result);
        void (*AudioSourceComponent_SetPan)(uint64_t entityID, float value);
        void (*AudioSourceComponent_GetPlayOnStart)(uint64_t entityID, bool *result);
        void (*AudioSourceComponent_SetPlayOnStart)(uint64_t entityID, bool value);
        void (*AudioSourceComponent_GetLoop)(uint64_t entityID, bool *result);
        void (*AudioSourceComponent_SetLoop)(uint64_t entityID, bool value);
        bool (*AudioSourceComponent_AddReverbDSP)(uint64_t entityID, float decayTime, float earlyDelay, float lateDelay, float highFrequencyReference, float diffusion, float density, float lowShelfGain, float highCut, float dryLevel, float wetLevel);
        bool (*AudioSourceComponent_AddDistortionDSP)(uint64_t entityID, float distortionLevel);
        bool (*AudioSourceComponent_AddChorusDSP)(uint64_t entityID, float mix, float rate, float depth);
        bool (*AudioSourceComponent_AddCompressorDSP)(uint64_t entityID, float threshold, float ratio, float release, float gainMakeup, bool useSidechain);
        bool (*AudioSourceComponent_AddDelayDSP)(uint64_t entityID, float delayMs, float feedback);
        void (*AudioSourceComponent_ClearDSPs)(uint64_t entityID);

        void (*TransformComponent_GetForward)(uint64_t entityID, glm::vec3 *result);
        void (*TransformComponent_SetForward)(uint64_t entityID, const glm::vec3 *value);
        void (*TransformComponent_GetRight)(uint64_t entityID, glm::vec3 *result);
        void (*TransformComponent_SetRight)(uint64_t entityID, const glm::vec3 *value);
        void (*TransformComponent_GetUp)(uint64_t entityID, glm::vec3 *result);
        void (*TransformComponent_SetUp)(uint64_t entityID, const glm::vec3 *value);
        void (*TransformComponent_GetTranslation)(uint64_t entityID, glm::vec3 *result);
        void (*TransformComponent_SetTranslation)(uint64_t entityID, const glm::vec3 *value);
        void (*TransformComponent_GetRotation)(uint64_t entityID, glm::quat *result);
        void (*TransformComponent_SetRotation)(uint64_t entityID, const glm::quat *value);
        void (*TransformComponent_GetEulerAngles)(uint64_t entityID, glm::vec3 *result);
        void (*TransformComponent_SetEulerAngles)(uint64_t entityID, const glm::vec3 *value);
        void (*TransformComponent_GetScale)(uint64_t entityID, glm::vec3 *result);
        void (*TransformComponent_SetScale)(uint64_t entityID, const glm::vec3 *value);

		void (*Sprite2DComponent_SetColor)(uint64_t entityID, const glm::vec4 *color);
		void (*Sprite2DComponent_GetColor)(uint64_t entityID, glm::vec4 *result);
		void (*Sprite2DComponent_SetTilingFactor)(uint64_t entityID, const glm::vec2 *tiling);
		void (*Sprite2DComponent_GetTilingFactor)(uint64_t entityID, glm::vec2 *result);

		void (*Circle2DComponent_SetColor)(uint64_t entityID, const glm::vec4 *color);
		void (*Circle2DComponent_GetColor)(uint64_t entityID, glm::vec4 *result);

        void (*Rigidbody2DComponent_GetType)(uint64_t entityID, int32_t *result);
        void (*Rigidbody2DComponent_SetType)(uint64_t entityID, int32_t value);
        void (*Rigidbody2DComponent_GetLinearVelocity)(uint64_t entityID, glm::vec2 *result);
        void (*Rigidbody2DComponent_SetLinearVelocity)(uint64_t entityID, const glm::vec2 *value);
        void (*Rigidbody2DComponent_GetAngularVelocity)(uint64_t entityID, float *result);
        void (*Rigidbody2DComponent_SetAngularVelocity)(uint64_t entityID, float value);
        void (*Rigidbody2DComponent_GetGravityScale)(uint64_t entityID, float *result);
        void (*Rigidbody2DComponent_SetGravityScale)(uint64_t entityID, float value);
        void (*Rigidbody2DComponent_GetLinearDamping)(uint64_t entityID, float *result);
        void (*Rigidbody2DComponent_SetLinearDamping)(uint64_t entityID, float value);
        void (*Rigidbody2DComponent_GetAngularDamping)(uint64_t entityID, float *result);
        void (*Rigidbody2DComponent_SetAngularDamping)(uint64_t entityID, float value);
        void (*Rigidbody2DComponent_GetIsAwake)(uint64_t entityID, bool *result);
        void (*Rigidbody2DComponent_SetIsAwake)(uint64_t entityID, bool value);
        void (*Rigidbody2DComponent_GetIsEnabled)(uint64_t entityID, bool *result);
        void (*Rigidbody2DComponent_SetIsEnabled)(uint64_t entityID, bool value);
        void (*Rigidbody2DComponent_GetIsEnableSleep)(uint64_t entityID, bool *result);
        void (*Rigidbody2DComponent_SetIsEnableSleep)(uint64_t entityID, bool value);
        void (*Rigidbody2DComponent_ApplyForce)(uint64_t entityID, glm::vec2 force, glm::vec2 point, bool wake);
        void (*Rigidbody2DComponent_ApplyForceToCenter)(uint64_t entityID, glm::vec2 force, bool wake);
        void (*Rigidbody2DComponent_ApplyLinearImpulse)(uint64_t entityID, glm::vec2 impulse, glm::vec2 point, bool wake);
        void (*Rigidbody2DComponent_ApplyLinearImpulseToCenter)(uint64_t entityID, glm::vec2 impulse, bool wake);
        void (*Rigidbody2DComponent_ApplyAngularImpulse)(uint64_t entityID, float impulse, bool wake);
        void (*Rigidbody2DComponent_ApplyTorque)(uint64_t entityID, float torque, bool wake);
        void (*Rigidbody2DComponent_GetMass)(uint64_t entityID, float *result);
        void (*Rigidbody2DComponent_GetIsBullet)(uint64_t entityID, bool *result);
        void (*Rigidbody2DComponent_SetIsBullet)(uint64_t entityID, bool value);

        void (*BoxCollider2DComponent_GetSize)(uint64_t entityID, glm::vec2 *result);
        void (*BoxCollider2DComponent_SetSize)(uint64_t entityID, glm::vec2 value);
        void (*BoxCollider2DComponent_GetOffset)(uint64_t entityID, glm::vec2 *result);
        void (*BoxCollider2DComponent_SetOffset)(uint64_t entityID, glm::vec2 value);
        void (*BoxCollider2DComponent_GetRestitution)(uint64_t entityID, float *result);
        void (*BoxCollider2DComponent_SetRestitution)(uint64_t entityID, float value);
        void (*BoxCollider2DComponent_GetFriction)(uint64_t entityID, float *result);
        void (*BoxCollider2DComponent_SetFriction)(uint64_t entityID, float value);
        void (*BoxCollider2DComponent_GetDensity)(uint64_t entityID, float *result);
        void (*BoxCollider2DComponent_SetDensity)(uint64_t entityID, float value);
        void (*BoxCollider2DComponent_GetIsSensor)(uint64_t entityID, bool *result);
        void (*BoxCollider2DComponent_SetIsSensor)(uint64_t entityID, bool value);

		void (*CircleCollider2DComponent_GetCenter)(uint64_t entityID, glm::vec2 *result);
		void (*CircleCollider2DComponent_SetCenter)(uint64_t entityID, glm::vec2 value);
		void (*CircleCollider2DComponent_GetRadius)(uint64_t entityID, float *result);
		void (*CircleCollider2DComponent_SetSetRadius)(uint64_t entityID, float value);
		void (*CircleCollider2DComponent_GetRestitution)(uint64_t entityID, float *result);
		void (*CircleCollider2DComponent_SetRestitution)(uint64_t entityID, float value);
		void (*CircleCollider2DComponent_GetFriction)(uint64_t entityID, float *result);
		void (*CircleCollider2DComponent_SetFriction)(uint64_t entityID, float value);
		void (*CircleCollider2DComponent_GetDensity)(uint64_t entityID, float *result);
		void (*CircleCollider2DComponent_SetDensity)(uint64_t entityID, float value);
		void (*CircleCollider2DComponent_GetIsSensor)(uint64_t entityID, bool *result);
		void (*CircleCollider2DComponent_SetIsSensor)(uint64_t entityID, bool value);

        void (*TextComponent_SetText)(uint64_t entityID, const char *value);
        void (*TextComponent_GetText)(uint64_t entityID, const char **result);
        void (*TextComponent_SetColor)(uint64_t entityID, const glm::vec4 &value);
        void (*TextComponent_GetColor)(uint64_t entityID, glm::vec4 *result);
        void (*TextComponent_SetKerning)(uint64_t entityID, float value);
        void (*TextComponent_GetKerning)(uint64_t entityID, float *result);
        void (*TextComponent_SetLineSpacing)(uint64_t entityID, float value);
        void (*TextComponent_GetLineSpacing)(uint64_t entityID, float *result);

        // RigidbodyComponent (3D)
        void (*RigidbodyComponent_GetType)(uint64_t entityID, int32_t *result);
        void (*RigidbodyComponent_SetType)(uint64_t entityID, int32_t value);
        void (*RigidbodyComponent_GetMotionQuality)(uint64_t entityID, int32_t *result);
        void (*RigidbodyComponent_GetUseGravity)(uint64_t entityID, bool *result);
        void (*RigidbodyComponent_SetUseGravity)(uint64_t entityID, bool value);
        void (*RigidbodyComponent_GetMass)(uint64_t entityID, float *result);
        void (*RigidbodyComponent_GetGravityFactor)(uint64_t entityID, float *result);
        void (*RigidbodyComponent_SetGravityFactor)(uint64_t entityID, float value);
        void (*RigidbodyComponent_GetLinearVelocity)(uint64_t entityID, glm::vec3 *result);
        void (*RigidbodyComponent_SetLinearVelocity)(uint64_t entityID, const glm::vec3 *value);
        void (*RigidbodyComponent_GetAngularVelocity)(uint64_t entityID, glm::vec3 *result);
        void (*RigidbodyComponent_GetPosition)(uint64_t entityID, glm::vec3 *result);
        void (*RigidbodyComponent_SetPosition)(uint64_t entityID, const glm::vec3 *value);
        void (*RigidbodyComponent_GetRotation)(uint64_t entityID, glm::quat *result);
        void (*RigidbodyComponent_SetRotation)(uint64_t entityID, const glm::quat *value);
        void (*RigidbodyComponent_GetCenterOfMass)(uint64_t entityID, glm::vec3 *result);
        void (*RigidbodyComponent_IsActive)(uint64_t entityID, bool *result);
        void (*RigidbodyComponent_ApplyForce)(uint64_t entityID, const glm::vec3 *force, const glm::vec3 *point);
        void (*RigidbodyComponent_ApplyForceToCenter)(uint64_t entityID, const glm::vec3 *force);
        void (*RigidbodyComponent_ApplyTorque)(uint64_t entityID, const glm::vec3 *torque);
        void (*RigidbodyComponent_ApplyLinearImpulse)(uint64_t entityID, const glm::vec3 *impulse, const glm::vec3 *point);
        void (*RigidbodyComponent_ApplyLinearImpulseToCenter)(uint64_t entityID, const glm::vec3 *impulse);
        void (*RigidbodyComponent_ApplyAngularImpulse)(uint64_t entityID, const glm::vec3 *impulse);
        void (*RigidbodyComponent_Activate)(uint64_t entityID);
        void (*RigidbodyComponent_Deactivate)(uint64_t entityID);
        void (*RigidbodyComponent_MoveKinematic)(uint64_t entityID, const glm::vec3 *targetPosition, const glm::vec3 *targetRotation, float deltaTime);
        void (*RigidbodyComponent_SetMass)(uint64_t entityID, float value);
        void (*RigidbodyComponent_GetLinearDamping)(uint64_t entityID, float *result);
        void (*RigidbodyComponent_SetLinearDamping)(uint64_t entityID, float value);
        void (*RigidbodyComponent_GetAngularDamping)(uint64_t entityID, float *result);
        void (*RigidbodyComponent_SetAngularDamping)(uint64_t entityID, float value);
        void (*RigidbodyComponent_GetFriction)(uint64_t entityID, float *result);
        void (*RigidbodyComponent_SetFriction)(uint64_t entityID, float value);
        void (*RigidbodyComponent_GetRestitution)(uint64_t entityID, float *result);
        void (*RigidbodyComponent_SetRestitution)(uint64_t entityID, float value);
        void (*RigidbodyComponent_GetMaxLinearVelocity)(uint64_t entityID, float *result);
        void (*RigidbodyComponent_SetMaxLinearVelocity)(uint64_t entityID, float value);
        void (*RigidbodyComponent_GetMaxAngularVelocity)(uint64_t entityID, float *result);
        void (*RigidbodyComponent_SetMaxAngularVelocity)(uint64_t entityID, float value);
        void (*RigidbodyComponent_GetApplyGyroscopicForce)(uint64_t entityID, bool *result);
        void (*RigidbodyComponent_SetApplyGyroscopicForce)(uint64_t entityID, bool value);
        void (*RigidbodyComponent_SetAngularVelocity)(uint64_t entityID, const glm::vec3 *value);
        void (*RigidbodyComponent_GetLayer)(uint64_t entityID, uint32_t *result);
        void (*RigidbodyComponent_SetLayer)(uint64_t entityID, uint32_t value);

        // BoxColliderComponent
        void (*BoxColliderComponent_GetCenter)(uint64_t entityID, glm::vec3 *result);
        void (*BoxColliderComponent_SetCenter)(uint64_t entityID, const glm::vec3 *value);
        void (*BoxColliderComponent_GetScale)(uint64_t entityID, glm::vec3 *result);
        void (*BoxColliderComponent_SetScale)(uint64_t entityID, const glm::vec3 *value);

        // SphereColliderComponent
        void (*SphereColliderComponent_GetCenter)(uint64_t entityID, glm::vec3 *result);
        void (*SphereColliderComponent_SetCenter)(uint64_t entityID, const glm::vec3 *value);
        void (*SphereColliderComponent_GetRadius)(uint64_t entityID, float *result);
        void (*SphereColliderComponent_SetRadius)(uint64_t entityID, float value);

        // CapsuleColliderComponent
        void (*CapsuleColliderComponent_GetCenter)(uint64_t entityID, glm::vec3 *result);
        void (*CapsuleColliderComponent_SetCenter)(uint64_t entityID, const glm::vec3 *value);
        void (*CapsuleColliderComponent_GetRadius)(uint64_t entityID, float *result);
        void (*CapsuleColliderComponent_SetRadius)(uint64_t entityID, float value);
        void (*CapsuleColliderComponent_GetHeight)(uint64_t entityID, float *result);
        void (*CapsuleColliderComponent_SetHeight)(uint64_t entityID, float value);

        // CharacterControllerComponent
        void (*CharacterControllerComponent_GetCenter)(uint64_t entityID, glm::vec3 *result);
        void (*CharacterControllerComponent_SetCenter)(uint64_t entityID, const glm::vec3 *value);
        void (*CharacterControllerComponent_GetRadius)(uint64_t entityID, float *result);
        void (*CharacterControllerComponent_SetRadius)(uint64_t entityID, float value);
        void (*CharacterControllerComponent_GetHeight)(uint64_t entityID, float *result);
        void (*CharacterControllerComponent_SetHeight)(uint64_t entityID, float value);
        void (*CharacterControllerComponent_GetMaxStepHeight)(uint64_t entityID, float *result);
        void (*CharacterControllerComponent_SetMaxStepHeight)(uint64_t entityID, float value);
        void (*CharacterControllerComponent_GetMaxSlopeAngle)(uint64_t entityID, float *result);
        void (*CharacterControllerComponent_SetMaxSlopeAngle)(uint64_t entityID, float value);
        void (*CharacterControllerComponent_GetMass)(uint64_t entityID, float *result);
        void (*CharacterControllerComponent_SetMass)(uint64_t entityID, float value);
        void (*CharacterControllerComponent_GetFriction)(uint64_t entityID, float *result);
        void (*CharacterControllerComponent_SetFriction)(uint64_t entityID, float value);
        void (*CharacterControllerComponent_GetGravityFactor)(uint64_t entityID, float *result);
        void (*CharacterControllerComponent_SetGravityFactor)(uint64_t entityID, float value);
        void (*CharacterControllerComponent_GetUp)(uint64_t entityID, glm::vec3 *result);
        void (*CharacterControllerComponent_SetUp)(uint64_t entityID, const glm::vec3 *value);
        void (*CharacterControllerComponent_GetLinearVelocity)(uint64_t entityID, glm::vec3 *result);
        void (*CharacterControllerComponent_SetLinearVelocity)(uint64_t entityID, const glm::vec3 *value);
        void (*CharacterControllerComponent_IsOnGround)(uint64_t entityID, bool *result);
        void (*CharacterControllerComponent_IsSupported)(uint64_t entityID, bool *result);
        void (*CharacterControllerComponent_GetGroundNormal)(uint64_t entityID, glm::vec3 *result);
        void (*CharacterControllerComponent_Move)(uint64_t entityID, const glm::vec3 *displacement, float deltaTime);
        void (*CharacterControllerComponent_GetPosition)(uint64_t entityID, glm::vec3 *result);
        void (*CharacterControllerComponent_SetPosition)(uint64_t entityID, const glm::vec3 *position);
        void (*CharacterControllerComponent_GetRotation)(uint64_t entityID, glm::quat *result);
        void (*CharacterControllerComponent_SetRotation)(uint64_t entityID, const glm::quat *rotation);

        // AnimatorComponent (SkeletalMeshComponent / AnimatorController)
        void (*AnimatorComponent_SetFloat)(uint64_t entityID, const char *paramName, float value);
        void (*AnimatorComponent_GetFloat)(uint64_t entityID, const char *paramName, float *result);
        void (*AnimatorComponent_SetInt)(uint64_t entityID, const char *paramName, int32_t value);
        void (*AnimatorComponent_GetInt)(uint64_t entityID, const char *paramName, int32_t *result);
        void (*AnimatorComponent_SetBool)(uint64_t entityID, const char *paramName, bool value);
        void (*AnimatorComponent_GetBool)(uint64_t entityID, const char *paramName, bool *result);
        void (*AnimatorComponent_SetString)(uint64_t entityID, const char *paramName, const char *value);
        void (*AnimatorComponent_GetString)(uint64_t entityID, const char *paramName, const char **result);
        void (*AnimatorComponent_SetState)(uint64_t entityID, const char *stateName);
        void (*AnimatorComponent_GetCurrentStateName)(uint64_t entityID, const char **result);

        // AnimationMontage
        int32_t (*AnimationMontage_GetNotifyCallbackCount)(uint64_t montageHandle);
        bool (*AnimationMontage_GetNotifyCallbackAt)(uint64_t montageHandle, int32_t index, float *outTimestep, uint8_t *outActionType, const char **outName);
        void (*AnimationMontage_AddNotifyCallback)(uint64_t montageHandle, float timestep, uint8_t actionType, const char *name);
        void (*AnimationMontage_RemoveNotifyCallback)(uint64_t montageHandle, int32_t index);

        // Entity removal helper
        void (*Entity_RemoveComponent)(uint64_t entityID, const char *componentTypeName);

        // RenderingComponent
        void (*RenderingComponent_GetVisible)(uint64_t entityID, bool *result);
        void (*RenderingComponent_SetVisible)(uint64_t entityID, bool value);

        // CameraComponent
        void (*CameraComponent_GetPrimary)(uint64_t entityID, bool *result);
        void (*CameraComponent_SetPrimary)(uint64_t entityID, bool value);
        void (*CameraComponent_GetProjectionType)(uint64_t entityID, int32_t *result);
        void (*CameraComponent_SetProjectionType)(uint64_t entityID, int32_t value);
        void (*CameraComponent_GetFov)(uint64_t entityID, float *result);
        void (*CameraComponent_SetFov)(uint64_t entityID, float value);
        void (*CameraComponent_GetOrthoSize)(uint64_t entityID, float *result);
        void (*CameraComponent_SetOrthoSize)(uint64_t entityID, float value);
        void (*CameraComponent_GetNearClip)(uint64_t entityID, float *result);
        void (*CameraComponent_SetNearClip)(uint64_t entityID, float value);
        void (*CameraComponent_GetFarClip)(uint64_t entityID, float *result);
        void (*CameraComponent_SetFarClip)(uint64_t entityID, float value);
        void (*CameraComponent_GetAspectRatioPreset)(uint64_t entityID, int32_t *result);
        void (*CameraComponent_SetAspectRatioPreset)(uint64_t entityID, int32_t value);

        // DirectionalLightComponent
        void (*DirectionalLightComponent_GetColor)(uint64_t entityID, glm::vec4 *result);
        void (*DirectionalLightComponent_SetColor)(uint64_t entityID, const glm::vec4 *value);
        void (*DirectionalLightComponent_GetIntensity)(uint64_t entityID, float *result);
        void (*DirectionalLightComponent_SetIntensity)(uint64_t entityID, float value);
        void (*DirectionalLightComponent_GetAngularRadius)(uint64_t entityID, float *result);
        void (*DirectionalLightComponent_SetAngularRadius)(uint64_t entityID, float value);
        void (*DirectionalLightComponent_GetShadowStrength)(uint64_t entityID, float *result);
        void (*DirectionalLightComponent_SetShadowStrength)(uint64_t entityID, float value);
        void (*DirectionalLightComponent_GetShadowDistance)(uint64_t entityID, float *result);
        void (*DirectionalLightComponent_SetShadowDistance)(uint64_t entityID, float value);
        void (*DirectionalLightComponent_GetShadowResolution)(uint64_t entityID, int32_t *result);
        void (*DirectionalLightComponent_SetShadowResolution)(uint64_t entityID, int32_t value);
        void (*DirectionalLightComponent_GetCascadeShadow)(uint64_t entityID, bool *result);
        void (*DirectionalLightComponent_SetCascadeShadow)(uint64_t entityID, bool value);
        void (*DirectionalLightComponent_GetSoftShadows)(uint64_t entityID, bool *result);
        void (*DirectionalLightComponent_SetSoftShadows)(uint64_t entityID, bool value);

        // PointLightComponent
        void (*PointLightComponent_GetColor)(uint64_t entityID, glm::vec4 *result);
        void (*PointLightComponent_SetColor)(uint64_t entityID, const glm::vec4 *value);
        void (*PointLightComponent_GetIntensity)(uint64_t entityID, float *result);
        void (*PointLightComponent_SetIntensity)(uint64_t entityID, float value);
        void (*PointLightComponent_GetRange)(uint64_t entityID, float *result);
        void (*PointLightComponent_SetRange)(uint64_t entityID, float value);
        void (*PointLightComponent_GetEnabled)(uint64_t entityID, bool *result);
        void (*PointLightComponent_SetEnabled)(uint64_t entityID, bool value);
        void (*PointLightComponent_GetConstantAttenuation)(uint64_t entityID, float *result);
        void (*PointLightComponent_SetConstantAttenuation)(uint64_t entityID, float value);
        void (*PointLightComponent_GetLinearAttenuation)(uint64_t entityID, float *result);
        void (*PointLightComponent_SetLinearAttenuation)(uint64_t entityID, float value);
        void (*PointLightComponent_GetQuadraticAttenuation)(uint64_t entityID, float *result);
        void (*PointLightComponent_SetQuadraticAttenuation)(uint64_t entityID, float value);

        // SpotLightComponent
        void (*SpotLightComponent_GetColor)(uint64_t entityID, glm::vec4 *result);
        void (*SpotLightComponent_SetColor)(uint64_t entityID, const glm::vec4 *value);
        void (*SpotLightComponent_GetIntensity)(uint64_t entityID, float *result);
        void (*SpotLightComponent_SetIntensity)(uint64_t entityID, float value);
        void (*SpotLightComponent_GetRange)(uint64_t entityID, float *result);
        void (*SpotLightComponent_SetRange)(uint64_t entityID, float value);
        void (*SpotLightComponent_GetEnabled)(uint64_t entityID, bool *result);
        void (*SpotLightComponent_SetEnabled)(uint64_t entityID, bool value);
        void (*SpotLightComponent_GetInnerConeAngle)(uint64_t entityID, float *result);
        void (*SpotLightComponent_SetInnerConeAngle)(uint64_t entityID, float value);
        void (*SpotLightComponent_GetOuterConeAngle)(uint64_t entityID, float *result);
        void (*SpotLightComponent_SetOuterConeAngle)(uint64_t entityID, float value);
        void (*SpotLightComponent_GetConstantAttenuation)(uint64_t entityID, float *result);
        void (*SpotLightComponent_SetConstantAttenuation)(uint64_t entityID, float value);
        void (*SpotLightComponent_GetLinearAttenuation)(uint64_t entityID, float *result);
        void (*SpotLightComponent_SetLinearAttenuation)(uint64_t entityID, float value);
        void (*SpotLightComponent_GetQuadraticAttenuation)(uint64_t entityID, float *result);
        void (*SpotLightComponent_SetQuadraticAttenuation)(uint64_t entityID, float value);

        // PointLight2DComponent
        void (*PointLight2DComponent_GetColor)(uint64_t entityID, glm::vec4 *result);
        void (*PointLight2DComponent_SetColor)(uint64_t entityID, const glm::vec4 *value);
        void (*PointLight2DComponent_GetRadius)(uint64_t entityID, float *result);
        void (*PointLight2DComponent_SetRadius)(uint64_t entityID, float value);
        void (*PointLight2DComponent_GetIntensity)(uint64_t entityID, float *result);
        void (*PointLight2DComponent_SetIntensity)(uint64_t entityID, float value);
        void (*PointLight2DComponent_GetEnabled)(uint64_t entityID, bool *result);
        void (*PointLight2DComponent_SetEnabled)(uint64_t entityID, bool value);

        // StaticMeshComponent
        void (*StaticMeshComponent_GetMeshHandle)(uint64_t entityID, uint64_t *result);
        void (*StaticMeshComponent_SetMeshHandle)(uint64_t entityID, uint64_t handle);
        void (*StaticMeshComponent_SetOverrideMaterial)(uint64_t entityID, int32_t slot, uint64_t materialHandle);
        void (*StaticMeshComponent_GetOverrideMaterial)(uint64_t entityID, int32_t slot, uint64_t *result);

        // WorldEnvironmentComponent
        void (*WorldEnvironmentComponent_GetExposure)(uint64_t entityID, float *result);
        void (*WorldEnvironmentComponent_SetExposure)(uint64_t entityID, float value);
        void (*WorldEnvironmentComponent_GetGamma)(uint64_t entityID, float *result);
        void (*WorldEnvironmentComponent_SetGamma)(uint64_t entityID, float value);
        void (*WorldEnvironmentComponent_GetAmbient)(uint64_t entityID, float *result);
        void (*WorldEnvironmentComponent_SetAmbient)(uint64_t entityID, float value);
        void (*WorldEnvironmentComponent_GetEnableIBL)(uint64_t entityID, bool *result);
        void (*WorldEnvironmentComponent_SetEnableIBL)(uint64_t entityID, bool value);
        void (*WorldEnvironmentComponent_GetFogDensity)(uint64_t entityID, float *result);
        void (*WorldEnvironmentComponent_SetFogDensity)(uint64_t entityID, float value);
        void (*WorldEnvironmentComponent_GetFogColor)(uint64_t entityID, glm::vec4 *result);
        void (*WorldEnvironmentComponent_SetFogColor)(uint64_t entityID, const glm::vec4 *value);
        void (*WorldEnvironmentComponent_GetFogStart)(uint64_t entityID, float *result);
        void (*WorldEnvironmentComponent_SetFogStart)(uint64_t entityID, float value);
        void (*WorldEnvironmentComponent_GetFogEnd)(uint64_t entityID, float *result);
        void (*WorldEnvironmentComponent_SetFogEnd)(uint64_t entityID, float value);
        void (*WorldEnvironmentComponent_MarkDirty)(uint64_t entityID);

        // ParticleSystemComponent
        void (*ParticleSystemComponent_GetIsPlaying)(uint64_t entityID, bool *result);
        void (*ParticleSystemComponent_SetIsPlaying)(uint64_t entityID, bool value);
        void (*ParticleSystemComponent_GetEmissionRate)(uint64_t entityID, float *result);
        void (*ParticleSystemComponent_SetEmissionRate)(uint64_t entityID, float value);
        void (*ParticleSystemComponent_GetMaxParticles)(uint64_t entityID, uint32_t *result);
        void (*ParticleSystemComponent_SetMaxParticles)(uint64_t entityID, uint32_t value);
        void (*ParticleSystemComponent_GetStartColor)(uint64_t entityID, glm::vec4 *result);
        void (*ParticleSystemComponent_SetStartColor)(uint64_t entityID, const glm::vec4 *value);
        void (*ParticleSystemComponent_GetEndColor)(uint64_t entityID, glm::vec4 *result);
        void (*ParticleSystemComponent_SetEndColor)(uint64_t entityID, const glm::vec4 *value);
        void (*ParticleSystemComponent_GetStartSize)(uint64_t entityID, glm::vec2 *result);
        void (*ParticleSystemComponent_SetStartSize)(uint64_t entityID, const glm::vec2 *value);
        void (*ParticleSystemComponent_GetEndSize)(uint64_t entityID, glm::vec2 *result);
        void (*ParticleSystemComponent_SetEndSize)(uint64_t entityID, const glm::vec2 *value);
        void (*ParticleSystemComponent_Play)(uint64_t entityID);
        void (*ParticleSystemComponent_Stop)(uint64_t entityID);
        void (*ParticleSystemComponent_Restart)(uint64_t entityID);
        void (*ParticleSystemComponent_SetPreset)(uint64_t entityID, int32_t preset);

        // NetworkIdentityComponent
        void (*NetworkIdentityComponent_GetNetId)(uint64_t entityID, uint64_t *result);
        void (*NetworkIdentityComponent_GetOwnerClient)(uint64_t entityID, uint32_t *result);
        void (*NetworkIdentityComponent_SetOwnerClient)(uint64_t entityID, uint32_t value);
        void (*NetworkIdentityComponent_GetIsLocalPlayer)(uint64_t entityID, bool *result);
        void (*NetworkIdentityComponent_SetIsLocalPlayer)(uint64_t entityID, bool value);
        void (*NetworkIdentityComponent_GetReplicate)(uint64_t entityID, bool *result);
        void (*NetworkIdentityComponent_SetReplicate)(uint64_t entityID, bool value);
        void (*NetworkIdentityComponent_GetPrefab)(uint64_t entityID, const char **result);
        void (*NetworkIdentityComponent_SetPrefab)(uint64_t entityID, const char *value);
        void (*NetworkIdentityComponent_GetReplicatedMovementSpeed)(uint64_t entityID, float *result);
        void (*NetworkIdentityComponent_SetReplicatedMovementSpeed)(uint64_t entityID, float value);
        void (*NetworkIdentityComponent_GetReplicatedIsGrounded)(uint64_t entityID, bool *result);
        void (*NetworkIdentityComponent_SetReplicatedIsGrounded)(uint64_t entityID, bool value);
        void (*NetworkIdentityComponent_GetSyncComponentsMask)(uint64_t entityID, uint32_t *result);
        void (*NetworkIdentityComponent_SetSyncComponentsMask)(uint64_t entityID, uint32_t value);
        void (*NetworkIdentityComponent_MarkDirty)(uint64_t entityID, uint32_t flags);

        // PlaneColliderComponent
        void (*PlaneColliderComponent_GetCenter)(uint64_t entityID, glm::vec3 *result);
        void (*PlaneColliderComponent_SetCenter)(uint64_t entityID, const glm::vec3 *value);
        void (*PlaneColliderComponent_GetScale)(uint64_t entityID, glm::vec3 *result);
        void (*PlaneColliderComponent_SetScale)(uint64_t entityID, const glm::vec3 *value);

        // TextComponent extensions
        void (*TextComponent_GetFontSize)(uint64_t entityID, float *result);
        void (*TextComponent_SetFontSize)(uint64_t entityID, float value);
        void (*TextComponent_GetScreenSpace)(uint64_t entityID, bool *result);
        void (*TextComponent_SetScreenSpace)(uint64_t entityID, bool value);
        void (*TextComponent_GetBillboard)(uint64_t entityID, bool *result);
        void (*TextComponent_SetBillboard)(uint64_t entityID, bool value);
        void (*TextComponent_GetCenterAlign)(uint64_t entityID, bool *result);
        void (*TextComponent_SetCenterAlign)(uint64_t entityID, bool value);

        // WidgetComponent extensions
        void (*WidgetComponent_GetCanvasEnabled)(uint64_t entityID, bool *result);
        void (*WidgetComponent_SetCanvasEnabled)(uint64_t entityID, bool enabled);
        bool (*WidgetComponent_HasItem)(uint64_t entityID, const char *itemName);
        void (*WidgetComponent_GetItemVisible)(uint64_t entityID, const char *itemName, bool *result);
        void (*WidgetComponent_SetItemVisible)(uint64_t entityID, const char *itemName, bool visible);
        void (*WidgetComponent_GetItemBackgroundColor)(uint64_t entityID, const char *itemName, glm::vec4 *result);
        void (*WidgetComponent_SetItemBackgroundColor)(uint64_t entityID, const char *itemName, const glm::vec4 *color);
        void (*WidgetComponent_GetItemBorderColor)(uint64_t entityID, const char *itemName, glm::vec4 *result);
        void (*WidgetComponent_SetItemBorderColor)(uint64_t entityID, const char *itemName, const glm::vec4 *color);
        void (*WidgetComponent_GetItemOpacity)(uint64_t entityID, const char *itemName, float *result);
        void (*WidgetComponent_SetItemOpacity)(uint64_t entityID, const char *itemName, float opacity);
        void (*WidgetComponent_GetItemSize)(uint64_t entityID, const char *itemName, glm::vec2 *result);
        void (*WidgetComponent_SetItemSize)(uint64_t entityID, const char *itemName, const glm::vec2 *size);
        void (*WidgetComponent_GetItemPosition)(uint64_t entityID, const char *itemName, glm::vec2 *result);
        void (*WidgetComponent_SetItemPosition)(uint64_t entityID, const char *itemName, const glm::vec2 *pos);
        void (*WidgetComponent_GetButtonText)(uint64_t entityID, const char *buttonName, const char **result);
        void (*WidgetComponent_SetButtonText)(uint64_t entityID, const char *buttonName, const char *text);
        void (*WidgetComponent_GetButtonInteractable)(uint64_t entityID, const char *buttonName, bool *result);
        void (*WidgetComponent_SetButtonInteractable)(uint64_t entityID, const char *buttonName, bool interactable);
        void (*WidgetComponent_IsButtonHovered)(uint64_t entityID, const char *buttonName, bool *result);
        void (*WidgetComponent_SetButtonColors)(uint64_t entityID, const char *buttonName, const glm::vec4 *normalColor, const glm::vec4 *hoverColor, const glm::vec4 *pressedColor);
    };

    class ComponentScriptGlue
    {
    public:
        static void RegisterComponents();
        static void RegisterFunctions();
        static const ComponentScriptGlueAPI *GetAPI();
    };
}

#endif
