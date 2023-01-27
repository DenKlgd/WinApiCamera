cmake_minimum_required (VERSION 3.8)

add_executable (WinApiCamera WIN32 main.cpp "resource.h" "WinApiCamera.rc")

target_link_libraries(WinApiCamera Vfw32.lib)
target_link_libraries(WinApiCamera Comctl32.lib)
