// Base32 implementation
//
// Copyright 2010 Google Inc.
// Author: Markus Gutschke
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Encode and decode from base32 encoding using the following alphabet:
//   ABCDEFGHIJKLMNOPQRSTUVWXYZ234567
// This alphabet is documented in RFC 4668/3548
//
// We allow white-space and hyphens, but all other characters are considered
// invalid.
//
// All functions return the number of output bytes or -1 on error. If the
// output buffer is too small, the result will silently be truncated.

#ifndef _BASE32_H_
#define _BASE32_H_

#include <stdint.h>
#include <stddef.h>

size_t base32_decode(const char *encoded, size_t length, uint8_t *result, size_t bufSize);
size_t base32_encode(const uint8_t *data, size_t length, char *result, size_t bufSize);

// number of characters required, including null terminator
size_t base32_encode_length(size_t rawLength);

// number of bytes, rounded down (truncates extra bits)
size_t base32_decode_floor(size_t base32Length);

#endif /* _BASE32_H_ */
