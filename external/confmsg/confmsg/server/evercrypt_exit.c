// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include <stdlib.h>

// TODO remove
void exit(int status) {
  (void)status;
  abort();
}