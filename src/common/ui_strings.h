/*******************************************************************************
 *   Ledger Seed Tool application
 *   (c) 2016-2026 Ledger SAS
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 ********************************************************************************/

#pragma once

/*
 * Every user-visible string in the application, for both interface stacks --
 * BAGL (the three Nano targets) and NBGL (the touch targets). Nothing here
 * changes what a screen says; this header only gives the two stacks one place
 * to read their wording from, so a future change to one is not made without
 * seeing whether the other needs it too.
 *
 * The two stacks do not cut their text the same way. NBGL carries a full
 * sentence per screen; BAGL splits the same idea across the 2-4 lines of a
 * UX_STEP, and often uses different words entirely -- the two stacks already
 * diverged before this header existed (see the PR this header was added in).
 * Where the concept is shared but the wording is not, both forms are declared
 * here, next to each other, under one comment. Nothing is deduplicated by
 * inventing a shared string and a splitting algorithm: the divergence stays
 * visible, in this file, rather than being hidden by code that reconciles it.
 *
 * Where the wording happens to already be byte-identical between the two
 * stacks -- "Quit" on every BAGL screen that has it, "12 words" on both --
 * a single macro serves every call site.
 *
 * `#define` rather than a table of `extern const char[]`: a string literal
 * behind a macro costs nothing beyond what the literal itself costs, and the
 * linker merges identical literals; an array of pointers would cost 4 bytes
 * of flash and a relocation per entry, everywhere it is linked in, including
 * `nanos`, which has the least flash of the six targets. The one thing a
 * table buys -- enumerating every string in a test -- is recovered without
 * that cost by tests/unit/test_ui_strings.c, which declares its own table
 * naming every macro below; nothing in this header needs to be enumerable
 * from device code.
 *
 * Two BAGL-only literals are deliberately not here:
 *   - the "1".."16" share-count menu labels in src/bagl/ux_sskr_menu.c are a
 *     generated table, not authored wording, and are sized by the
 *     TARGET_NANOS-conditional _Static_assert against SSS_MAX_SHARE_COUNT
 *     immediately above them in that file; moving the labels out would
 *     separate them from the invariant that bounds them;
 *   - PRINTF trace strings, on both stacks, which the user never sees.
 */

/*
 * Common to both stacks, byte-identical -- one macro, several call sites.
 */
#define UI_STR_QUIT               "Quit"
#define UI_STR_VERSION_LABEL      "Version"
#define UI_STR_BIP39_PHRASE_TITLE "BIP39 Phrase"
#define UI_STR_WORDS_12           "12 words"
#define UI_STR_WORDS_18           "18 words"
#define UI_STR_WORDS_24           "24 words"

/*
 * "Processing", shown on nanos while a blocking comparison or SSKR
 * generation runs (src/bagl/nanos_enter_phrase.c), and on nanox/nanos+ for
 * the same wait (src/bagl/nanox_enter_phrase.c). Not used on NBGL, which has
 * no equivalent blocking step.
 */
#define UI_STR_BAGL_PROCESSING "Processing"

/*
 * Home / tool selection
 */

/*
 * NBGL: the four buttons of the menu (src/nbgl/ui.c), one per intention.
 *
 * They used to name formats -- "BIP39 Check", "SSKR Check", "BIP85 Generate"
 * -- and generating a backup was not among them: it was only offered after a
 * check had succeeded, so someone who came to back up a phrase found nothing
 * that matched what they came for. These name what the user wants instead,
 * and the two things worth the most in a backup tool are the two that gained
 * an entry.
 *
 * "backup" rather than "SSKR" on the two middle entries: someone who does not
 * know what SSKR is has to be able to find the function. The term itself
 * stays inside the flow, and above all on the paper the user copies down --
 * see UI_STR_NBGL_SSKR_SHARE_HEADER below.
 *
 * The Recover entry says "from backup" and not "from backup shares", which is
 * both shorter and, on reflection, the better half to drop. Measured first:
 * the longer form is 268px in a 268px button on apex_p -- not clipped, but
 * with no margin at all, which is the state just before clipping and
 * indistinguishable from it on a screenshot. And "shares" is the format's
 * word while "backup" is the word of someone holding the paper, which is who
 * this entry is for.
 */
