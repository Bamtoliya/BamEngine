find_package(Python3 COMPONENTS Interpreter REQUIRED)

get_target_property(_engine_metadata_files Engine REFLECTION_METADATA_FILES)

if(NOT _engine_metadata_files)
    message(FATAL_ERROR "Engine reflection metadata outputs are not registered")
endif()

set(_reflection_localization_script
    "${CMAKE_SOURCE_DIR}/Tools/Codegen/ReflectionLocalization/export_localization.py"
)

if(EXISTS "${CMAKE_SOURCE_DIR}/Engine/Resources/Data/Localization")
    set(_reflection_catalog_dir "${CMAKE_SOURCE_DIR}/Engine/Resources/Data/Localization")
else()
    set(_reflection_catalog_dir "${CMAKE_SOURCE_DIR}/Resources/Data/Localization")
endif()

set(_reflection_localization_csv
    "${_reflection_catalog_dir}/Engine.csv"
)

add_custom_target(ExportReflectionLocalization ALL
    COMMAND "${Python3_EXECUTABLE}" "${_reflection_localization_script}"
        --input ${_engine_metadata_files}
        --catalog-dir "${_reflection_catalog_dir}"
        --base Engine.csv
        --output "${_reflection_localization_csv}"
        --in-place
    DEPENDS
        ${_engine_metadata_files}
        "${_reflection_localization_script}"
    COMMENT "Exporting reflection localization keys"
    COMMAND_EXPAND_LISTS
    VERBATIM
)

add_dependencies(ExportReflectionLocalization Engine)

if(BUILD_TESTING)
    add_test(NAME ReflectionLocalizationUtility
        COMMAND "${Python3_EXECUTABLE}" -B -m unittest discover
            -s "${CMAKE_SOURCE_DIR}/Tools/Codegen/ReflectionLocalization" -p "test_*.py" -v
    )
endif()
