/*
 * The executable that carries the header checks. The work is done at compile
 * time by the object libraries this depends on -- see header_self_contained.c
 * and the loop that builds one of them per header in CMakeLists.txt. If they
 * compile, the property holds.
 */

#include <cmocka.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>

static void test_headers_are_self_contained(void **state) {
    (void) state;
    /* Reaching here means every header compiled as the first thing in a
     * translation unit of its own. */
    assert_true(1);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_headers_are_self_contained),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
