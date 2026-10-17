armino_build_get_property(armino_soc ARMINO_SOC)
if(NOT armino_soc STREQUAL "bk7258_ap")
    armino_component_register()
    return()
endif()

set(incs
    include
    platforms/bk725x/modules/controller
)

set(priv_incs
    src/internal
    platforms/bk725x/adapter
    platforms/bk725x/modules/announce
    platforms/bk725x/modules/audio
    platforms/bk725x/modules/button
    platforms/bk725x/modules/display
    platforms/bk725x/modules/event
    platforms/bk725x/modules/key
    platforms/bk725x/modules/log
    platforms/bk725x/modules/network
    platforms/bk725x/modules/storage
)

set(sdk_srcs
    src/core/mybot_app.c
    src/core/mybot_device_command.c
    src/core/mybot_device_lifecycle.c
    src/core/mybot_presenter.c
    src/core/mybot_state_model.c
    src/core/mybot_version.c
    src/media/mybot_audio.c
    src/media/mybot_media_pipeline.c
    src/media/mybot_video.c
    src/media/mybot_wake_words.c
    src/platform/mybot_announce.c
    src/platform/mybot_https.c
    src/platform/mybot_key.c
    src/platform/mybot_kv_store.c
    src/platform/mybot_lcd.c
    src/platform/mybot_platform.c
    src/platform/mybot_wifi.c
    src/rtc/mybot_agora_rtc.c
    src/service/mybot_device_client.c
    src/support/mybot_http_client.c
    src/support/mybot_json.c
    src/support/mybot_ringbuf.c
)

set(adapter_srcs
    platforms/bk725x/adapter/bk725x_announce_pcm.c
    platforms/bk725x/adapter/bk725x_audio_capture.c
    platforms/bk725x/adapter/bk725x_audio_playback.c
    platforms/bk725x/adapter/bk725x_audio_volume.c
    platforms/bk725x/adapter/bk725x_https_mbedtls.c
    platforms/bk725x/adapter/bk725x_key_button.c
    platforms/bk725x/adapter/bk725x_kv_store_bk_env.c
    platforms/bk725x/adapter/bk725x_lcd_dual_screen.c
    platforms/bk725x/adapter/bk725x_platform_adapters.c
    platforms/bk725x/adapter/bk725x_wifi_connectivity.c
)

set(announce_module_srcs
    platforms/bk725x/modules/announce/mybot_announce_pcm_bk725x.c
)

set(audio_module_srcs
    platforms/bk725x/modules/audio/mybot_audio_capture_bk725x.c
    platforms/bk725x/modules/audio/mybot_audio_playback_bk725x.c
    platforms/bk725x/modules/audio/mybot_audio_power_bk725x.c
    platforms/bk725x/modules/audio/mybot_audio_volume_bk725x.c
    platforms/bk725x/modules/audio/mybot_ogg_pcm_bk725x.c
    platforms/bk725x/modules/audio/mybot_prompt_player_bk725x.c
)

set(application_module_srcs
    platforms/bk725x/modules/button/mybot_button_bk725x.c
    platforms/bk725x/modules/controller/mybot_controller_bk725x.c
    platforms/bk725x/modules/display/mybot_display_bk725x.c
    platforms/bk725x/modules/event/mybot_event.c
    platforms/bk725x/modules/key/mybot_key_dispatcher_bk725x.c
)

if(CONFIG_MYBOT_DEBUG_CPU)
    list(APPEND priv_incs platforms/bk725x/modules/monitor)
    list(APPEND application_module_srcs
        platforms/bk725x/modules/monitor/mybot_cpu_monitor_bk725x.c
    )
endif()

set(network_module_srcs
    platforms/bk725x/modules/network/mybot_connectivity_bk725x.c
    platforms/bk725x/modules/network/mybot_https_bk725x.c
    platforms/bk725x/modules/network/mybot_network_bk725x.c
    platforms/bk725x/modules/network/mybot_provisioning_bk725x.c
    platforms/bk725x/modules/network/mybot_wifi_credentials.c
    platforms/bk725x/modules/network/mybot_wifi_runtime.c
)

set(storage_module_srcs
    platforms/bk725x/modules/storage/mybot_assets.c
    platforms/bk725x/modules/storage/mybot_kv_bk_env.c
    platforms/bk725x/modules/storage/mybot_sdcard_bk725x.c
    platforms/bk725x/modules/storage/mybot_sdcard_msc_bk725x.c
)

set(srcs
    ${sdk_srcs}
    ${adapter_srcs}
    ${announce_module_srcs}
    ${audio_module_srcs}
    ${application_module_srcs}
    ${network_module_srcs}
    ${storage_module_srcs}
)

armino_component_register(
    SRCS "${srcs}"
    INCLUDE_DIRS "${incs}"
    PRIV_INCLUDE_DIRS "${priv_incs}"
    PRIV_REQUIRES mybot_aosl mybot_rtsa psa_mbedtls lwip_intf_v2_1 easy_flash bk_common bk_rtos bk_uid
                  audio_pipeline onboard_mic_stream onboard_speaker_stream raw_stream
                  bk_display bk_peripheral multimedia bk_wifi bk_event bk_netif json
                  bk_vfs fatfs bk_usb
)
