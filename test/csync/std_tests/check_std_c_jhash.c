/*
 * Tests are taken form lookup2.c and lookup8.c
 * by Bob Jenkins, December 1996, Public Domain.
 *
 * See http://burtleburtle.net/bob/hash/evahash.html
 */
#include "torture.h"

#include "common/c_jhash.h"

#define HASHSTATE 1
#define HASHLEN 1
#define MAXPAIR 80
#define MAXLEN 70

static void check_c_jhash_trials(void **state)
{
    uint8_t qa[MAXLEN + 1], qb[MAXLEN + 2], *a = &qa[0], *b = &qb[1];
    uint64_t c[HASHSTATE], d[HASHSTATE], i, j = 0, k, l, m, z;
    uint64_t e[HASHSTATE], f[HASHSTATE], g[HASHSTATE], h[HASHSTATE];
    uint64_t x[HASHSTATE], y[HASHSTATE];
    uint64_t hlen;

    (void)state; /* unused */

    for (hlen = 0; hlen < MAXLEN; ++hlen) {
        z = 0;
        for (i = 0; i < hlen; ++i) { /*----------------------- for each input byte, */
            for (j = 0; j < 8; ++j) { /*------------------------ for each input bit, */
                for (m = 1; m < 8; ++m) { /*------------ for serveral possible initvals, */
                    for (l = 0; l < HASHSTATE; ++l)
                        e[l] = f[l] = g[l] = h[l] = x[l] = y[l] = ~(0);

                    /*---- check that every output bit is affected by that input bit */
                    for (k = 0; k < MAXPAIR; k += 2) {
                        uint64_t finished = 1;
                        /* keys have one bit different */
                        for (l = 0; l < hlen + 1; ++l) {
                            a[l] = b[l] = 0;
                        }
                        /* have a and b be two keys differing in only one bit */
                        a[i] ^= (k << j);
                        a[i] ^= (k >> (8 - j));
                        c[0] = c_jhash64(a, hlen, m);
                        b[i] ^= ((k + 1) << j);
                        b[i] ^= ((k + 1) >> (8 - j));
                        d[0] = c_jhash64(b, hlen, m);
                        /* check every bit is 1, 0, set, and not set at least once */
                        for (l = 0; l < HASHSTATE; ++l) {
                            e[l] &= (c[l] ^ d[l]);
                            f[l] &= ~(c[l] ^ d[l]);
                            g[l] &= c[l];
                            h[l] &= ~c[l];
                            x[l] &= d[l];
                            y[l] &= ~d[l];
                            if (e[l] | f[l] | g[l] | h[l] | x[l] | y[l])
                                finished = 0;
                        }
                        if (finished)
                            break;
                    }
                    if (k > z)
                        z = k;
                    if (k == MAXPAIR) {
                        print_error("Some bit didn't change: ");
                        print_error("%.8x %.8x %.8x %.8x %.8x %.8x  ",
                            e[0], f[0], g[0], h[0], x[0], y[0]);
                        print_error("i %d j %d m %d len %d\n", i, j, m, hlen);
                    }
                    if (z == MAXPAIR) {
                        if (z < MAXPAIR) {
                            assert_true(z < MAXPAIR);
                            // print_error("%u trials needed, should be less than 40\n", z/2);
                            return;
                        }
                    }
                }
            }
        }
    }
}

static void check_c_jhash_alignment_problems(void **state)
{
    uint64_t test;
    uint8_t buf[MAXLEN + 20], *b;
    uint64_t len;
    uint8_t q[] = "This is the time for all good men to come to the aid of their country";
    uint8_t qq[] = "xThis is the time for all good men to come to the aid of their country";
    uint8_t qqq[] = "xxThis is the time for all good men to come to the aid of their country";
    uint8_t qqqq[] = "xxxThis is the time for all good men to come to the aid of their country";
    uint64_t h, i, j, ref, x, y;

    (void)state; /* unused */

    test = c_jhash64(q, sizeof(q) - 1, 0);
    assert_true(test == c_jhash64(qq + 1, sizeof(q) - 1, 0));
    assert_true(test == c_jhash64(qq + 1, sizeof(q) - 1, 0));
    assert_true(test == c_jhash64(qqq + 2, sizeof(q) - 1, 0));
    assert_true(test == c_jhash64(qqqq + 3, sizeof(q) - 1, 0));
    for (h = 0, b = buf + 1; h < 8; ++h, ++b) {
        for (i = 0; i < MAXLEN; ++i) {
            len = i;
            for (j = 0; j < i; ++j)
                *(b + j) = 0;

            /* these should all be equal */
            ref = c_jhash64(b, len, 1);
            *(b + i) = ~0;
            *(b - 1) = ~0;
            x = c_jhash64(b, len, 1);
            y = c_jhash64(b, len, 1);
            assert_false((ref != x) || (ref != y));
        }
    }
}