#define UI_STR_NBGL_MENU_CHECK   "Check recovery phrase"
#define UI_STR_NBGL_MENU_BACKUP  "Generate backup shares"
#define UI_STR_NBGL_MENU_RECOVER "Recover from backup"
#define UI_STR_NBGL_MENU_DERIVE  "Derive with BIP85"
/*
 * The break is placed rather than left to the layout. The text area this is
 * drawn in breaks on characters because generic_screen_set_top_title()
 * (src/nbgl/layout_generic_screen.c) leaves `wrapping` clear, as every title
 * in that file does; without the break Stax drew "What do you want to d" and
 * "o?". Setting the field would wrap on words instead -- the break is kept
 * because these two halves are chosen rather than discovered.
 *
 * Nothing under it, though two of the four entries would be the better for a
 * line of their own -- "Check recovery phrase" does not say what it is
 * checked against, and "Derive with BIP85" says nothing to someone who does
 * not already know. There is no room on any of the three screens. Between the
 * back button and the fourth entry Flex has 96px, of which a single title
 * line takes 44; a subtitle wrapped to two lines was measured under Speculos
 * drawing its second line *under the first button*, which does not clip it,
 * it deletes it. One line would fit, and one line is 26 characters on apex_p.
 *
 * The list component that carries a subtitle per entry does not fit either:
 * nbgl_layoutAddTouchableBar() makes an entry 94px on apex_p with a one-line
 * subtitle, and four of them come to 376px against 340px of usable height.
 */
#define UI_STR_NBGL_MENU_TITLE "What do you want\nto do?"

/*
 * BAGL: the same intentions on the Nano idle menu (src/bagl/ui.c).
 *
 * Three, not four, and this is the one place the two stacks genuinely differ
 * rather than merely word things differently: BIP-85 has no BAGL screen at
 * all, on any Nano, so there is nothing for a fourth entry to lead to. The
 * three that are here are the same three intentions, in the same order.
 *
 * They read shorter than the touch labels, and not by preference. A pbb step
 * draws its two lines in an icon-flanked box that is 87px wide on the 128x32
 * Nano S, and that box does not wrap -- it clips, silently.
 * tests/unit/tests/ui_strings.c measures all six of these against it.
 *
 * The two entries these replace ended on "recovery phrase", which is 93px and
 * had therefore always been over that budget -- one of the two strings that
 * test recorded as failing rather than asserting, and so one of the two it
 * could say nothing about. Rewriting these lines was the occasion to stop
 * shipping it.
 *
 * The last two say, word for word, what the touch buttons say: "Generate" +
 * "backup shares" is 53px + 82px, "Recover" + "from backup" is 47px + 72px,
 * and both pairs clear the budget. Only the first entry has to differ --
 * "Check recovery" alone is 88px -- and what it does with the room is worth
 * more than the consistency it loses: "Check phrase" / "on this Ledger" says
 * what the phrase is checked against, which is the one thing the touch
 * button has no room to say.
 */
#define UI_STR_BAGL_IDLE_CHECK_L1   "Check phrase"
#define UI_STR_BAGL_IDLE_CHECK_L2   "on this Ledger"
#define UI_STR_BAGL_IDLE_BACKUP_L1  "Generate"
#define UI_STR_BAGL_IDLE_BACKUP_L2  "backup shares"
#define UI_STR_BAGL_IDLE_RECOVER_L1 "Recover"
#define UI_STR_BAGL_IDLE_RECOVER_L2 "from backup"

/*
 * The Nano form of the screen that says why the phrase is asked for. Same
 * reason as UI_STR_NBGL_BACKUP_EXPLAIN_DESC further down, cut for a step that
 * does not wrap: the first two lines are a complete sentence and are all a
 * nanos nn step shows, and the taller Nanos get the third.
 */
#define UI_STR_BAGL_BACKUP_EXPLAIN_L1 "This Ledger cannot"
#define UI_STR_BAGL_BACKUP_EXPLAIN_L2 "show you its phrase."
#define UI_STR_BAGL_BACKUP_EXPLAIN_L3 "Enter it to split it."

/* NBGL home screen description and action (src/nbgl/ui.c). */
#define UI_STR_NBGL_HOME_DESCRIPTION \
    "This Ledger application\nprovides some useful seed\nmanagement utilities."
