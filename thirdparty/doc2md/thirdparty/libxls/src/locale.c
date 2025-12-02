/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 * Copyright 2020 Evan Miller
 *
 * This file is part of libxls -- A multiplatform, C/C++ library for parsing
 * Excel(TM) files.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ''AS
 * IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
#include "config.h"
#include <stdlib.h>
#include <limits.h>
#include "../include/libxls/locale.h"

#if defined(_WIN32) || defined(WIN32) || defined(_WIN64) || defined(WIN64) || defined(WINDOWS)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

xls_locale_t xls_createlocale(void) {
#if defined(_WIN32) || defined(WIN32) || defined(_WIN64) || defined(WIN64) || defined(WINDOWS)
    xls_locale_t locale = (xls_locale_t)malloc(sizeof(*locale));
    if (!locale)
        return NULL;
    locale->codepage = CP_UTF8;
    return locale;
#elif defined(__APPLE__)
    return newlocale(LC_CTYPE_MASK, "UTF-8", NULL);
#else
    return newlocale(LC_CTYPE_MASK, "C.UTF-8", NULL);
#endif
}

void xls_freelocale(xls_locale_t locale) {
    if (!locale)
        return;
#if defined(_WIN32) || defined(WIN32) || defined(_WIN64) || defined(WIN64) || defined(WINDOWS)
    free(locale);
#else
    freelocale(locale);
#endif
}

size_t xls_wcstombs_l(char *restrict s, const wchar_t *restrict pwcs, size_t n, xls_locale_t loc) {
#if defined(_WIN32) || defined(WIN32) || defined(_WIN64) || defined(WIN64) || defined(WINDOWS)
    if (!pwcs)
        return (size_t)-1;

    UINT codepage = CP_UTF8;
    if (loc)
        codepage = loc->codepage;

    if (!s) {
        int required = WideCharToMultiByte(codepage, 0, pwcs, -1, NULL, 0, NULL, NULL);
        if (required == 0)
            return (size_t)-1;
        return (size_t)(required - 1);
    }

    if (n == 0)
        return 0;

    size_t capped = n > (size_t)INT_MAX ? (size_t)INT_MAX : n;
    int written = WideCharToMultiByte(codepage, 0, pwcs, -1, s, (int)capped, NULL, NULL);
    if (written == 0)
        return (size_t)-1;
    return (size_t)(written - 1);
#elif defined(HAVE_WCSTOMBS_L)
    return wcstombs_l(s, pwcs, n, loc);
#else
    locale_t oldlocale = uselocale(loc);
    size_t result = wcstombs(s, pwcs, n);
    uselocale(oldlocale);
    return result;
#endif
}
