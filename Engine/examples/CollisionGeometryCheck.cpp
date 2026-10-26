#include "Engine_Includes.h"
#include "Collision.h"

namespace
{
    using namespace Engine;

    int Fail(const char* message) { std::cerr << message << '\n'; return 1; }
    bool Near(f32 left, f32 right) { return glm::abs(left - right) < 0.0001f; }

    int CheckCapsuleGeometry()
    {
        Capsule empty;
        if (!empty.IsValid() || !empty.Contains(vec3(0.f)))
            return Fail("Default capsule is not a valid point.");

        const Capsule capsule(vec3(0.f, -1.f, 0.f), vec3(0.f, 1.f, 0.f), 0.5f);
        if (!capsule.Contains(vec3(0.5f, 0.f, 0.f)) ||
            capsule.Contains(vec3(0.6f, 0.f, 0.f)) ||
            !capsule.Contains(vec3(0.f, 1.5f, 0.f)))
            return Fail("Capsule containment failed.");

        HitResult hit{};
        const BoundingSphere side(vec3(0.75f, 0.f, 0.f), 0.5f);
        if (!Collision::Intersects(capsule, side, hit) ||
            !Near(hit.denetrationDepth, 0.25f) || !Near(hit.normal.x, -1.f))
            return Fail("Capsule side contact failed.");

        if (!Collision::Intersects(side, capsule, hit) || !Near(hit.normal.x, 1.f))
            return Fail("Reversed capsule contact failed.");

        const BoundingSphere end(vec3(0.f, 1.75f, 0.f), 0.5f);
        if (!Collision::Intersects(capsule, end, hit) ||
            !Near(hit.denetrationDepth, 0.25f) || !Near(hit.normal.y, -1.f))
            return Fail("Capsule end contact failed.");

        const Capsule horizontal(vec3(-1.f, 0.f, 0.f), vec3(1.f, 0.f, 0.f), 0.5f);
        const BoundingSphere centered(vec3(0.f), 0.25f);
        if (!Collision::Intersects(horizontal, centered, hit) ||
            !Near(hit.denetrationDepth, 0.75f) || !Near(hit.normal.x, 0.f) ||
            !Near(glm::length(hit.normal), 1.f) || !Near(hit.normal.z, 0.f))
            return Fail("Capsule center-line normal failed.");

        const Capsule pointCapsule(vec3(0.f), vec3(0.f), 0.5f);
        if (!Collision::Intersects(pointCapsule, side, hit) ||
            !Near(hit.denetrationDepth, 0.25f))
            return Fail("Zero-length capsule contact failed.");

        const BoundingSphere separated(vec3(3.f, 0.f, 0.f), 0.5f);
        if (Collision::Intersects(capsule, separated, hit) || hit.hasHit)
            return Fail("Capsule miss retained an old hit.");

        Capsule invalid = capsule;
        invalid.radius = -0.5f;
        if (invalid.IsValid() || invalid.Contains(vec3(0.f)) ||
            Collision::Intersects(invalid, side, hit))
            return Fail("Negative capsule radius was accepted.");

        const Capsule large(vec3(0.f), vec3(1.0e20f, 0.f, 0.f), 1.0e20f);
        if (large.Contains(vec3(0.f, 3.0e20f, 0.f)))
            return Fail("Large capsule containment overflowed.");

        return 0;
    }

