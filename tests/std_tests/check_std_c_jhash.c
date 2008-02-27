/*
 * Tests are taken form lookup2.c and lookup8.c
 * by Bob Jenkins, December 1996, Public Domain.
 *
 * See http://burtleburtle.net/bob/hash/evahash.html
 */
#include "support.h"

#include "std/c_jhash.h"

#define HASHSTATE 1
#define HASHLEN   1
#define MAXPAIR 80
#define MAXLEN 70

START_TEST (check_c_jhash_trials)
{
  uint8_t qa[MAXLEN+1], qb[MAXLEN+2], *a = &qa[0], *b = &qb[1];
  uint32_t c[HASHSTATE], d[HASHSTATE], i, j=0, k, l, m, z;
  uint32_t e[HASHSTATE],f[HASHSTATE],g[HASHSTATE],h[HASHSTATE];
  uint32_t x[HASHSTATE],y[HASHSTATE];
  uint32_t hlen;

  for (hlen=0; hlen < MAXLEN; ++hlen) {
    z=0;
    for (i=0; i<hlen; ++i) {  /*----------------------- for each input byte, */
      for (j=0; j<8; ++j) { /*------------------------ for each input bit, */
        for (m=1; m<8; ++m) { /*------------ for serveral possible initvals, */
          for (l=0; l<HASHSTATE; ++l) e[l]=f[l]=g[l]=h[l]=x[l]=y[l]=~((uint32_t)0);

          /*---- check that every output bit is affected by that input bit */
          for (k=0; k<MAXPAIR; k+=2) {
            uint32_t finished=1;
            /* keys have one bit different */
            for (l=0; l<hlen+1; ++l) {a[l] = b[l] = (uint8_t)0;}
            /* have a and b be two keys differing in only one bit */
            a[i] ^= (k<<j);
            a[i] ^= (k>>(8-j));
            c[0] = c_jhash(a, hlen, m);
            b[i] ^= ((k+1)<<j);
            b[i] ^= ((k+1)>>(8-j));
            d[0] = c_jhash(b, hlen, m);
            /* check every bit is 1, 0, set, and not set at least once */
            for (l=0; l<HASHSTATE; ++l) {
              e[l] &= (c[l]^d[l]);
              f[l] &= ~(c[l]^d[l]);
              g[l] &= c[l];
              h[l] &= ~c[l];
              x[l] &= d[l];
              y[l] &= ~d[l];
              if (e[l]|f[l]|g[l]|h[l]|x[l]|y[l]) finished=0;
            }
            if (finished) break;
          }
          if (k>z) z=k;
          if (k==MAXPAIR) {
             printf("Some bit didn't change: ");
             printf("%.8x %.8x %.8x %.8x %.8x %.8x  ",
                    e[0], f[0], g[0], h[0], x[0], y[0]);
             printf("i %d j %d m %d len %d\n",i,j,m,hlen);
          }
          if (z==MAXPAIR) goto done;
        }
      }
    }
   done:
    if (z < MAXPAIR) {
      fail_unless(z < MAXPAIR, "%ld trials needed, should be less than 40", z/2);
    }
  }
}
END_TEST

START_TEST (check_c_jhash_alignment_problems)
{
  uint32_t test;
  uint8_t buf[MAXLEN+20], *b;
  uint32_t len;
  uint8_t q[] = "This is the time for all good men to come to the aid of their country";
  uint8_t qq[] = "xThis is the time for all good men to come to the aid of their country";
  uint8_t qqq[] = "xxThis is the time for all good men to come to the aid of their country";
  uint8_t qqqq[] = "xxxThis is the time for all good men to come to the aid of their country";
  uint32_t h,i,j,ref,x,y;

  test = c_jhash(q, sizeof(q)-1, (uint32_t)0);
  fail_unless(test == c_jhash(qq+1, sizeof(q)-1, (uint32_t)0), NULL);
  fail_unless(test == c_jhash(qq+1, sizeof(q)-1, (uint32_t)0), NULL);
  fail_unless(test == c_jhash(qqq+2, sizeof(q)-1, (uint32_t)0), NULL);
  fail_unless(test == c_jhash(qqqq+3, sizeof(q)-1, (uint32_t)0), NULL);
  for (h=0, b=buf+1; h<8; ++h, ++b) {
    for (i=0; i<MAXLEN; ++i) {
      len = i;
      for (j=0; j<i; ++j) *(b+j)=0;

      /* these should all be equal */
      ref = c_jhash(b, len, (uint32_t)1);
      *(b+i)=(uint8_t)~0;
      *(b-1)=(uint8_t)~0;
      x = c_jhash(b, len, (uint32_t)1);
      y = c_jhash(b, len, (uint32_t)1);
      fail_if((ref != x) || (ref != y), "alignment error: %.8lx %.8lx %.8lx %ld %ld\n", ref, x, y, h, i);
    }
  }
}
END_TEST

