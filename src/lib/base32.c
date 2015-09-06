// Base32 implementation
//
// Copyright 2010 Google Inc.
// Author: Markus Gutschke
//
// Modified 2015 Jeremie Miller
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

#include <string.h>

#include "telehash.h"

size_t base32_decode(const char *encoded, size_t length, uint8_t *result, size_t bufSize) {
  int buffer = 0;
  size_t bitsLeft = 0;
  size_t count = 0;
  const char *ptr = encoded;
  if(!encoded || !result  || bufSize <= 0) return 0;
  if(!length) length = strlen(encoded);
  for (; (size_t)(ptr-encoded) < length && count < bufSize; ++ptr) {
    uint8_t ch = (uint8_t)*ptr;
    if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' || ch == '-') {
      continue;
    }
    buffer <<= 5;

    // Deal with commonly mistyped characters
    if (ch == '0') {
      ch = 'O';
    } else if (ch == '1') {
      ch = 'L';
    } else if (ch == '8') {
      ch = 'B';
    }

    // Look up one base32 digit
    if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z')) {
      ch = (ch & 0x1F) - 1;
    } else if (ch >= '2' && ch <= '7') {
      ch -= '2' - 26;
    } else {
      return 0;
    }

    buffer |= ch;
    bitsLeft += 5;
    if (bitsLeft >= 8) {
      result[count++] = buffer >> (bitsLeft - 8);
      bitsLeft -= 8;
    }
  }
  if (count < bufSize) {
    result[count] = '\000';
  }
  return count;
}


size_t base32_encode(const uint8_t *data, size_t length, char *result, size_t bufSize) {
  if (!data || !result || !bufSize || !length) return 0;
  size_t count = 0;
  if (length > 0) {
    int buffer = data[0];
    size_t next = 1;
    int bitsLeft = 8;
    while (count < bufSize && (bitsLeft > 0 || next < length)) {
      if (bitsLeft < 5) {
        if (next < length) {
          buffer <<= 8;
          buffer |= data[next++] & 0xFF;
          bitsLeft += 8;
        } else {
          int pad = 5 - bitsLeft;
          buffer <<= pad;
          bitsLeft += pad;
        }
      }
      int index = 0x1F & (buffer >> (bitsLeft - 5));
      bitsLeft -= 5;
      result[count++] = "abcdefghijklmnopqrstuvwxyz234567"[index];
    }
  }
  if (count < bufSize) {
    result[count] = '\000';
  }
  return count;
}

size_t base32_encode_length(size_t rawLength)
{
  return ((rawLength * 8) / 5) + ((rawLength % 5) != 0) + 1;
}

size_t base32_decode_floor(size_t base32Length)
{
  return ((base32Length * 5) / 8);
}