    int CheckCapsule3DContacts()
    {
        const Capsule capsule(vec3(0.f, -1.f, 0.f), vec3(0.f, 1.f, 0.f), 0.5f);

        // Z좌표도 거리 계산과 접촉 법선에 반영되어야 합니다.
        if (!capsule.Contains(vec3(0.f, 0.f, 0.5f)) ||
            capsule.Contains(vec3(0.f, 0.f, 0.6f)))
            return Fail("Capsule containment ignored the Z coordinate.");

        HitResult hit{};
        const BoundingSphere front(vec3(0.f, 0.f, 0.75f), 0.5f);
        if (!Collision::Intersects(capsule, front, hit) ||
            !Near(hit.denetrationDepth, 0.25f) ||
            !Near(hit.normal.x, 0.f) || !Near(hit.normal.y, 0.f) ||
            !Near(hit.normal.z, -1.f) || !Near(hit.point.z, 0.5f))
            return Fail("Capsule Z-axis contact failed.");

        if (!Collision::Intersects(front, capsule, hit) || !Near(hit.normal.z, 1.f))
            return Fail("Reversed capsule Z-axis contact failed.");

        const BoundingSphere separated(vec3(0.f, 0.f, 2.f), 0.5f);
        if (Collision::Intersects(capsule, separated) ||
            Collision::Intersects(capsule, separated, hit) || hit.hasHit)
            return Fail("Capsule Z-axis separation failed.");

        // 세 축으로 기울어진 중심선의 최근접점을 검사합니다.
        const Capsule tilted(vec3(-1.f), vec3(1.f), 0.5f);
        const vec3 radial = glm::normalize(vec3(1.f, -1.f, 0.f));
        const BoundingSphere offset(radial * 0.75f, 0.5f);

        if (!Collision::Intersects(tilted, offset, hit) ||
            !Near(hit.denetrationDepth, 0.25f) ||
            glm::length(hit.normal + radial) > 0.0001f)
            return Fail("Tilted capsule contact failed.");

        // Z축 중심선 위의 구에도 중심선과 수직인 분리 방향이 필요합니다.
        const Capsule alongZ(vec3(0.f, 0.f, -1.f), vec3(0.f, 0.f, 1.f), 0.5f);
        const BoundingSphere centered(vec3(0.f), 0.25f);

        if (!Collision::Intersects(alongZ, centered, hit) ||
            !Near(hit.denetrationDepth, 0.75f) ||
            !Near(glm::length(hit.normal), 1.f) || !Near(hit.normal.z, 0.f))
            return Fail("Z-axis capsule center-line normal failed.");

        Capsule moved = alongZ;
        const vec3 correction = hit.normal * (hit.denetrationDepth + 0.001f);
        moved.pointA += correction;
        moved.pointB += correction;

        if (Collision::Intersects(moved, centered))
            return Fail("Capsule correction did not separate the shapes.");

        return 0;
    }

    int CheckCapsuleCircleContacts()
    {
        const Capsule capsule(vec3(0.f, -1.f, 0.f), vec3(0.f, 1.f, 0.f), 0.5f);
        const Circle side(vec2(0.75f, 0.f), 0.5f);
        HitResult hit{};

        if (!Collision::Intersects(capsule, side) ||
            !Collision::Intersects(capsule, side, hit) || !hit.hasHit ||
            !Near(hit.denetrationDepth, 0.25f) ||
            !Near(hit.normal.x, -1.f) || !Near(hit.normal.y, 0.f) ||
            !Near(hit.normal.z, 0.f) || !Near(hit.point.z, 0.f))
            return Fail("Capsule-circle side contact failed.");

        if (!Collision::Intersects(side, capsule) ||
            !Collision::Intersects(side, capsule, hit) ||
            !Near(hit.normal.x, 1.f) || !Near(hit.normal.z, 0.f))
            return Fail("Reversed capsule-circle contact failed.");

        const Circle end(vec2(0.f, 1.75f), 0.5f);
        if (!Collision::Intersects(capsule, end, hit) ||
            !Near(hit.denetrationDepth, 0.25f) || !Near(hit.normal.y, -1.f))
            return Fail("Capsule-circle end contact failed.");

        const Circle centered(vec2(0.f), 0.25f);
        if (!Collision::Intersects(capsule, centered, hit) ||
            !Near(hit.denetrationDepth, 0.75f) ||
            !Near(glm::length(hit.normal), 1.f) || !Near(hit.normal.z, 0.f))
            return Fail("Capsule-circle center-line contact failed.");

        const Circle tangent(vec2(1.f, 0.f), 0.5f);
        if (!Collision::Intersects(capsule, tangent, hit) ||
            !Near(hit.denetrationDepth, 0.f))
            return Fail("Capsule-circle tangency failed.");

        const Circle separated(vec2(2.f, 0.f), 0.5f);
        if (Collision::Intersects(capsule, separated) ||
            Collision::Intersects(capsule, separated, hit) || hit.hasHit)
            return Fail("Capsule-circle miss retained an old hit.");

        // 어느 한 끝점이라도 XY 평면 밖이면 거부해야 합니다.
        for (const bool changeFirst : { false, true })
        {
            Capsule outside = capsule;
            if (changeFirst)
                outside.pointA.z = 0.1f;
            else
                outside.pointB.z = 0.1f;

            hit.hasHit = true;
            if (Collision::Intersects(outside, side) ||
                Collision::Intersects(outside, side, hit) || hit.hasHit)
                return Fail("Capsule-circle accepted a non-planar capsule.");

            hit.hasHit = true;
            if (Collision::Intersects(side, outside) ||
                Collision::Intersects(side, outside, hit) || hit.hasHit)
                return Fail("Reversed capsule-circle accepted a non-planar capsule.");
        }

        Circle invalid = side;
        invalid.radius = -0.5f;
        hit.hasHit = true;
        if (Collision::Intersects(capsule, invalid) ||
            Collision::Intersects(capsule, invalid, hit) || hit.hasHit)
            return Fail("Capsule-circle accepted an invalid circle.");

        return 0;
    }

