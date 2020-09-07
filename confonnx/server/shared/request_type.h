// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <stdint.h>

enum class RequestType : uint8_t {
  ProvisionModelKey = 0,
  Score = 1
};
