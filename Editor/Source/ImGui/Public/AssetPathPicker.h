#pragma once

#include "ResourcePicker.h"
#include <string>

namespace Editor
{
	struct AssetPathPickerDesc
	{
		Detail::ResourceVisual Visual;
		// 목록과 드롭에서 받을 파일인지 판단한다. nullptr이면 모든 파일을 받는다.
		bool (*Accepts)(const std::filesystem::path& path) = nullptr;
	};

	// 문자열 경로로 참조하는 에셋(Animator 컨트롤러, 액션 타임라인 등)을 ResourcePicker와 같은 모양으로 고른다.
	// 목록에서 고르거나 끌어다 놓은 파일은 가상 URI로 바꿔 selected에 담고 true를 돌려준다. 연결 해제는 빈 문자열이다.
	// 값을 실제로 반영할지는 호출부가 정한다(예: 컨트롤러는 로드에 성공했을 때만 저장).
	bool DrawInspectorAssetPathPicker(const std::string& current, std::string& selected, const AssetPathPickerDesc& desc, bool canEdit = true);
}
