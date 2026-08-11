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
#define UI_STR_QUIT          "Quit"
#define UI_STR_VERSION_LABEL "Version"
#define UI_STR_WORDS_12      "12 words"
#define UI_STR_WORDS_18      "18 words"
#define UI_STR_WORDS_24      "24 words"

/*
 * The label over a BIP-39 phrase the application is showing back.
 *
 * One macro served both stacks and read "BIP39 Phrase". On BAGL that was
 * shipping clipped: the label is a bnnn_paging title in src/bagl/ux_bip39.c,
 * and bnnn_paging appends its own "(page/total)" counter at display time. At
 * two digits either side -- which is every 24-word phrase on the 128x32 Nano S,
 * where a page holds one line -- "BIP39 Phrase (24/24)" is 117px against the
 * 114px a Nano title line holds, and a bnnn_paging title clips rather than
 * wraps. The screen has been drawing "BIP39 Phrase (12/1".
 *
 * Nothing caught it. The macro was measured against the 87px PBB box, which it
 * cleared at 75px, and the counter is not part of the string, so measuring the
 * string alone could not see it. test_phrase_title_fits_the_nano_title_line()
 * in tests/unit/tests/ui_strings.c now measures what the widget actually draws,
 * as the SSKR share label beside it already was.
 *
 * So the two stacks separate, which is what this header is for: they have
 * different budgets and only one of them was over.
 *
 * The Nano form is "Your Phrase" -- 68px in the PBB box and 110px once the
 * counter is on it. "Recovery Phrase", which is what Ledger's own applications
 * call this, is 96px and 138px: it fits neither of the two boxes the Nano form
 * is drawn in, so the shorter noun there is a constraint rather than a
 * preference. The touch header has the room and takes the full form.
 */
#define UI_STR_NBGL_BIP39_PHRASE_TITLE "Recovery Phrase"
#define UI_STR_BAGL_BIP39_PHRASE_TITLE "Your Phrase"

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
 * The Recover entry says "from Backup" and not "from Backup Shares", which is
 * both shorter and, on reflection, the better half to drop. Measured first:
 * the longer form is 268px in a 268px button on apex_p -- not clipped, but
 * with no margin at all, which is the state just before clipping and
 * indistinguishable from it on a screenshot. And "shares" is the format's
 * word while "backup" is the word of someone holding the paper, which is who
 * this entry is for.
 */
#define UI_STR_NBGL_MENU_CHECK   "Check Recovery Phrase"
#define UI_STR_NBGL_MENU_BACKUP  "Generate Backup Shares"
#define UI_STR_NBGL_MENU_RECOVER "Recover from Backup"
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
 * line of their own -- "Check Recovery Phrase" does not say what it is
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
 * "Backup Shares" is 53px + 82px, "Recover" + "from Backup" is 47px + 72px,
 * and both pairs clear the budget. Only the first entry has to differ --
 * "Check Recovery" alone is 88px -- and what it does with the room is worth
 * more than the consistency it loses: "Check Phrase" / "on this Ledger" says
 * what the phrase is checked against, which is the one thing the touch
 * button has no room to say.
 *
 * The capitals cost nothing in this font: all six fragments measure exactly
 * what their lowercase forms did.
 */
#define UI_STR_BAGL_IDLE_CHECK_L1   "Check Phrase"
#define UI_STR_BAGL_IDLE_CHECK_L2   "on this Ledger"
#define UI_STR_BAGL_IDLE_BACKUP_L1  "Generate"
#define UI_STR_BAGL_IDLE_BACKUP_L2  "Backup Shares"
#define UI_STR_BAGL_IDLE_RECOVER_L1 "Recover"
#define UI_STR_BAGL_IDLE_RECOVER_L2 "from Backup"

/*
 * The Nano form of the screen that says why the phrase is asked for. Same
 * reason as UI_STR_NBGL_BACKUP_EXPLAIN_DESC further down, cut for a step that
 * does not wrap: the first two lines are a complete sentence and are all a
 * nanos nn step shows, and the taller Nanos get the third.
 */
#define UI_STR_BAGL_BACKUP_EXPLAIN_L1 "This Ledger cannot"
#define UI_STR_BAGL_BACKUP_EXPLAIN_L2 "read back its Phrase."
#define UI_STR_BAGL_BACKUP_EXPLAIN_L3 "Enter it to split it."
/*
 * The same screen on Nano S, which has two lines where the others have three.
 *
 * Dropping the third line was the obvious thing and is what this replaces: it
 * left the reader told what the device cannot do and never told what to do
 * about it. Both halves are kept by tightening instead, and the pair is held
 * to the same 116px box as every other nn line by the test in
 * tests/unit/tests/ui_strings.c -- which is the only check there can be, since
 * no emulator runs this target.
 */
/*
 * What the journey makes, said before the screen that asks for the Phrase.
 *
 * The touch stack has had this since the Backup journey was written and the
 * Nano did not, so a Nano reader typed twenty-four words and reached a share
 * count never having been told what a Share is. The second half is the one
 * that earns the screen: splitting a Phrase is only a backup if the pieces are
 * kept apart, and someone who writes them all on one sheet has made a copy of
 * their Phrase with none of the protection.
 */
#define UI_STR_BAGL_BACKUP_SSKR_L1       "Your Phrase is split"
#define UI_STR_BAGL_BACKUP_SSKR_L2       "into Shares to write."
#define UI_STR_BAGL_BACKUP_SSKR_L3       "Keep them apart."
#define UI_STR_BAGL_BACKUP_SSKR_L1_NANOS "Phrase split into"
#define UI_STR_BAGL_BACKUP_SSKR_L2_NANOS "Shares. Keep apart."

#define UI_STR_BAGL_BACKUP_EXPLAIN_L1_NANOS "Ledger cannot read"
#define UI_STR_BAGL_BACKUP_EXPLAIN_L2_NANOS "its Phrase. Enter it."
/*
 * The same two lines serve the Check journey, which differs only in what it
 * does with the phrase once it has it. The third line is where they part, and
 * it exists for the same reason the touch stack gained its own Check screen:
 * the menu entry says what is compared, not what happens next.
 */

/*
 * What the Recover journey never said on this stack, and the widest gap this
 * work left behind.
 *
 * The touch screens have carried "You do not need all of them." since the
 * explanation screens were added; two buttons went straight from "Recover /
 * from Backup" to "Enter first word of first share". Someone holding two
 * sheets of three has no reason to believe it will work, so they do not try --
 * and a backup that is never attempted is a backup lost, on exactly the day
 * the threshold existed for.
 *
 * The first two lines carry the whole fact, because a nanos nn step shows only
 * two; the taller Nanos get the third.
 */
#define UI_STR_BAGL_RECOVER_EXPLAIN_L1 "Not all your Shares"
#define UI_STR_BAGL_RECOVER_EXPLAIN_L2 "are needed."
#define UI_STR_BAGL_RECOVER_EXPLAIN_L3 "Any order works."
/*
 * The same screen on Nano S. "Any order works" is the line that was being
 * dropped, and it is the one that changes what the reader does: without it
 * someone hunts for the share numbered 1 before starting. It is also the claim
 * that was checked on the device before being printed -- two shares entered
 * reversed give the same verdict -- so losing it on one target and keeping it
 * on the others would state it inconsistently rather than briefly.
 */
#define UI_STR_BAGL_RECOVER_EXPLAIN_L1_NANOS "Not all Shares needed."
#define UI_STR_BAGL_RECOVER_EXPLAIN_L2_NANOS "Any order works."

