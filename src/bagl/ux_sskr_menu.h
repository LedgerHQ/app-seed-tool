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

// The two SSKR menus of the BAGL stack -- "how many shares" and "what
// threshold" -- are both a list of consecutive numbers starting at 1, drawn
// from one table of labels. This is that table's bound and its lookup, away
// from the UX_STEP_MENULIST plumbing that reads it.

// How many labels the table holds, and so the largest share count the
// share-count menu will offer. Target-dependent: fewer entries on Nano S,
// whose share buffer is sized for a smaller set.
unsigned int sskr_descriptor_count(void);

// Label for entry `idx` of a menu offering `count` consecutive values from
// "1", or NULL when `idx` is past the last one -- which is what a
// UX_STEP_MENULIST getter returns to end the list.
//
// Callers must pass a `count` no larger than sskr_descriptor_count(). The
// share-count menu passes exactly that; the threshold menu passes the share
// count the user just chose from it, which is therefore bounded by it too.
const char *sskr_descriptor_label(unsigned int idx, unsigned int count);
