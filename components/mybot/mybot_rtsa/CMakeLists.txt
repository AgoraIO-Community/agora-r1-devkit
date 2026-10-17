armino_build_get_property(armino_soc ARMINO_SOC)
if(NOT armino_soc STREQUAL "bk7258_ap")
    armino_component_register()
    return()
endif()

if(CONFIG_AGORA_IOT_SDK)
    message(FATAL_ERROR "Disable CONFIG_AGORA_IOT_SDK: MyBot provides its own RTSA and AOSL components")
endif()

set(rtsa_header "${CMAKE_CURRENT_LIST_DIR}/include/agora_rtc_api.h")
set(rtsa_library "${CMAKE_CURRENT_LIST_DIR}/lib/arm/libagora-rtc-sdk.a")
file(SHA256 "${rtsa_header}" rtsa_header_sha256)
file(SHA256 "${rtsa_library}" rtsa_library_sha256)
if(NOT rtsa_header_sha256 STREQUAL "29b1c21cbe416ee703eaa197ce623469619b1ed68cd3e9c66cb3abe2fafec7db")
    message(FATAL_ERROR "MyBot BK7258 RTSA header does not match the pinned 1.10.1 package")
endif()
if(NOT rtsa_library_sha256 STREQUAL "fb3d08126e14cf2eb99c531390822a591b094537d54abf8df95c5a5f587be8ec")
    message(FATAL_ERROR "MyBot BK7258 RTSA library does not match the pinned 1.10.1 package")
endif()

armino_component_register(
    SRCS hal/agora_thread.c
    INCLUDE_DIRS include
    PRIV_REQUIRES mybot_aosl bk_common bk_rtos
)

add_prebuilt_library(mybot_agora_rtsa "${rtsa_library}")
target_link_libraries(${COMPONENT_LIB} INTERFACE mybot_agora_rtsa)