    int CheckCapsuleRectContacts()
    {
        const Rect rect(-1.f, -1.f, 2.f, 2.f);

        const auto check = [&](const Capsule& capsule, f32 expectedDepth, const vec2& expectedNormal)
            {
                HitResult hit{}, reversed{};
                if (!Collision::Intersects(capsule, rect) ||
                    !Collision::Intersects(capsule, rect, hit) || !hit.hasHit ||
                    !Near(hit.denetrationDepth, expectedDepth) ||
                    glm::length(vec2(hit.normal) - expectedNormal) > 0.0001f ||
                    !Near(hit.normal.z, 0.f) || !Near(hit.point.z, 0.f))
                    return false;

                if (!Collision::Intersects(rect, capsule) ||
                    !Collision::Intersects(rect, capsule, reversed) ||
                    !Near(reversed.denetrationDepth, hit.denetrationDepth) ||
                    glm::length(reversed.normal + hit.normal) > 0.0001f)
                    return false;

                Capsule moved = capsule;
                const vec3 correction = hit.normal * (hit.denetrationDepth + 0.001f);
                moved.pointA += correction;
                moved.pointB += correction;
                return !Collision::Intersects(moved, rect);
            };

        const Capsule side(vec3(1.25f, -0.5f, 0.f), vec3(1.25f, 0.5f, 0.f), 0.5f);
        if (!check(side, 0.25f, vec2(1.f, 0.f)))
            return Fail("Capsule-rect side contact failed.");

        const Capsule end(vec3(0.f, 1.25f, 0.f), vec3(0.f, 2.25f, 0.f), 0.5f);
        if (!check(end, 0.25f, vec2(0.f, 1.f)))
            return Fail("Capsule-rect end contact failed.");

        const Capsule corner(vec3(1.3f, 1.4f, 0.f), vec3(1.3f, 1.4f, 0.f), 0.75f);
        if (!check(corner, 0.25f, vec2(0.6f, 0.8f)))
            return Fail("Capsule-rect corner contact failed.");

        const Capsule contained(vec3(0.f, -0.25f, 0.f), vec3(0.f, 0.25f, 0.f), 0.25f);
        if (!check(contained, 1.25f, vec2(-1.f, 0.f)))
            return Fail("Contained capsule-rect contact failed.");

        const Capsule crossing(vec3(-2.f, 0.f, 0.f), vec3(2.f, 0.f, 0.f), 0.25f);
        if (!check(crossing, 1.25f, vec2(0.f, -1.f)))
            return Fail("Crossing capsule-rect contact failed.");

        const Capsule tilted(vec3(-2.f, -2.f, 0.f), vec3(2.f, 2.f, 0.f), 0.25f);
        if (!check(tilted, std::sqrt(2.f) + 0.25f, glm::normalize(vec2(1.f, -1.f))))
            return Fail("Tilted capsule-rect contact failed.");

        const Capsule tangent(vec3(1.5f, -0.5f, 0.f), vec3(1.5f, 0.5f, 0.f), 0.5f);
        if (!check(tangent, 0.f, vec2(1.f, 0.f)))
            return Fail("Capsule-rect tangency failed.");

        // X·Y 투영은 겹치지만 둥근 끝과 박스 모서리 사이에는 틈이 있습니다.
        const Capsule separated(vec3(1.4f, 1.4f, 0.f), vec3(1.4f, 1.4f, 0.f), 0.5f);
        HitResult hit{};
        hit.hasHit = true;
        if (Collision::Intersects(separated, rect) ||
            Collision::Intersects(separated, rect, hit) || hit.hasHit)
            return Fail("Capsule-rect corner separation failed.");

        Capsule outside = side;
        outside.pointB.z = 0.1f;
        hit.hasHit = true;
        if (Collision::Intersects(outside, rect) ||
            Collision::Intersects(outside, rect, hit) || hit.hasHit)
            return Fail("Capsule-rect accepted a non-planar capsule.");

        const Rect invalid(-1.f, -1.f, 0.f, 2.f);
        hit.hasHit = true;
        if (Collision::Intersects(side, invalid) ||
            Collision::Intersects(side, invalid, hit) || hit.hasHit)
            return Fail("Capsule-rect accepted an invalid rect.");

        return 0;
    }

