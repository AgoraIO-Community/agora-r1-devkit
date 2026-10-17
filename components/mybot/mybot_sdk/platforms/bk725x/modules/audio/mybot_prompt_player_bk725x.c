/* SPDX-License-Identifier: Apache-2.0 */
#include "mybot_prompt_player_bk725x.h"

#include "mybot_audio_internal_bk725x.h"
#include "mybot_audio_playback_bk725x.h"
#include "mybot_assets.h"
#include "mybot_ogg_pcm_bk725x.h"

#include <common/bk_err.h>
#include "mybot_language.h"
#include "mybot_platform_log.h"
#include <os/mem.h>
#include <os/os.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TAG "mybot_prompt"

/* ~16 s of s16le mono, far beyond any provisioning prompt. */
#define PROMPT_MAX_FRAMES (512U * 1024U / 2U)
#define PROMPT_RATE_HZ 16000
#define PROMPT_CHANNELS 1
#define PROMPT_BITS 16
#define PROMPT_FRAMES_PER_WRITE 320
#define PROMPT_DRAIN_MS 200
#define PROMPT_MAX_CONSECUTIVE_TIMEOUTS 100
#define PROVISIONING_PROMPT_PATH                                                         \
    MYBOT_ASSETS_DIR "/locales/" MYBOT_LANGUAGE_TAG "/wificonfig.ogg"
#define SUCCESS_PROMPT_PATH                                                             \
    MYBOT_ASSETS_DIR "/locales/" MYBOT_LANGUAGE_TAG "/success.ogg"

static const char *s_prompt_path;

static int load_pcm(int16_t **pcm, int *frames) {
    mybot_asset_t asset;
    mybot_ogg_pcm_t ogg;

    *pcm = NULL;
    *frames = 0;

    if (!s_prompt_path) {
        MYBOT_LOGE(TAG, "no prompt path configured");
        return -1;
    }

    if (mybot_asset_find(s_prompt_path, &asset) < 0) {
        MYBOT_LOGW(TAG, "embedded prompt is unavailable: %s", s_prompt_path);
        return -1;
    }

    if (mybot_ogg_pcm_load_memory(s_prompt_path, asset.data, asset.size, PROMPT_RATE_HZ,
                                  &ogg) < 0) {
        MYBOT_LOGW(TAG, "failed to decode embedded prompt: %s", s_prompt_path);
        return -1;
    }
    if (ogg.frames <= 0 || (size_t)ogg.frames > PROMPT_MAX_FRAMES) {
        MYBOT_LOGW(TAG, "decoded prompt out of range: frames=%d", ogg.frames);
        mybot_ogg_pcm_free(&ogg);
        return -1;
    }

    *pcm = ogg.pcm;
    *frames = ogg.frames;
    MYBOT_LOGI(TAG, "embedded prompt decoded: %s, frames=%d", s_prompt_path, ogg.frames);
    return 0;
}

static int play_pcm(const int16_t *pcm, int frames) {
    int offset = 0;
    int consecutive_timeouts = 0;
    int result = -1;
    void *playback_ctx = NULL;
    bool playback_started = false;

    if (mybot_audio_bk725x_playback_init(&playback_ctx, PROMPT_RATE_HZ,
                                         PROMPT_CHANNELS, PROMPT_BITS) < 0) {
        MYBOT_LOGE(TAG, "prompt playback init failed");
        return -1;
    }
    if (mybot_audio_bk725x_volume_apply() < 0) {
        MYBOT_LOGW(TAG, "prompt playback volume apply failed");
    }
    if (mybot_audio_bk725x_playback_start(playback_ctx) < 0) {
        MYBOT_LOGE(TAG, "prompt playback start failed");
        goto cleanup;
    }
    playback_started = true;

    while (offset < frames) {
        int requested = frames - offset;
        if (requested > PROMPT_FRAMES_PER_WRITE) {
            requested = PROMPT_FRAMES_PER_WRITE;
        }
        int written = mybot_audio_bk725x_playback_write(playback_ctx, pcm + offset, requested);
        if (written < 0) {
            MYBOT_LOGE(TAG, "prompt write failed at frame %d/%d", offset, frames);
            goto cleanup;
        }
        if (written == 0) {
            if (++consecutive_timeouts >= PROMPT_MAX_CONSECUTIVE_TIMEOUTS) {
                MYBOT_LOGE(TAG, "prompt write timed out repeatedly");
                goto cleanup;
            }
            continue;
        }
        consecutive_timeouts = 0;
        offset += written;
    }

    if (offset == frames) {
        /* Let the raw-stream and speaker buffers drain. */
        rtos_delay_milliseconds(PROMPT_DRAIN_MS);
        result = 0;
    } else {
        result = -1;
    }

cleanup:
    if (playback_started) {
        (void)mybot_audio_bk725x_playback_stop(playback_ctx);
    }
    mybot_audio_bk725x_playback_destroy(playback_ctx);
    return result;
}

static int play_prompt_sync(const char *path) {
    int16_t *pcm = NULL;
    int frames = 0;
    int result = -1;

    s_prompt_path = path;
    if (load_pcm(&pcm, &frames) == 0) {
        result = play_pcm(pcm, frames);
    }
    if (pcm) {
        psram_free(pcm);
    }
    s_prompt_path = NULL;
    return result;
}

int mybot_prompt_player_bk725x_play_provisioning(void) {
    MYBOT_LOGI(TAG, "play provisioning prompt requested");
    return play_prompt_sync(PROVISIONING_PROMPT_PATH);
}

int mybot_prompt_player_bk725x_play_success_sync(void) {
    MYBOT_LOGI(TAG, "play success prompt synchronously");
    return play_prompt_sync(SUCCESS_PROMPT_PATH);
}

void mybot_prompt_player_bk725x_stop(void) {
    s_prompt_path = NULL;
}
