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
    // bounded by n alone: an exact-sized unterminated allocation
    // must never be read past n, even while looking for codepoint 0
    {
        const char* lit = "A\xe4\xb8\x96";
        size_t len = strlen(lit);
        char* t = malloc(len);
        assert(t != NULL);
        memcpy(t, lit, len);
        assert(utf8_peek_at(t, 1, 0) == 'A');
        assert(utf8_peek_at(t, len, 1) == 0x4E16);
        // a shorter n truncates the sequence instead of reading past it
        assert(utf8_peek_at(t, 3, 1) == 0xFFFD);
        // past the end there is no codepoint
        assert(utf8_peek_at(t, len, 2) == 0);
        free(t);
    }
    // invalid bytes index as their replacement characters
    assert(utf8_peek_at("\x80\x80" "ab", 4, 1) == 0xFFFD);
    assert(utf8_peek_at("\x80\x80" "ab", 4, 2) == 'a');
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
    // a casefold may change the encoded width: ſ (two bytes) folds to
    // s (one byte), K (U+212A, three bytes) folds to k. the comparison
    // is over decoded folded codepoints, not bytes
    {
        const char* s1 = "ſ";
        const char* s2 = "s";
        assert(utf8_icmp(s1, strlen(s1), s2, strlen(s2)) == 0);
    }
    {
        const char* s1 = "\xe2\x84\xaa";  // K, the kelvin sign
        const char* s2 = "k";
        assert(utf8_icmp(s1, strlen(s1), s2, strlen(s2)) == 0);
    }
    // the order must be total: ſx < sxx < sxy, transitively.
    // comparing byte lengths made ſx "equal" to both sxx and sxy
    {
        const char* a = "ſx";
        const char* b = "sxx";
        const char* c = "sxy";
        assert(utf8_icmp(a, strlen(a), b, strlen(b)) < 0);
        assert(utf8_icmp(b, strlen(b), c, strlen(c)) < 0);
        assert(utf8_icmp(a, strlen(a), c, strlen(c)) < 0);
    }
    // a string that folds to a prefix of the other is the smaller one
    {
        const char* s1 = "s";
        const char* s2 = "ſx";
        assert(utf8_icmp(s1, strlen(s1), s2, strlen(s2)) < 0);
        assert(utf8_icmp(s2, strlen(s2), s1, strlen(s1)) > 0);
    }
    printf("OK\n");
}

// utf8_next must consume every byte exactly once and decode each maximal
// ill-formed subpart as U+FFFD, never reading past the given length.
static void test_next(void) {
    printf("test_next...");
    static const struct {
        const char* input;
        const uint32_t runes[4];
        size_t count;
    } tests[] = {
        {"a\xd0\xb1" "c", {'a', 0x0431, 'c'}, 3},  // valid
        {"\xc3", {0xFFFD}, 1},                     // truncated at the end
        {"a\xe4\xb8" "b", {'a', 0xFFFD, 'b'}, 3},  // truncated in the middle
        {"\x80\x80", {0xFFFD, 0xFFFD}, 2},         // stray continuation bytes
        {"\xc3" "a", {0xFFFD, 'a'}, 2},            // the breaking byte survives
    };
    for (size_t t = 0; t < sizeof(tests) / sizeof(*tests); t++) {
        size_t n = strlen(tests[t].input);
        // exact-sized allocation, unterminated: utf8_next takes an explicit
        // length, so a sanitizer build catches it touching s[n]
        char* s = malloc(n);
        assert(s != NULL);
        memcpy(s, tests[t].input, n);
        size_t i = 0, count = 0;
        while (i < n) {
            size_t prev = i;
            uint32_t c = utf8_next(s, n, &i);
            assert(i > prev && i <= n);
            assert(c == tests[t].runes[count]);
            count++;
        }
        assert(count == tests[t].count);
        free(s);
    }
    printf("OK\n");
}