    int CheckCapsulePairContacts()
    {
        const auto check = [](const Capsule& a, const Capsule& b, f32 expectedDepth)
            {
                HitResult hit{}, reversed{};
                if (!Collision::Intersects2D(a, b) ||
                    !Collision::Intersects2D(a, b, hit) || !hit.hasHit ||
                    !Near(hit.denetrationDepth, expectedDepth) ||
                    !Near(glm::length(hit.normal), 1.f) ||
                    !Near(hit.normal.z, 0.f) || !Near(hit.point.z, 0.f))
                    return false;

                if (!Collision::Intersects2D(b, a) ||
                    !Collision::Intersects2D(b, a, reversed) || !reversed.hasHit ||
                    !Near(reversed.denetrationDepth, expectedDepth) ||
                    !Near(glm::length(reversed.normal), 1.f) ||
                    !Near(reversed.normal.z, 0.f))
                    return false;

                Capsule movedA = a;
                const vec3 correctionA = hit.normal * (hit.denetrationDepth + 0.001f);
                movedA.pointA += correctionA;
                movedA.pointB += correctionA;
                if (Collision::Intersects2D(movedA, b))
                    return false;

                Capsule movedB = b;
                const vec3 correctionB = reversed.normal * (reversed.denetrationDepth + 0.001f);
                movedB.pointA += correctionB;
                movedB.pointB += correctionB;
                return !Collision::Intersects2D(a, movedB);
            };

        const Capsule vertical(vec3(0.f, -1.f, 0.f), vec3(0.f, 1.f, 0.f), 0.5f);
        const Capsule side(vec3(0.75f, -1.f, 0.f), vec3(0.75f, 1.f, 0.f), 0.5f);
        if (!check(vertical, side, 0.25f))
            return Fail("Parallel capsule contact failed.");

        const Capsule end(vec3(0.f, 1.75f, 0.f), vec3(0.f, 2.75f, 0.f), 0.5f);
        if (!check(vertical, end, 0.25f))
            return Fail("Capsule end contact failed.");

        const Capsule corner(vec3(0.6f, 1.8f, 0.f), vec3(0.6f, 1.8f, 0.f), 0.75f);
        if (!check(vertical, corner, 0.25f))
            return Fail("Capsule curved-end contact failed.");

        if (!check(vertical, vertical, 1.f))
            return Fail("Coincident capsule contact failed.");

        // 반지름 합은 0.5지만, XY 평면에서 분리하려면 2.5만큼 이동해야 합니다.
        const Capsule horizontal(vec3(-2.f, 0.f, 0.f), vec3(2.f, 0.f, 0.f), 0.25f);
        const Capsule crossing(vec3(0.f, -2.f, 0.f), vec3(0.f, 2.f, 0.f), 0.25f);
        if (!check(horizontal, crossing, 2.5f))
            return Fail("Crossing capsule penetration depth failed.");

        const Capsule point(vec3(0.f), vec3(0.f), 0.25f);
        if (!check(point, vertical, 0.75f))
            return Fail("Point capsule contact failed.");

        const Capsule tangent(vec3(1.f, -1.f, 0.f), vec3(1.f, 1.f, 0.f), 0.5f);
        if (!check(vertical, tangent, 0.f))
            return Fail("Capsule pair tangency failed.");

        Capsule reversedEndpoints = vertical;
        std::swap(reversedEndpoints.pointA, reversedEndpoints.pointB);
        if (!check(reversedEndpoints, side, 0.25f))
            return Fail("Reversed capsule endpoints changed contact.");

        const Capsule firstPoint(vec3(0.f), vec3(0.f), 0.5f);
        const Capsule separated(vec3(0.8f, 0.8f, 0.f), vec3(0.8f, 0.8f, 0.f), 0.5f);
        HitResult hit{};
        hit.hasHit = true;
        if (Collision::Intersects2D(firstPoint, separated) ||
            Collision::Intersects2D(firstPoint, separated, hit) || hit.hasHit)
            return Fail("Separated capsule pair retained an old hit.");

        Capsule outside = side;
        outside.pointA.z = 0.1f;
        hit.hasHit = true;
        if (Collision::Intersects2D(vertical, outside, hit) || hit.hasHit)
            return Fail("Capsule pair accepted a non-planar second shape.");

        hit.hasHit = true;
        if (Collision::Intersects2D(outside, vertical, hit) || hit.hasHit)
            return Fail("Capsule pair accepted a non-planar first shape.");

        Capsule invalid = side;
        invalid.radius = -0.5f;
        hit.hasHit = true;
        if (Collision::Intersects2D(vertical, invalid, hit) || hit.hasHit)
            return Fail("Capsule pair accepted an invalid radius.");

        return 0;
    }