/* NBGL home screen description and action (src/nbgl/ui.c). */
/*
 * The first sentence anyone reads, and it used to say "provides some useful
 * seed management utilities" -- three lines that name none of the four things
 * the application does and could sit under any application at all. This names
 * them, in the order the menu offers them.
 */
#define UI_STR_NBGL_HOME_DESCRIPTION \
    "Check a Recovery Phrase,\nback it up as Shares,\nor derive a new secret."
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
#define UI_STR_NBGL_BIP39_LENGTH_TITLE_CHECK    "How long is your\nRecovery Phrase?"
#define UI_STR_NBGL_BIP39_LENGTH_TITLE_DERIVE   "Length of BIP39\nPhrase?"
#define UI_STR_BAGL_BIP39_LENGTH_TITLE_L1_NANOS "How long is your"
#define UI_STR_BAGL_BIP39_LENGTH_TITLE_L2_NANOS "Recovery Phrase?"
#define UI_STR_BAGL_BIP39_LENGTH_TITLE_L1       "Select the number of"
#define UI_STR_BAGL_BIP39_LENGTH_TITLE_L2       "words written on"
#define UI_STR_BAGL_BIP39_LENGTH_TITLE_L3       "your Recovery Sheet"
#define UI_STR_BAGL_BIP39_LENGTH_BACK           "Back"

/*
 * SSKR entry start (src/bagl/ui.c). nanos gets 2 lines, nanox/nanos+ get 3
 * and start the flow directly from the step's own callback rather than a
 * shared init call -- an existing difference, not introduced here.
 */
#define UI_STR_BAGL_SSKR_START_TITLE_L1_NANOS "Enter your"
#define UI_STR_BAGL_SSKR_START_TITLE_L2_NANOS "SSKR Shares"
#define UI_STR_BAGL_SSKR_START_TITLE_L1       "Enter first word of"
#define UI_STR_BAGL_SSKR_START_TITLE_L2       "first share of your"
#define UI_STR_BAGL_SSKR_START_TITLE_L3       "SSKR Backup"

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
#define UI_STR_NBGL_ENTER_BIP39_WORD "Enter word no. %d of %d\nof your Recovery Phrase"
#define UI_STR_NBGL_ENTER_SSKR_WORD  "Enter Share %d Word %d\nof your SSKR Backup"

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
#define UI_STR_NBGL_RESULT_PHRASE_INVALID_TITLE "Invalid\nRecovery Phrase"
#define UI_STR_NBGL_RESULT_PHRASE_NOMATCH_TITLE "Mismatched\nRecovery Phrase"
#define UI_STR_NBGL_RESULT_PHRASE_VALID_TITLE   "Valid\nRecovery Phrase"
/*
 * The Recover journey names what it was actually given.
 *
 * It reached this screen under "Valid Secret Recovery Phrase", and that title
 * asserts something check_result_callback() (src/nbgl/ui.c) deliberately does
 * not test: it gates on sskr_shares_check(), that the shares recombined, and
 * never on seed_match. So the screen claimed an agreement with this device that
 * nothing had checked, two screens before a warning saying the opposite.
 *
 * What was verified is that the shares are well formed, and that is what these
 * say. verdict_title[][] carries them, the same shape verdict_body[][] already
 * had -- the body was chosen per intention while the title over it was not.
 */
#define UI_STR_NBGL_RESULT_SHARES_INVALID_TITLE "Invalid\nSSKR Shares"
#define UI_STR_NBGL_RESULT_SHARES_NOMATCH_TITLE "Mismatched\nSSKR Shares"
#define UI_STR_NBGL_RESULT_SHARES_VALID_TITLE   "Valid\nSSKR Shares"
#define UI_STR_NBGL_RESULT_BIP39_INVALID        "The Phrase you have entered\nis not valid."
#define UI_STR_NBGL_RESULT_SSKR_INVALID         "The SSKR Shares you have\nentered are not valid."
#define UI_STR_NBGL_RESULT_BIP39_NOMATCH \
    "The Phrase you have entered\ndoesn't match the one present\non this Ledger device."
#define UI_STR_NBGL_RESULT_BIP39_MATCH \
    "The Phrase you have entered\nmatches the one present\non this Ledger device."
#define UI_STR_NBGL_RESULT_SSKR_NOMATCH                                                           \
    "The Shares you have entered\nrebuild a Phrase that is not\nthe one present on this\nLedger " \
    "device."
#define UI_STR_NBGL_RESULT_SSKR_MATCH \
    "The Shares you have entered\nrebuild the Phrase present\non this Ledger device."
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
    "You would be backing up\na Phrase this Ledger\ncannot recover."
#define UI_STR_NBGL_RESULT_BACKUP_MATCH \
    "This is the Recovery Phrase\non this Ledger device.\nIt can be split into Shares."
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

/*
 * The three BIP-39 verdicts on a Nano, which now differ in their second line
 * only.
 *
 * There is no UI_STR_BAGL_BIP39_INVALID_TITLE_L1 any more, and that is the
 * second half of the clipping fix above. The two that match and mismatch never
 * had a first line of their own -- src/bagl/ux_nano.c draws
 * UI_STR_BAGL_BIP39_PHRASE_TITLE over both -- while the invalid one carried
 * "BIP39 Recovery", 90px against the 87px PBB box on nanos, drawn as
 * "BIP39 Recover". It was the one string tests/unit/tests/ui_strings.c recorded
 * as failing rather than asserting, because nothing had touched that screen
 * since the budget was written down.
 *
 * Giving it the same title as its two siblings closes the gap by removing the
 * string rather than by shortening it, and leaves the three screens reading as
 * three answers to one question -- which is the shape the touch stack's verdict
 * titles already have, for the same reason.
 *
 * "is not valid" rather than "phrase invalid" follows from that: with the noun
 * on the line above, the second line is a predicate on all three screens.
 */
#define UI_STR_BAGL_BIP39_INVALID_TITLE_L2 "is not valid"
#define UI_STR_BAGL_BIP39_REENTER_PHRASE   "Re-enter Phrase"
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

/*
 * The Nano forms of the three Recover verdicts. What the user entered is a set
 * of shares and the screens say so; "SSKR Phrase" named an object this journey
 * never handles.
 *
 * One title macro for the three, as the BIP-39 verdicts opposite already do:
 * the three screens differ in their second line only, and three macros holding
 * the same eleven characters is three places to change when one is meant.
 *
 * The plural also costs nothing: "SSKR Shares" is 69px in the PBB box against
 * 87, where "SSKR Recovery" was 82 -- the technical noun is the shorter one
 * here.
 */
#define UI_STR_BAGL_SSKR_SHARES_TITLE     "SSKR Shares"
#define UI_STR_BAGL_SSKR_INVALID_TITLE_L2 "are not valid"
#define UI_STR_BAGL_SSKR_REENTER_SHARES   "Re-enter Shares"
#define UI_STR_BAGL_SSKR_NOMATCH_TITLE_L2 "don't match"
#define UI_STR_BAGL_SSKR_MATCH_TITLE_L2   "are correct"

