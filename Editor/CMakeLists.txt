Project("Editor")

add_executable(Editor WIN32 
	src/Editor.cpp
	src/Editor.h
)

target_link_libraries(Editor PRIVATE Engine)