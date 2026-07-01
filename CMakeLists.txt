cmake_minimum_required(VERSION 3.10)
project(FractalDumpset)

#set(CMAKE_CXX_FLAGS_DEBUG "-Og /MD")
#set(CMAKE_CXX_FLAGS_RELEASE "-Ofast /MD")
set(CMAKE_CXX_STANDARD 17)

if (CMAKE_CXX_FLAGS_RELEASE)
    if(MSVC)# MSVC 配置
        add_compile_options(
              /GL /Gw /DNDEBUG /EHs-c- /Zc:threadSafeInit-
        )
        add_link_options(
            /LTCG /OPT:REF /OPT:ICF /ENTRY:mainCRTStartup
        )
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
    else() # MinGW
        add_compile_options(
         -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti
        )
        add_link_options(
        -Wl,--gc-sections -Wl,--strip-all -s -mwindows
        )
    endif()
endif()

include_directories(${CMAKE_SOURCE_DIR}/include)
link_directories(${WINDOWS_SDK_LIB_PATH})
add_executable(FractalDumpset 
    Source/main.cpp 
    Source/External/glad/src/glad.c
)

target_link_libraries(FractalDumpset  dwmapi.lib)
target_link_libraries(FractalDumpset uxtheme.lib)

find_package(OpenGL REQUIRED)
if(OPENGL_FOUND)
    target_link_libraries(FractalDumpset ${OPENGL_LIBRARIES})
else()
    message(FATAL_ERROR "OpenGL not found!")
endif()

