/*
 * This file is part of libplacebo.
 *
 * libplacebo is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * libplacebo is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with libplacebo. If not, see <http://www.gnu.org/licenses/>.
 */

#include "common.h"

static void grow_str(void *tactx, pl_str *str, size_t len)
{
    if (len > talloc_get_size(str->buf)) {
        size_t new_size = xta_calc_prealloc_elems(len);
        str->buf = talloc_realloc_size(tactx, str->buf, new_size);
    }
}

void pl_str_xappend(void *tactx, pl_str *str, pl_str append)
{
    if (!append.len)
        return;

    // Also append an extra \0 for convenience, since a lot of the time
    // this function will be used to generate a string buffer
    grow_str(tactx, str, str->len + append.len + 1);
    memcpy(str->buf + str->len, append.buf, append.len);
    str->len += append.len;
    str->buf[str->len] = '\0';
}

void pl_str_xappend_asprintf(void *tactx, pl_str *str, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    pl_str_xappend_vasprintf(tactx, str, fmt, ap);
    va_end(ap);
}

void pl_str_xappend_vasprintf(void *tactx, pl_str *str, const char *fmt, va_list ap)
{
    // First, we need to determine the size that will be required for
    // printing the entire string. Do this by making a copy of the va_list
    // and printing it to a null buffer.
    va_list copy;
    va_copy(copy, ap);
    int size = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);

    // Make room in `str` and format to there directly
    grow_str(tactx, str, str->len + size + 1);
    str->len += vsnprintf(str->buf + str->len, size + 1, fmt, ap);
}

int pl_str_sscanf(pl_str str, const char *fmt, ...)
{
    char *tmp = pl_strdup0(NULL, str);
    va_list va;
    va_start(va, fmt);
    int ret = vsscanf(tmp, fmt, va);
    va_end(va);
    talloc_free(tmp);
    return ret;
}

int pl_strchr(pl_str str, int c)
{
    if (!str.len)
        return -1;

    void *pos = memchr(str.buf, c, str.len);
    if (pos)
        return (intptr_t) pos - (intptr_t) str.buf;
    return -1;
}

size_t pl_strspn(pl_str str, const char *accept)
{
    for (size_t i = 0; i < str.len; i++) {
        if (!strchr(accept, str.buf[i]))
            return i;
    }

    return str.len;
}

size_t pl_strcspn(pl_str str, const char *reject)
{
    for (size_t i = 0; i < str.len; i++) {
        if (strchr(reject, str.buf[i]))
            return i;
    }

    return str.len;
}

pl_str pl_str_strip(pl_str str)
{
    static const char *whitespace = " \n\r\t\v\f";
    while (str.len && strchr(whitespace, str.buf[0])) {
        str.buf++;
        str.len--;
    }
    while (str.len && strchr(whitespace, str.buf[str.len - 1]))
        str.len--;
    return str;
}

int pl_str_find(pl_str haystack, pl_str needle)
{
    if (!needle.len)
        return 0;

    for (size_t i = 0; i + needle.len <= haystack.len; i++) {
        if (memcmp(&haystack.buf[i], needle.buf, needle.len) == 0)
            return i;
    }

    return -1;
}

pl_str pl_str_split_char(pl_str str, char sep, pl_str *out_rest)
{
    int pos = pl_strchr(str, sep);
    if (pos < 0) {
        if (out_rest)
            *out_rest = (pl_str) {0};
        return str;
    } else {
        if (out_rest)
            *out_rest = pl_str_drop(str, pos + 1);
        return pl_str_take(str, pos);
    }
}

pl_str pl_str_split_str(pl_str str, pl_str sep, pl_str *out_rest)
{
    int pos = pl_str_find(str, sep);
    if (pos < 0) {
        if (out_rest)
            *out_rest = (pl_str) {0};
        return str;
    } else {
        if (out_rest)
            *out_rest = pl_str_drop(str, pos + sep.len);
        return pl_str_take(str, pos);
    }
}

static int h_to_i(unsigned char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;

    return -1; // invalid char
}

bool pl_str_decode_hex(void *tactx, pl_str hex, pl_str *out)
{
    if (!out)
        return false;

    char *buf = talloc_array(tactx, char, hex.len / 2);
    int len = 0;

    while (hex.len >= 2) {
        int a = h_to_i(hex.buf[0]);
        int b = h_to_i(hex.buf[1]);
        hex.buf += 2;
        hex.len -= 2;

        if (a < 0 || b < 0) {
            talloc_free(buf);
            return false;
        }

        buf[len++] = (a << 4) | b;
    }

    *out = (pl_str) { buf, len };
    return true;
}
