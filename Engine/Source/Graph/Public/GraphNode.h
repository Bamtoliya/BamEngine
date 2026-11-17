#pragma once

#include "GraphPin.h"
#include <unordered_map>
#include <vector>
#include <reflection/Annotations.h>

namespace Engine
{

enum class EGraphReflectionKind { None, Function, PropertyGet, PropertySet };

// Stable lookup information, not borrowed pointers into a reflection registry.
struct GraphReflectionSource
{
	EGraphReflectionKind Kind = EGraphReflectionKind::None;
	std::string Owner;
	std::string Member;
	std::string Signature;
};

struct GraphNodeDefinition
{
	std::string Type;
	std::vector<EGraphDomain> Domains;
	std::vector<GraphPin> Pins;
	std::string DisplayName;
	std::string Category;
	std::string Tooltip;
	GraphReflectionSource Reflection;
};

STRUCT()
struct GraphNode
{
	PROPERTY()
	GraphNodeId Id = 0;

	PROPERTY()
	std::string Type;

	PROPERTY()
	std::unordered_map<std::string, GraphValue> InputValues;

	// 에디터 캔버스 위치. 실행에는 쓰지 않는다. 위치 필드가 없는 예전 파일을 위해 ALLOW_MISSING, 위치 유무는 HasPosition으로 구분한다.
	PROPERTY(ALLOW_MISSING)
	float PositionX = 0.0f;

	PROPERTY(ALLOW_MISSING)
	float PositionY = 0.0f;

	PROPERTY(ALLOW_MISSING)
	bool HasPosition = false;

	// 주석 박스 제목 같은 노드 메모. 실행·셰이더 생성에는 쓰지 않는다.
	PROPERTY(ALLOW_MISSING)
	std::string Comment;
};

}
