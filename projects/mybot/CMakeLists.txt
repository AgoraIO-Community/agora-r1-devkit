cmake_minimum_required(VERSION 3.5)

include($ENV{ARMINO_TOOLS_PATH}/build_tools/cmake/project.cmake)

# prepare_vendor.py provides the official solution helpers alongside the
# repository-owned MyBot component container in this staged directory.
set(EXTRA_COMPONENTS_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/../../components
    ${CMAKE_CURRENT_SOURCE_DIR}/../../components/mybot
)

project(app)
