cmake_minimum_required(VERSION 3.12)
project(renderer_gpu CXX CUDA)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CUDA_FLAGS ${CMAKE_CUDA_FLAGS} "-arch=sm_50 -ccbin=c++ --relocatable-device-code=true")

# Add module path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}")

include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/external )
include_directories(/usr/local/cuda/include/)
include_directories(external/stb)

##################################################################
# Find OpenGL
##################################################################
find_package(OpenGL REQUIRED)

if (OPENGL_FOUND AND OPENGL_INCLUDE_DIR)
    include_directories( "${OPENGL_INCLUDE_DIR}" )
endif()

##################################################################
# GLFW compilation
##################################################################
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(external/glfw)
include_directories(external/glfw/include)

##################################################################
# IMGUI compilation
##################################################################
add_subdirectory( external/imgui )

##################################################################
# Find OptiX
##################################################################
# Search for the OptiX libraries and include files.
find_package(OptiX REQUIRED)
# Add the path to the OptiX headers to our include paths.
include_directories("${OptiX_INCLUDE}")

##################################################################
# Find Assimp
##################################################################
find_package(Assimp REQUIRED)
include_directories(${ASSIMP_INCLUDE_DIRS})

##################################################################
# Find PugiXML
##################################################################
add_subdirectory(external/pugixml-1.9)
include_directories(external/pugixml-1.9/src)


set(RENDERER_KERNELS
        src/shaders/raygeneration.cu
        src/shaders/exception.cu
        src/shaders/miss.cu
        src/shaders/triangle_bbox.cu
        src/shaders/triangle_intersection.cu
        src/shaders/closest_hit.cu
        src/shaders/light_sampling.cu
        src/shaders/any_hit.cu
        src/shaders/bsdf_sampling.cu
        src/math/rng.h src/math/basic.h)

set(RENDERER_SOURCE_FILES
        src/main.cpp

        src/core/opengl_renderer.h
        src/core/opengl_renderer.cpp
        src/core/camera.h
        src/core/camera.cpp
        src/core/perraydata.h
        src/utils/config.h
        src/core/primitivepool.h
        src/core/primitivepool.cpp
        src/core/vertexattributes.h
        src/utils/log.h
        src/utils/log.cpp
        src/core/scene.h
        src/core/scene.cpp
        src/utils/fileutil.h
        src/utils/fileutil.cpp
        src/utils/stats.h
        src/utils/stats.cpp src/core/materialpool.h src/core/materialpool.cpp src/core/materialdata.h src/core/geometrypool.h src/core/geometrypool.cpp src/core/lightpool.h src/core/lightpool.cpp src/core/lightdata.h src/core/flags.h src/core/image.h src/core/image.cpp src/core/texture.h src/core/texture.cpp src/core/optix_renderer.h src/core/optix_renderer.cpp src/core/globalsettings.h src/core/globalsettings.cpp)


add_library(CudaPTX OBJECT ${RENDERER_KERNELS})
set_property(TARGET CudaPTX PROPERTY CUDA_PTX_COMPILATION ON)

add_executable(gui ${RENDERER_SOURCE_FILES})
add_dependencies(gui CudaPTX)
target_link_libraries(gui optix glfw imgui ${ASSIMP_LIBRARIES} pugixml ${OPENGL_gl_LIBRARY} GLEW)

install(TARGETS CudaPTX DESTINATION ".")
#install(TARGETS gui RUNTIME DESTINATION bin/)