    int CheckTransformedRectContacts()
    {
        const auto makeTransform = [](vec2 position, f32 degrees)
            {
                const f32 angle = glm::radians(degrees);
                const f32 cosine = std::cos(angle);
                const f32 sine = std::sin(angle);

                mat3 result(1.f);
                result[0] = vec3(cosine, sine, 0.f);
                result[1] = vec3(-sine, cosine, 0.f);
                result[2] = vec3(position, 1.f);
                return result;
            };

        const auto check = [](const Rect& a, const mat3& transformA, const Rect& b, const mat3& transformB,
            f32 expectedDepth)
            {
                HitResult hit{}, reversed{};
                if (!Collision::Intersects(a, transformA, b, transformB) ||
                    !Collision::Intersects(a, transformA, b, transformB, hit) || !hit.hasHit ||
                    !Near(hit.denetrationDepth, expectedDepth) ||
                    !Near(glm::length(hit.normal), 1.f) || !Near(hit.normal.z, 0.f))
                    return false;

                if (!Collision::Intersects(b, transformB, a, transformA, reversed) ||
                    !Near(reversed.denetrationDepth, expectedDepth))
                    return false;

                mat3 movedA = transformA;
                movedA[2] += vec3(vec2(hit.normal) * (hit.denetrationDepth + 0.001f), 0.f);
                if (Collision::Intersects(a, movedA, b, transformB))
                    return false;

                mat3 movedB = transformB;
                movedB[2] += vec3(vec2(reversed.normal) * (reversed.denetrationDepth + 0.001f), 0.f);
                return !Collision::Intersects(a, transformA, b, movedB);
            };

        const Rect unit(-0.5f, -0.5f, 1.f, 1.f);
        const mat3 identity(1.f);

        if (!check(unit, identity, unit, makeTransform(vec2(0.75f, 0.f), 0.f), 0.25f))
            return Fail("Translated rect contact failed.");

        const vec2 diagonal = glm::normalize(vec2(1.f));
        if (!check(unit, makeTransform(vec2(0.f), 45.f),
            unit, makeTransform(diagonal * 0.75f, 45.f), 0.25f))
            return Fail("Rotated rect contact failed.");

        const Rect _small(-0.25f, -0.25f, 0.5f, 0.5f);
        const Rect large(-1.f, -1.f, 2.f, 2.f);
        if (!check(_small, identity, large, identity, 1.25f))
            return Fail("Contained transformed rect contact failed.");

        mat3 scaled(1.f);
        scaled[0][0] = 2.f;
        if (!check(unit, scaled, unit, makeTransform(vec2(1.25f, 0.f), 0.f), 0.25f))
            return Fail("Scaled rect contact failed.");

        if (!check(unit, identity, unit, makeTransform(vec2(1.f, 0.f), 0.f), 0.f))
            return Fail("Transformed rect tangency failed.");

        const Rect thin(-1.f, -0.1f, 2.f, 0.2f);
        const mat3 first = makeTransform(vec2(0.f), 45.f);
        const mat3 second = makeTransform(vec2(-diagonal.y, diagonal.x) * 0.3f, 45.f);

        HitResult hit{};
        hit.hasHit = true;
        if (Collision::Intersects(thin, first, thin, second) ||
            Collision::Intersects(thin, first, thin, second, hit) || hit.hasHit)
            return Fail("Separated rotated rects were treated as overlapping AABBs.");

        mat3 collapsed(1.f);
        collapsed[0] = vec3(0.f);
        hit.hasHit = true;
        if (Collision::Intersects(unit, collapsed, unit, identity, hit) || hit.hasHit)
            return Fail("Collapsed rect transform was accepted.");

        mat3 perspective(1.f);
        perspective[0][2] = 0.1f;
        hit.hasHit = true;
        if (Collision::Intersects(unit, perspective, unit, identity, hit) || hit.hasHit)
            return Fail("Perspective rect transform was accepted.");

        return 0;
    }

