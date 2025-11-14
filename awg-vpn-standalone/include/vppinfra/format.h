/*
 * Minimal VPP format/logging compatibility shim
 */

#ifndef __included_vppinfra_format_h__
#define __included_vppinfra_format_h__

#include <vppinfra/types.h>
#include <vppinfra/vec.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

/* Format string to u8 vector - simplified version */
typedef u8 *(*format_function_t) (u8 *s, va_list *args);

/* Basic string operations on u8 vectors */
static_always_inline u8 *
format (u8 *s, const char *fmt, ...)
{
  va_list args;
  va_start (args, fmt);

  /* Calculate required size */
  va_list args_copy;
  va_copy (args_copy, args);
  int needed = vsnprintf (NULL, 0, fmt, args_copy);
  va_end (args_copy);

  if (needed < 0)
    {
      va_end (args);
      return s;
    }

  /* Resize vector */
  uword old_len = vec_len (s);
  uword new_len = old_len + needed;
  vec_validate (s, new_len);

  /* Format into vector */
  vsnprintf ((char *) (s + old_len), needed + 1, fmt, args);
  va_end (args);

  return s;
}

/* Note: snprintf is from stdio.h, no need to redeclare */

/* Logging macros - print to stderr with prefix */
#define clib_warning(fmt, ...) \
  fprintf(stderr, "[WARN] " fmt "\n", ##__VA_ARGS__)

#define clib_error(fmt, ...) \
  fprintf(stderr, "[ERROR] " fmt "\n", ##__VA_ARGS__)

#define clib_info(fmt, ...) \
  fprintf(stderr, "[INFO] " fmt "\n", ##__VA_ARGS__)

#ifdef DEBUG
#define clib_debug(fmt, ...) \
  fprintf(stderr, "[DEBUG] " fmt "\n", ##__VA_ARGS__)
#else
#define clib_debug(fmt, ...) do {} while (0)
#endif

/* va_list variants */
#define va_format(s, fmt, va) \
  ({ \
    va_list _args_copy; \
    va_copy(_args_copy, *(va)); \
    int _needed = vsnprintf(NULL, 0, fmt, _args_copy); \
    va_end(_args_copy); \
    if (_needed >= 0) { \
      uword _old_len = vec_len(s); \
      vec_validate(s, _old_len + _needed); \
      vsnprintf((char *)(s + _old_len), _needed + 1, fmt, *(va)); \
    } \
    s; \
  })

#endif /* __included_vppinfra_format_h__ */
