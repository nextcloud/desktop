/*
 * c_jhash.c Jenkins Hash
 *
 * Copyright (c) 1997 Bob Jenkins <bob_jenkins@burtleburtle.net>
 *
 * lookup8.c, by Bob Jenkins, January 4 1997, Public Domain.
 * hash(), hash2(), hash3, and _c_mix() are externally useful functions.
 * Routines to test the hash are included if SELF_TEST is defined.
 * You can use this free for any purpose.  It has no warranty.
 *
 * See http://burtleburtle.net/bob/hash/evahash.html
 */

/**
 * @file c_jhash.h
 *
 * @brief Interface of the cynapses jhash implementation
 *
 * @defgroup cynJHashInternals cynapses libc jhash function
 * @ingroup cynLibraryAPI
 *
 * @{
 */
#ifndef _C_JHASH_H
#define _C_JHASH_H

#include <stdint.h>

#define c_hashsize(n) ((uint8_t) 1 << (n))
#define c_hashmask(n) (xhashsize(n) - 1)

/**
 * _c_mix -- Mix 3 32-bit values reversibly.
 *
 * For every delta with one or two bit set, and the deltas of all three
 * high bits or all three low bits, whether the original value of a,b,c
 * is almost all zero or is uniformly distributed,
 * If _c_mix() is run forward or backward, at least 32 bits in a,b,c
 * have at least 1/4 probability of changing.
 * If _c_mix() is run forward, every bit of c will change between 1/3 and
 * 2/3 of the time.  (Well, 22/100 and 78/100 for some 2-bit deltas.)
 * _c_mix() was built out of 36 single-cycle latency instructions in a 
 * structure that could supported 2x parallelism, like so:
 *     a -= b;
 *     a -= c; x = (c>>13);
 *     b -= c; a ^= x;
 *     b -= a; x = (a<<8);
 *     c -= a; b ^= x;
 *     c -= b; x = (b>>13);
 *     ...
 *
 * Unfortunately, superscalar Pentiums and Sparcs can't take advantage
 * of that parallelism.  They've also turned some of those single-cycle
 * latency instructions into multi-cycle latency instructions.  Still,
 * this is the fastest good hash I could find.  There were about 2^^68
 * to choose from.  I only looked at a billion or so.
 */
#define _c_mix(a,b,c) \
{ \
  a -= b; a -= c; a ^= (c>>13); \
  b -= c; b -= a; b ^= (a<<8); \
  c -= a; c -= b; c ^= (b>>13); \
  a -= b; a -= c; a ^= (c>>12);  \
  b -= c; b -= a; b ^= (a<<16); \
  c -= a; c -= b; c ^= (b>>5); \
  a -= b; a -= c; a ^= (c>>3);  \
  b -= c; b -= a; b ^= (a<<10); \
  c -= a; c -= b; c ^= (b>>15); \
}

/**
 * _c_mix64 -- Mix 3 64-bit values reversibly.
 *
 * _c_mix64() takes 48 machine instructions, but only 24 cycles on a superscalar
 * machine (like Intel's new MMX architecture).  It requires 4 64-bit
 * registers for 4::2 parallelism.
 * All 1-bit deltas, all 2-bit deltas, all deltas composed of top bits of
 * (a,b,c), and all deltas of bottom bits were tested.  All deltas were
 * tested both on random keys and on keys that were nearly all zero.
 * These deltas all cause every bit of c to change between 1/3 and 2/3
 * of the time (well, only 113/400 to 287/400 of the time for some
 * 2-bit delta).  These deltas all cause at least 80 bits to change
 * among (a,b,c) when the _c_mix is run either forward or backward (yes it
 * is reversible).
 * This implies that a hash using _c_mix64 has no funnels.  There may be
 * characteristics with 3-bit deltas or bigger, I didn't test for
 * those.
 */
#define _c_mix64(a,b,c) \
{ \
  a -= b; a -= c; a ^= (c>>43); \
  b -= c; b -= a; b ^= (a<<9); \
  c -= a; c -= b; c ^= (b>>8); \
  a -= b; a -= c; a ^= (c>>38); \
  b -= c; b -= a; b ^= (a<<23); \
  c -= a; c -= b; c ^= (b>>5); \
  a -= b; a -= c; a ^= (c>>35); \
  b -= c; b -= a; b ^= (a<<49); \
  c -= a; c -= b; c ^= (b>>11); \
  a -= b; a -= c; a ^= (c>>12); \
  b -= c; b -= a; b ^= (a<<18); \
  c -= a; c -= b; c ^= (b>>22); \
}

/**
 * @brief hash a variable-length key into a 32-bit value
 *
 * The best hash table sizes are powers of 2.  There is no need to do
 * mod a prime (mod is sooo slow!).  If you need less than 32 bits,
 * use a bitmask.  For example, if you need only 10 bits, do
 *   h = (h & hashmask(10));
 * In which case, the hash table should have hashsize(10) elements.
 *
 * Use for hash table lookup, or anything where one collision in 2^32 is
 * acceptable.  Do NOT use for cryptographic purposes.
 *
 * @param k        The key (the unaligned variable-length array of bytes).
 *
 * @param length   The length of the key, counting by bytes.
 *
 * @param initval  Initial value, can be any 4-byte value.
 *
 * @return    Returns a 32-bit value.  Every bit of the key affects every bit
 *            of the return value.  Every 1-bit and 2-bit delta achieves
 *            avalanche. About 36+6len instructions.
 */
