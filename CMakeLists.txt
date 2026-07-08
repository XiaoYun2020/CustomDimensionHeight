cmake_minimum_required(VERSION 3.20)
project(CustomDimensionHeight VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# LeviLamina SDK 路径（请根据你的本地SDK位置调整）
add_subdirectory(./LeviLamina)

add_library(${PROJECT_NAME} SHARED
    src/CustomDimensionHeight.cpp
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
    LeviLamina::LeviLamina
    nlohmann_json::nlohmann_json
)

# 输出产物到标准插件目录结构
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/output/plugins/${PROJECT_NAME}"
)