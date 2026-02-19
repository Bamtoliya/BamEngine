#pragma once

#include "Base.h"

BEGIN(Engine)

class ENGINE_API ConfigManager final : public Base
{
	DECLARE_SINGLETON(ConfigManager)

#pragma region Constructor&Destructor
private:
	EResult Initialize(void* arg);
	virtual void Free() override;
#pragma endregion
public:
	void LoadSettings();
	void SaveSettings();

	template<typename T>
	bool Save(T& OutConfig, const string& FileName)
	{
		filesystem::path Path(FileName);
		if (Path.has_parent_path())
			filesystem::create_directories(Path.parent_path());

		// Glaze를 이용한 저장 (Prettify 옵션 사용)
		auto ErrorCode = glz::write_file_json < glz::opts{ .prettify = true } > (InConfig, FullPath, string{});
		return ErrorCode == glz::error_code::none;
	}

	template<typename T>
	bool Load(T& InConfig, const string& FileName)
	{
		if (!filesystem::exists(FileName))
			return false;
		auto ErrorCode = glz::read_json(FileName, InConfig);
		return ErrorCode == glz::Error::None;
	}
};
END