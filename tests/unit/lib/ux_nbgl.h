#pragma once

/*
 * Deliberately empty, like lib/glyphs.h and lib/bolos_target.h.
 *
 * Defining HAVE_NBGL for a target makes the SDK's own include chain reach this
 * header -- os.h pulls in ux.h, which does `#ifdef HAVE_NBGL / #include
 * "ux_nbgl.h"` -- and the real one lives in lib_ux_nbgl, a directory the
 * harness does not put on the include path, behind nbgl_screen.h and
 * nbgl_touch.h and the rest of the graphics library.
 *
 * None of that is reachable from what the HAVE_NBGL targets actually compile:
 * the guarded blocks of seed_sskr.c and seed_bip39.c are string matching over
 * a wordlist and take nothing from the UX state. So this file exists only to
 * let the SDK's `#include` resolve, and the moment anything in it were needed,
 * the link would say so rather than this quietly standing in.
 */
