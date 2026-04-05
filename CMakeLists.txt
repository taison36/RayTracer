cmake_minimum_required(VERSION 3.31)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(Raytracer)

find_package(glm REQUIRED)
find_package(Vulkan REQUIRED)
set(SLANGC_EXECUTABLE $ENV{SLANG_PATH})

function(PREPEND var )
   set(listVar "")
   foreach(f ${${var}})
       list(APPEND listVar "${CMAKE_CURRENT_SOURCE_DIR}/${f}")
   endforeach(f)
   set(${var} "${listVar}" PARENT_SCOPE)
endfunction(PREPEND)

function(compileShaderSlang TARGET)
    cmake_parse_arguments(SHADER "" "" "SOURCES" ${ARGN})
    set(SHADERS_DIR ${CMAKE_CURRENT_LIST_DIR}/shaders)
    make_directory(${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders/)

    foreach(src IN LISTS SHADER_SOURCES)
        get_filename_component(name_we ${src} NAME_WE)
        set(out_spv ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders/${name_we}.spv)

        set(entry_points -entry computeMain)

        add_custom_command(
            OUTPUT ${out_spv}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADERS_DIR}
            COMMAND ${SLANGC_EXECUTABLE} ${src}
                    -target spirv
                    -profile spirv_1_4
                    -emit-spirv-directly
                    -fvk-use-entrypoint-name
                    ${entry_points}
                    -o ${out_spv}
            WORKING_DIRECTORY ${SHADERS_DIR}
            DEPENDS ${src}
            COMMENT "Compiling Slang shader ${src} to SPIR-V"
            VERBATIM
        )

        list(APPEND spv_outputs ${out_spv})
    endforeach()

    add_custom_target(${TARGET} DEPENDS ${spv_outputs})
endfunction()

set(SHADERS
    ${CMAKE_CURRENT_LIST_DIR}/shaders/checker.slang
)

compileShaderSlang(compileShaders
    SOURCES ${SHADERS}
)


set(TINYGLTF_HEADER_ONLY ON CACHE INTERNAL "" FORCE)
set(TINYGLTF_INSTALL OFF CACHE INTERNAL "" FORCE)

set(SHADERS
   ${CMAKE_CURRENT_LIST_DIR}/shaders/checker.comp
)

add_subdirectory(src)

add_executable(Raytracer ${Raytracer_SRC}
        src/core/RayTracerApplication.h
        src/core/RayTracerApplication.cpp)

add_custom_command(TARGET Raytracer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/resources $<TARGET_FILE_DIR:Raytracer>/resources
)

target_include_directories(${PROJECT_NAME} PRIVATE "lib/tinygltf")
target_include_directories(${PROJECT_NAME} PRIVATE "resources")
target_link_libraries(${PROJECT_NAME}  PRIVATE Vulkan::Vulkan)
target_link_libraries(${PROJECT_NAME} PRIVATE glm::glm)
