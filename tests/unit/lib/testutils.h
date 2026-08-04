#ifndef TESTUTILS_H
#define TESTUTILS_H

#define WIDE

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* The suite's deterministic generator, in the shape sss_split_secret() and
 * sskr_generate_shards() now require. Writes 0,1,2,... and always succeeds;
 * tests that need a failing draw pass their own. */
bool test_rng(uint8_t *buffer, size_t len);

#endif /* TESTUTILS_H */
