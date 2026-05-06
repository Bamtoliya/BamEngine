//#pragma region Helper
//static quat ExtractRotationQuat(const mat4& matrix)
//{
//	vec3 scale;
//	vec3 translation;
//	vec3 skew;
//	vec4 perspective;
//	quat rotation;
//
//	glm::decompose(matrix, scale, rotation, translation, skew, perspective);
//	return glm::normalize(rotation);
//}
//
//static vec3 ExtractDeltaEulerDegrees(const mat4& deltaMatrix)
//{
//	const quat deltaRotation = ExtractRotationQuat(deltaMatrix);
//	return glm::degrees(glm::eulerAngles(deltaRotation));
//}
//
//static bool WorldToViewportScreen(
//	const vec3& worldPos,
//	const mat4& viewProj,
//	const ImVec2& imageScreenPos,
//	const ImVec2& imageSize,
//	ImVec2& outScreenPos)
//{
//	vec4 clip = viewProj * vec4(worldPos, 1.0f);
//
//	if (glm::abs(clip.w) < 0.000001f || clip.w <= 0.0f)
//		return false;
//
//	vec3 ndc = vec3(clip) / clip.w;
//
//	const bool inDepthGL = (ndc.z >= -1.0f && ndc.z <= 1.0f);
//	const bool inDepthVK = (ndc.z >= 0.0f && ndc.z <= 1.0f);
//	if (!(inDepthGL || inDepthVK))
//		return false;
//
//	if (ndc.x < -1.0f || ndc.x > 1.0f || ndc.y < -1.0f || ndc.y > 1.0f)
//		return false;
//
//	const float u = ndc.x * 0.5f + 0.5f;
//	const float v = 1.0f - (ndc.y * 0.5f + 0.5f);
//
//	outScreenPos.x = imageScreenPos.x + u * imageSize.x;
//	outScreenPos.y = imageScreenPos.y + v * imageSize.y;
//	return true;
//}
//
//static bool UnprojectFrustumCorner(
//	const mat4& invViewProj,
//	const vec3& ndcPos,
//	vec3& outWorldPos)
//{
//	vec4 worldPos = invViewProj * vec4(ndcPos, 1.0f);
//	if (glm::abs(worldPos.w) < 0.000001f)
//		return false;
//
//	outWorldPos = vec3(worldPos) / worldPos.w;
//	return true;
//}
//
//static void DrawViewportLineIfVisible(
//	ImDrawList* drawList,
//	const vec3& worldStart,
//	const vec3& worldEnd,
//	const mat4& viewProj,
//	const ImVec2& imageScreenPos,
//	const ImVec2& imageSize,
//	ImU32 color,
//	float thickness)
//{
//	ImVec2 screenStart;
//	ImVec2 screenEnd;
//
//	if (!WorldToViewportScreen(worldStart, viewProj, imageScreenPos, imageSize, screenStart))
//		return;
//	if (!WorldToViewportScreen(worldEnd, viewProj, imageScreenPos, imageSize, screenEnd))
//		return;
//
//	drawList->AddLine(screenStart, screenEnd, color, thickness);
//}
//#pragma endregion