/*
 * Recover BIP39 from SSKR -- the screen between a successful verdict on a set
 * of shares and the phrase they rebuild.
 *
 * It used to be an offer: "Recover BIP39 Phrase?", with a description saying
 * the user could choose to rebuild the phrase from their valid shares. It said
 * nothing about a secret being about to appear, which is the only thing on
 * that screen worth saying -- the user chose "Recover from Backup" from the
 * menu and has already answered the offer.
 *
 * What it says now, and why the second sentence is there
 * -----------------------------------------------------
 *
 * This path reveals the rebuilt phrase whether or not it matches the seed this
 * Ledger holds. That is deliberate and it is the point of the feature:
 * rebuilding your phrase from your own shares onto a spare or replacement
 * device is what a backup is for, and it is precisely the case where the
 * device does not already have the phrase. Requiring a match would remove the
 * feature on the day it is needed.
 *
 * check_result_callback() (src/nbgl/ui.c) therefore gates this on
 * sskr_shares_check() succeeding -- the shares recombined -- and not on
 * seed_match, and ux_sskr_nomatch_flow (src/bagl/ux_nano.c) reaches the same
 * step. What was missing was any screen saying so. The second sentence says
 * it: what appears is what the entered shares rebuild, which is not
 * necessarily what this device holds. The verdict screen immediately before
 * has already reported whether the two agreed.
 *
 * BAGL folds the same warning into the verdict flow's own steps
 * (src/bagl/ux_nano.c), cut to a Nano's line lengths. Wording differs;
 * declared together.
 */
#define UI_STR_NBGL_RECOVER_WARN_TITLE "A Recovery Phrase will be shown"
#define UI_STR_NBGL_RECOVER_WARN_DESC                                        \
    "Anyone who sees this Phrase can spend from it. It is what your Shares " \
    "rebuild, not necessarily the one this Ledger holds."

/*
 * nbgl_useCaseChoice()'s reject parameter is named rejectString in
 * lib_nbgl/include/nbgl_use_case.h; getRejectReviewText() in
 * lib_nbgl/src/nbgl_use_case.c returns "Cancel" for this kind of
 * offer-or-decline choice. Shared by both choice screens above and below.
 */
#define UI_STR_NBGL_CANCEL "Cancel"

/*
 * The two numbers, read immediately before the keypad that asks for the first
 * of them.
 *
 * This screen used to open the Backup journey, and that was its defect: it
 * described sheets of ByteWords and a threshold to someone whose next thirty
 * screens are a phrase-length choice, a BIP-39 keyboard and a verdict. It
 * explained the end of the journey at its beginning, which is the moment of
 * least relevance -- and it left two explanations back to back.
 *
 * From here each row lands on the keypad it explains: the first on "Enter
 * number of SSKR Shares to generate", the second on "Enter threshold value",
 * whose title is the only place the word threshold appears cold.
 *
 * The move also buys a number that could not be shown before. The words per
 * Share depend on the phrase length, which is unknown at the top of the
 * journey and known here: bolos_ux_sskr_share_wordcount() answers 29, 38 or 46
 * for a 12, 18 or 24-word phrase, so the first row is composed rather than
 * literal. Earlier it could only have offered a range, which is worse than
 * silence.
 *
 * "The others can be lost" is kept word for word. It is the only line in these
 * screens that states a consequence in the user's own world rather than a
 * mechanism, and it is what makes a threshold worth more than a photocopy.
 */
#define UI_STR_NBGL_SSKR_NUMBERS_TITLE      "How many Shares?"
#define UI_STR_NBGL_SSKR_NUMBERS_ROW_CREATE "You choose how many Shares to create."
#define UI_STR_NBGL_SSKR_NUMBERS_ROW_WORDS  "Each one is %d words to write down."

/*
 * The word "threshold" is here because the keypad two screens on is titled
 * "Enter threshold value" and nothing else in the application defines it.
 * Avoiding it made this screen easier to read and left that keypad
 * unexplained, which is the opposite of what an explanation is for: the term
 * is met once with its meaning attached, and is a label from then on.
 */
/*
 * The threshold, on its own screen and immediately before the keypad titled
 * "Enter threshold value" -- the only place that word appears cold.
 *
 * It used to be the second row of "How many Shares?", which meant the notion
 * was defined before the user had chosen a share count, two screens before it
 * is asked for. Splitting also buys back an antecedent: the old row said "how
 * many Shares rebuild it" with no noun for "it" on that screen, and the space
 * for "your Phrase" did not exist while both ideas shared seven lines.
 *
 * The title copies "What is an index?", which sits in the same relation to its
 * own keypad two journeys away.
 */
#define UI_STR_NBGL_SSKR_THRESHOLD_CONCEPT_TITLE "What is a threshold?"
#define UI_STR_NBGL_SSKR_THRESHOLD_CONCEPT_ROW_REBUILD \
    "The threshold is how many Shares rebuild your Phrase."
#define UI_STR_NBGL_SSKR_THRESHOLD_CONCEPT_ROW_LOST "The other Shares can be lost."

#define UI_STR_NBGL_EXPLANATION_CONTINUE "Continue"

/*
 * Why the device cannot answer on its own, said once and used by the Backup
 * journey only.
 *
 * The Check journey had a screen carrying this too and no longer does. It goes
 * straight from the menu to the phrase length, which is the shape
 * app-recovery-check has and the shape this journey had before the menu
 * existed: entering the Phrase *is* checking it, and a screen explaining that
 * a check needs the Phrase tells the reader what they already decided.
 *
 * Backup is the opposite case. There the user asked for Shares, and being
 * asked for twenty-four words is a surprise -- compare_recovery_phrase()
 * (src/common/common_seed.c) gets a seed back from the device and never the
 * words, so the words have to come from the person. That is what this sentence
 * accounts for, and it is why it survives on one journey and not the other.
 */
#define UI_STR_NBGL_PHRASE_NOT_READABLE "This Ledger cannot read its own Phrase."

/*
 * What an index is, read immediately before the keypad that demands one.
 *
 * "Enter index (0 - 9,999,999)" asks for a number between zero and ten million
 * and nothing anywhere says what it is. Fifteen occurrences of the word in this
 * file and not one of them defines it.
 *
 * It is also the only part of the derivation path the user chooses, so the
 * warning two screens earlier -- write the path down or the secret is lost --
 * is a warning about a value this screen is about to let them invent blind.
 *
 * "The same index gives it again" used to close this screen and is gone from
 * it: the page before now says the same thing about the path as a whole, and
 * saying it twice in one journey spends a page on nothing.
 */
#define UI_STR_NBGL_BIP85_INDEX_CONCEPT_TITLE     "What is an index?"
#define UI_STR_NBGL_BIP85_INDEX_CONCEPT_ROW_TELLS "The index tells one secret from another."
#define UI_STR_NBGL_BIP85_INDEX_CONCEPT_ROW_COUNT "Use 0 for the first, then 1, 2, and so on."

/*
 * What deriving a secret means, read before an application is chosen.
 *
 * The first row is the property that makes BIP-85 worth using at all. The
 * second is the trap it comes with, and it is deliberately not what the review
 * says: the review *shows* the path, and showing is not telling. A path that is
 * seen and not written down is an index the user will not have next year, and
 * the secret is then unreachable while the seed that produced it is perfectly
 * intact. Nothing in this flow said so.
 */

/*
 * "A new secret", not "The secret": the definite article pointed at something
 * the reader had never been shown, and the two rows under it are both about
 * losing it -- two thirds of the screen warning about the loss of a thing that
 * had not been introduced.
 *
 * "seed" stays, and it is the only user-visible string in this application that
 * uses the word. Any paraphrase would blur what is exact here: a BIP-85 secret
 * is computed from the seed this device already holds, and this journey never
 * asks the user to type anything.
 */
