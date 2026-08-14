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

unsigned const char BASE64_TABLE[] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
    'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
    'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/'};

// The RFC 1924 alphabet, in RFC 1924 order.
//
// BIP-85 writes only "Base85 encode" and never names the variant, which is an
// ambiguity in the specification rather than a detail left to the
// implementation: Ascii85 (btoa), RFC 1924 and Z85 all encode 85 symbols and
// all three use different alphabets. Only the password the specification
// publishes for m/83696968'/707785'/12'/0' settles it. Encoding that vector's
// entropy with each alphabet gives:
//
//   RFC 1924        _s`{TW89)i4`   <- what the specification publishes
//   Ascii85 (btoa)  pWqr>A)*eM%q
//   Z85             {S}@tw89!I4}
//
// So this table is not interchangeable with another 85-symbol alphabet, and
// reordering it -- or "modernising" it to Z85 -- would silently produce
// passwords no other BIP-85 implementation derives. tests/unit/tests/base85.c
// pins the full 80-character encoding of that same vector.
unsigned const char BASE85_TABLE[] = {
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E',
    'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T',
    'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i',
    'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x',
    'y', 'z', '!', '#', '$', '%', '&', '(', ')', '*', '+', '-', ';', '<', '=',
    '>', '?', '@', '^', '_', '`', '{', '|', '}', '~'};