#define UI_STR_NBGL_HOME_ACTION    "Select an action"
#define UI_STR_NBGL_HOME_COPYRIGHT "(c) 2018-2026 Ledger"

/*
 * BIP39 phrase length selection
 *
 * display_bip39_select_phrase_length_page() (src/nbgl/ui.c) serves three of
 * the four intentions, and only two titles: checking a phrase and backing one
 * up ask the same question -- how long is the phrase you are about to type --
 * so they read the same title, while deriving asks how long a phrase to
 * produce, which is a different question. BAGL has one screen for the same
 * choice (src/bagl/ui.c), worded for the Check flow only, and split 2 lines
 * on nanos, 3 lines on nanox/nanos+.
 */
#define UI_STR_NBGL_BIP39_LENGTH_TITLE_CHECK    "How long is your\nBIP39 Recovery\nPhrase?"
#define UI_STR_NBGL_BIP39_LENGTH_TITLE_DERIVE   "Length of BIP39\nphrase to\ngenerate?"
#define UI_STR_BAGL_BIP39_LENGTH_TITLE_L1_NANOS "Enter number"
#define UI_STR_BAGL_BIP39_LENGTH_TITLE_L2_NANOS "of BIP39 words"
#define UI_STR_BAGL_BIP39_LENGTH_TITLE_L1       "Select the number of"
#define UI_STR_BAGL_BIP39_LENGTH_TITLE_L2       "words written on"
#define UI_STR_BAGL_BIP39_LENGTH_TITLE_L3       "your Recovery Sheet"
#define UI_STR_BAGL_BIP39_LENGTH_BACK           "Back"

/*
 * SSKR entry start (src/bagl/ui.c). nanos gets 2 lines, nanox/nanos+ get 3
 * and start the flow directly from the step's own callback rather than a
 * shared init call -- an existing difference, not introduced here.
 */
#define UI_STR_BAGL_SSKR_START_TITLE_L1_NANOS "Enter SSKR"
#define UI_STR_BAGL_SSKR_START_TITLE_L2_NANOS "recovery phrase"
#define UI_STR_BAGL_SSKR_START_TITLE_L1       "Enter first word of"
#define UI_STR_BAGL_SSKR_START_TITLE_L2       "first share of SSKR"
#define UI_STR_BAGL_SSKR_START_TITLE_L3       "recovery phrase"

/*
 * Word / share entry keyboard
 *
 * NBGL carries one header sentence per keystroke (src/nbgl/ui.c). BAGL
 * rebuilds a short label into G_ux.string_buffer on every keystroke too, but
 * the exact wording differs between nanos (src/bagl/nanos_enter_phrase.c)
 * and nanox/nanos+ (src/bagl/nanox_enter_phrase.c) -- including the
 * capitalization of "word" and whether "Share" and "#" have a space between
 * them. None of that is touched here.
 */
#define UI_STR_NBGL_ENTER_BIP39_WORD "Enter word n. %d/%d of your\nBIP39 Recovery Phrase"
#define UI_STR_NBGL_ENTER_SSKR_WORD  "Enter Share %d Word %d\nof your Recovery Phrase"

/*
 * BAGL nanos (src/bagl/nanos_enter_phrase.c). UI_STR_BAGL_NANOS_WORD_HEADER_LOWER
 * has two call sites -- the "restart from" label and the entry-screen title
 * -- with byte-identical text; one macro serves both.
 */
#define UI_STR_BAGL_NANOS_WORD_HEADER       "Word #%d"
#define UI_STR_BAGL_NANOS_WORD_HEADER_LOWER "word #%d"
#define UI_STR_BAGL_NANOS_ENTER_SSKR_WORD   "Share#%d Word#%d"
#define UI_STR_BAGL_NANOS_RESTART_FROM      "Restart from"
#define UI_STR_BAGL_NANOS_WORD_INDEX_PREFIX "#%d "

/* BAGL nanox/nanos+ (src/bagl/nanox_enter_phrase.c). */
#define UI_STR_BAGL_NANOX_INTRO_L1         "Enter first letters"
#define UI_STR_BAGL_NANOX_INTRO_L2         "Next, enter letters"
#define UI_STR_BAGL_NANOX_INTRO_L3         "Finally, enter letters"
#define UI_STR_BAGL_NANOX_CLEAR_WORD       "Clear word"
#define UI_STR_BAGL_NANOX_ENTER_BIP39_WORD "Enter word #%d"
#define UI_STR_BAGL_NANOX_ENTER_SSKR_WORD  "Enter Share#%d word#%d"
#define UI_STR_BAGL_NANOX_OF_WORD          "of word #%d"
#define UI_STR_BAGL_NANOX_SELECT_WORD      "Select word #%d"

