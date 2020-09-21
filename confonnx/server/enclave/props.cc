// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include <openenclave/enclave.h>

// Default parameters, can be overridden during signing.
OE_SET_ENCLAVE_SGX(
    1,           /* ProductID */
    1,           /* SecurityVersion */
    true,        /* AllowDebug */
    609600 / 10, /* HeapPageCount */
    131072 / 10, /* StackPageCount */
    8);          /* TCSCount */
