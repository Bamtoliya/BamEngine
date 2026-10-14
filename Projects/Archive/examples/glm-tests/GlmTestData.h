#pragma once

#include "Adapters/GlmArchiveAdapter.h"

namespace archive_glm_tests
{
    inline bool SameVec3(const glm::vec3& lhs, const glm::vec3& rhs)
    {
        return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
    }

    inline bool SameQuat(const glm::quat& lhs, const glm::quat& rhs)
    {
        return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z && lhs.w == rhs.w;
    }

    inline bool SameMat4(const glm::mat4& lhs, const glm::mat4& rhs)
    {
        for (int column = 0; column < 4; ++column)
        {
            for (int row = 0; row < 4; ++row)
            {
                if (lhs[column][row] != rhs[column][row])
                {
                    return false;
                }
            }
        }

        return true;
    }
}
