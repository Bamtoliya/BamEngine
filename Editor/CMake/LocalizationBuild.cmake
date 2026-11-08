# --- CMake/LocalizationBuild.cmake ---

set(LOCALIZATION_SCRIPT "${CMAKE_SOURCE_DIR}/Tools/Codegen/CSVToJsonConverter/localizationcsvtojson.py")

# 1. Engine Localization
set(ENGINE_LOCALIZATION_DIR "${CMAKE_SOURCE_DIR}/Engine/Resources/Data/Localization")
file(GLOB ENGINE_LOCALIZATION_CSV_FILES CONFIGURE_DEPENDS "${ENGINE_LOCALIZATION_DIR}/*.csv")
set(ENGINE_LOCALIZATION_OUTPUTS
    "${ENGINE_LOCALIZATION_DIR}/English.json"
    "${ENGINE_LOCALIZATION_DIR}/Korean.json"
)

add_custom_command(
    OUTPUT ${ENGINE_LOCALIZATION_OUTPUTS}
    COMMAND ${Python3_EXECUTABLE} "${LOCALIZATION_SCRIPT}" --input "${ENGINE_LOCALIZATION_DIR}" --output "${ENGINE_LOCALIZATION_DIR}"
    DEPENDS
        "${LOCALIZATION_SCRIPT}"
        ${ENGINE_LOCALIZATION_CSV_FILES}
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Generating Engine localization JSON files"
)

# 2. Editor Localization
set(EDITOR_LOCALIZATION_DIR "${CMAKE_SOURCE_DIR}/Editor/Resources/Data/Localization")
file(GLOB EDITOR_LOCALIZATION_CSV_FILES CONFIGURE_DEPENDS "${EDITOR_LOCALIZATION_DIR}/*.csv")
set(EDITOR_LOCALIZATION_OUTPUTS
    "${EDITOR_LOCALIZATION_DIR}/English.json"
    "${EDITOR_LOCALIZATION_DIR}/Korean.json"
)

add_custom_command(
    OUTPUT ${EDITOR_LOCALIZATION_OUTPUTS}
    COMMAND ${Python3_EXECUTABLE} "${LOCALIZATION_SCRIPT}" --input "${EDITOR_LOCALIZATION_DIR}" --output "${EDITOR_LOCALIZATION_DIR}"
    DEPENDS
        "${LOCALIZATION_SCRIPT}"
        ${EDITOR_LOCALIZATION_CSV_FILES}
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Generating Editor localization JSON files"
)

add_custom_target(GenerateLocalization ALL DEPENDS ${ENGINE_LOCALIZATION_OUTPUTS} ${EDITOR_LOCALIZATION_OUTPUTS})
add_dependencies(GenerateLocalization ExportReflectionLocalization)

# --- 다국어 및 UI 하드코딩 정적 감사 타깃 ---
set(LOCALIZATION_AUDIT_SCRIPT "${CMAKE_SOURCE_DIR}/Tools/Audit/LocalizationAuditor/audit_localization.py")

add_custom_target(AuditLocalization
    COMMAND ${Python3_EXECUTABLE} "${LOCALIZATION_AUDIT_SCRIPT}" --root "${CMAKE_SOURCE_DIR}" --strict
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Auditing localization keys and hardcoded UI strings"
    VERBATIM
)

if(BUILD_TESTING)
    add_test(NAME LocalizationAuditUtility
        COMMAND "${Python3_EXECUTABLE}" -B -m unittest discover
            -s "${CMAKE_SOURCE_DIR}/Tools/Audit/LocalizationAuditor" -p "test_*.py" -v)
endif()