/*
 * Restore/entry navigation (src/bagl/ux_nano.c), shared by both the BIP39
 * and SSKR entry flows on every BAGL target.
 */
#define UI_STR_BAGL_ENTER_LABEL    "Enter"
#define UI_STR_BAGL_RETURN_TO_MENU "Return to menu"

/*
 * Check result / verdict
 *
 * NBGL (src/nbgl/ui.c) used to give the "valid but doesn't match" and "valid
 * and matches" sub-cases the *same* title, "Valid Secret\nRecovery Phrase" --
 * only the body text differed. It now gives all three outcomes their own
 * title, matching what BAGL (src/bagl/ux_nano.c) already did with two
 * distinct flows for "doesn't match" vs. "is correct". The wording is still
 * not shared between the stacks -- NBGL keeps its own
 * "<Adjective> Secret\nRecovery Phrase" template for all three (Invalid /
 * Mismatched / Valid), rather than adopting BAGL's "<Tool> Phrase" wording.
 * The template is deliberate: the three screens differ only in that one
 * word, so scanning them in sequence reads as three answers to the same
 * question rather than three differently-shaped messages.
 */
#define UI_STR_NBGL_RESULT_INVALID_TITLE "Invalid Secret\nRecovery Phrase"
#define UI_STR_NBGL_RESULT_NOMATCH_TITLE "Mismatched Secret\nRecovery Phrase"
#define UI_STR_NBGL_RESULT_VALID_TITLE   "Valid Secret\nRecovery Phrase"
#define UI_STR_NBGL_RESULT_BIP39_INVALID "The BIP39 Recovery Phrase\nyou have entered is not valid"
#define UI_STR_NBGL_RESULT_SSKR_INVALID  "The SSKR Recovery Phrase\nyou have entered is not valid"
#define UI_STR_NBGL_RESULT_BIP39_NOMATCH                                                          \
    "The BIP39 Recovery Phrase\nyou have entered\ndoesn't match the one present\non this Ledger " \
    "device."
#define UI_STR_NBGL_RESULT_BIP39_MATCH \
    "The BIP39 Recovery Phrase\nyou have entered\nmatches the one present\non this Ledger device."
#define UI_STR_NBGL_RESULT_SSKR_NOMATCH                                                          \
    "The SSKR Recovery Phrase\nyou have entered\ndoesn't match the one present\non this Ledger " \
    "device."
#define UI_STR_NBGL_RESULT_SSKR_MATCH \
    "The SSKR Recovery Phrase\nyou have entered\nmatches the one present\non this Ledger device."
#define UI_STR_NBGL_RESULT_TAP_TO_DISMISS "Tap to dismiss"

/*
 * The same three outcomes, read on the way to generating a backup.
 *
 * The verdict is a destination when the user came to check a phrase and a
 * step on the way when they came to back one up, and a failure does not mean
 * the same thing in the two. "Doesn't match the one present on this Ledger
 * device" is a complete answer to "is this my phrase?"; it is only half of
 * one to "can I back this up?", where what matters is that the shares about
 * to be written down would restore something this device cannot.
 *
 * The invalid outcome is not repeated here: a phrase that is not well formed
 * is not well formed for either purpose, and UI_STR_NBGL_RESULT_BIP39_INVALID
 * above says so in both flows, advice line included.
 *
 * The footer changes with it. A screen that continues cannot be labelled "Tap
 * to dismiss" -- and only the match continues, so the failures above keep the
 * dismissal.
 */
#define UI_STR_NBGL_RESULT_BACKUP_NOMATCH \
    "You would be backing up\na phrase this Ledger\ncannot recover."
#define UI_STR_NBGL_RESULT_BACKUP_MATCH \
    "This is the recovery phrase\non this Ledger device.\nIt can be split into shares."
#define UI_STR_NBGL_RESULT_TAP_TO_CONTINUE "Tap to continue"

