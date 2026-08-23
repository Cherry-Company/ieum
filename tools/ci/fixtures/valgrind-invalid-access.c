// SPDX-FileCopyrightText: (C) 2026 Ieum contributors
// SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception

#include <stdlib.h>

int main(void)
{
  volatile unsigned char *memory = malloc(1);
  if (memory == NULL) {
    return EXIT_FAILURE;
  }

  memory[0] = 0;
  free((void *)memory);

  // The workflow must prove that this intentional invalid write fails the
  // composite Valgrind action and, in turn, the protected CI result.
  memory[0] = 1;
  return EXIT_SUCCESS;
}