static inline uint32_t c_jhash(const uint8_t *k, uint32_t length, uint32_t initval) {
   uint32_t a,b,c,len;

   /* Set up the internal state */
   len = length;
   a = b = 0x9e3779b9; /* the golden ratio; an arbitrary value */
   c = initval; /* the previous hash value */

   while (len >= 12) {
      a += (k[0] +((uint32_t)k[1]<<8) +((uint32_t)k[2]<<16) +((uint32_t)k[3]<<24));
      b += (k[4] +((uint32_t)k[5]<<8) +((uint32_t)k[6]<<16) +((uint32_t)k[7]<<24));
      c += (k[8] +((uint32_t)k[9]<<8) +((uint32_t)k[10]<<16)+((uint32_t)k[11]<<24));
      _c_mix(a,b,c);
      k += 12; len -= 12;
   }

   /* handle the last 11 bytes */
   c += length;
   /* all the case statements fall through */
   switch(len) {
     case 11: c+=((uint32_t)k[10]<<24);
     case 10: c+=((uint32_t)k[9]<<16);
     case 9 : c+=((uint32_t)k[8]<<8);
     /* the first byte of c is reserved for the length */
     case 8 : b+=((uint32_t)k[7]<<24);
     case 7 : b+=((uint32_t)k[6]<<16);
     case 6 : b+=((uint32_t)k[5]<<8);
     case 5 : b+=k[4];
     case 4 : a+=((uint32_t)k[3]<<24);
     case 3 : a+=((uint32_t)k[2]<<16);
     case 2 : a+=((uint32_t)k[1]<<8);
     case 1 : a+=k[0];
     /* case 0: nothing left to add */
   }
   _c_mix(a,b,c);

   return c;
}

/**
 * @brief hash a variable-length key into a 64-bit value
 *
 * The best hash table sizes are powers of 2.  There is no need to do
 * mod a prime (mod is sooo slow!).  If you need less than 64 bits,
 * use a bitmask.  For example, if you need only 10 bits, do
 *   h = (h & hashmask(10));
 * In which case, the hash table should have hashsize(10) elements.
 *
 * Use for hash table lookup, or anything where one collision in 2^^64
 * is acceptable.  Do NOT use for cryptographic purposes.
 *
 * @param k       The key (the unaligned variable-length array of bytes).
 * @param length  The length of the key, counting by bytes.
 * @param intval  Initial value, can be any 8-byte value.
 *
 * @return    A 64-bit value. Every bit of the key affects every bit of
 *            the return value.  No funnels.  Every 1-bit and 2-bit delta
 *            achieves avalanche. About 41+5len instructions.
 */
static inline uint64_t c_jhash64(const uint8_t *k, uint64_t length, uint64_t intval) {
  uint64_t a,b,c,len;

  /* Set up the internal state */
  len = length;
  a = b = intval; /* the previous hash value */
  c = 0x9e3779b97f4a7c13LL; /* the golden ratio; an arbitrary value */

  /* handle most of the key */
  while (len >= 24)
  {
    a += (k[0]        +((uint64_t)k[ 1]<< 8)+((uint64_t)k[ 2]<<16)+((uint64_t)k[ 3]<<24)
     +((uint64_t)k[4 ]<<32)+((uint64_t)k[ 5]<<40)+((uint64_t)k[ 6]<<48)+((uint64_t)k[ 7]<<56));
    b += (k[8]        +((uint64_t)k[ 9]<< 8)+((uint64_t)k[10]<<16)+((uint64_t)k[11]<<24)
     +((uint64_t)k[12]<<32)+((uint64_t)k[13]<<40)+((uint64_t)k[14]<<48)+((uint64_t)k[15]<<56));
    c += (k[16]       +((uint64_t)k[17]<< 8)+((uint64_t)k[18]<<16)+((uint64_t)k[19]<<24)
     +((uint64_t)k[20]<<32)+((uint64_t)k[21]<<40)+((uint64_t)k[22]<<48)+((uint64_t)k[23]<<56));
    _c_mix64(a,b,c);
    k += 24; len -= 24;
  }

  /* handle the last 23 bytes */
  c += length;
  switch(len) {
    case 23: c+=((uint64_t)k[22]<<56);
    case 22: c+=((uint64_t)k[21]<<48);
    case 21: c+=((uint64_t)k[20]<<40);
    case 20: c+=((uint64_t)k[19]<<32);
    case 19: c+=((uint64_t)k[18]<<24);
    case 18: c+=((uint64_t)k[17]<<16);
    case 17: c+=((uint64_t)k[16]<<8);
    /* the first byte of c is reserved for the length */
    case 16: b+=((uint64_t)k[15]<<56);
    case 15: b+=((uint64_t)k[14]<<48);
    case 14: b+=((uint64_t)k[13]<<40);
    case 13: b+=((uint64_t)k[12]<<32);
    case 12: b+=((uint64_t)k[11]<<24);
    case 11: b+=((uint64_t)k[10]<<16);
    case 10: b+=((uint64_t)k[ 9]<<8);
    case  9: b+=((uint64_t)k[ 8]);
    case  8: a+=((uint64_t)k[ 7]<<56);
    case  7: a+=((uint64_t)k[ 6]<<48);
    case  6: a+=((uint64_t)k[ 5]<<40);
    case  5: a+=((uint64_t)k[ 4]<<32);
    case  4: a+=((uint64_t)k[ 3]<<24);
    case  3: a+=((uint64_t)k[ 2]<<16);
    case  2: a+=((uint64_t)k[ 1]<<8);
    case  1: a+=((uint64_t)k[ 0]);
    /* case 0: nothing left to add */
  }
  _c_mix64(a,b,c);

  return c;
}

/**
 * }@
 */
#endif /* _C_JHASH_H */

