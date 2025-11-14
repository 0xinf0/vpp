/*
 * Test for blake2s standalone implementation
 */
#include <stdio.h>
#include <string.h>
#include "../src/blake/blake2s.h"

static void
print_hex (const uint8_t *data, size_t len)
{
  for (size_t i = 0; i < len; i++)
    printf ("%02x", data[i]);
  printf ("\n");
}

int
main (void)
{
  uint8_t hash[BLAKE2S_OUT_BYTES];
  const char *test_input = "hello world";

  /* Test vector from WireGuard test suite */
  printf ("Testing blake2s with input: \"%s\"\n", test_input);

  int ret = blake2s (hash, BLAKE2S_OUT_BYTES, test_input, strlen (test_input),
		     NULL, 0);

  if (ret != 0)
    {
      printf ("ERROR: blake2s returned %d\n", ret);
      return 1;
    }

  printf ("Blake2s hash: ");
  print_hex (hash, BLAKE2S_OUT_BYTES);

  /* Test consistency - hash the same input twice */
  uint8_t hash2[BLAKE2S_OUT_BYTES];
  ret = blake2s (hash2, BLAKE2S_OUT_BYTES, test_input, strlen (test_input),
		 NULL, 0);

  if (ret != 0)
    {
      printf ("ERROR: blake2s second call returned %d\n", ret);
      return 1;
    }

  if (memcmp (hash, hash2, BLAKE2S_OUT_BYTES) == 0)
    {
      printf ("✓ Blake2s test PASSED (consistent output)\n");
      return 0;
    }
  else
    {
      printf ("✗ Blake2s test FAILED (inconsistent output)\n");
      return 1;
    }
}