#define UI_STR_NBGL_BIP85_TITLE       "How BIP85 works"
#define UI_STR_NBGL_BIP85_ROW_DERIVED "A new secret is computed from this Ledger's seed."
/*
 * This is the second page, and it is the trap rather than the property: the
 * page before says what BIP-85 gives, this one says what it costs to lose.
 *
 * It used to read "Write down the path", an order the reader cannot obey --
 * the path is not on screen yet, it appears on the review further on. Saying
 * where it will be turns an impossible instruction into a warning that
 * arrives before the thing it warns about.
 */
#define UI_STR_NBGL_BIP85_ROW_PATH "The path is shown before the secret. Write it down."
#define UI_STR_NBGL_BIP85_ROW_LOST "Without the path, this secret is lost."

/*
 * What rebuilding from Shares asks for, read before the first word.
 *
 * The first row is the one that earns the screen, and it is not a convenience:
 * someone who has lost a sheet may never attempt the recovery at all, not
 * knowing that a subset is enough. That is precisely the case the backup was
 * made for, failing for want of a sentence.
 */

/*
 * "in any order" is a claim about the device, so it was asked rather than read.
 * sskr.c groups shards by the member_index each carries in its own bytes, not
 * by the order they arrive in -- and two Shares entered reversed under Speculos
 * gave the same verdict as in order. The interface is what suggests otherwise,
 * numbering the keyboard "Share 1", then "Share 2".
 *
 * The second row replaces one that told the user to remember "the number you
 * chose when you made them". That number is not on the sheet:
 * UI_STR_NBGL_SSKR_SHARE_HEADER prints an index and a total, never the
 * threshold. The app was asking for something it had never given. What it can
 * say instead is what will happen -- bolos_ux_sskr_entry_header_update() reads
 * the threshold out of the first Share's own header, and
 * sskr_shares_complete_check() stops there.
 */
#define UI_STR_NBGL_RECOVER_TITLE      "How recovery works"
#define UI_STR_NBGL_RECOVER_ROW_SUBSET "Enter your Shares one at a time, in any order."
#define UI_STR_NBGL_RECOVER_ROW_ORDER \
    "You do not need all of them. This Ledger stops when it has enough."

/*
 * Backing up: why the phrase is asked for before anything is split.
 *
 * This screen exists because the question it answers had no answer on screen
 * at all. Someone who picks "Generate Backup Shares" is immediately asked to
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
/*
 * The title names the process, and joins the family the other three journey
 * heads already form.
 *
 * It read "Why your Phrase?", and before that "Why we ask for it" -- both
 * answering an objection the reader has not raised. Someone who has just tapped
 * "Generate Backup Shares" is asking what happens now, not why a phrase is
 * legitimate to request, and being met with a justification suggests there was
 * something to justify.
 *
 * Two things made it worse here than elsewhere. This is the only one of the six
 * explanation screens whose button says something other than "Continue" -- the
 * demand is on the button, which is read last, so the old title only made sense
 * in retrospect. And it was the only one sharing no word with the menu entry
 * that opens it: Check reaches "How the check works", Recover reaches "How
 * recovery works", Derive reaches "How BIP85 works", and "Generate Backup
 * Shares" reached a screen naming neither backup nor Shares.
 *
 * "Before the Shares" was tried and withdrawn: no string in lib_nbgl or in
 * app-recovery-check begins with a preposition -- their titles are imperatives
 * ("Go back to message"), noun groups ("Blind signing ahead") or questions
 * ("Phrase Length?"). "How Shares are made" is nineteen characters, the same as
 * the "How the check works" already shipping beside it, and it puts the menu's
 * own word back on the screen.
 *
 * Read downwards now: How Shares are made -- this Ledger cannot read back its
 * own Phrase -- what you enter is checked against this Ledger -- Enter Recovery
 * Phrase. The rows support the button instead of defending the screen.
 *
 * The rows are unchanged and the screen is at its ceiling, five lines of the
 * five a button-pair screen holds on Flex, so the title was the only free
 * variable. Row 1 is shared with the Check screen (src/nbgl/ui.c) and changing
 * it would change two journeys.
 */
/*
 * The two screens that explain SSKR, before the third asks for the Phrase.
 *
 * The first says what the journey makes, the second what to do with it -- and
 * the second is the one that earns its screen. Splitting a Phrase into Shares
 * is only a backup if the Shares are kept apart; a reader who writes all of
 * them on one sheet has made a copy of their Phrase and none of the safety.
 * Nothing in this journey said so.
 *
 * Neither repeats the two screens further on. "How many Shares" and the
 * threshold are choices the reader makes after the Phrase is checked, and they
 * are explained where they are asked for.
 */
#define UI_STR_NBGL_BACKUP_SSKR_TITLE "How the backup works"
#define UI_STR_NBGL_BACKUP_ROW_SPLIT  "Your Phrase is split into Shares to write down."
/*
 * The row that earns the screen. Splitting a Phrase into Shares is only a
 * backup if the Shares are kept apart; a reader who writes them all on one
 * sheet has made a copy of their Phrase and none of the safety. Nothing in
 * this journey said so.
 */
#define UI_STR_NBGL_BACKUP_ROW_APART "Keep each one in a different place."

#define UI_STR_NBGL_BACKUP_PHRASE_TITLE "Why your Phrase?"
#define UI_STR_NBGL_BACKUP_ROW_CHECKED  "It checks what you enter."
/*
 * The second page carries no new sentence of its own: it is
 * UI_STR_NBGL_PHRASE_ASKED_FOR, the one the Check journey also shows. Both ask
 * for the Phrase for the same reason, and one macro is what keeps them from
 * drifting apart.
 *
 * "against this Ledger", not "against the one present on this Ledger device".
 * The long form is Ledger's own and is kept word for word where it settles the
 * ambiguity the user has to act on -- the three verdicts. Here the sentence has
 * just said this device holds a Phrase it cannot read back, so the antecedent
 * is already on screen and the full formula only adds length. Measured across
 * the corpus, Ledger uses it in 7% of its sentences; this application had
 * reached 30%.
 */
#define UI_STR_NBGL_BACKUP_EXPLAIN_CONFIRM "Enter Recovery Phrase"

/*
 * nbgl_useCaseGenericReview()'s reject parameter is named rejectText in
 * lib_nbgl/include/nbgl_use_case.h; "Close" is what lib_nbgl uses for
 * dismissing a single already-displayed page with no pending offer (e.g.
 * info.navWithButtons.quitText in lib_nbgl/src/nbgl_use_case.c). Shared by
 * the two generic-review screens in src/nbgl/ui.c.
 */
#define UI_STR_NBGL_CLOSE "Close"

/*
 * The pair lib_nbgl puts on a warning that stands in front of something the
 * user may not want to do: buttonsInfo.bottomText / .topText in
 * lib_nbgl/src/nbgl_use_case.c, on the blind-signing warning. Taken rather
 * than invented, and used on all three screens in this application that stand
 * in front of a secret being drawn.
 *
 * "Back to safety" is the safe half in the SDK's use and in ours: every screen
 * that offers it reaches reset_globals() through display_home_page(), so
 * declining does not merely go back, it erases.
 */
#define UI_STR_NBGL_CONTINUE_ANYWAY "Continue anyway"
#define UI_STR_NBGL_BACK_TO_SAFETY  "Back to safety"

