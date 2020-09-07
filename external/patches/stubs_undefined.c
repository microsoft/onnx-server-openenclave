// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include <stdio.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>

#include <mbedtls/pk.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/x509_crl.h>

// The following functions are defined in Open Enclave's standard
// library headers but lack a corresponding implementation.

// The following functions are used by spdlog.

int isatty(int fd)
{
  (void)fd;
  return 0;
}

void syslog(int priority, const char *message, ...)
{
  (void)priority;
  (void)message;
  puts("FATAL: syslog() stub called, message:");
  puts(message);
  abort();
}

void openlog(const char *ident, int opt, int facility)
{
  (void)ident;
  (void)opt;
  (void)facility;
  puts("FATAL: openlog() stub called");
  abort();
}

void closelog(void)
{
  puts("FATAL: closelog() stub called");
  abort();
}

int fcntl(int fd, int cmd, ...)
{
  (void)fd;
  (void)cmd;
  puts("FATAL: fcntl() stub called");
  abort();
}

int mbedtls_x509_crt_parse_file( mbedtls_x509_crt *chain, const char *path )
{
  (void)(chain);
  (void)(path);
  puts("FATAL: mbedtls_x509_crt_parse_file() stub called, path:");
  puts(path);
  abort();
}

int mbedtls_x509_crt_parse_path( mbedtls_x509_crt *chain, const char *path )
{
  (void)(chain);
  (void)(path);
  puts("FATAL: mbedtls_x509_crt_parse_path() stub called, path:");
  puts(path);
  abort();
}

int mbedtls_pk_parse_keyfile( mbedtls_pk_context *ctx, const char *path, const char *password )
{
  (void)ctx;
  (void)path;
  (void)password;
  puts("FATAL: mbedtls_pk_parse_keyfile() stub called, path:");
  puts(path);
  abort();
}

int mbedtls_x509_crl_parse_file( mbedtls_x509_crl *chain, const char *path )
{
  (void)chain;
  (void)path;
  puts("FATAL: mbedtls_x509_crl_parse_file() stub called, path:");
  puts(path);
  abort();
}
