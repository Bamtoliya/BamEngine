#ifndef BAM_MATRIX_HLSLI
#define BAM_MATRIX_HLSLI

float4x4 Inverse4x4(float4x4 matrix)
{
    float4x4 inverseMatrix = float4x4(
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    );

    for (uint column = 0u; column < 4u; ++column)
    {
        uint pivotRow = column;
        float largestPivot = abs(matrix[column][column]);

        for (uint row = column + 1u; row < 4u; ++row)
        {
            float candidatePivot = abs(matrix[row][column]);
            if (candidatePivot > largestPivot)
            {
                largestPivot = candidatePivot;
                pivotRow = row;
            }
        }

        if (largestPivot < 0.000001f)
        {
            return inverseMatrix;
        }

        if (pivotRow != column)
        {
            float4 matrixRow = matrix[column];
            matrix[column] = matrix[pivotRow];
            matrix[pivotRow] = matrixRow;

            float4 inverseRow = inverseMatrix[column];
            inverseMatrix[column] = inverseMatrix[pivotRow];
            inverseMatrix[pivotRow] = inverseRow;
        }

        float pivot = matrix[column][column];
        matrix[column] /= pivot;
        inverseMatrix[column] /= pivot;

        for (uint row = 0u; row < 4u; ++row)
        {
            if (row == column)
            {
                continue;
            }

            float factor = matrix[row][column];
            matrix[row] -= factor * matrix[column];
            inverseMatrix[row] -= factor * inverseMatrix[column];
        }
    }

    return inverseMatrix;
}

#endif