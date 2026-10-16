find_package(Python3 COMPONENTS Interpreter REQUIRED)

get_target_property(_engine_metadata_files Engine REFLECTION_METADATA_FILES)

if(NOT _engine_metadata_files)
    message(FATAL_ERROR "Engine reflection metadata outputs are not registered")
endif()

set(_reflection_localization_script
    "${CMAKE_SOURCE_DIR}/Utility/ReflectionLocalization/export_localization.py"
)

set(_reflection_localization_csv
    "${CMAKE_SOURCE_DIR}/Resources/Data/Localization/Engine.csv"
)

add_custom_target(ExportReflectionLocalization ALL
    COMMAND "${Python3_EXECUTABLE}" "${_reflection_localization_script}"
        --input ${_engine_metadata_files}
        --catalog-dir "${CMAKE_SOURCE_DIR}/Resources/Data/Localization"
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
            -s "${CMAKE_SOURCE_DIR}/Utility/ReflectionLocalization" -p "test_*.py" -v
    )
endif()
