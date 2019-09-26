cmake_minimum_required(VERSION 3.0)

project(GServer VERSION 0.1.0 LANGUAGES CXX)
set(API_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}")
set(BUILD_VERSION "${PROJECT_VERSION}")

set(SSOURCES
  "Server.cpp"
)

set(CSOURCES
  #"Client.cpp"
  "Game.cpp"
  "Main.cpp"
  "Renderer.cpp"
  "RenderDeviceManager.cpp"
  "Resource.cpp"
  "Vertex.cpp"
  "Window.cpp"
)

set(SHADERS
  "shaders/default.vert"
  "shaders/default.frag"
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")

add_executable("AtlasGenerator" "AtlasGenerator.cpp")
target_include_directories("AtlasGenerator" PRIVATE "stb")
#add_executable("GServer" "${SSOURCES}")
add_executable("GClient" "${CSOURCES}")
target_include_directories("GClient" PRIVATE "stb")

add_dependencies("GClient" "AtlasGenerator")

set(COMPILED_SHADERS "")
foreach(SHADER ${SHADERS})
  get_filename_component(basename ${SHADER} NAME)
  set(output ${CMAKE_BINARY_DIR}/${basename}.spv)
  list(APPEND COMPILED_SHADERS ${output})
  add_custom_command(OUTPUT ${output}
  COMMAND glslc ${CMAKE_SOURCE_DIR}/${SHADER} -g -o ${output}
  DEPENDS ${SHADER}
  COMMENT "Rebuilding ${output}" )
  message(STATUS "Generating build commands for ${output}")
endforeach()


set(RC_DEPENDS "${COMPILED_SHADERS}")

function(add_resource input)
  get_filename_component(basename ${input} NAME)
  string(MAKE_C_IDENTIFIER ${basename} input_identifier)
  set(output "${input_identifier}.o")
  target_link_libraries("GClient" ${CMAKE_BINARY_DIR}/${output})

  add_custom_command(
    OUTPUT ${output}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMAND ${CMAKE_LINKER} --relocatable --format binary --output ${output} ${basename}
    DEPENDS ${input}
  )

  set(RC_DEPENDS ${RC_DEPENDS} ${CMAKE_BINARY_DIR}/${output} PARENT_SCOPE)
endfunction()

# Resource files
foreach(SHADER ${COMPILED_SHADERS})
  add_resource("${SHADER}")
endforeach()

add_custom_target(rc ALL DEPENDS ${RC_DEPENDS})

#target_link_libraries("GServer" "enet")

# Graphics Stuffs
find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)

target_include_directories("GClient" PRIVATE "${Vulkan_INCLUDE_DIRS}")
target_link_libraries("GClient" glfw "${Vulkan_LIBRARIES}")