/*
 * Invalid-phrase advice. BAGL (src/bagl/ux_nano.c) splits it over two lines,
 * shared by the BIP39 and SSKR invalid flows; NBGL (src/nbgl/ui.c) now
 * carries the same advice as a third, gray line under the invalid result's
 * body text (nbgl_pageInfoDescription_t.centeredInfo.text3, which requires
 * LARGE_CASE_GRAY_INFO -- not the LARGE_CASE_INFO style the other verdict
 * screens use -- to render as its own line rather than silently replacing
 * text2's content; see nbgl_layoutAddCenteredInfo() in the SDK).
 */
#define UI_STR_NBGL_RESULT_INVALID_ADVICE "Check length, order and spelling"
#define UI_STR_BAGL_INVALID_ADVICE_L1     "Check length,"
#define UI_STR_BAGL_INVALID_ADVICE_L2     "order and spelling"

#define UI_STR_BAGL_BIP39_INVALID_TITLE_L1 "BIP39 Recovery"
#define UI_STR_BAGL_BIP39_INVALID_TITLE_L2 "phrase invalid"
#define UI_STR_BAGL_BIP39_REENTER_PHRASE   "Re-enter phrase"
#define UI_STR_BAGL_BIP39_NOMATCH_TITLE_L2 "doesn't match"
#define UI_STR_BAGL_BIP39_MATCH_TITLE_L2   "is correct"

/*
 * What the mismatch adds when the phrase was being backed up rather than
 * checked -- the Nano's two lines of what UI_STR_NBGL_RESULT_BACKUP_NOMATCH
 * says in one sentence on the touch screens. It follows the "doesn't match"
 * title as its own step, the way the invalid advice above already does,
 * because a pbb title has no room for it.
 */
#define UI_STR_BAGL_BACKUP_NOMATCH_L1 "It would not restore"
#define UI_STR_BAGL_BACKUP_NOMATCH_L2 "this Ledger"

#define UI_STR_BAGL_SSKR_INVALID_TITLE_L1 "SSKR Recovery"
#define UI_STR_BAGL_SSKR_INVALID_TITLE_L2 "phrase invalid"
#define UI_STR_BAGL_SSKR_REENTER_SHARES   "Re-enter shares"
#define UI_STR_BAGL_SSKR_NOMATCH_TITLE_L1 "SSKR Phrase"
#define UI_STR_BAGL_SSKR_NOMATCH_TITLE_L2 "doesn't match"
#define UI_STR_BAGL_SSKR_MATCH_TITLE_L1   "SSKR Phrase"
#define UI_STR_BAGL_SSKR_MATCH_TITLE_L2   "is correct"

/*
 * Recover BIP39 from SSKR -- the screen between a successful verdict on a set
 * of shares and the phrase they rebuild.
 *
 * It is the only thing standing in front of a revealed recovery phrase, so it
 * stays even though the user chose "Recover from backup shares" from the menu
 * and has already said what they want. The screen that used to sit opposite
 * it, "Generate SSKR Phrase?", does not: nothing secret is on screen there,
 * and it now duplicates a menu entry.
 *
 * NBGL asks with a title + description + two buttons (src/nbgl/ui.c); BAGL
 * folds the same choice into the verdict flow's own steps
 * (src/bagl/ux_nano.c). Wording differs; declared together.
 */
#define UI_STR_NBGL_RECOVER_BIP39_TITLE "Recover BIP39 Phrase?"
#define UI_STR_NBGL_RECOVER_BIP39_DESC \
    "Choose if you wish to\nrecover the BIP39 phrase\nfrom your valid\nSSKR shares."
#define UI_STR_NBGL_RECOVER_BIP39_CONFIRM "Recover BIP39"

/*
 * nbgl_useCaseChoice()'s reject parameter is named rejectString in
 * lib_nbgl/include/nbgl_use_case.h; getRejectReviewText() in
 * lib_nbgl/src/nbgl_use_case.c returns "Cancel" for this kind of
 * offer-or-decline choice. Shared by both choice screens above and below.
 */
#define UI_STR_NBGL_CANCEL "Cancel"