/*
 * The review shown before a backup is generated.
 *
 * It carries no title of its own, and that is a consequence of the component
 * rather than a choice: nbgl_useCaseGenericReview() lays out the pairs and
 * then the final page, with no leading centered page for a heading to sit on.
 * The rows label themselves, and the screen is arrived at from the keypad that
 * entered the last of them.
 *
 * What it is for: nothing was said about the size of the job. The threshold
 * keypad was the last screen, and the next thing that happened was 46 words a
 * share appearing, five times over. Someone choosing 5 shares is choosing to
 * hand-copy 230 words onto paper, and being told that before rather than
 * during is the difference between a decision and a discovery.
 *
 * All three numbers are composed at display time, and none of them is a
 * literal: the count and threshold are what the two keypads accepted, and the
 * word totals come from bolos_ux_sskr_share_wordcount()
 * (src/common/sskr/seed_sskr.c), which is the generator's own arithmetic --
 * see the comment there for why a second computation exists at all and what
 * holds it to the first.
 *
 * The finish page's title is free (getFinishTitle() in
 * lib_nbgl/src/nbgl_use_case.c returns finishTitle whenever it is not NULL);
 * the button under it is not, and reads "Approve" for
 * nbgl_useCaseReviewLight(). Both that word and "Reject" beside it are the
 * SDK's own.
 */
#define UI_STR_NBGL_SSKR_REVIEW_FINISH "Generate Backup Shares"
/*
 * The vocabulary of the two screens that collected these values, not a plainer
 * one invented for the review.
 *
 * The share count was asked for as "number of SSKR shares" and the threshold
 * as "threshold value", one and two screens earlier. Rendering them back as
 * "Shares to write down" and "Needed to restore" -- which is what this review
 * first did -- gives the same two numbers two names inside one flow, and the
 * review is the screen whose whole job is to say back what was entered.
 *
 * It is also the rule already recorded for this application: plain words at
 * the menu, so that someone who does not know what SSKR is can still find the
 * function, and the technical term inside the flow and on the paper the user
 * ends up holding. Over-applying the plain form inside the flow is a mistake
 * this interface made once before and corrected.
 *
 * "Format: SSKR" is here for the same reason the BIP85 review carries the
 * derivation path: it is the one thing that makes the result usable without
 * this application. A sheet of forty-six four-letter words that does not name
 * its own format cannot be searched for, so neither Gordian SeedTool nor
 * seedtool-cli nor this app can be found from the paper alone -- and the case
 * that matters is precisely the one where this app is gone.
 */
#define UI_STR_NBGL_SSKR_REVIEW_ITEM_FORMAT    "Format"
#define UI_STR_NBGL_SSKR_REVIEW_ITEM_SHARES    "Number of Shares"
#define UI_STR_NBGL_SSKR_REVIEW_ITEM_THRESHOLD "Threshold"
/*
 * "write", not "copy". The same act was called two things two screens apart:
 * this row said "Words to copy" while the screen before it said "%d words to
 * write down". Ledger's own wording settles which survives -- app-recovery-check
 * has "Select the number of words written on your Recovery Sheet" -- so the
 * verb is write, here and on the Nano review beside it.
 */
#define UI_STR_NBGL_SSKR_REVIEW_ITEM_WORDS   "Words to write"
#define UI_STR_NBGL_SSKR_REVIEW_FORMAT_VALUE "SSKR"
/*
 * Each %d stands for a value of at most two digits except the word total,
 * which reaches three (16 shares of 46 words is 736). src/nbgl/ui.c sizes its
 * buffers on a constant rather than on these formats, and static-asserts each
 * bound against the constant that supplies it -- sizeof(format) is exactly
 * what gets the word total wrong.
 */
/*
 * One macro for the share count and the threshold: two call sites, and the
 * same bare number in both.
 *
 * The threshold used to read "3 of 5", which repeated the share count the row
 * above already gave -- three rows carrying two numbers. A bare 3 against a
 * bare 5 also puts the contrast where it belongs: what you will hold against
 * what an attacker needs.
 *
 * The sheet size and the total share one row, and that is a measured choice
 * rather than a tidy one. Split into "Each share" and "To write down" -- which
 * is how the interface specification draws them, and which reads better -- the
 * review no longer fits one page: Flex and Apex paginate it at four rows, so
 * the fifth lands alone on page two, and the number that lands there is the
 * total. Isolating the one figure that should make someone reconsider the
 * scheme, on a page of its own, is worse than packing it beside the per-share
 * figure that gives it its scale.
 *
 * The cost is the special case below: at 1-of-1 the two numbers are equal and
 * the parenthesis would say the same thing twice.
 */
#define UI_STR_NBGL_SSKR_REVIEW_COUNT_VALUE        "%d"
#define UI_STR_NBGL_SSKR_REVIEW_WORDS_VALUE        "%d (%d per share)"
#define UI_STR_NBGL_SSKR_REVIEW_WORDS_VALUE_SINGLE "%d"

/*
 * The warning, which is the review's own last page rather than a screen after
 * it.
 *
 * It was a separate nbgl_useCaseChoice at first, and that was wrong: the
 * review already ends on a page whose button performs the act, so a second
 * screen asking the same question made two confirmations for one decision --
 * and the second carried nothing the user could not have read before pressing
 * the first. Two near-empty screens in a row is also how a warning stops being
 * read.
 *
 * This is the shape lib_nbgl uses for blind signing: the last page of the
 * review carries the alert icon, the sentence, and the button that acts. So
 * the warning is the last thing read before the reveal, which is what it was
 * for, and it costs no extra gesture.
 *
 * One string, and a short one, because the page has one text field and it is
 * drawn in the large font. INFO_BUTTON maps it to nbgl_contentCenter_t::title
 * (nbgl_page.c), and that struct's smaller registers -- `smallTitle`,
 * `description`, `subText` -- are left NULL by the component, so there is no
 * way to put a headline over a body here. Length is the only lever, and seven
 * lines of large text was a wall rather than a warning.
 *
 * What is kept is the risk, which is the sentence that is specific to what is
 * about to be drawn. The instruction that stood beside it -- "make sure no one
 * can see this screen" -- is what the risk already implies, and it was the
 * generic half.
 *
 * The reveal warning that is *not* here is the SSKR reconstitution one: that
 * path has no review to end, so its warning stays a screen of its own.
 */
/*
 * "rebuild", not "restore". This application uses three verbs and two of them
 * are right: a Phrase *restores* or *recovers* a wallet ("It would not restore
 * this Ledger", "a Phrase this Ledger cannot recover"), and Shares *rebuild* a
 * Phrase. Those are different operations and keeping different verbs for them
 * is worth more than a single word everywhere. This sentence and its Nano twin
 * were the two that used the wallet verb for the Shares operation.
 */
#define UI_STR_NBGL_SSKR_REVEAL_WARN \
    "Anyone who collects enough of these Shares can rebuild your Recovery Phrase."

/*
 * Closing the shares screen destroys them, and until now it did so in silence.
 *
 * review_done() called reset_globals() and went home, so the back arrow on the
 * last share was indistinguishable from the back arrow anywhere else -- except
 * that it cost the twenty-four words needed to generate the set again. This
 * says what the gesture does before it does it.
 *
 * The title carries the share count, so it is composed at display time into a
 * buffer that outlives the call, and src/nbgl/ui.c sizes that buffer on this
 * format.
 */
