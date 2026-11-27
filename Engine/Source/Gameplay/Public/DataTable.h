#pragma once

#include "Engine_Includes.h"
#include <cstddef>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Engine
{
	// ── 외부 데이터 테이블 로더 및 검증기 ──
	// RFC 4180 CSV 파싱, 필수 컬럼 검증, 키 컬럼 기반 O(1) 행 조회, 타입 변환 조회를 제공한다.
	class ENGINE_API DataTable
	{
	public:
		DataTable() = default;
		~DataTable() = default;

		// RFC 4180 CSV 문자열을 파싱하여 테이블을 로드한다.
		// keyColumn이 비어 있지 않으면 해당 컬럼을 키로 지정하고 중복 키를 검증한다.
		// 실패 시 테이블을 비우고 outError에 원인을 기록한 뒤 false를 반환한다.
		[[nodiscard]] bool LoadFromCsvString(
			std::string_view csvText,
			std::string* outError = nullptr,
			std::string_view keyColumn = {});

		// CSV 파일을 읽어 테이블을 로드한다.
		[[nodiscard]] bool LoadFromCsvFile(
			const std::filesystem::path& path,
			std::string* outError = nullptr,
			std::string_view keyColumn = {});

		// 지정한 컬럼을 기본 키 컬럼으로 설정하고 키 인덱스를 구축한다.
		// 빈 문자열을 전달하면 키 인덱스를 해제한다.
		[[nodiscard]] bool SetKeyColumn(std::string_view keyColumn, std::string* outError = nullptr);

		// 필수 컬럼 목록이 모두 존재하는지 검증한다.
		// 누락된 컬럼이 있으면 outMissingColumns(비널인 경우)에 기록하고 false를 반환한다.
		[[nodiscard]] bool ValidateRequiredColumns(
			const std::vector<std::string>& requiredColumns,
			std::vector<std::string>* outMissingColumns = nullptr) const;

		// ── 테이블 메타/행 조회 API ──
		[[nodiscard]] std::size_t GetRowCount() const noexcept { return m_rows.size(); }
		[[nodiscard]] std::size_t GetColumnCount() const noexcept { return m_headers.size(); }
		[[nodiscard]] bool IsEmpty() const noexcept { return m_rows.empty(); }
		[[nodiscard]] const std::vector<std::string>& GetColumns() const noexcept { return m_headers; }
		[[nodiscard]] const std::string& GetKeyColumn() const noexcept { return m_keyColumn; }

		[[nodiscard]] bool HasColumn(std::string_view name) const;

		// keyColumn == GetKeyColumn()인 경우 O(1) 인덱스 조회, 그 외 컬럼은 선형 탐색한다.
		[[nodiscard]] std::optional<std::size_t> FindRowIndex(
			std::string_view keyColumn,
			std::string_view keyValue) const;

		// 지정된 기본 키 컬럼으로 행 인덱스를 조회한다.
		[[nodiscard]] std::optional<std::size_t> FindRowIndexByKey(std::string_view keyValue) const;

		// 행/컬럼이 유효하지 않으면 defaultVal을 반환한다.
		[[nodiscard]] std::string GetString(
			std::size_t row,
			std::string_view col,
			std::string_view defaultVal = {}) const;

		// 셀이 비어 있거나 유효한 정수가 아니면 defaultVal을 반환한다.
		[[nodiscard]] int32 GetInt(
			std::size_t row,
			std::string_view col,
			int32 defaultVal = 0) const;

		// 셀이 비어 있거나 유효한 유한 실수가 아니면 defaultVal을 반환한다.
		[[nodiscard]] f32 GetFloat(
			std::size_t row,
			std::string_view col,
			f32 defaultVal = 0.0f) const;

		// "true/1/yes/on" -> true, "false/0/no/off" -> false (대소문자 무시), 그 외 defaultVal.
		[[nodiscard]] bool GetBool(
			std::size_t row,
			std::string_view col,
			bool defaultVal = false) const;

		// 모든 헤더, 행 데이터 및 키 인덱스를 초기화한다.
		void Clear();

		// ── 테이블 편집 및 저장 API ──
		// 지정한 행/열의 셀 값을 반환한다. 범위 밖이면 빈 문자열을 반환한다.
		[[nodiscard]] const std::string& GetCell(std::size_t row, std::size_t col) const;

		// 지정한 행/열의 셀 값을 수정한다. 범위 밖이면 false를 반환한다.
		// 기본 키 컬럼을 수정한 경우 키 인덱스를 함께 갱신한다.
		bool SetCell(std::size_t row, std::size_t col, std::string value);

		// 현재 컬럼 수에 맞는 빈 행을 끝에 추가한다. 헤더가 비어 있으면 false를 반환한다.
		bool AddEmptyRow();

		// 지정한 행을 삭제한다. 범위 밖이면 false를 반환한다.
		bool RemoveRow(std::size_t row);

		// 현재 테이블 내용을 RFC 4180 CSV 형식으로 파일에 저장한다.
		[[nodiscard]] bool SaveToCsvFile(
			const std::filesystem::path& path,
			std::string* outError = nullptr) const;

	private:
		struct TransparentStringHash
		{
			using is_transparent = void;
			[[nodiscard]] std::size_t operator()(std::string_view sv) const noexcept
			{
				return std::hash<std::string_view>{}(sv);
			}
		};

		using StringIndexMap = std::unordered_map<std::string, std::size_t, TransparentStringHash, std::equal_to<>>;

		[[nodiscard]] std::optional<std::size_t> FindColumnIndex(std::string_view col) const;
		[[nodiscard]] const std::string* GetCellPtr(std::size_t row, std::string_view col) const;

		std::vector<std::string> m_headers;
		StringIndexMap m_columnToIndex;
		std::vector<std::vector<std::string>> m_rows;

		std::string m_keyColumn;
		StringIndexMap m_keyToRow;
	};
}