START_TEST (check_c_jhash_null_strings)
{
  uint8_t buf[1];
  uint32_t h, i, t, state[HASHSTATE];

  buf[0] = ~0;
  for (i=0; i<HASHSTATE; ++i) state[i] = 1;
  for (i=0, h=0; i<8; ++i) {
    t = h;
    h = c_jhash(buf, (uint32_t)0, h);
    fail_if(t == h, "0-byte-string check failed: t = %.8lx, h = %.8lx", t, h);
  }
}
END_TEST

START_TEST (check_c_jhash64_trials)
{
  uint8_t qa[MAXLEN + 1], qb[MAXLEN + 2];
  uint8_t *a, *b;
  uint64_t c[HASHSTATE], d[HASHSTATE], i, j=0, k, l, m, z;
  uint64_t e[HASHSTATE],f[HASHSTATE],g[HASHSTATE],h[HASHSTATE];
  uint64_t x[HASHSTATE],y[HASHSTATE];
  uint64_t hlen;

  a = &qa[0];
  b = &qb[1];

  for (hlen=0; hlen < MAXLEN; ++hlen) {
    z=0;
    for (i=0; i<hlen; ++i) { /*----------------------- for each byte, */
      for (j=0; j<8; ++j) { /*------------------------ for each bit, */
        for (m=0; m<8; ++m) { /*-------- for serveral possible levels, */
          for (l=0; l<HASHSTATE; ++l) e[l]=f[l]=g[l]=h[l]=x[l]=y[l]=~((uint64_t)0);

          /*---- check that every input bit affects every output bit */
          for (k=0; k<MAXPAIR; k+=2) {
            uint64_t finished=1;
            /* keys have one bit different */
            for (l=0; l<hlen+1; ++l) {a[l] = b[l] = (uint8_t)0;}
            /* have a and b be two keys differing in only one bit */
            a[i] ^= (k<<j);
            a[i] ^= (k>>(8-j));
             c[0] = c_jhash64(a, hlen, m);
            b[i] ^= ((k+1)<<j);
            b[i] ^= ((k+1)>>(8-j));
             d[0] = c_jhash64(b, hlen, m);
            /* check every bit is 1, 0, set, and not set at least once */
            for (l=0; l<HASHSTATE; ++l)
            {
              e[l] &= (c[l]^d[l]);
              f[l] &= ~(c[l]^d[l]);
              g[l] &= c[l];
              h[l] &= ~c[l];
              x[l] &= d[l];
              y[l] &= ~d[l];
              if (e[l]|f[l]|g[l]|h[l]|x[l]|y[l]) finished=0;
            }
            if (finished) break;
          }
          if (k>z) z=k;
          if (k==MAXPAIR) {
             printf("Some bit didn't change: ");
             printf("%.8lx %.8lx %.8lx %.8lx %.8lx %.8lx  ",
                    e[0],f[0],g[0],h[0],x[0],y[0]);
             printf("i %d j %d m %d len %d\n",
                    (uint32_t)i,(uint32_t)j,(uint32_t)m,(uint32_t)hlen);
          }
          if (z==MAXPAIR) goto done;
        }
      }
    }
   done:
    if (z < MAXPAIR) {
      fail_unless(z < MAXPAIR, "%ld trials needed, should be less than 40", z/2);
    }
  }
}
END_TEST

