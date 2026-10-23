/* SPDX-License-Identifier: Apache-2.0 */
#include "mybot_http_client.h"

#include <string.h>

bool mybot_http_header_value_is_safe(const char *value, bool allow_tab) {
    if (!value || !value[0]) {
        return false;
    }
    for (const unsigned char *p = (const unsigned char *)value; *p; p++) {
        if ((*p < 0x20 && !(allow_tab && *p == '\t')) || *p > 0x7e) {
            return false;
        }
    }
    return true;
}
