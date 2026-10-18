/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MYBOT_PROMPT_PLAYER_BK725X_H_
#define MYBOT_PROMPT_PLAYER_BK725X_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Plays the local "enter provisioning mode" PCM prompt synchronously. */
int mybot_prompt_player_bk725x_play_provisioning(void);

/* Starts the success prompt and waits until it has drained. */
int mybot_prompt_player_bk725x_play_success_sync(void);

/* Kept as a lifecycle no-op for controller cleanup compatibility. */
void mybot_prompt_player_bk725x_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* MYBOT_PROMPT_PLAYER_BK725X_H_ */
