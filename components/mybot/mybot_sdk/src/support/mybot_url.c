/* SPDX-License-Identifier: Apache-2.0 */
#include "mybot_url.h"

#include <mybot/mybot_build_config.h>

#include <string.h>

static bool host_is_safe(const char *host) {
    if (!host || !host[0]) {
        return false;
    }
    for (const unsigned char *p = (const unsigned char *)host; *p; p++) {
        if (*p <= 0x20 || *p > 0x7e || *p == '/' || *p == '\\' || *p == '?' || *p == '#' ||
            *p == '@') {
            return false;
        }
    }
    return true;
}

static bool request_target_is_safe(const char *target) {
    if (!target || target[0] != '/') {
        return false;
    }
    for (const unsigned char *p = (const unsigned char *)target; *p; p++) {
        if (*p <= 0x20 || *p > 0x7e || *p == '#' || *p == '\\') {
            return false;
        }
    }
    return true;
}

int mybot_url_parse(const char *url, mybot_url_parts_t *parts) {
    if (!url || !parts) {
        return -1;
    }

    memset(parts, 0, sizeof(*parts));
    parts->path[0] = '/';

    const char *p = url;

#if MYBOT_ENABLE_HTTPS
    if (strncmp(p, "https://", 8) == 0) {
        parts->use_tls = true;
        parts->port = MYBOT_URL_HTTPS_PORT;
        p += 8;
    } else
#endif
#if MYBOT_ALLOW_INSECURE_HTTP
        if (strncmp(p, "http://", 7) == 0) {
        parts->use_tls = false;
        parts->port = MYBOT_URL_HTTP_PORT;
        p += 7;
    } else
#endif
    {
        return -1;
    }

    const char *host_start = p;
    while (*p && *p != ':' && *p != '/') {
        p++;
    }
    size_t host_len = (size_t)(p - host_start);
    if (host_len == 0 || host_len >= sizeof(parts->host)) {
        return -1;
    }
    memcpy(parts->host, host_start, host_len);
    parts->host[host_len] = '\0';
    if (!host_is_safe(parts->host)) {
        return -1;
    }

    if (*p == ':') {
        p++;
        if (*p < '0' || *p > '9') {
            return -1;
        }
        unsigned int port = 0;
        while (*p >= '0' && *p <= '9') {
            unsigned int digit = (unsigned int)(*p - '0');
            if (port > 6553U || (port == 6553U && digit > 5U)) {
                return -1;
            }
            port = port * 10U + digit;
            p++;
        }
        if (port == 0) {
            return -1;
        }
        parts->port = (int)port;
    }

    if (*p == '/') {
        size_t path_len = strlen(p);
        if (path_len >= sizeof(parts->path)) {
            return -1;
        }
        memcpy(parts->path, p, path_len + 1U);
    } else if (*p != '\0') {
        return -1;
    }

    return request_target_is_safe(parts->path) ? 0 : -1;
}