static void check_c_jhash_null_strings(void **state)
{
    uint8_t buf[1];
    uint64_t h, i, t;

    (void)state; /* unused */

    buf[0] = ~0;
    for (i = 0, h = 0; i < 8; ++i) {
        t = h;
        h = c_jhash64(buf, 0, h);
        assert_false(t == h);
        // print_error("0-byte-string check failed: t = %.8x, h = %.8x", t, h);
    }
}

static void check_c_jhash64_trials(void **state)
{
    uint8_t qa[MAXLEN + 1], qb[MAXLEN + 2];
    uint8_t *a, *b;
    uint64_t c[HASHSTATE], d[HASHSTATE], i, j = 0, k, l, m, z;
    uint64_t e[HASHSTATE], f[HASHSTATE], g[HASHSTATE], h[HASHSTATE];
    uint64_t x[HASHSTATE], y[HASHSTATE];
    uint64_t hlen;

    (void)state; /* unused */

    a = &qa[0];
    b = &qb[1];

    for (hlen = 0; hlen < MAXLEN; ++hlen) {
        z = 0;
        for (i = 0; i < hlen; ++i) { /*----------------------- for each byte, */
            for (j = 0; j < 8; ++j) { /*------------------------ for each bit, */
                for (m = 0; m < 8; ++m) { /*-------- for serveral possible levels, */
                    for (l = 0; l < HASHSTATE; ++l)
                        e[l] = f[l] = g[l] = h[l] = x[l] = y[l] = ~(0);

                    /*---- check that every input bit affects every output bit */
                    for (k = 0; k < MAXPAIR; k += 2) {
                        uint64_t finished = 1;
                        /* keys have one bit different */
                        for (l = 0; l < hlen + 1; ++l) {
                            a[l] = b[l] = 0;
                        }
                        /* have a and b be two keys differing in only one bit */
                        a[i] ^= (k << j);
                        a[i] ^= (k >> (8 - j));
                        c[0] = c_jhash64(a, hlen, m);
                        b[i] ^= ((k + 1) << j);
                        b[i] ^= ((k + 1) >> (8 - j));
                        d[0] = c_jhash64(b, hlen, m);
                        /* check every bit is 1, 0, set, and not set at least once */
                        for (l = 0; l < HASHSTATE; ++l) {
                            e[l] &= (c[l] ^ d[l]);
                            f[l] &= ~(c[l] ^ d[l]);
                            g[l] &= c[l];
                            h[l] &= ~c[l];
                            x[l] &= d[l];
                            y[l] &= ~d[l];
                            if (e[l] | f[l] | g[l] | h[l] | x[l] | y[l])
                                finished = 0;
                        }
                        if (finished)
                            break;
                    }
                    if (k > z)
                        z = k;
                    if (k == MAXPAIR) {
#if 0
             print_error("Some bit didn't change: ");
             print_error("%.8llx %.8llx %.8llx %.8llx %.8llx %.8llx  ",
                         (long long unsigned int) e[0],
                         (long long unsigned int) f[0],
                         (long long unsigned int) g[0],
                         (long long unsigned int) h[0],
                         (long long unsigned int) x[0],
                         (long long unsigned int) y[0]);
             print_error("i %d j %d m %d len %d\n",
                         i,j,m,hlen);
#endif
                    }
                    if (z == MAXPAIR) {
                        if (z < MAXPAIR) {
#if 0
                  print_error("%lu trials needed, should be less than 40", z/2);
#endif
                            assert_true(z < MAXPAIR);
                        }
                        return;
                    }
                }
            }
        }
    }
}

