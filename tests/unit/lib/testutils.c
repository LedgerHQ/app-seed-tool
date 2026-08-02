#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "bolos/cxlib.h"

// LEDGER_ASSERT() calls this on a failing condition (see the SDK's
// ledger_assert_internals.h). The SDK's own unit tests stub this the same
// way (unit-tests/lib_tlv/mock_os.c) but with exit(0); this uses exit(1)
// instead so that a LEDGER_ASSERT unexpectedly firing inside any function
// linked into a test target makes ctest report a failure, rather than the
// process silently exiting 0 with no cmocka summary printed.
void assert_exit(bool confirm) {
    (void)confirm;
    exit(1);
}

// Fake random generator used only for testing
void cx_rng_no_throw(uint8_t *buffer, size_t len)
{
    for (size_t i = 0; i < len; i++) {
       buffer[i] = (uint8_t) i;
    }
}

// Fake secure memory comparison used only for testing
char os_secure_memcmp(const void *src1, const void *src2, size_t length)
{
#define SRC1 ((unsigned char const *) src1)
#define SRC2 ((unsigned char const *) src2)
    unsigned int  l      = length;
    unsigned char xoracc = 0;
    // don't || to ensure all condition are evaluated
    while (!(!length && !l)) {
        length--;
        xoracc |= SRC1[length] ^ SRC2[length];
        l--;
    }
    return xoracc;
}

uint32_t cx_crc32(const void *buf, size_t len)
{
  uint32_t crc;
  crc = cx_crc32_update(0xFFFFFFFF, buf, len);

  return crc;
}
