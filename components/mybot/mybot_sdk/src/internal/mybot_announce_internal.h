/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MYBOT_ANNOUNCE_INTERNAL_H_
#define MYBOT_ANNOUNCE_INTERNAL_H_

#include <mybot/platform/mybot_announce.h>

#include <stdbool.h>
#include <stdint.h>

#include <hal/aosl_hal_thread.h>

#define MYBOT_ANNOUNCE_MAX_CODE_LEN 16
#define MYBOT_ANNOUNCE_MAX_QUEUE (1 + MYBOT_ANNOUNCE_MAX_CODE_LEN)

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const mybot_announce_ops_t *ops;
    void *ops_ctx;
    aosl_mutex_t lock;
    bool active;
    void *handles[MYBOT_ANNOUNCE_MAX_QUEUE];
    int queue_len;
    int queue_pos;
    uint32_t generation;
} mybot_announce_t;

/* Playback reads source sounds under lock; the playback worker owns PCM delivery. */

/** Initialize the registered implementation. No-op when none is registered. */
int mybot_announce_init(mybot_announce_t *announce);

/** Release the implementation and any queued announcement. Idempotent. */
void mybot_announce_deinit(mybot_announce_t *announce);

/** Queue a pair-code announcement. A failed replacement preserves the current one. */
int mybot_announce_play_pair_code(mybot_announce_t *announce, const char *code);

/** Discard source sounds and invalidate PCM buffered by the playback worker. */
void mybot_announce_stop(mybot_announce_t *announce);

/** Whether source sounds still have PCM left to feed into the playback buffer. */
bool mybot_announce_is_active(mybot_announce_t *announce);

/** Read the generation published by a successful replacement or stop. */
uint32_t mybot_announce_get_generation(mybot_announce_t *announce);

/** Copy the next announcement PCM frames (16 kHz mono s16).
 *  Source reads and the optional generation snapshot share the same lock, so
 *  the returned PCM belongs to one announcement even across sound boundaries.
 *  @return frames copied, at most max_frames (0 when idle/finished). */
int mybot_announce_read_pcm(mybot_announce_t *announce, int16_t *dst, int max_frames,
                            uint32_t *generation);

#ifdef __cplusplus
}
#endif

#endif /* MYBOT_ANNOUNCE_INTERNAL_H_ */