#define UI_STR_NBGL_SSKR_CLOSE_CONFIRM_TITLE "Close and erase all %d Shares?"
#define UI_STR_NBGL_SSKR_CLOSE_CONFIRM_DESC                            \
    "They cannot be shown again. Generating them a second time means " \
    "entering your Recovery Phrase again."
/*
 * The two buttons answer the title, which "Close" and "Back" did not.
 *
 * lib_nbgl's own confirmations are built this way -- "Reject transaction?" is
 * answered by "Yes, reject" and "Go back to transaction"
 * (lib_nbgl/src/nbgl_use_case.c) -- and the shape matters more here than
 * elsewhere, because the destructive answer is the one that looks like leaving.
 *
 * The title states the consequence rather than asking after the user's memory:
 * what the screen knows is that closing erases, and whether the sheets have
 * been copied is not something it can check.
 */
#define UI_STR_NBGL_SSKR_CLOSE_CONFIRM_YES "Yes, close"
#define UI_STR_NBGL_SSKR_CLOSE_CONFIRM_NO  "Go back to Shares"

/*
 * The step on the verdict flow that opens the share-count menu.
 *
 * It said "Generate / SSKR phrases", and neither half was true. Its callback is
 * set_sskr_descriptor_values() (src/bagl/ux_nano.c): it opens the menu that
 * asks how many shares to make, and nothing is generated until three screens
 * later, where UI_STR_BAGL_SSKR_REVIEW_CONFIRM_L1/L2 does say "Generate" and
 * does generate. Two steps in one journey promising the same act, only one of
 * them performing it.
 *
 * "phrases" was the other half: what this produces is shares, and this was the
 * last place in the application still calling them phrases.
 *
 * "SSKR Backup" and not "your Backup", which the review title uses: this step
 * is the first time the word SSKR appears anywhere in the Nano journey -- there
 * is no BAGL equivalent of the touch flow's explanatory screen -- and the term
 * has to be met before the sheets carry it.
 */
#define UI_STR_BAGL_GENERATE_SSKR_L1 "Set up"
#define UI_STR_BAGL_GENERATE_SSKR_L2 "SSKR Backup"
/*
 * The step that draws the rebuilt phrase.
 *
 * "Recover / BIP39 phrase" was the verb of the idle entry that started this
 * journey, three screens earlier, and repeating it here says the recovery is
 * still ahead when it has already happened -- the phrase is in words_buffer by
 * the time this step is reachable. What is left to do is show it.
 */
#define UI_STR_BAGL_RECOVER_BIP39_L1 "Show"
#define UI_STR_BAGL_RECOVER_BIP39_L2 "the Phrase"

/*
 * The Nano form of the review shown before a backup is generated, and of the
 * warning after it. Same two things the touch screens say, cut to a Nano's
 * lines.
 *
 * The two composed lines are the review. A `nn` step draws two lines in the
 * 116px box, which is exactly enough for the total and the scheme, and those
 * two numbers are the whole of what the touch review adds -- the per-share
 * figure that sits beside the total there does not fit here and is the less
 * useful half: what makes someone reconsider is 460, not 46.
 *
 * Both are formats. The word total comes from
 * bolos_ux_sskr_share_wordcount(), the same call the touch review makes, and
 * reaches three digits (ten shares of forty-six words under TARGET_NANOS),
 * which is one more than expand_worst_case() in tests/unit/tests/ui_strings.c
 * substitutes -- so they are measured by a case of their own there, built
 * from SSS_MAX_SHARE_COUNT rather than from placeholder nines.
 */
#define UI_STR_BAGL_SSKR_REVIEW_TITLE_L1 "Review"
#define UI_STR_BAGL_SSKR_REVIEW_TITLE_L2 "your Backup"
/*
 * The scheme line names the format, as the touch review's first row does, and
 * for the same reason: SSKR is what makes the sheets recoverable by anything
 * other than this application.
 *
 * "SSKR: any %d of %d" and not "SSKR threshold %d of %d", which is the wording
 * the touch rows add up to: the longer form is 124px against a 116px box at two
 * digits either side, so it would be clipped in silence. It is 96px.
 *
 * "any" is what makes this a threshold and not a subset. The form it replaces,
 * "SSKR %d of %d shares", reads as "three of the five" -- a selection out of a
 * set -- and sits four screens from the share header
 * UI_STR_BAGL_SSKR_SHARE_HEADER ("SSKR 2/3"), which really is an index over a
 * count. Two different claims in the same shape.
 *
 * Scheme first, then the cost, which is the order the touch review reads in.
 */
#define UI_STR_BAGL_SSKR_REVIEW_SHARES "SSKR: any %d of %d"
#define UI_STR_BAGL_SSKR_REVIEW_WORDS  "%d words to write"
/*
 * The step that ends the review and starts the generation.
 *
 * Not "Generate / SSKR phrases", which is what it first said: that is word for
 * word UI_STR_BAGL_GENERATE_SSKR_L1/L2, the step in the verdict flow that
 * opens the share-count menu -- so the same journey held two identical steps
 * doing different things, three screens apart. Whichever one a reader or a
 * test matched first would be the wrong one half the time.
 *
 * It says what the touch review's finish button says instead, which is also
 * what the idle entry that started the journey says. Those are the same act,
 * unlike the two above.
 */
#define UI_STR_BAGL_SSKR_REVIEW_CONFIRM_L1 "Generate"
#define UI_STR_BAGL_SSKR_REVIEW_CONFIRM_L2 "Backup Shares"

/*
 * The last step before the shares are drawn. A pbb title and one nn step of
 * body, the shape ux_invalid_step_2 already uses for a sentence that does not
 * fit two short title lines.
 */
#define UI_STR_BAGL_SSKR_REVEAL_WARN_L1 "Shares will"
#define UI_STR_BAGL_SSKR_REVEAL_WARN_L2 "be shown"
/*
 * The instruction is one sentence and it is the same one on both screens that
 * reveal something, so it is one macro -- the rule this header opens with.
 * "be shown" above is not merged with its Recover twin for the opposite
 * reason: it is the tail of two different sentences ("Shares will be shown",
 * "Phrase will be shown"), and sharing it would couple two sentences that are
 * not the same sentence.
 */
#define UI_STR_BAGL_SCREEN_PRIVACY_L1 "Make sure no one"
#define UI_STR_BAGL_SCREEN_PRIVACY_L2 "can see the screen"
/*
 * A third step, and the one the touch stack has had all along.
 *
 * The two lines above are an instruction; UI_STR_NBGL_SSKR_REVEAL_WARN is a
 * risk -- "anyone who collects enough of these Shares can restore your Recovery
 * Phrase" -- and the touch warning is that sentence and nothing else. So the
 * two stacks were not warning about the same thing: a Nano user was told to
 * shield the screen and never told that a subset of the sheets rebuilds the
 * seed, which is the property that makes where they are stored matter.
 *
 * "shares" stays lowercase here where the paper says "Shares", because this is
 * a running sentence rather than a label; "your Phrase" is the same noun the
 * verdict screens of this journey use.
 */
#define UI_STR_BAGL_SSKR_REVEAL_WARN_L5 "Enough shares can"
#define UI_STR_BAGL_SSKR_REVEAL_WARN_L6 "rebuild your Phrase"