    int CheckTransformedRectCircleContacts()
    {
        const auto check = [](const Rect& rect, const mat3& transform, const Circle& circle, f32 expectedDepth)
            {
                HitResult hit{}, reversed{};
                if (!Collision::Intersects(rect, transform, circle) ||
                    !Collision::Intersects(rect, transform, circle, hit) || !hit.hasHit ||
                    !Near(hit.denetrationDepth, expectedDepth) ||
                    !Near(glm::length(hit.normal), 1.f) ||
                    !Near(hit.normal.z, 0.f) || !Near(hit.point.z, 0.f))
                    return false;

                if (!Collision::Intersects(circle, rect, transform) ||
                    !Collision::Intersects(circle, rect, transform, reversed) ||
                    !Near(reversed.denetrationDepth, expectedDepth) ||
                    glm::length(hit.normal + reversed.normal) > 0.0001f)
                    return false;

                mat3 movedRect = transform;
                movedRect[2] += vec3(vec2(hit.normal) * (hit.denetrationDepth + 0.001f), 0.f);
                if (Collision::Intersects(rect, movedRect, circle))
                    return false;

                Circle movedCircle = circle;
                movedCircle.center += vec2(reversed.normal) * (reversed.denetrationDepth + 0.001f);
                return !Collision::Intersects(rect, transform, movedCircle);
            };

        const Rect unit(-0.5f, -0.5f, 1.f, 1.f);
        const mat3 identity(1.f);
        const f32 diagonal = std::sqrt(0.5f);
        const vec2 right(diagonal, diagonal);
        const vec2 up(-diagonal, diagonal);
        const vec2 center(2.f, 3.f);

        mat3 rotated(1.f);
        rotated[0] = vec3(right, 0.f);
        rotated[1] = vec3(up, 0.f);
        rotated[2] = vec3(center, 1.f);

        if (!check(unit, rotated, Circle(center + right * 0.75f, 0.5f), 0.25f))
            return Fail("Rotated rect-circle side contact failed.");

        if (!check(unit, rotated, Circle(center, 0.25f), 0.75f))
            return Fail("Circle inside a rotated rect failed.");

        if (!check(unit, identity, Circle(vec2(0.f), 2.f), 2.5f))
            return Fail("Rect inside a circle failed.");

        if (!check(unit, identity, Circle(vec2(0.8f, 0.9f), 0.75f), 0.25f))
            return Fail("Rect-circle corner contact failed.");

        if (!check(unit, identity, Circle(vec2(0.5f, 0.f), 0.25f), 0.25f))
            return Fail("Circle centered on a rect boundary failed.");

        if (!check(unit, identity, Circle(vec2(1.f, 0.f), 0.5f), 0.f))
            return Fail("Transformed rect-circle tangency failed.");

        if (!check(unit, identity, Circle(vec2(0.f), 0.f), 0.5f))
            return Fail("Point circle inside a rect failed.");

        mat3 scaled(1.f);
        scaled[0][0] = 2.f;
        if (!check(unit, scaled, Circle(vec2(1.25f, 0.f), 0.5f), 0.25f))
            return Fail("Nonuniform rect scale changed the circle radius.");

        scaled[0][0] = -2.f;
        if (!check(unit, scaled, Circle(vec2(1.25f, 0.f), 0.5f), 0.25f))
            return Fail("Mirrored rect-circle contact failed.");

        mat3 sheared(1.f);
        sheared[1][0] = 0.5f;
        const vec2 faceNormal = glm::normalize(vec2(1.f, -0.5f));
        const Circle nearShearedFace(vec2(0.5f, 0.f) + faceNormal * 0.25f, 0.5f);
        if (!check(unit, sheared, nearShearedFace, 0.25f))
            return Fail("Sheared rect-circle contact failed.");

        const Rect thin(-1.f, -0.1f, 2.f, 0.2f);
        const Circle separated(center + up * 0.3f, 0.1f);
        HitResult hit{};
        hit.hasHit = true;
        if (Collision::Intersects(thin, rotated, separated) ||
            Collision::Intersects(thin, rotated, separated, hit) || hit.hasHit)
            return Fail("Rotated rect-circle separation used an enclosing AABB.");

        const Circle cornerMiss(vec2(0.9f), 0.5f);
        hit.hasHit = true;
        if (Collision::Intersects(unit, identity, cornerMiss) ||
            Collision::Intersects(unit, identity, cornerMiss, hit) || hit.hasHit)
            return Fail("Rect-circle corner separation failed.");

        const Circle invalid(vec2(0.f), -0.5f);
        hit.hasHit = true;
        if (Collision::Intersects(unit, identity, invalid) ||
            Collision::Intersects(unit, identity, invalid, hit) || hit.hasHit)
            return Fail("Transformed rect accepted an invalid circle.");

        return 0;
    }

