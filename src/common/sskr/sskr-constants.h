//
//  sskr-constants.h
//
//  Copyright © 2020-2026 by Blockchain Commons, LLC
//  Licensed under the "BSD-2-Clause Plus Patent License"
//

#ifndef SSKR_CONSTANTS_H
#define SSKR_CONSTANTS_H

#include "sss-constants.h"

#define SSKR_METADATA_LENGTH_BYTES 5
#define SSKR_MIN_STRENGTH_BYTES    16
#define SSKR_MAX_STRENGTH_BYTES    32
// Upstream bc-sskr allows 16 here, and BCR-2020-011 encodes group-count in 4
// bits, so 16 is what a shard set may name. 1 is what this port holds: the
// arrays it sizes -- groups[], gx[], gy[] in sskr_combine_shards_internal(),
// group_shares[] in sskr_generate_shards_internal() -- are fixed-size on the
// stack rather than allocated as upstream does.
//
// The consequence falls on recovery, not on generation: both interfaces
// generate a single group, but a multi-group backup made elsewhere cannot be
// recombined here, including the worked example of BCR-2020-011 itself. It is
// refused with SSKR_ERROR_INVALID_SHARD_SET (shards of different groups) or
// SSKR_ERROR_NOT_ENOUGH_GROUPS (the shards of one group of several).
#define SSKR_MAX_GROUP_COUNT             1
#define SSKR_MIN_SERIALIZED_LENGTH_BYTES (SSKR_METADATA_LENGTH_BYTES + SSKR_MIN_STRENGTH_BYTES)

// The CRC-32 that BCR-2020-011 appends to every serialized share. It was
// sizeof(uint32_t) at the one place that wrote it and a bare 4 at the two that
// reserved room for it; naming it is what lets the word count a review
// announces be written as the same sum the generator forms.
#define SSKR_CRC32_LENGTH_BYTES 4

// A CBOR byte string carries its length in the low five bits of its initial
// byte up to 23, and needs one following byte from 24 on (RFC 8949 3.1). That
// boundary decides whether a serialized share's header is four bytes or five,
// which is a ByteWord either way -- so it is part of how long a share reads,
// not only of how it is encoded.
#define SSKR_CBOR_SHORT_FORM_MAX_LENGTH 23
/*
 * The two forms the CBOR byte-string header takes, in bytes. RFC 8949 puts a
 * length of up to 23 in the initial byte and needs one following byte from 24
 * on. bolos_ux_sskr_cbor_header_length() returns one or the other, and the
 * long form is what sizes the wire buffer.
 */
#define SSKR_CBOR_SHORT_FORM_HEADER_LENGTH 4
#define SSKR_CBOR_LONG_FORM_HEADER_LENGTH  5

#define SSKR_ERROR_NOT_ENOUGH_SERIALIZED_BYTES (-1)
#define SSKR_ERROR_SECRET_TOO_SHORT            (-2)
#define SSKR_ERROR_INVALID_GROUP_THRESHOLD     (-3)
#define SSKR_ERROR_INVALID_SINGLETON_MEMBER    (-4)
#define SSKR_ERROR_INSUFFICIENT_SPACE          (-5)
#define SSKR_ERROR_INVALID_RESERVED_BITS       (-6)
#define SSKR_ERROR_SECRET_LENGTH_NOT_EVEN      (-7)
#define SSKR_ERROR_INVALID_SHARD_SET           (-8)
#define SSKR_ERROR_EMPTY_SHARD_SET             (-9)
#define SSKR_ERROR_DUPLICATE_MEMBER_INDEX      (-10)
#define SSKR_ERROR_NOT_ENOUGH_MEMBER_SHARDS    (-11)
#define SSKR_ERROR_INVALID_MEMBER_THRESHOLD    (-12)
#define SSKR_ERROR_INVALID_PADDING             (-13)
#define SSKR_ERROR_NOT_ENOUGH_GROUPS           (-14)
#define SSKR_ERROR_INVALID_SHARD_BUFFER        (-15)
#define SSKR_ERROR_SECRET_TOO_LONG             (-16)
#define SSKR_ERROR_INVALID_GROUP_LENGTH        (-17)
#define SSKR_ERROR_INVALID_GROUP_COUNT         (-18)
#define SSKR_ERROR_RNG_FAILURE                 (-19)

#endif /* SSKR_CONSTANTS_H */
