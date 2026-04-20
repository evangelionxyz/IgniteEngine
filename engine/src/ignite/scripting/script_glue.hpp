// Copyright (c) 2026 Evangelion Manuhutu

#ifndef SCRIPT_GLUE_HPP
#define SCRIPT_GLUE_HPP

#include <cstdint>
#include <glm/glm.hpp>

namespace ignite
{
    struct ScriptGlueAPI
    {
        void (*Debug_Log)(const char *message);

        uint64_t(*Scene_PickEntityAt)(float mouseX, float mouseY, glm::vec2 viewportMin, glm::vec2 viewportMax);

        bool (*Entity_HasComponent)(uint64_t entityID, const char *componentTypeName);
        void (*Entity_AddComponent)(uint64_t entityID, const char *componentTypeName);
        uint64_t (*Entity_FindEntityByName)(const char *name);
        uint64_t (*Entity_Instantiate)(uint64_t entityID, glm::vec3 value);
        void (*Entity_Destroy)(uint64_t entityID);
        void (*Entity_SetVisibility)(uint64_t entityID, bool value);
        void (*Entity_GetVisibility)(uint64_t entityID, bool *result);
        bool (*WidgetComponent_HasButton)(uint64_t entityID, const char *buttonName);
        bool (*WidgetComponent_AddButtonEventCallback)(uint64_t entityID, const char *buttonName, int32_t eventType, const char *methodName);
        bool (*WidgetComponent_RemoveButtonEventCallback)(uint64_t entityID, const char *buttonName, int32_t eventType, const char *methodName);

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

        bool (*Input_IsKeyPressed)(uint32_t keyCode);
        bool (*Input_IsModifierPressed)(uint16_t modCode);
        bool (*Input_IsMouseButtonPressed)(uint8_t button);
        void (*Input_GetMousePosition)(glm::vec2 *result);
        void (*Input_SetMouseToCenter)();
        void (*Input_SetCursorMode)(int32_t mode);

        void (*TransformComponent_GetForward)(uint64_t entityID, glm::vec3 *result);
        void (*TransformComponent_SetForward)(uint64_t entityID, glm::vec3 value);
        void (*TransformComponent_GetRight)(uint64_t entityID, glm::vec3 *result);
        void (*TransformComponent_SetRight)(uint64_t entityID, glm::vec3 value);
        void (*TransformComponent_GetUp)(uint64_t entityID, glm::vec3 *result);
        void (*TransformComponent_SetUp)(uint64_t entityID, glm::vec3 value);
        void (*TransformComponent_GetTranslation)(uint64_t entityID, glm::vec3 *result);
        void (*TransformComponent_SetTranslation)(uint64_t entityID, glm::vec3 value);
        void (*TransformComponent_GetRotation)(uint64_t entityID, glm::quat *result);
        void (*TransformComponent_SetRotation)(uint64_t entityID, glm::quat value);
        void (*TransformComponent_GetEulerAngles)(uint64_t entityID, glm::vec3 *result);
        void (*TransformComponent_SetEulerAngles)(uint64_t entityID, glm::vec3 value);
        void (*TransformComponent_GetScale)(uint64_t entityID, glm::vec3 *result);
        void (*TransformComponent_SetScale)(uint64_t entityID, glm::vec3 value);

		void (*Sprite2DComponent_SetColor)(uint64_t entityID, glm::vec4 color);
		void (*Sprite2DComponent_GetColor)(uint64_t entityID, glm::vec4 *result);
		void (*Sprite2DComponent_SetTilingFactor)(uint64_t entityID, glm::vec2 tiling);
		void (*Sprite2DComponent_GetTilingFactor)(uint64_t entityID, glm::vec2 *result);

		void (*Circle2DComponent_SetColor)(uint64_t entityID, glm::vec4 color);
		void (*Circle2DComponent_GetColor)(uint64_t entityID, glm::vec4 *result);

        void (*Rigidbody2DComponent_GetType)(uint64_t entityID, int32_t *result);
        void (*Rigidbody2DComponent_SetType)(uint64_t entityID, int32_t value);
        void (*Rigidbody2DComponent_GetLinearVelocity)(uint64_t entityID, glm::vec2 *result);
        void (*Rigidbody2DComponent_SetLinearVelocity)(uint64_t entityID, glm::vec2 value);
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
    };

    class ScriptGlue
    {
    public:
        static void RegisterComponents();
        static void RegisterFunctions();
        static const ScriptGlueAPI *GetAPI();
    };
}

#endif