    int CheckTransformedRectCapsuleContacts()
    {
        const auto check = [](const Capsule& capsule, const Rect& rect, const mat3& transform, f32 expectedDepth)
            {
                HitResult hit{}, reversed{};
                if (!Collision::Intersects(capsule, rect, transform) ||
                    !Collision::Intersects(capsule, rect, transform, hit) || !hit.hasHit ||
                    !Near(hit.denetrationDepth, expectedDepth) ||
                    !Near(glm::length(hit.normal), 1.f) ||
                    !Near(hit.normal.z, 0.f) || !Near(hit.point.z, 0.f))
                    return false;

                if (!Collision::Intersects(rect, transform, capsule) ||
                    !Collision::Intersects(rect, transform, capsule, reversed) ||
                    !Near(reversed.denetrationDepth, expectedDepth) ||
                    glm::length(hit.normal + reversed.normal) > 0.0001f)
                    return false;

                Capsule movedCapsule = capsule;
                const vec3 correction = hit.normal * (hit.denetrationDepth + 0.001f);
                movedCapsule.pointA += correction;
                movedCapsule.pointB += correction;
                if (Collision::Intersects(movedCapsule, rect, transform))
                    return false;

                mat3 movedRect = transform;
                movedRect[2] += vec3(vec2(reversed.normal) * (reversed.denetrationDepth + 0.001f), 0.f);
                return !Collision::Intersects(capsule, rect, movedRect);
            };

        const Rect unit(-0.5f, -0.5f, 1.f, 1.f);
        const mat3 identity(1.f);
        const f32 diagonal = std::sqrt(0.5f);
        const vec2 right(diagonal, diagonal);
        const vec2 up(-diagonal, diagonal);
        const vec2 center(2.f, 3.f);

        mat3 rotated(1.f);
        rotated[0] = vec3(right, 0.f);
        rotated[1] = vec3(up, 0.f);
        rotated[2] = vec3(center, 1.f);

        const auto fromLocal = [&](vec2 first, vec2 second, f32 radius)
            {
                return Capsule(vec3(center + right * first.x + up * first.y, 0.f),
                    vec3(center + right * second.x + up * second.y, 0.f), radius);
            };

        const Capsule side = fromLocal(vec2(0.75f, -0.25f), vec2(0.75f, 0.25f), 0.5f);
        if (!check(side, unit, rotated, 0.25f))
            return Fail("Rotated rect-capsule side contact failed.");

        const Capsule corner = fromLocal(vec2(0.8f, 0.9f), vec2(0.8f, 0.9f), 0.75f);
        if (!check(corner, unit, rotated, 0.25f))
            return Fail("Rotated rect-capsule corner contact failed.");

        const Capsule contained = fromLocal(vec2(0.f, -0.1f), vec2(0.f, 0.1f), 0.25f);
        if (!check(contained, unit, rotated, 0.75f))
            return Fail("Capsule inside a rotated rect failed.");

        const Capsule crossing = fromLocal(vec2(-2.f, 0.f), vec2(2.f, 0.f), 0.25f);
        if (!check(crossing, unit, rotated, 0.75f))
            return Fail("Capsule crossing a rotated rect failed.");

        const Capsule vertical(vec3(center + vec2(0.f, -2.f), 0.f),
            vec3(center + vec2(0.f, 2.f), 0.f), 0.25f);
        if (!check(vertical, unit, rotated, diagonal + 0.25f))
            return Fail("Capsule perpendicular axis was not considered.");

        const Capsule tangent(vec3(1.f, -0.25f, 0.f), vec3(1.f, 0.25f, 0.f), 0.5f);
        if (!check(tangent, unit, identity, 0.f))
            return Fail("Transformed rect-capsule tangency failed.");

        const Capsule nearScaled(vec3(1.25f, -0.25f, 0.f), vec3(1.25f, 0.25f, 0.f), 0.5f);
        mat3 scaled(1.f);
        scaled[0][0] = 2.f;
        if (!check(nearScaled, unit, scaled, 0.25f))
            return Fail("Nonuniform rect scale changed the capsule radius.");

        scaled[0][0] = -2.f;
        if (!check(nearScaled, unit, scaled, 0.25f))
            return Fail("Mirrored rect-capsule contact failed.");

        const Rect thin(-1.f, -0.1f, 2.f, 0.2f);
        const Capsule separated = fromLocal(vec2(-0.25f, 0.3f), vec2(0.25f, 0.3f), 0.1f);
        HitResult hit{};
        hit.hasHit = true;
        if (Collision::Intersects(separated, thin, rotated) ||
            Collision::Intersects(separated, thin, rotated, hit) || hit.hasHit)
            return Fail("Rotated rect-capsule separation used an enclosing AABB.");

        const Capsule cornerMiss = fromLocal(vec2(0.9f), vec2(0.9f), 0.5f);
        hit.hasHit = true;
        if (Collision::Intersects(cornerMiss, unit, rotated) ||
            Collision::Intersects(cornerMiss, unit, rotated, hit) || hit.hasHit)
            return Fail("Transformed rect-capsule corner separation failed.");

        Capsule outside = side;
        outside.pointA.z = 0.1f;
        hit.hasHit = true;
        if (Collision::Intersects(outside, unit, rotated) ||
            Collision::Intersects(outside, unit, rotated, hit) || hit.hasHit)
            return Fail("Transformed rect accepted a non-planar capsule.");

        return 0;
    }
}

int main()
{
    if (const int result = CheckCapsuleGeometry(); result != 0)
        return result;

    if (const int result = CheckCapsule3DContacts(); result != 0)
        return result;

    if (const int result = CheckCapsuleCircleContacts(); result != 0)
        return result;

    if (const int result = CheckCapsuleRectContacts(); result != 0)
        return result;

    if (const int result = CheckCapsulePairContacts(); result != 0)
        return result;

    if (const int result = CheckTransformedRectContacts(); result != 0)
        return result;

    if (const int result = CheckTransformedRectCircleContacts(); result != 0)
        return result;

    if (const int result = CheckTransformedRectCapsuleContacts(); result != 0)
        return result;

    std::cout << "Collision geometry checks passed.\n";
    return 0;
}