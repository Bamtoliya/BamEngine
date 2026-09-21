# --- CMake/LocalizationBuild.cmake ---

set(LOCALIZATION_DIR "${CMAKE_SOURCE_DIR}/Resources/Data/Localization")
set(LOCALIZATION_SCRIPT "${CMAKE_SOURCE_DIR}/Utility/CSVToJsonConverter/localizationcsvtojson.py")
file(GLOB LOCALIZATION_CSV_FILES CONFIGURE_DEPENDS "${LOCALIZATION_DIR}/*.csv")

set(LOCALIZATION_OUTPUTS
    "${LOCALIZATION_DIR}/English.json"
    "${LOCALIZATION_DIR}/Korean.json"
)

add_custom_command(
    OUTPUT ${LOCALIZATION_OUTPUTS}
    COMMAND ${Python3_EXECUTABLE} "${LOCALIZATION_SCRIPT}" --input "${LOCALIZATION_DIR}" --output "${LOCALIZATION_DIR}"
    DEPENDS
        "${LOCALIZATION_SCRIPT}"
        ${LOCALIZATION_CSV_FILES}
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Generating localization JSON files"
)

add_custom_target(GenerateLocalization ALL DEPENDS ${LOCALIZATION_OUTPUTS})