/*
 * Backing up: why the phrase is asked for before anything is split.
 *
 * This screen exists because the question it answers had no answer on screen
 * at all. Someone who picks "Generate backup shares" is immediately asked to
 * type twenty-four words into a device that already holds them, and nothing
 * said why.
 *
 * The reason is verifiable rather than reassuring: compare_recovery_phrase()
 * (src/common/common_seed.c) derives a seed from what is typed and compares
 * it with one derived from the device. What the device gives back is a seed,
 * never the words -- so the words have to come from the person, and the
 * shares are built from those words.
 *
 * The confirmation button names the next screen rather than the SDK's generic
 * wording, as the two other choice screens in this application already do.
 *
 * No line breaks in the body, unlike the keypad titles further down. Those
 * are drawn in a text area that breaks on characters, so a break has to be
 * placed by hand or a range gets cut in half; nbgl_useCaseChoice() wraps this
 * one on word boundaries on its own, and hand-placed breaks only fought with
 * it -- Stax drew "This Ledger cannot read back" and then "the" alone on the
 * next line, because the break was put after a word that no longer fitted.
 */
#define UI_STR_NBGL_BACKUP_EXPLAIN_TITLE "Enter your recovery phrase"
#define UI_STR_NBGL_BACKUP_EXPLAIN_DESC                                  \
    "This Ledger cannot read back the recovery phrase it holds. Enter "  \
    "it, and it will be checked against this device before it is split " \
    "into SSKR shares."
#define UI_STR_NBGL_BACKUP_EXPLAIN_CONFIRM "Enter recovery phrase"

/*
 * nbgl_useCaseGenericReview()'s reject parameter is named rejectText in
 * lib_nbgl/include/nbgl_use_case.h; "Close" is what lib_nbgl uses for
 * dismissing a single already-displayed page with no pending offer (e.g.
 * info.navWithButtons.quitText in lib_nbgl/src/nbgl_use_case.c). Shared by
 * the two generic-review screens in src/nbgl/ui.c.
 */
#define UI_STR_NBGL_CLOSE "Close"

#define UI_STR_BAGL_GENERATE_SSKR_L1 "Generate"
#define UI_STR_BAGL_GENERATE_SSKR_L2 "SSKR phrases"
#define UI_STR_BAGL_RECOVER_BIP39_L1 "Recover"
#define UI_STR_BAGL_RECOVER_BIP39_L2 "BIP39 phrase"

/*
 * The label heading each generated share -- the one the user copies onto the
 * sheet along with the words.
 *
 * It now names the total as well as the index. A sheet reading "SSKR Share
 * #2" said nothing about how many sheets the set has, so whoever finds the
 * box later cannot tell a complete backup from a partial one, nor know how
 * many more to look for -- and SSKR is a threshold scheme, where that count
 * is what says whether the secret can still be rebuilt.
 *
 * "SSKR" stays in it. A sheet carrying dozens of four-letter words and no
 * name for its own format cannot be recovered by anyone who no longer has
 * this application: there is nothing to search for, so neither this app nor
 * Gordian SeedTool nor seedtool-cli can be found from the paper alone. That
 * this application can itself rebuild a BIP39 phrase from SSKR shares does
 * not cover that case -- the case is precisely not having it.
 *
 * The two stacks need different lengths, so both forms are declared here
 * rather than one being shortened at the call site: BAGL's bnnn_paging
 * appends its own "(page/total)" counter to this title, and the long form
 * plus that suffix overflows the Nano title area (seen truncated as
 * "SSKR Share... of 3 (4/5)" under Speculos).
 */
#define UI_STR_NBGL_SSKR_SHARE_HEADER "SSKR share %d of %d"
#define UI_STR_BAGL_SSKR_SHARE_HEADER "SSKR %d/%d"

/*
 * SSKR share count / threshold selection
 */
/*
 * The 16 spelled out in these two is SSS_MAX_SHARE_COUNT on every target that
 * links NBGL. It stays a literal here -- nothing composed at runtime, since
 * nothing about it varies -- and src/nbgl/ui.c static-asserts that the bound
 * it applies is still that number, so the two cannot drift apart in silence.
 */
#define UI_STR_NBGL_SSKR_NUMSHARES_TITLE       "Enter number of SSKR shares\nto generate (1 - 16)"
#define UI_STR_NBGL_SSKR_NUMSHARES_RANGE_ERROR "Number of SSKR shares must be between 1 and 16"
#define UI_STR_BAGL_SSKR_NUMSHARES_TITLE_L1    "Select number"
#define UI_STR_BAGL_SSKR_NUMSHARES_TITLE_L2    "of shares"

