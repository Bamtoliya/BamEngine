#pragma once

#include "Reflection/ComponentAnnotations.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace Engine
{
    STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_TRANSFORM"), CATEGORY("Core"))
    struct TransformComponent
    {
        REFLECT_BODY()

        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_POSITION"), RESET_VALUE(0.0), STEP(0.1), SNAP_STEP(1.0))
        glm::vec3 position{ 0.0f };

        PROPERTY(READONLY, VISIBLE(false), DISPLAY_NAME("PROP_ROTATION"))
        glm::quat rotation = glm::quat::wxyz(1.0f, 0.0f, 0.0f, 0.0f);

        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_EULER_ROTATION"), RESET_VALUE(0.0), STEP(0.5), SNAP_STEP(15.0))
        glm::vec3 eulerRotation{ 0.0f };

        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_SCALE"), RESET_VALUE(1.0), STEP(0.01), SNAP_STEP(0.1))
        glm::vec3 scale{ 1.0f };

        [[nodiscard]]
        bool SetLocalPosition(const glm::vec3& value)
        {
            if (!IsFinite(value))
            {
                return false;
            }

            position = value;
            return true;
        }

        [[nodiscard]]
        bool SetLocalScale(const glm::vec3& value)
        {
            if (!IsFinite(value))
            {
                return false;
            }

            // 음수와 0도 유효한 로컬 크기로 허용합니다.
            scale = value;
            return true;
        }

        [[nodiscard]]
        bool TranslateLocal(const glm::vec3& delta)
        {
            if (!IsFinite(delta) || !SynchronizeRotation())
            {
                return false;
            }

            return SetLocalPosition(position + rotation * delta);
        }

        [[nodiscard]]
        bool TranslateParentSpace(const glm::vec3& delta)
        {
            if (!IsFinite(delta))
            {
                return false;
            }

            return SetLocalPosition(position + delta);
        }

        // rotation에 직접 대입했다면 먼저 SynchronizeRotation()을 호출해야 합니다.
        [[nodiscard]]
        glm::mat4 GetLocalMatrix() const
        {
            return glm::translate(glm::mat4(1.0f), position) *
                glm::mat4_cast(rotation) * glm::scale(glm::mat4(1.0f), scale);
        }

        [[nodiscard]]
        glm::vec3 GetLocalRight() const
        {
            return rotation * glm::vec3(1.0f, 0.0f, 0.0f);
        }

        [[nodiscard]]
        glm::vec3 GetLocalUp() const
        {
            return rotation * glm::vec3(0.0f, 1.0f, 0.0f);
        }

        [[nodiscard]]
        glm::vec3 GetLocalForward() const
        {
            return rotation * glm::vec3(0.0f, 0.0f, 1.0f);
        }

        // 입력한 각도와 누적값을 그대로 보존합니다.
        [[nodiscard]]
        bool SetEulerRotation(const glm::vec3& degrees)
        {
            if (!IsFinite(degrees))
            {
                return false;
            }

            CommitRotation(QuaternionFromEuler(degrees), degrees);
            return true;
        }

        // 절대적인 로컬 회전을 설정합니다.
        // Quaternion만으로는 추가 회전 횟수를 알 수 없습니다.
        [[nodiscard]]
        bool SetLocalRotation(const glm::quat& value)
        {
            glm::quat candidate;

            if (!TryNormalizeRotation(value, candidate))
            {
                return false;
            }

            glm::vec3 hint = m_SynchronizedEuler;

            if (!IsSameRotation(candidate, m_SynchronizedRotation))
            {
                hint = ClosestEuler(candidate, hint);
            }

            if (!IsFinite(hint))
            {
                return false;
            }

            CommitRotation(candidate, hint);
            return true;
        }

        // 저장된 회전과 Euler 표현을 복원하고 동기화 캐시도 갱신합니다.
        [[nodiscard]]
        bool RestoreLocalRotation(const glm::quat& value, const glm::vec3& eulerHint)
        {
            glm::quat candidate;

            if (!IsFinite(eulerHint) || !TryNormalizeRotation(value, candidate))
            {
                return false;
            }

            glm::vec3 restoredEuler = eulerHint;

            if (!IsSameRotation(candidate, QuaternionFromEuler(eulerHint)))
            {
                restoredEuler = ClosestEuler(candidate, eulerHint);

                if (!IsFinite(restoredEuler))
                {
                    return false;
                }
            }

            CommitRotation(candidate, restoredEuler);
            return true;
        }

        // 현재 객체의 로컬 축을 기준으로 회전합니다.
        [[nodiscard]]
        bool RotateLocal(const glm::vec3& axis, double deltaDegrees)
        {
            return ApplyRotation(axis, deltaDegrees, true);
        }

        // 부모 좌표계의 축을 기준으로 회전합니다.
        // 부모가 회전했다면 월드 공간 회전과 다릅니다.
        [[nodiscard]]
        bool RotateParentSpace(const glm::vec3& axis, double deltaDegrees)
        {
            return ApplyRotation(axis, deltaDegrees, false);
        }

        // Reflection 등 기존의 직접 멤버 대입을 연결하는 호환 경로입니다.
        [[nodiscard]]
        bool SynchronizeRotation()
        {
            const bool eulerChanged = eulerRotation != m_SynchronizedEuler;
            const bool quaternionChanged = rotation != m_SynchronizedRotation;

            if (eulerChanged)
            {
                if (!IsFinite(eulerRotation))
                {
                    return RejectRotationChange();
                }

                if (quaternionChanged)
                {
                    glm::quat candidate;

                    if (!TryNormalizeRotation(rotation, candidate) ||
                        !IsSameRotation(candidate, QuaternionFromEuler(eulerRotation)))
                    {
                        return RejectRotationChange();
                    }
                }

                return SetEulerRotation(eulerRotation);
            }

            if (quaternionChanged && !SetLocalRotation(rotation))
            {
                return RejectRotationChange();
            }

            return true;
        }

    private:
        static bool IsFinite(const glm::vec3& value)
        {
            return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
        }

        static glm::dquat ToDouble(const glm::quat& value)
        {
            return glm::dquat::wxyz(value.w, value.x, value.y, value.z);
        }

        static glm::quat ToFloat(const glm::dquat& value)
        {
            return glm::normalize(glm::quat::wxyz(static_cast<float>(value.w),
                static_cast<float>(value.x), static_cast<float>(value.y), static_cast<float>(value.z)));
        }

        static bool TryNormalizeRotation(const glm::quat& value, glm::quat& normalized)
        {
            const glm::dquat input = ToDouble(value);
            const double length = glm::length(input);

            if (!std::isfinite(length) || length < 1.0e-12)
            {
                return false;
            }

            normalized = ToFloat(input / length);
            return true;
        }

        static glm::quat QuaternionFromEuler(const glm::vec3& degrees)
        {
            const glm::dvec3 reduced(std::remainder(static_cast<double>(degrees.x), 360.0),
                std::remainder(static_cast<double>(degrees.y), 360.0),
                std::remainder(static_cast<double>(degrees.z), 360.0));

            return ToFloat(glm::normalize(glm::dquat(glm::radians(reduced))));
        }

        static bool IsSameRotation(const glm::quat& left, const glm::quat& right)
        {
            const glm::dvec4 a(left.x, left.y, left.z, left.w);
            const glm::dvec4 b(right.x, right.y, right.z, right.w);
            const glm::dvec4 difference = a - b;
            const glm::dvec4 oppositeDifference = a + b;

            return std::min(glm::dot(difference, difference),
                glm::dot(oppositeDifference, oppositeDifference)) <= 1.0e-12;
        }

        static double UnwrapDegrees(double value, double reference)
        {
            return value + 360.0 * std::round((reference - value) / 360.0);
        }

        static glm::dvec3 UnwrapEuler(glm::dvec3 value, const glm::dvec3& reference)
        {
            for (int axis = 0; axis < 3; ++axis)
            {
                value[axis] = UnwrapDegrees(value[axis], reference[axis]);
            }

            return value;
        }

        static double EulerDistanceSquared(const glm::dvec3& value, const glm::dvec3& reference)
        {
            const glm::dvec3 difference = value - reference;
            return glm::dot(difference, difference);
        }

        static glm::vec3 ClosestEuler(const glm::quat& value, const glm::vec3& previous)
        {
            const glm::dquat normalized = glm::normalize(ToDouble(value));
            const glm::dvec3 base = glm::degrees(glm::eulerAngles(normalized));
            const glm::dvec3 reference(previous);

            glm::dvec3 best = UnwrapEuler(base, reference);
            double bestDistance = EulerDistanceSquared(best, reference);

            const glm::dvec3 alternate = UnwrapEuler(
                glm::dvec3(base.x + 180.0, 180.0 - base.y, base.z + 180.0), reference);

            const double alternateDistance = EulerDistanceSquared(alternate, reference);

            if (alternateDistance < bestDistance)
            {
                best = alternate;
                bestDistance = alternateDistance;
            }

            // GLM의 Euler 구성에서 Y가 ±90도이면 X와 Z가 서로 결합됩니다.
            // 기존 Hint에 가까운 조합을 후보로 만듭니다.
            if (std::abs(std::cos(glm::radians(base.y))) <= 1.0e-6)
            {
                const double sign = base.y >= 0.0 ? 1.0 : -1.0;
                const double coupled = glm::degrees(2.0 * std::atan2(normalized.x, normalized.w));
                const double previousCoupled = reference.x - sign * reference.z;
                const double difference = UnwrapDegrees(coupled, previousCoupled) - previousCoupled;

                const glm::dvec3 singular(reference.x + difference * 0.5,
                    UnwrapDegrees(sign * 90.0, reference.y),
                    reference.z - sign * difference * 0.5);

                const glm::vec3 candidate(singular);
                const double distance = EulerDistanceSquared(singular, reference);

                // 허용 오차 안에서 같은 자세를 나타내는 경우에만 사용합니다.
                if (IsFinite(candidate) && distance < bestDistance &&
                    IsSameRotation(QuaternionFromEuler(candidate), value))
                {
                    best = singular;
                }
            }

            return glm::vec3(best);
        }

        bool ApplyRotation(const glm::vec3& axis, double deltaDegrees, bool localSpace)
        {
            constexpr double StepDegrees = 30.0;
            constexpr int MaxSteps = 4096;

            if (!IsFinite(axis) || !std::isfinite(deltaDegrees) ||
                std::abs(deltaDegrees) > StepDegrees * MaxSteps)
            {
                return false;
            }

            const glm::dvec3 inputAxis(axis);
            const double axisLength = glm::length(inputAxis);

            if (!std::isfinite(axisLength) || axisLength < 1.0e-12)
            {
                return false;
            }

            if (!SynchronizeRotation())
            {
                return false;
            }

            if (deltaDegrees == 0.0)
            {
                return true;
            }

            // 알려진 회전 경로를 작은 구간으로 따라가며 Hint를 갱신합니다.
            const int steps = std::max(1, static_cast<int>(std::ceil(std::abs(deltaDegrees) / StepDegrees)));
            const double stepRadians = glm::radians(deltaDegrees / static_cast<double>(steps));
            const glm::dquat delta = glm::angleAxis(stepRadians, inputAxis / axisLength);

            glm::dquat current = ToDouble(rotation);
            glm::quat candidate = rotation;
            glm::vec3 hint = eulerRotation;

            for (int step = 0; step < steps; ++step)
            {
                current = glm::normalize(localSpace ? current * delta : delta * current);
                candidate = ToFloat(current);
                hint = ClosestEuler(candidate, hint);

                if (!IsFinite(hint))
                {
                    return false;
                }
            }

            // Hint에서 실제 회전을 재구성하지 않습니다.
            CommitRotation(candidate, hint);
            return true;
        }

        void CommitRotation(glm::quat value, const glm::vec3& hint)
        {
            // 같은 자세의 Quaternion 부호가 불필요하게 뒤집히지 않게 합니다.
            if (glm::dot(value, m_SynchronizedRotation) < 0.0f)
            {
                value = -value;
            }

            rotation = value;
            eulerRotation = hint;
            m_SynchronizedRotation = rotation;
            m_SynchronizedEuler = eulerRotation;
        }

        bool RejectRotationChange()
        {
            rotation = m_SynchronizedRotation;
            eulerRotation = m_SynchronizedEuler;
            return false;
        }

        glm::vec3 m_SynchronizedEuler{ 0.0f };
        glm::quat m_SynchronizedRotation = glm::quat::wxyz(1.0f, 0.0f, 0.0f, 0.0f);
    };
}