START_TEST (check_c_jhash64_alignment_problems)
{
  uint8_t buf[MAXLEN+20], *b;
  uint64_t len;
  uint8_t q[] = "This is the time for all good men to come to the aid of their country";
  uint8_t qq[] = "xThis is the time for all good men to come to the aid of their country";
  uint8_t qqq[] = "xxThis is the time for all good men to come to the aid of their country";
  uint8_t qqqq[] = "xxxThis is the time for all good men to come to the aid of their country";
  uint8_t o[] = "xxxxThis is the time for all good men to come to the aid of their country";
  uint8_t oo[] = "xxxxxThis is the time for all good men to come to the aid of their country";
  uint8_t ooo[] = "xxxxxxThis is the time for all good men to come to the aid of their country";
  uint8_t oooo[] = "xxxxxxxThis is the time for all good men to come to the aid of their country";
  uint64_t h,i,j,ref,t,x,y;

  h = c_jhash64(q+0, (uint64_t)(sizeof(q)-1), (uint64_t)0);
  t = h;
  fail_unless(t == h, "%.8lx%.8lx\n", (uint32_t)h, (uint32_t)(h>>32));
  h = c_jhash64(qq+1, (uint64_t)(sizeof(q)-1), (uint64_t)0);
  fail_unless(t == h, "%.8lx%.8lx\n", (uint32_t)h, (uint32_t)(h>>32));
  h = c_jhash64(qqq+2, (uint64_t)(sizeof(q)-1), (uint64_t)0);
  fail_unless(t == h, "%.8lx%.8lx\n", (uint32_t)h, (uint32_t)(h>>32));
  h = c_jhash64(qqqq+3, (uint64_t)(sizeof(q)-1), (uint64_t)0);
  fail_unless(t == h, "%.8lx%.8lx\n", (uint32_t)h, (uint32_t)(h>>32));
  h = c_jhash64(o+4, (uint64_t)(sizeof(q)-1), (uint64_t)0);
  fail_unless(t == h, "%.8lx%.8lx\n", (uint32_t)h, (uint32_t)(h>>32));
  h = c_jhash64(oo+5, (uint64_t)(sizeof(q)-1), (uint64_t)0);
  fail_unless(t == h, "%.8lx%.8lx\n", (uint32_t)h, (uint32_t)(h>>32));
  h = c_jhash64(ooo+6, (uint64_t)(sizeof(q)-1), (uint64_t)0);
  fail_unless(t == h, "%.8lx%.8lx\n", (uint32_t)h, (uint32_t)(h>>32));
  h = c_jhash64(oooo+7, (uint64_t)(sizeof(q)-1), (uint64_t)0);
  fail_unless(t == h, "%.8lx%.8lx\n", (uint32_t)h, (uint32_t)(h>>32));
  for (h=0, b=buf+1; h<8; ++h, ++b) {
    for (i=0; i<MAXLEN; ++i) {
      len = i;
      for (j=0; j<i; ++j) *(b+j)=0;

      /* these should all be equal */
      ref = c_jhash64(b, len, (uint64_t)1);
      *(b+i)=(uint8_t)~0;
      *(b-1)=(uint8_t)~0;
      x = c_jhash64(b, len, (uint64_t)1);
      y = c_jhash64(b, len, (uint64_t)1);
      fail_if((ref != x) || (ref != y), "alignment error: %.8lx %.8lx %.8lx %ld %ld\n", ref, x, y, h, i);
    }
  }
}
END_TEST

START_TEST (check_c_jhash64_null_strings)
{
  uint8_t buf[1];
  uint64_t h, i, t, state[HASHSTATE];


  buf[0] = ~0;
  for (i=0; i<HASHSTATE; ++i) state[i] = 1;
  for (i=0, h=0; i<8; ++i) {
    t = h;
    h = c_jhash64(buf, (uint64_t)0, h);
    fail_if(t == h, "0-byte-string check failed: t = %.8lx, h = %.8lx", t, h);
  }
}
END_TEST


static Suite *make_c_jhash_suite(void) {
  Suite *s = suite_create("std:path:xsrbtree");

  create_case(s, "check_c_jhash_trials", check_c_jhash_trials);
  create_case(s, "check_c_jhash_alignment_problems", check_c_jhash_alignment_problems);
  create_case(s, "check_c_jhash_null_strings", check_c_jhash_null_strings);

  create_case(s, "check_c_jhash64_trials", check_c_jhash64_trials);
  create_case(s, "check_c_jhash64_alignment_problems", check_c_jhash64_alignment_problems);
  create_case(s, "check_c_jhash64_null_strings", check_c_jhash64_null_strings);

  return s;
}

int main(void) {
  int nf;

  Suite *s = make_c_jhash_suite();
  /* Suite *s2 = make_xstrdup_suite(); */

  SRunner *sr;
  sr = srunner_create(s);
  /* srunner_add_suite (sr, s2); */
  srunner_run_all(sr, CK_VERBOSE);
  nf = srunner_ntests_failed(sr);
  srunner_free(sr);

  return (nf == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