/*
 * Neither bound of the threshold keypad is a constant, so the title is a
 * format composed at display time rather than a literal. The upper `%d` is
 * the share count the user entered on the previous screen. The lower one is
 * 2 as soon as there is more than one share: a threshold of 1 over several
 * shares is the 1-of-m case refused below, so a title reading `(1 - N)` there
 * would promise a value the next screen rejects. Both are the same calls the
 * validator makes, so the screen cannot announce a range the code does not
 * accept.
 *
 * Each `%d` here stands for a value of at most two digits, and `%d` is itself
 * two characters wide, so the composed title is never longer than this format
 * literal. src/nbgl/ui.c sizes its title buffer on that, and static-asserts
 * the two-digit half of it against the constants themselves.
 */
#define UI_STR_NBGL_SSKR_THRESHOLD_TITLE      "Enter threshold value (%d - %d)"
#define UI_STR_NBGL_SSKR_THRESHOLD_ZERO_ERROR "Threshold value cannot be 0"
#define UI_STR_NBGL_SSKR_THRESHOLD_RANGE_ERROR \
    "Threshold value cannot be greater than number of shares"
#define UI_STR_NBGL_SSKR_THRESHOLD_ONE_OF_M_ERROR "1-of-m shares where\nm > 1 is not supported"
#define UI_STR_BAGL_SSKR_THRESHOLD_TITLE_L1       "Select"
#define UI_STR_BAGL_SSKR_THRESHOLD_TITLE_L2       "threshold"
#define UI_STR_BAGL_SSKR_ONE_OF_M_WARN_L1         "1-of-m shares"
#define UI_STR_BAGL_SSKR_ONE_OF_M_WARN_L2         "where m > 1"
#define UI_STR_BAGL_SSKR_ONE_OF_M_NOT_L1          "Not"
#define UI_STR_BAGL_SSKR_ONE_OF_M_NOT_L2          "Supported"

/*
 * BIP85 -- NBGL only. No BAGL target has a BIP85 screen at all.
 */
#define UI_STR_NBGL_BIP85_APP_BIP39        "BIP39"
#define UI_STR_NBGL_BIP85_APP_PWD_BASE64   "Password (Base64)"
#define UI_STR_NBGL_BIP85_APP_PWD_BASE85   "Password (Base85)"
#define UI_STR_NBGL_BIP85_SELECT_APP_TITLE "Which BIP85\napplication do you\nwish to use?"

#define UI_STR_NBGL_BIP85_BIP39_HEADER  "BIP39 Phrase (Index #%d)"
#define UI_STR_NBGL_BIP85_BASE64_HEADER "Base64 Password (Index #%d)"
#define UI_STR_NBGL_BIP85_BASE85_HEADER "Base85 Password (Index #%d)"

/*
 * The bound in the title is the keypad's own digit cap
 * (BIP85_INDEX_MAX_NUMBER_LENGTH, seven digits), which is what the person
 * typing can actually reach -- the same number, and for the same reason, as
 * the error message beside it. See the comment on bip85_index_validate() in
 * src/nbgl/ui.c for why the 31-bit derivation bound it also checks is a
 * different number on purpose.
 */
#define UI_STR_NBGL_BIP85_INDEX_TITLE       "Enter index (0 - 9,999,999)"
#define UI_STR_NBGL_BIP85_INDEX_RANGE_ERROR "BIP85 index must be between 0 and 9,999,999"

/*
 * Both bounds depend on which password application was chosen, so the title
 * is composed at display time from the same two values the validator applies,
 * exactly as the error message beside it already was. Same two-digit argument
 * for the buffer sizing as UI_STR_NBGL_SSKR_THRESHOLD_TITLE above.
 *
 * The line break is placed rather than left to the layout: without it the
 * keypad title wraps inside the range itself, and all three touch devices
 * drew "(20 - " on one line and "86)" on the next. A bound split across a
 * line break is the one thing this title exists not to do.
 */
#define UI_STR_NBGL_BIP85_PWD_LENGTH_TITLE       "Enter password length\n(%d - %d)"
#define UI_STR_NBGL_BIP85_PWD_LENGTH_RANGE_ERROR "BIP85 password length must be between %d and %d"