/*
 * What the shares flow says before the step that leaves it.
 *
 * On the Nano the only way out of the share display is Quit, and quitting runs
 * clean_exit(), which memzeroes sskr_words_buffer. So leaving destroys the
 * shares here exactly as closing the review does on the touch screens, and
 * said just as little about it.
 *
 * It is a step rather than a confirmation because the flow it sits in is a
 * FLOW_LOOP: reading the question and pressing on reaches Quit, pressing back
 * returns to the shares. There is no branch to add, and adding one would have
 * given this flow a second way out to keep track of.
 */
#define UI_STR_BAGL_SSKR_CLOSE_CONFIRM_L1 "Written down"
#define UI_STR_BAGL_SSKR_CLOSE_CONFIRM_L2 "all %d Shares?"

/*
 * The Nano form of the warning in front of a rebuilt recovery phrase.
 *
 * Three steps rather than two, and the third is the one that matters: this
 * path shows the phrase the entered shares rebuild whether or not it matches
 * the seed this Ledger holds -- ux_sskr_nomatch_flow reaches the same
 * revealing step ux_sskr_match_flow does. That is what makes recovering onto a
 * spare or replacement device work, and it is what nothing on either stack
 * said out loud. See UI_STR_NBGL_RECOVER_WARN_DESC for the whole argument.
 */
#define UI_STR_BAGL_RECOVER_WARN_L1 "Phrase will"
#define UI_STR_BAGL_RECOVER_WARN_L2 "be shown"
/*
 * "Not necessarily this / Ledger's phrase" rather than "...the phrase of this
 * Ledger", which said the same thing in 110px of a 116px box. Six pixels is
 * under one character: it is not clipped, but it is the state immediately
 * before clipping and indistinguishable from it on a screenshot. This pair
 * leaves 16px and 36px.
 */
#define UI_STR_BAGL_RECOVER_WARN_L5 "Not necessarily this"
#define UI_STR_BAGL_RECOVER_WARN_L6 "Ledger's Phrase"

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
#define UI_STR_NBGL_SSKR_SHARE_HEADER "SSKR Share %d of %d"
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
#define UI_STR_NBGL_SSKR_NUMSHARES_TITLE       "Enter number of SSKR Shares\nto generate (1 - 16)"
#define UI_STR_NBGL_SSKR_NUMSHARES_RANGE_ERROR "Number of SSKR Shares must be between 1 and 16"
#define UI_STR_BAGL_SSKR_NUMSHARES_TITLE_L1    "Select number"
#define UI_STR_BAGL_SSKR_NUMSHARES_TITLE_L2    "of SSKR Shares"

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
    "Threshold value cannot be greater than number of Shares"
/*
 * The one refusal in this application that told the user nothing to do.
 *
 * "1-of-m shares where m > 1 is not supported" states the unsupported case in
 * the notation of the scheme and leaves open which of the two numbers to
 * change -- the count on the previous screen, or the threshold on this one.
 * What is true is narrower and actionable: a threshold of 1 means any single
 * Share rebuilds the phrase, which only makes sense when there is one Share.
 * The keypad's own title already announces a range starting at 2; this now
 * agrees with it instead of describing a mathematical form.
 */
#define UI_STR_NBGL_SSKR_THRESHOLD_ONE_OF_M_ERROR \
    "A threshold of 1 works only with a single Share.\nEnter 2 or more."
/*
 * What a threshold is, read immediately before the menu that asks for one.
 *
 * That menu says "Select / threshold" and nothing on the Nano defines the
 * word -- the same gap the touch stack had, and it was fixed there and not
 * here. The review further on shows "any k of n", which demonstrates the
 * relationship but arrives after the choice has been made.
 */
#define UI_STR_BAGL_SSKR_THRESHOLD_CONCEPT_L1       "The threshold is how"
#define UI_STR_BAGL_SSKR_THRESHOLD_CONCEPT_L2       "many Shares rebuild"
#define UI_STR_BAGL_SSKR_THRESHOLD_CONCEPT_L3       "your Phrase."
#define UI_STR_BAGL_SSKR_THRESHOLD_CONCEPT_L1_NANOS "Threshold: Shares"
#define UI_STR_BAGL_SSKR_THRESHOLD_CONCEPT_L2_NANOS "needed to rebuild."

#define UI_STR_BAGL_SSKR_THRESHOLD_TITLE_L1 "Select"
#define UI_STR_BAGL_SSKR_THRESHOLD_TITLE_L2 "threshold"
/*
 * The same refusal on two buttons, cut across the two steps it already had.
 * Between them the old pair named the case ("1-of-m shares / where m > 1") and
 * then its verdict ("Not / Supported"), and never the way out. Both boxes are
 * the tight 87px one, which is what decides the cut.
 */
#define UI_STR_BAGL_SSKR_ONE_OF_M_WARN_L1 "A threshold of 1"
#define UI_STR_BAGL_SSKR_ONE_OF_M_WARN_L2 "needs 1 Share"
#define UI_STR_BAGL_SSKR_ONE_OF_M_NOT_L1  "Enter 2"
#define UI_STR_BAGL_SSKR_ONE_OF_M_NOT_L2  "or more"

/*
 * BIP85 -- NBGL only. No BAGL target has a BIP85 screen at all.
 */
#define UI_STR_NBGL_BIP85_APP_BIP39      "BIP39"
#define UI_STR_NBGL_BIP85_APP_PWD_BASE64 "Password (Base64)"
#define UI_STR_NBGL_BIP85_APP_PWD_BASE85 "Password (Base85)"
/*
 * "PIN", and not "Dice": the entry names what the user came for.
 *
 * The derivation underneath is BIP-85's DICE application with a ten-sided
 * die, and the path on the review says so -- `m/83696968'/89101'/10'/6'/0'`,
 * with 89101 being DICE and nothing else. That is what makes this PIN
 * reproducible by any other BIP-85 implementation, and it is why the screen
 * naming it "PIN" is not a private extension: the label is the use, the path
 * is the standard, and both are on screen before anything is derived.
 */
#define UI_STR_NBGL_BIP85_APP_PIN "PIN"
/*
 * "BIP85" stays: a screen is also where a term is learned, and this one names
 * a standard the user will need in order to derive the same secret anywhere
 * else. "application" goes: on a Ledger that word means the app itself, which
 * the home screen two screens earlier uses it for. It is a false friend rather
 * than jargon, and the only word here that could be read as something else.
 *
 * One line, and no placed break, unlike every other title in this file: this
 * one is a header title now (nbgl_useCaseGenericConfiguration()), drawn in
 * the bar beside the back arrow rather than as a text area over the buttons.
 * That bar is one line high on all three devices and clips rather than wraps,
 * so the question is short enough to fit the narrowest of them -- checked
 * under Speculos on apex_p, which is where it would be cut.
 */
#define UI_STR_NBGL_BIP85_SELECT_APP_TITLE "Which BIP85 secret?"

#define UI_STR_NBGL_BIP85_BIP39_HEADER  "BIP39 Phrase (Index #%d)"
#define UI_STR_NBGL_BIP85_BASE64_HEADER "Base64 Password (Index #%d)"
#define UI_STR_NBGL_BIP85_BASE85_HEADER "Base85 Password (Index #%d)"
#define UI_STR_NBGL_BIP85_PIN_HEADER    "PIN (Index #%d)"

/*
 * How long a PIN, asked with three buttons rather than with the keypad the
 * password length uses.
 *
 * The keypad exists because a password length is any number in a range of
 * sixty-odd values. A PIN is 4, 6 or 8 digits here -- three values, which is
 * a choice and not an entry -- and a keypad for it would put a number pad in
 * front of someone about to be shown a number, with a range error waiting
 * behind every other value they could type.
 *
 * The unit is named on every button, as the phrase-length screen names words:
 * "4" alone on a screen reached from a menu of secrets does not say four of
 * what.
 */
