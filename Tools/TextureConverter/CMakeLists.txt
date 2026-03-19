# Tools/TextureConverter/CMakeLists.txt

project("TextureConverter")

find_package(directxtex CONFIG REQUIRED)

# 소스 파일 추가
file(GLOB_RECURSE SOURCES "Private/*.cpp" "Public/*.h")

add_library(TextureConverter STATIC ${SOURCES})

# 헤더 경로 설정
target_include_directories(TextureConverter PUBLIC "Public")

target_link_libraries(TextureConverter PUBLIC 
	Engine
	Microsoft::DirectXTex
)

set_target_properties(TextureConverter PROPERTIES FOLDER "Tools")