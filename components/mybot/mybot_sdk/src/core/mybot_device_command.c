/* SPDX-License-Identifier: Apache-2.0 */
#include "mybot_device_command.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

/* DeviceCommand objects are small and fixed: one pass rejects unknown or repeated keys. */
static bool read_fields(const mybot_json_t *object, const char *const *names, size_t count,
                        unsigned required, const mybot_json_t **fields) {
    if (!object || object->type != MYBOT_JSON_OBJECT) {
        return false;
    }
    memset(fields, 0, count * sizeof(const mybot_json_t *));
    unsigned seen = 0;
    for (const mybot_json_t *item = object->child; item; item = item->next) {
        size_t index = 0;
        while (index < count && (!item->string || strcmp(item->string, names[index]) != 0)) {
            index++;
        }
        if (index == count || (seen & (1u << index))) {
            return false;
        }
        seen |= 1u << index;
        fields[index] = item;
    }
    return (seen & required) == required;
}

static bool is_string(const mybot_json_t *value, const char *expected) {
    const char *text = mybot_json_get_string(value);
    return text && strcmp(text, expected) == 0;
}

static bool read_id(const mybot_json_t *value, char *destination) {
    const char *text = mybot_json_get_string(value);
    if (!text) {
        return false;
    }
    size_t length = strlen(text);
    if (length == 0 || length >= MYBOT_DEVICE_COMMAND_ID_CAPACITY) {
        return false;
    }
    memcpy(destination, text, length + 1);
    return true;
}

static bool read_integer(const mybot_json_t *value, int minimum, int maximum, int *result) {
    if (!value || value->type != MYBOT_JSON_NUMBER || !isfinite(value->valuedouble) ||
        value->valuedouble < minimum || value->valuedouble > maximum ||
        value->valuedouble != (double)value->valueint) {
        return false;
    }
    *result = (int)value->valueint;
    return true;
}

static bool read_action(const mybot_json_t *name, const mybot_json_t *params,
                        mybot_device_command_t *command) {
    const mybot_json_t *field[1];
    if (is_string(name, "audio.set_volume")) {
        static const char *const names[] = {"level"};
        command->type = MYBOT_DEVICE_COMMAND_SET_VOLUME;
        return read_fields(params, names, 1, 1, field) &&
               read_integer(field[0], 0, 100, &command->params.volume);
    }
    if (is_string(name, "audio.adjust_volume")) {
        static const char *const names[] = {"delta"};
        command->type = MYBOT_DEVICE_COMMAND_ADJUST_VOLUME;
        return read_fields(params, names, 1, 1, field) &&
               read_integer(field[0], -100, 100, &command->params.delta);
    }
    if (is_string(name, "audio.set_mute")) {
        static const char *const names[] = {"muted"};
        if (!read_fields(params, names, 1, 1, field) ||
            (field[0]->type != MYBOT_JSON_TRUE && field[0]->type != MYBOT_JSON_FALSE)) {
            return false;
        }
        command->type = MYBOT_DEVICE_COMMAND_SET_MUTE;
        command->params.muted = field[0]->type == MYBOT_JSON_TRUE;
        return true;
    }
    return false;
}

static bool read_emotion(const mybot_json_t *name, const mybot_json_t *params,
                         mybot_device_command_t *command) {
    static const struct {
        const char *name;
        mybot_lcd_indicator_t indicator;
    } emotions[] = {
        {"emotion.happy", MYBOT_LCD_INDICATOR_HAPPY},
        {"emotion.sad", MYBOT_LCD_INDICATOR_SAD},
        {"emotion.angry", MYBOT_LCD_INDICATOR_ANGRY},
        {"emotion.surprised", MYBOT_LCD_INDICATOR_SURPRISED},
    };
    size_t index = 0;
    while (index < sizeof(emotions) / sizeof(emotions[0]) &&
           !is_string(name, emotions[index].name)) {
        index++;
    }
    if (index == sizeof(emotions) / sizeof(emotions[0])) {
        return false;
    }
    /* Current platform presets carry no intensity or duration overrides. */
    if (!params || params->type != MYBOT_JSON_OBJECT || params->child) {
        return false;
    }
    command->type = MYBOT_DEVICE_COMMAND_EMOTION;
    command->params.emotion = emotions[index].indicator;
    return true;
}

bool mybot_device_command_parse(const mybot_json_t *root, mybot_device_command_t *command) {
    static const char *const names[] = {"type",       "schema",     "version",
                                        "message_id", "command_id", "conversation_id",
                                        "command",    "issued_at",  "expires_at"};
    const mybot_json_t *fields[9];
    int version;
    if (!command || !read_fields(root, names, 9, (1u << 7) - 1, fields) ||
        !is_string(fields[0], "device_command") ||
        !is_string(fields[1], "botstation.device-command") ||
        !read_integer(fields[2], 1, 1, &version)) {
        return false;
    }
    memset(command, 0, sizeof(*command));
    if (!read_id(fields[3], command->message_id) || !read_id(fields[4], command->command_id) ||
        !read_id(fields[5], command->conversation_id)) {
        return false;
    }
    static const char *const command_names[] = {"type", "name", "params"};
    const mybot_json_t *command_fields[3];
    if (!read_fields(fields[6], command_names, 3, 7, command_fields)) {
        return false;
    }
    if (is_string(command_fields[0], "action")) {
        return read_action(command_fields[1], command_fields[2], command);
    }
    return is_string(command_fields[0], "scene") &&
           read_emotion(command_fields[1], command_fields[2], command);
}
