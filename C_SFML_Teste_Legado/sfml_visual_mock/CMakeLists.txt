cmake_minimum_required(VERSION 3.20)

project(AgentBioSimVisualMock LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT SFML_ROOT)
    set(_LOCAL_SFML_ROOT "C:/Users/Alex Martins/Desktop/Meus Projetos/simulacoes_biologicas/AntSimulator-master/AntSimulator-master/third_party/SFML-2.6.2")
    if(EXISTS "${_LOCAL_SFML_ROOT}")
        set(SFML_ROOT "${_LOCAL_SFML_ROOT}" CACHE PATH "SFML 2.6.2 root directory")
    endif()
endif()

if(NOT EXISTS "${SFML_ROOT}/include/SFML/Graphics.hpp")
    message(FATAL_ERROR "SFML_ROOT invalido. Aponte para a pasta SFML-2.6.2.")
endif()

add_executable(AgentBioSimVisualMock
    src/main.cpp
)

target_include_directories(AgentBioSimVisualMock BEFORE PRIVATE "${SFML_ROOT}/include")
target_link_libraries(AgentBioSimVisualMock PRIVATE
    "${SFML_ROOT}/lib/sfml-graphics.lib"
    "${SFML_ROOT}/lib/sfml-window.lib"
    "${SFML_ROOT}/lib/sfml-system.lib"
)

if(WIN32 AND SFML_ROOT)
    set(_SFML_BIN_DIR "${SFML_ROOT}/bin")
    foreach(_dll sfml-graphics-2.dll sfml-window-2.dll sfml-system-2.dll)
        add_custom_command(TARGET AgentBioSimVisualMock POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_SFML_BIN_DIR}/${_dll}"
                "$<TARGET_FILE_DIR:AgentBioSimVisualMock>/${_dll}"
        )
    endforeach()
endif()
