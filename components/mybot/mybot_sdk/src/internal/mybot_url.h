/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MYBOT_URL_H_
#define MYBOT_URL_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MYBOT_URL_HOST_CAPACITY 128
#define MYBOT_URL_PATH_CAPACITY 512
#define MYBOT_URL_HTTP_PORT 80
#define MYBOT_URL_HTTPS_PORT 443

typedef struct {
    bool use_tls;
    char host[MYBOT_URL_HOST_CAPACITY];
    int port;
    char path[MYBOT_URL_PATH_CAPACITY];
} mybot_url_parts_t;

/**
 * Parse an absolute HTTP(S) URL into bounded components.
 *
 * HTTPS is accepted when MYBOT_ENABLE_HTTPS is enabled. Plain HTTP is accepted
 * only when MYBOT_ALLOW_INSECURE_HTTP is explicitly enabled. The host must be a
 * non-empty ASCII DNS name or address without userinfo; the port must be
 * 1..65535 and the path/query must be a printable request target without a
 * fragment or backslash.
 *
 * @return 0 on success, -1 on malformed or disabled URL forms.
 */
int mybot_url_parse(const char *url, mybot_url_parts_t *parts);

#ifdef __cplusplus
}
#endif

#endif /* MYBOT_URL_H_ */