// utf8_icmp must stay within the given lengths even when the strings are not
// valid utf8. Exact-sized unterminated allocations let a sanitizer build
// catch any overrun.
static void test_icmp_invalid(void) {
    printf("test_icmp_invalid...");
    static const char* inputs[] = {"\x80", "\xc3", "a\xe4\xb8", "\xff\xff", "A\xffZ"};
    for (size_t t = 0; t < sizeof(inputs) / sizeof(*inputs); t++) {
        size_t n = strlen(inputs[t]);
        char* s = malloc(n);
        assert(s != NULL);
        memcpy(s, inputs[t], n);
        assert(utf8_icmp(s, n, s, n) == 0);
        assert(utf8_icmp(s, n, "b", 1) != 0);
        free(s);
    }
    // invalid sequences compare equal case-insensitively around them
    assert(utf8_icmp("A\xff" "B", 3, "a\xff" "b", 3) == 0);
    // both decode to a single U+FFFD, so they compare equal even though
    // their byte lengths differ
    assert(utf8_icmp("\xe4\xb8", 2, "\xff", 1) == 0);
    printf("OK\n");
}

static void test_valid(void) {
    printf("test_valid...");
    const char* s = "Hello, 世界!";
    assert(utf8_valid(s, strlen(s)));
    printf("OK\n");
}

static void test_tolower(void) {
    printf("test_tolower...");
    {
        char s[] = "Hello, WORLD!";
        utf8_tolower(s, strlen(s));
        assert(strcmp(s, "hello, world!") == 0);
    }
    {
        char s[] = "Hello, 世界!";
        utf8_tolower(s, strlen(s));
        assert(strcmp(s, "hello, 世界!") == 0);
    }
    {
        char s[] = "CÓMO ESTÁS";
        utf8_tolower(s, strlen(s));
        assert(strcmp(s, "cómo estás") == 0);
    }
    {
        char s[] = "Привет, МИР!";
        utf8_tolower(s, strlen(s));
        assert(strcmp(s, "привет, мир!") == 0);
    }
    printf("OK\n");
}

static void test_toupper(void) {
    printf("test_toupper...");
    {
        char s[] = "Hello, world!";
        utf8_toupper(s, strlen(s));
        assert(strcmp(s, "HELLO, WORLD!") == 0);
    }
    {
        char s[] = "Hello, 世界!";
        utf8_toupper(s, strlen(s));
        assert(strcmp(s, "HELLO, 世界!") == 0);
    }
    {
        char s[] = "cómo estás";
        utf8_toupper(s, strlen(s));
        assert(strcmp(s, "CÓMO ESTÁS") == 0);
    }
    {
        char s[] = "Привет, мир!";
        utf8_toupper(s, strlen(s));
        assert(strcmp(s, "ПРИВЕТ, МИР!") == 0);
    }
    printf("OK\n");
}

static void test_totitle(void) {
    printf("test_totitle...");
    {
        char s[] = "hello, world!";
        utf8_totitle(s, strlen(s));
        assert(strcmp(s, "Hello, World!") == 0);
    }
    {
        char s[] = "hello, 世界!";
        utf8_totitle(s, strlen(s));
        assert(strcmp(s, "Hello, 世界!") == 0);
    }
    {
        char s[] = "cómo estás";
        utf8_totitle(s, strlen(s));
        assert(strcmp(s, "Cómo Estás") == 0);
    }
    {
        char s[] = "привет, мир!";
        utf8_totitle(s, strlen(s));
        assert(strcmp(s, "Привет, Мир!") == 0);
    }
    printf("OK\n");
}

static void test_casefold(void) {
    printf("test_casefold...");
    {
        char s[] = "Hello, WORLD!";
        utf8_casefold(s, strlen(s));
        assert(strcmp(s, "hello, world!") == 0);
    }
    {
        char s[] = "Hello, 世界!";
        utf8_casefold(s, strlen(s));
        assert(strcmp(s, "hello, 世界!") == 0);
    }
    {
        char s[] = "CÓMO ESTÁS";
        utf8_casefold(s, strlen(s));
        assert(strcmp(s, "cómo estás") == 0);
    }
    {
        char s[] = "Привет, МИР!";
        utf8_casefold(s, strlen(s));
        assert(strcmp(s, "привет, мир!") == 0);
    }
    printf("OK\n");
}

int main(void) {
    test_len();
    test_peek();
    test_peek_at();
    test_icmp();
    test_icmp_invalid();
    test_next();
    test_valid();
    test_tolower();
    test_toupper();
    test_totitle();
    test_casefold();
}
