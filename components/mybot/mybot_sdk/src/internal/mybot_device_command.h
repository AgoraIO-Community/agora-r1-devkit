/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MYBOT_DEVICE_COMMAND_H_
#define MYBOT_DEVICE_COMMAND_H_

#include "mybot_json.h"
#include "mybot/platform/mybot_lcd.h"

#include <stdbool.h>

#define MYBOT_DEVICE_COMMAND_ID_CAPACITY 128

typedef enum {
    MYBOT_DEVICE_COMMAND_SET_VOLUME,
    MYBOT_DEVICE_COMMAND_ADJUST_VOLUME,
    MYBOT_DEVICE_COMMAND_SET_MUTE,
    MYBOT_DEVICE_COMMAND_EMOTION,
} mybot_device_command_type_t;

typedef struct {
    mybot_device_command_type_t type;
    char message_id[MYBOT_DEVICE_COMMAND_ID_CAPACITY];
    char command_id[MYBOT_DEVICE_COMMAND_ID_CAPACITY];
    char conversation_id[MYBOT_DEVICE_COMMAND_ID_CAPACITY];
    union {
        int volume;
        int delta;
        bool muted;
        mybot_lcd_indicator_t emotion;
    } params;
} mybot_device_command_t;

/* Parse DeviceCommand v1 without execution or session access.
 * issued_at and expires_at are optional and ignored.
 * root is borrowed; command owns its fields and is valid only on success. */
bool mybot_device_command_parse(const mybot_json_t *root, mybot_device_command_t *command);

#endif /* MYBOT_DEVICE_COMMAND_H_ */