#define UI_STR_NBGL_BIP85_PIN_LENGTH_TITLE "How many digits\nin the PIN?"
#define UI_STR_NBGL_BIP85_PIN_DIGITS_4     "4 digits"
#define UI_STR_NBGL_BIP85_PIN_DIGITS_6     "6 digits"
#define UI_STR_NBGL_BIP85_PIN_DIGITS_8     "8 digits"

/*
 * The label above a derived secret, once the path has been folded into it.
 *
 * The path is on the review before this screen as well, and that is not a
 * duplicate: the review is where the user decides, this is where the user
 * copies onto paper -- and a result without its path cannot be reproduced
 * anywhere else, which is the whole reason to derive a secret rather than
 * store one.
 *
 * It is part of the label rather than a tag/value row of its own because a
 * pair is never split across review pages and two pairs are: measured under
 * Speculos, a 24-word phrase pushed the path and the words onto separate
 * pages, which is the defect this was meant to fix rather than move.
 */
#define UI_STR_NBGL_BIP85_RESULT_LABEL_SEPARATOR "\n"

/*
 * The review shown before a BIP-85 secret is derived, and the path it carries.
 *
 * The path is the reason this screen exists. A BIP-85 result is worth deriving
 * rather than storing only because it can be derived again -- and derived
 * again somewhere else, from the same seed, by software that is not this
 * application. That needs `m/83696968'/39'/0'/24'/42'`, which no screen here
 * has ever shown. Without it the user is holding a password they can only
 * regenerate by remembering which three screens they tapped, in an application
 * whose three parameter screens do not appear together anywhere.
 *
 * So the path is displayed, and it is displayed as the derivation builds it:
 * bip85_app_path_format() (src/nbgl/bip85_app.c) calls the formatter that sits
 * beside the matching derivation, with the arguments that derivation will be
 * called with. A path assembled from what the screens collected would be a
 * second definition of it, and a path wrong in one component still derives a
 * perfectly good secret -- the wrong one, silently.
 *
 * The three applications name themselves with the same strings the selection
 * screen used, so the review says back what was chosen rather than a synonym.
 */
#define UI_STR_NBGL_BIP85_REVIEW_FINISH "Derive this secret"
/*
 * Four rows, and the path shares the screen with the three parameters that
 * produced it. That is the whole point of the screen: someone copying this
 * down has to read the parameters and the path together, and a path on a page
 * of its own is a path read separately from what it means.
 *
 * The specification draws five rows, with Language on its own. Four is what
 * fits: a review page is paginated by height, and on Flex and Apex a fifth row
 * pushes the path onto a second page -- measured under Speculos at the worst
 * case this application can produce, a 24-word phrase at index 9,999,999.
 * (NB_MAX_DISPLAYED_PAIRS_IN_REVIEW, which reads like a cap of four, is a
 * documentation constant: it appears nowhere in lib_nbgl's logic.)
 *
 * So Language joins the application it qualifies rather than being dropped:
 * "BIP39 (English)". It is a real path component -- the `0'` after `39'` --
 * and the reader reproducing this elsewhere needs the language index rather
 * than having to infer it. The password applications have no language
 * component in their paths and get no parenthesis.
 */
#define UI_STR_NBGL_BIP85_REVIEW_ITEM_APP    "Application"
#define UI_STR_NBGL_BIP85_REVIEW_ITEM_LENGTH "Length"
#define UI_STR_NBGL_BIP85_REVIEW_ITEM_INDEX  "Index"
#define UI_STR_NBGL_BIP85_REVIEW_ITEM_PATH   "Path"
/*
 * Appended to the application's own name, rather than a format taking both.
 * String conversions are refused anywhere under src/ -- they are how a secret
 * leaks through a trace statement -- and the rule is enforced by grep rather
 * than by reading, so it holds for a screen's format string as much as for a
 * PRINTF. Every composition here is built by appending instead.
 */
#define UI_STR_NBGL_BIP85_REVIEW_APP_LANGUAGE      " (English)"
#define UI_STR_NBGL_BIP85_REVIEW_INDEX_VALUE       "%d"
#define UI_STR_NBGL_BIP85_REVIEW_LENGTH_WORDS      "%d words"
#define UI_STR_NBGL_BIP85_REVIEW_LENGTH_CHARACTERS "%d characters"
/*
 * A third unit, for the same reason the first two are named: the length row
 * carries "8" for a PIN of eight digits and "8" would also be a password of
 * eight characters, which this application does not offer and the reader has
 * no way of knowing. Digits are also rolls -- one ten-sided roll each -- and
 * "digits" is the word for what is on the screen the user is about to copy.
 */
#define UI_STR_NBGL_BIP85_REVIEW_LENGTH_DIGITS "%d digits"

/*
 * The same page, for the derivation flow. See UI_STR_NBGL_SSKR_REVEAL_WARN
 * above for why the warning is the review's last page and not a screen after
 * it.
 *
 * Three forms, because the four applications do not share a danger.
 *
 * The risk is the same everywhere and is the first sentence: a derived secret
 * is usable by whoever reads it, and this screen is the last one before it is
 * drawn.
 *
 * What differs is what the user might mistake it for. A BIP39 derivation puts
 * twenty-four English words on screen, and those words *are* a valid recovery
 * phrase -- for a wallet that is not this one. Someone who writes them down as
 * a backup of this device has backed up nothing, and would find out on the day
 * they needed it. So that screen names what it is not, in the formula Ledger's
 * own applications use for the same distinction.
 *
 * A Base64 or Base85 password carries no such confusion: nobody mistakes
 * "dKLoepugzdVJvdL56ogNV" for a recovery phrase, and telling them it is not one
 * is a sentence about something they were never going to think. It said exactly
 * that until this was split -- one string served all three, and two of them
 * produce passwords.
 *
 * display_bip85_review() (src/nbgl/ui.c) already switches on
 * bip85_type_get() twice, for the Length row's unit and for the Application
 * row's language; this is the third place the same distinction is drawn.
 */
#define UI_STR_NBGL_BIP85_REVEAL_WARN_BIP39 \
    "Anyone who sees this secret can use it. It is not the Phrase on this Ledger."

#define UI_STR_NBGL_BIP85_REVEAL_WARN_PWD "Anyone who sees this secret can use it."

/*
 * The fourth arm, and it names the thing rather than calling it a secret. A
 * PIN is read off a screen and typed into something else, so what is at stake
 * is not abstract: whoever reads these digits opens whatever they open.
 */
#define UI_STR_NBGL_BIP85_REVEAL_WARN_PIN "Anyone who sees this PIN can use it."

/*
 * Shown when a PIN could not be derived, which no parameter this flow accepts
 * produces: the DRNG stream would have to run out under rejection sampling
 * for a request of four to eight rolls from a ten-sided die.
 *
 * It exists because the alternative to saying so is drawing what was
 * produced, and a PIN that came back one digit short looks exactly like a
 * PIN. Nothing is displayed, everything is erased, and this is what the user
 * gets instead -- see bip85_app_pin_gen() (src/nbgl/bip85_app.c).
 */
#define UI_STR_NBGL_BIP85_PIN_DERIVE_ERROR "PIN could not be derived"

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
