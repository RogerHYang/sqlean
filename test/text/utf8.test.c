// Copyright (c) 2024 Anton Zhiyanov, MIT License
// https://github.com/nalgeon/sqlean

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "text/utf8/utf8.h"

static void test_len(void) {
    printf("test_len...");
    const char* s = "Hello, 世界!";
    assert(utf8_len(s, strlen(s)) == 10);
    printf("OK\n");
}

static void test_peek(void) {
    printf("test_peek...");
    const char* s = "Hello, 世界!";
    assert(utf8_peek(s) == 'H');
    assert(utf8_peek(s + 2) == 'l');
    assert(utf8_peek(s + 4) == 'o');
    assert(utf8_peek(s + 6) == ' ');
    assert(utf8_peek(s + 7) == 0x4E16);
    assert(utf8_peek(s + 10) == 0x754c);
    assert(utf8_peek(s + 13) == '!');
    printf("OK\n");
}

static void test_peek_at(void) {
    printf("test_peek_at...");
    const char* s = "Hello, 世界!";
    size_t n = strlen(s);
    assert(utf8_peek_at(s, n, 0) == 'H');
    assert(utf8_peek_at(s, n, 2) == 'l');
    assert(utf8_peek_at(s, n, 4) == 'o');
    assert(utf8_peek_at(s, n, 6) == ' ');
    assert(utf8_peek_at(s, n, 7) == 0x4E16);
    assert(utf8_peek_at(s, n, 8) == 0x754C);
    assert(utf8_peek_at(s, n, 9) == '!');
    printf("OK\n");
}

static void test_icmp(void) {
    printf("test_icmp...");
    {
        const char* s1 = "Hello, 世界!";
        const char* s2 = "hello, 世界!";
        assert(utf8_icmp(s1, strlen(s1), s2, strlen(s2)) == 0);
    }
    {
        const char* s1 = "Hello, 世界!";
        const char* s2 = "HELLO, 世界!";
        assert(utf8_icmp(s1, strlen(s1), s2, strlen(s2)) == 0);
    }
    {
        const char* s1 = "Hello, 世界!";
        const char* s2 = "HELLO, 世界";
        assert(utf8_icmp(s1, strlen(s1), s2, strlen(s2)) > 0);
    }
    {
        const char* s1 = "Hello, 世界";
        const char* s2 = "HELLO, 世界!";
        assert(utf8_icmp(s1, strlen(s1), s2, strlen(s2)) < 0);
    }
    printf("OK\n");
}

static void test_valid(void) {
    printf("test_valid...");
    const char* s = "Hello, 世界!";
    assert(utf8_valid(s, strlen(s)));
    printf("OK\n");
}

// check_case converts src using fn and asserts the result equals want.
static void check_case(bool (*fn)(const char*, size_t, char*, size_t, size_t*),
                       const char* src,
                       const char* want) {
    char dst[256];
    size_t n = strlen(src);
    assert(n * 2 + 1 <= sizeof(dst));
    size_t len = 0;
    assert(fn(src, n, dst, sizeof(dst), &len));
    assert(len == strlen(want));
    assert(strcmp(dst, want) == 0);
}

static void test_tolower(void) {
    printf("test_tolower...");
    check_case(utf8_tolower, "Hello, WORLD!", "hello, world!");
    check_case(utf8_tolower, "Hello, 世界!", "hello, 世界!");
    check_case(utf8_tolower, "CÓMO ESTÁS", "cómo estás");
    check_case(utf8_tolower, "Привет, МИР!", "привет, мир!");
    // conversions that change the utf8 width of a character
    check_case(utf8_tolower, "İ", "i");
    check_case(utf8_tolower, "İstanbul", "istanbul");
    check_case(utf8_tolower, "K", "k");
    check_case(utf8_tolower, "Ⱥbcd", "ⱥbcd");
    printf("OK\n");
}

static void test_toupper(void) {
    printf("test_toupper...");
    check_case(utf8_toupper, "Hello, world!", "HELLO, WORLD!");
    check_case(utf8_toupper, "Hello, 世界!", "HELLO, 世界!");
    check_case(utf8_toupper, "cómo estás", "CÓMO ESTÁS");
    check_case(utf8_toupper, "Привет, мир!", "ПРИВЕТ, МИР!");
    // conversions that change the utf8 width of a character
    check_case(utf8_toupper, "ß", "ẞ");
    check_case(utf8_toupper, "ſ", "S");
    check_case(utf8_toupper, "ⱥbcd", "ȺBCD");
    printf("OK\n");
}

static void test_totitle(void) {
    printf("test_totitle...");
    check_case(utf8_totitle, "hello, world!", "Hello, World!");
    check_case(utf8_totitle, "hello, 世界!", "Hello, 世界!");
    check_case(utf8_totitle, "cómo estás", "Cómo Estás");
    check_case(utf8_totitle, "привет, мир!", "Привет, Мир!");
    // conversions that change the utf8 width of a character
    check_case(utf8_totitle, "aK", "Ak");
    check_case(utf8_totitle, "straße", "Straße");
    check_case(utf8_totitle, "ßx", "ẞx");
    printf("OK\n");
}

static void test_case_embedded_zero(void) {
    printf("test_case_embedded_zero...");
    // sqlite text may contain zero bytes; they are part of the value
    char dst[64];
    size_t len = 0;
    assert(utf8_tolower("A\0BC", 4, dst, sizeof(dst), &len));
    assert(len == 4);
    assert(memcmp(dst, "a\0bc", 4) == 0);
    assert(utf8_toupper("a\0bc", 4, dst, sizeof(dst), &len));
    assert(len == 4);
    assert(memcmp(dst, "A\0BC", 4) == 0);
    printf("OK\n");
}

static void test_casefold(void) {
    printf("test_casefold...");
    check_case(utf8_casefold, "Hello, WORLD!", "hello, world!");
    check_case(utf8_casefold, "Hello, 世界!", "hello, 世界!");
    check_case(utf8_casefold, "CÓMO ESTÁS", "cómo estás");
    check_case(utf8_casefold, "Привет, МИР!", "привет, мир!");
    // conversions that change the utf8 width of a character
    check_case(utf8_casefold, "ẞ", "ß");
    check_case(utf8_casefold, "K", "k");
    check_case(utf8_casefold, "Ⱥbcd", "ⱥbcd");
    printf("OK\n");
}

static void test_case_invalid(void) {
    printf("test_case_invalid...");
    char dst[64];
    size_t len = 0;
    // a stray continuation byte and a truncated sequence are both rejected
    // rather than sending the decoder past the end of the input
    assert(utf8_tolower("\x80", 1, dst, sizeof(dst), &len) == false);
    assert(utf8_toupper("\xc3", 1, dst, sizeof(dst), &len) == false);
    assert(utf8_totitle("a\xe4\xb8", 3, dst, sizeof(dst), &len) == false);
    assert(utf8_casefold("\xff", 1, dst, sizeof(dst), &len) == false);
    // a result that does not fit is rejected too
    assert(utf8_tolower("abc", 3, dst, 2, &len) == false);
    printf("OK\n");
}

int main(void) {
    test_len();
    test_peek();
    test_peek_at();
    test_icmp();
    test_valid();
    test_tolower();
    test_toupper();
    test_totitle();
    test_casefold();
    test_case_invalid();
    test_case_embedded_zero();
}
