#include "DataTable.h"
#include <cctype>
#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <system_error>

namespace Engine
{
	namespace DataTableDetail
	{
		void SetError(std::string* outError, std::string_view message)
		{
			if (outError != nullptr)
			{
				*outError = message;
			}
		}

		std::string_view TrimAsciiWhitespace(std::string_view sv)
		{
			while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.front())) != 0)
			{
				sv.remove_prefix(1);
			}
			while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.back())) != 0)
			{
				sv.remove_suffix(1);
			}
			return sv;
		}

		bool EqualsIgnoreCaseAscii(std::string_view a, std::string_view b)
		{
			if (a.size() != b.size())
				return false;

			for (std::size_t i = 0; i < a.size(); ++i)
			{
				const auto ca = static_cast<unsigned char>(a[i]);
				const auto cb = static_cast<unsigned char>(b[i]);
				if (std::tolower(ca) != std::tolower(cb))
					return false;
			}
			return true;
		}

		// RFC 4180 CSV 파서: CRLF/LF/CR, 따옴표("...") 및 이스케이프("")를 지원한다.
		bool ParseCsvRecords(
			std::string_view input,
			std::vector<std::vector<std::string>>& outRecords,
			std::string* outError)
		{
			outRecords.clear();

			// UTF-8 BOM 제거
			if (input.size() >= 3 &&
				static_cast<unsigned char>(input[0]) == 0xEF &&
				static_cast<unsigned char>(input[1]) == 0xBB &&
				static_cast<unsigned char>(input[2]) == 0xBF)
			{
				input.remove_prefix(3);
			}

			std::size_t pos = 0;
			std::size_t line = 1;
			const std::size_t len = input.size();

			while (pos < len)
			{
				// 완전히 빈 줄(\r\n, \n, \r)은 건너뛴다.
				if (input[pos] == '\r')
				{
					++pos;
					if (pos < len && input[pos] == '\n')
					{
						++pos;
					}
					++line;
					continue;
				}
				if (input[pos] == '\n')
				{
					++pos;
					++line;
					continue;
				}

				std::vector<std::string> row;
				const std::size_t recordStartLine = line;

				while (true)
				{
					std::string field;
					if (pos < len && input[pos] == '"')
					{
						// 따옴표로 감싼 필드
						++pos;
						bool closed = false;
						while (pos < len)
						{
							const char ch = input[pos++];
							if (ch == '"')
							{
								if (pos < len && input[pos] == '"')
								{
									// 이스케이프된 따옴표("")
									field.push_back('"');
									++pos;
								}
								else
								{
									closed = true;
									break;
								}
							}
							else
							{
								if (ch == '\r')
								{
									if (pos < len && input[pos] == '\n')
									{
										++pos;
									}
									field.push_back('\n');
									++line;
								}
								else if (ch == '\n')
								{
									field.push_back('\n');
									++line;
								}
								else
								{
									field.push_back(ch);
								}
							}
						}

						if (!closed)
						{
							SetError(outError, "Unterminated quoted field starting at line " + std::to_string(recordStartLine));
							return false;
						}

						// 닫는 따옴표 뒤에는 구분자(,), 줄바꿈, EOF만 허용한다.
						if (pos < len && input[pos] != ',' && input[pos] != '\r' && input[pos] != '\n')
						{
							SetError(outError, "Unexpected character after closing quote at line " + std::to_string(line));
							return false;
						}
					}
					else
					{
						// 따옴표 없는 일반 필드
						while (pos < len)
						{
							const char ch = input[pos];
							if (ch == ',' || ch == '\r' || ch == '\n')
								break;
							if (ch == '"')
							{
								SetError(outError, "Unexpected quote in unquoted field at line " + std::to_string(line));
								return false;
							}
							field.push_back(ch);
							++pos;
						}
					}

					row.push_back(std::move(field));

					if (pos < len && input[pos] == ',')
					{
						++pos;
						// 쉼표 뒤가 바로 줄바꿈/EOF이면 빈 마지막 필드를 추가하고 행을 종료한다.
						if (pos >= len || input[pos] == '\r' || input[pos] == '\n')
						{
							row.emplace_back();
							if (pos < len)
							{
								if (input[pos] == '\r')
								{
									++pos;
									if (pos < len && input[pos] == '\n')
									{
										++pos;
									}
								}
								else
								{
									++pos;
								}
								++line;
							}
							break;
						}
						continue;
					}

					// 줄바꿈 또는 EOF로 레코드 종료
					if (pos < len)
					{
						if (input[pos] == '\r')
						{
							++pos;
							if (pos < len && input[pos] == '\n')
							{
								++pos;
							}
						}
						else if (input[pos] == '\n')
						{
							++pos;
						}
						++line;
					}
					break;
				}

				outRecords.push_back(std::move(row));
			}

			return true;
		}
	}

	// ═══════════════════════════════════════════════════════════════════════════
	// DataTable
	// ═══════════════════════════════════════════════════════════════════════════

	bool DataTable::LoadFromCsvString(
		std::string_view csvText,
		std::string* outError,
		std::string_view keyColumn)
	{
		Clear();

		std::vector<std::vector<std::string>> records;
		if (!DataTableDetail::ParseCsvRecords(csvText, records, outError))
		{
			return false;
		}

		if (records.empty())
		{
			DataTableDetail::SetError(outError, "CSV input is empty (missing header row)");
			return false;
		}

		// 첫 행을 헤더로 등록하고 빈 컬럼명 및 중복 컬럼명을 검증한다.
		std::vector<std::string> headers = std::move(records.front());
		StringIndexMap columnMap;
		columnMap.reserve(headers.size());

		for (std::size_t i = 0; i < headers.size(); ++i)
		{
			if (headers[i].empty())
			{
				DataTableDetail::SetError(outError, "Empty column name in header at index " + std::to_string(i));
				return false;
			}
			if (!columnMap.emplace(headers[i], i).second)
			{
				DataTableDetail::SetError(outError, "Duplicate column name in header: " + headers[i]);
				return false;
			}
		}

		// 데이터 행의 컬럼 수 일치 여부를 검증한다.
		const std::size_t expectedColumns = headers.size();
		std::vector<std::vector<std::string>> rows;
		rows.reserve(records.size() - 1);

		for (std::size_t i = 1; i < records.size(); ++i)
		{
			if (records[i].size() != expectedColumns)
			{
				DataTableDetail::SetError(
					outError,
					"Column count mismatch at data row " + std::to_string(i) +
					" (expected " + std::to_string(expectedColumns) +
					", got " + std::to_string(records[i].size()) + ")");
				return false;
			}
			rows.push_back(std::move(records[i]));
		}

		m_headers = std::move(headers);
		m_columnToIndex = std::move(columnMap);
		m_rows = std::move(rows);

		if (!keyColumn.empty())
		{
			if (!SetKeyColumn(keyColumn, outError))
			{
				Clear();
				return false;
			}
		}

		return true;
	}

	bool DataTable::LoadFromCsvFile(
		const std::filesystem::path& path,
		std::string* outError,
		std::string_view keyColumn)
	{
		Clear();

		std::error_code ec;
		if (!std::filesystem::exists(path, ec) || ec)
		{
			DataTableDetail::SetError(outError, "CSV file does not exist: " + path.string());
			return false;
		}

		std::ifstream file(path, std::ios::binary | std::ios::ate);
		if (!file.is_open())
		{
			DataTableDetail::SetError(outError, "Failed to open CSV file: " + path.string());
			return false;
		}

		const std::streamsize fileSize = file.tellg();
		if (fileSize < 0)
		{
			DataTableDetail::SetError(outError, "Failed to determine size of CSV file: " + path.string());
			return false;
		}

		std::string content(static_cast<std::size_t>(fileSize), '\0');
		file.seekg(0, std::ios::beg);
		if (fileSize > 0 && !file.read(content.data(), fileSize))
		{
			DataTableDetail::SetError(outError, "Failed to read CSV file: " + path.string());
			return false;
		}

		return LoadFromCsvString(content, outError, keyColumn);
	}

	bool DataTable::SetKeyColumn(std::string_view keyColumn, std::string* outError)
	{
		if (keyColumn.empty())
		{
			m_keyColumn.clear();
			m_keyToRow.clear();
			return true;
		}

		const auto colOpt = FindColumnIndex(keyColumn);
		if (!colOpt.has_value())
		{
			DataTableDetail::SetError(outError, "Key column not found: " + std::string(keyColumn));
			return false;
		}

		const std::size_t colIdx = *colOpt;
		StringIndexMap newKeyMap;
		newKeyMap.reserve(m_rows.size());

		for (std::size_t rowIdx = 0; rowIdx < m_rows.size(); ++rowIdx)
		{
			const std::string& key = m_rows[rowIdx][colIdx];
			if (!newKeyMap.emplace(key, rowIdx).second)
			{
				DataTableDetail::SetError(
					outError,
					"Duplicate key '" + key + "' in key column '" + std::string(keyColumn) + "'");
				return false;
			}
		}

		m_keyColumn = std::string(keyColumn);
		m_keyToRow = std::move(newKeyMap);
		return true;
	}

	bool DataTable::ValidateRequiredColumns(
		const std::vector<std::string>& requiredColumns,
		std::vector<std::string>* outMissingColumns) const
	{
		if (outMissingColumns != nullptr)
		{
			outMissingColumns->clear();
		}

		bool allPresent = true;
		for (const std::string& col : requiredColumns)
		{
			if (!HasColumn(col))
			{
				allPresent = false;
				if (outMissingColumns != nullptr)
				{
					outMissingColumns->push_back(col);
				}
			}
		}
		return allPresent;
	}

	bool DataTable::HasColumn(std::string_view name) const
	{
		return m_columnToIndex.find(name) != m_columnToIndex.end();
	}

	std::optional<std::size_t> DataTable::FindRowIndex(
		std::string_view keyColumn,
		std::string_view keyValue) const
	{
		if (!m_keyColumn.empty() && keyColumn == m_keyColumn)
		{
			return FindRowIndexByKey(keyValue);
		}

		const auto colOpt = FindColumnIndex(keyColumn);
		if (!colOpt.has_value())
			return std::nullopt;

		const std::size_t colIdx = *colOpt;
		for (std::size_t rowIdx = 0; rowIdx < m_rows.size(); ++rowIdx)
		{
			if (m_rows[rowIdx][colIdx] == keyValue)
				return rowIdx;
		}
		return std::nullopt;
	}

	std::optional<std::size_t> DataTable::FindRowIndexByKey(std::string_view keyValue) const
	{
		if (m_keyColumn.empty())
			return std::nullopt;

		const auto it = m_keyToRow.find(keyValue);
		if (it == m_keyToRow.end())
			return std::nullopt;

		return it->second;
	}

	std::string DataTable::GetString(
		std::size_t row,
		std::string_view col,
		std::string_view defaultVal) const
	{
		const std::string* cell = GetCellPtr(row, col);
		if (cell == nullptr)
			return std::string(defaultVal);

		return *cell;
	}

	int32 DataTable::GetInt(
		std::size_t row,
		std::string_view col,
		int32 defaultVal) const
	{
		const std::string* cell = GetCellPtr(row, col);
		if (cell == nullptr)
			return defaultVal;

		const std::string_view trimmed = DataTableDetail::TrimAsciiWhitespace(*cell);
		if (trimmed.empty())
			return defaultVal;

		int32 parsed = 0;
		const char* begin = trimmed.data();
		const char* end = begin + trimmed.size();
		const auto [ptr, ec] = std::from_chars(begin, end, parsed);
		if (ec != std::errc{} || ptr != end)
			return defaultVal;

		return parsed;
	}

	f32 DataTable::GetFloat(
		std::size_t row,
		std::string_view col,
		f32 defaultVal) const
	{
		const std::string* cell = GetCellPtr(row, col);
		if (cell == nullptr)
			return defaultVal;

		const std::string_view trimmed = DataTableDetail::TrimAsciiWhitespace(*cell);
		if (trimmed.empty())
			return defaultVal;

#if defined(__cpp_lib_to_chars) && __cpp_lib_to_chars >= 201611L
		f32 parsed = 0.0f;
		const char* begin = trimmed.data();
		const char* end = begin + trimmed.size();
		const auto [ptr, ec] = std::from_chars(begin, end, parsed);
		if (ec != std::errc{} || ptr != end || !std::isfinite(parsed))
			return defaultVal;
		return parsed;
#else
		std::string buffer(trimmed);
		errno = 0;
		char* endPtr = nullptr;
		const f32 parsed = std::strtof(buffer.c_str(), &endPtr);
		if (errno == ERANGE || endPtr != buffer.c_str() + buffer.size() || !std::isfinite(parsed))
			return defaultVal;
		return parsed;
#endif
	}

	bool DataTable::GetBool(
		std::size_t row,
		std::string_view col,
		bool defaultVal) const
	{
		const std::string* cell = GetCellPtr(row, col);
		if (cell == nullptr)
			return defaultVal;

		const std::string_view trimmed = DataTableDetail::TrimAsciiWhitespace(*cell);
		if (trimmed.empty())
			return defaultVal;

		if (DataTableDetail::EqualsIgnoreCaseAscii(trimmed, "true") ||
			trimmed == "1" ||
			DataTableDetail::EqualsIgnoreCaseAscii(trimmed, "yes") ||
			DataTableDetail::EqualsIgnoreCaseAscii(trimmed, "on"))
		{
			return true;
		}

		if (DataTableDetail::EqualsIgnoreCaseAscii(trimmed, "false") ||
			trimmed == "0" ||
			DataTableDetail::EqualsIgnoreCaseAscii(trimmed, "no") ||
			DataTableDetail::EqualsIgnoreCaseAscii(trimmed, "off"))
		{
			return false;
		}

		return defaultVal;
	}

	void DataTable::Clear()
	{
		m_headers.clear();
		m_columnToIndex.clear();
		m_rows.clear();
		m_keyColumn.clear();
		m_keyToRow.clear();
	}

	std::optional<std::size_t> DataTable::FindColumnIndex(std::string_view col) const
	{
		const auto it = m_columnToIndex.find(col);
		if (it == m_columnToIndex.end())
			return std::nullopt;
		return it->second;
	}

	const std::string* DataTable::GetCellPtr(std::size_t row, std::string_view col) const
	{
		if (row >= m_rows.size())
			return nullptr;

		const auto colOpt = FindColumnIndex(col);
		if (!colOpt.has_value())
			return nullptr;

		return &m_rows[row][*colOpt];
	}
}