static void check_c_jhash64_alignment_problems(void **state)
{
    uint8_t buf[MAXLEN + 20], *b;
    uint64_t len;
    uint8_t q[] = "This is the time for all good men to come to the aid of their country";
    uint8_t qq[] = "xThis is the time for all good men to come to the aid of their country";
    uint8_t qqq[] = "xxThis is the time for all good men to come to the aid of their country";
    uint8_t qqqq[] = "xxxThis is the time for all good men to come to the aid of their country";
    uint8_t o[] = "xxxxThis is the time for all good men to come to the aid of their country";
    uint8_t oo[] = "xxxxxThis is the time for all good men to come to the aid of their country";
    uint8_t ooo[] = "xxxxxxThis is the time for all good men to come to the aid of their country";
    uint8_t oooo[] = "xxxxxxxThis is the time for all good men to come to the aid of their country";
    uint64_t h, i, j, ref, t, x, y;

    (void)state; /* unused */

    h = c_jhash64(q + 0, (sizeof(q) - 1), 0);
    t = h;
    assert_true(t == h);
    // , "%.8lx%.8lx\n", h, (h>>32));
    h = c_jhash64(qq + 1, (sizeof(q) - 1), 0);
    assert_true(t == h);
    // , "%.8lx%.8lx\n", h, (h>>32));
    h = c_jhash64(qqq + 2, (sizeof(q) - 1), 0);
    assert_true(t == h);
    // , "%.8lx%.8lx\n", h, (h>>32));
    h = c_jhash64(qqqq + 3, (sizeof(q) - 1), 0);
    assert_true(t == h);
    // , "%.8lx%.8lx\n", h, (h>>32));
    h = c_jhash64(o + 4, (sizeof(q) - 1), 0);
    assert_true(t == h);
    // , "%.8lx%.8lx\n", h, (h>>32));
    h = c_jhash64(oo + 5, (sizeof(q) - 1), 0);
    assert_true(t == h);
    // , "%.8lx%.8lx\n", h, (h>>32));
    h = c_jhash64(ooo + 6, (sizeof(q) - 1), 0);
    assert_true(t == h);
    // , "%.8lx%.8lx\n", h, (h>>32));
    h = c_jhash64(oooo + 7, (sizeof(q) - 1), 0);
    assert_true(t == h);
    // , "%.8lx%.8lx\n", h, (h>>32));
    for (h = 0, b = buf + 1; h < 8; ++h, ++b) {
        for (i = 0; i < MAXLEN; ++i) {
            len = i;
            for (j = 0; j < i; ++j)
                *(b + j) = 0;

            /* these should all be equal */
            ref = c_jhash64(b, len, 1);
            *(b + i) = ~0;
            *(b - 1) = ~0;
            x = c_jhash64(b, len, 1);
            y = c_jhash64(b, len, 1);
            assert_false((ref != x) || (ref != y));
#if 0
      print_error("alignment error: %.8lx %.8lx %.8lx %ld %ld\n", ref, x, y, h, i);
#endif
        }
    }
}

static void check_c_jhash64_null_strings(void **state)
{
    uint8_t buf[1];
    uint64_t h, i, t;

    (void)state; /* unused */

    buf[0] = ~0;
    for (i = 0, h = 0; i < 8; ++i) {
        t = h;
        h = c_jhash64(buf, 0, h);
        assert_false(t == h);
#if 0
    print_error("0-byte-string check failed: t = %.8lx, h = %.8lx", t, h);
#endif
    }
}

int torture_run_tests(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(check_c_jhash_trials),
        cmocka_unit_test(check_c_jhash_alignment_problems),
        cmocka_unit_test(check_c_jhash_null_strings),
        cmocka_unit_test(check_c_jhash64_trials),
        cmocka_unit_test(check_c_jhash64_alignment_problems),
        cmocka_unit_test(check_c_jhash64_null_strings),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
