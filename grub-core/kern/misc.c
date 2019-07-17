/* misc.c - definitions of misc functions */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 1999,2000,2001,2002,2003,2004,2005,2006,2007,2008,2009,2010  Free Software Foundation, Inc.
 *
 *  GRUB is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  GRUB is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GRUB.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <grub/misc.h>
#include <grub/err.h>
#include <grub/mm.h>
#include <stdarg.h>
#include <grub/term.h>
#include <grub/env.h>
#include <grub/i18n.h>
#if defined(GRUB_MACHINE_EFI)
#include <grub/efi/api.h>
#include <grub/efi/efi.h>
#endif

union printf_arg
{
  /* Yes, type is also part of union as the moment we fill the value
     we don't need to store its type anymore (when we'll need it, we'll
     have format spec again. So save some space.  */
  enum
    {
      INT8, INT16, INT, LONG, LONGLONG,
      UINT8, UINT16, UNSIGNED_INT, UNSIGNED_LONG, UNSIGNED_LONGLONG
    } type;
  long long ll;
};

struct printf_args
{
  union printf_arg prealloc[32];
  union printf_arg *ptr;
  grub_size_t count;
};

static void
parse_printf_args (const char *fmt0, struct printf_args *args,
		   va_list args_in);
static int
grub_vsnprintf_real (char *str, grub_size_t max_len, const char *fmt0,
		     struct printf_args *args);

static void
free_printf_args (struct printf_args *args)
{
  if (args->ptr != args->prealloc)
    grub_free (args->ptr);
}

static int
grub_iswordseparator (int c)
{
  return (grub_isspace (c) || c == ',' || c == ';' || c == '|' || c == '&');
}

/* grub_gettext_dummy is not translating anything.  */
static const char *
grub_gettext_dummy (const char *s)
{
  return s;
}

const char* (*grub_gettext) (const char *s) = grub_gettext_dummy;

void *
grub_memmove (void *dest, const void *src, grub_size_t n)
{
  char *d = (char *) dest;
  const char *s = (const char *) src;

  if (d < s)
    while (n--)
      *d++ = *s++;
  else
    {
      d += n;
      s += n;

      while (n--)
	*--d = *--s;
    }

  return dest;
}

char *
grub_strcpy (char *dest, const char *src)
{
  char *p = dest;

  while ((*p++ = *src++) != '\0')
    ;

  return dest;
}

int
grub_printf (const char *fmt, ...)
{
  va_list ap;
  int ret;

  va_start (ap, fmt);
  ret = grub_vprintf (fmt, ap);
  va_end (ap);

  return ret;
}

int
grub_printf_ (const char *fmt, ...)
{
  va_list ap;
  int ret;

  va_start (ap, fmt);
  ret = grub_vprintf (_(fmt), ap);
  va_end (ap);

  return ret;
}

int
grub_puts_ (const char *s)
{
  return grub_puts (_(s));
}

#if defined (__APPLE__) && ! defined (GRUB_UTIL)
int
grub_err_printf (const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start (ap, fmt);
	ret = grub_vprintf (fmt, ap);
	va_end (ap);

	return ret;
}
#endif

#if ! defined (__APPLE__) && ! defined (GRUB_UTIL)
int grub_err_printf (const char *fmt, ...)
__attribute__ ((alias("grub_printf")));
#endif

int
grub_debug_enabled (const char * condition)
{
  const char *debug;
  char *negcond;
  int negated = 0;

  debug = grub_env_get ("debug");
  if (!debug)
    return 0;

  negcond = grub_zalloc (grub_strlen (condition) + 2);
  if (negcond)
    {
      grub_strcpy (negcond, "-");
      grub_strcpy (negcond+1, condition);
      negated = grub_strword (debug, negcond);
      grub_free (negcond);
    }

  if (!negated &&
      (grub_strword (debug, "all") || grub_strword (debug, condition)))
    return 1;

  return 0;
}

void
grub_real_dprintf (const char *file, const int line, const char *condition,
		   const char *fmt, ...)
{
  va_list args;

  if (grub_debug_enabled (condition))
    {
      grub_printf ("%s:%d: ", file, line);
      va_start (args, fmt);
      grub_vprintf (fmt, args);
      va_end (args);
      grub_refresh ();
    }
}

void
grub_qdprintf (const char *condition, const char *fmt, ...)
{
  va_list args;

  if (!grub_debug_enabled(condition))
    return;

  va_start (args, fmt);
  grub_vprintf (fmt, args);
  va_end (args);
  grub_refresh ();
}

#define PREALLOC_SIZE 255

int
grub_vprintf (const char *fmt, va_list ap)
{
  grub_size_t s;
  static char buf[PREALLOC_SIZE + 1];
  char *curbuf = buf;
  struct printf_args args;

  parse_printf_args (fmt, &args, ap);

  s = grub_vsnprintf_real (buf, PREALLOC_SIZE, fmt, &args);
  if (s > PREALLOC_SIZE)
    {
      curbuf = grub_malloc (s + 1);
      if (!curbuf)
	{
	  grub_errno = GRUB_ERR_NONE;
	  buf[PREALLOC_SIZE - 3] = '.';
	  buf[PREALLOC_SIZE - 2] = '.';
	  buf[PREALLOC_SIZE - 1] = '.';
	  buf[PREALLOC_SIZE] = 0;
	  curbuf = buf;
	}
      else
	s = grub_vsnprintf_real (curbuf, s, fmt, &args);
    }

  free_printf_args (&args);

  grub_xputs (curbuf);

  if (curbuf != buf)
    grub_free (curbuf);

  return s;
}

int
grub_memcmp (const void *s1, const void *s2, grub_size_t n)
{
  const grub_uint8_t *t1 = s1;
  const grub_uint8_t *t2 = s2;

  while (n--)
    {
      if (*t1 != *t2)
	return (int) *t1 - (int) *t2;

      t1++;
      t2++;
    }

  return 0;
}

int
grub_strcmp (const char *s1, const char *s2)
{
  while (*s1 && *s2)
    {
      if (*s1 != *s2)
	break;

      s1++;
      s2++;
    }

  return (int) (grub_uint8_t) *s1 - (int) (grub_uint8_t) *s2;
}

int
grub_strncmp (const char *s1, const char *s2, grub_size_t n)
{
  if (n == 0)
    return 0;

  while (*s1 && *s2 && --n)
    {
      if (*s1 != *s2)
	break;

      s1++;
      s2++;
    }

  return (int) (grub_uint8_t) *s1 - (int) (grub_uint8_t)  *s2;
}

char *
grub_strchr (const char *s, int c)
{
  do
    {
      if (*s == c)
	return (char *) s;
    }
  while (*s++);

  return 0;
}

char *
grub_strrchr (const char *s, int c)
{
  char *p = NULL;

  do
    {
      if (*s == c)
	p = (char *) s;
    }
  while (*s++);

  return p;
}

int
grub_strword (const char *haystack, const char *needle)
{
  const char *n_pos = needle;

  while (grub_iswordseparator (*haystack))
    haystack++;

  while (*haystack)
    {
      /* Crawl both the needle and the haystack word we're on.  */
      while(*haystack && !grub_iswordseparator (*haystack)
            && *haystack == *n_pos)
        {
          haystack++;
          n_pos++;
        }

      /* If we reached the end of both words at the same time, the word
      is found. If not, eat everything in the haystack that isn't the
      next word (or the end of string) and "reset" the needle.  */
      if ( (!*haystack || grub_iswordseparator (*haystack))
         && (!*n_pos || grub_iswordseparator (*n_pos)))
        return 1;
      else
        {
          n_pos = needle;
          while (*haystack && !grub_iswordseparator (*haystack))
            haystack++;
          while (grub_iswordseparator (*haystack))
            haystack++;
        }
    }

  return 0;
}

int
grub_isspace (int c)
{
  return (c == '\n' || c == '\r' || c == ' ' || c == '\t');
}

unsigned long
grub_strtoul (const char * restrict str, const char ** const restrict end,
	      int base)
{
  unsigned long long num;

  num = grub_strtoull (str, end, base);
#if GRUB_CPU_SIZEOF_LONG != 8
  if (num > ~0UL)
    {
      grub_error (GRUB_ERR_OUT_OF_RANGE, N_("overflow is detected"));
      return ~0UL;
    }
#endif

  return (unsigned long) num;
}

unsigned long long
grub_strtoull (const char * restrict str, const char ** const restrict end,
	       int base)
{
  unsigned long long num = 0;
  int found = 0;

  /* Skip white spaces.  */
  /* grub_isspace checks that *str != '\0'.  */
  while (grub_isspace (*str))
    str++;

  /* Guess the base, if not specified. The prefix `0x' means 16, and
     the prefix `0' means 8.  */
  if (str[0] == '0')
    {
      if (str[1] == 'x')
	{
	  if (base == 0 || base == 16)
	    {
	      base = 16;
	      str += 2;
	    }
	}
      else if (base == 0 && str[1] >= '0' && str[1] <= '7')
	base = 8;
    }

  if (base == 0)
    base = 10;

  while (*str)
    {
      unsigned long digit;

      digit = grub_tolower (*str) - '0';
      if (digit >= 'a' - '0')
	digit += '0' - 'a' + 10;
      else if (digit > 9)
	break;

      if (digit >= (unsigned long) base)
	break;

      found = 1;

      /* NUM * BASE + DIGIT > ~0ULL */
      if (num > grub_divmod64 (~0ULL - digit, base, 0))
	{
	  grub_error (GRUB_ERR_OUT_OF_RANGE,
		      N_("overflow is detected"));
	  return ~0ULL;
	}

      num = num * base + digit;
      str++;
    }

  if (! found)
    {
      grub_error (GRUB_ERR_BAD_NUMBER,
		  N_("unrecognized number"));
      return 0;
    }

  if (end)
    *end = (char *) str;

  return num;
}

char *
grub_strdup (const char *s)
{
  grub_size_t len;
  char *p;

  len = grub_strlen (s) + 1;
  p = (char *) grub_malloc (len);
  if (! p)
    return 0;

  return grub_memcpy (p, s, len);
}

char *
grub_strndup (const char *s, grub_size_t n)
{
  grub_size_t len;
  char *p;

  len = grub_strlen (s);
  if (len > n)
    len = n;
  p = (char *) grub_malloc (len + 1);
  if (! p)
    return 0;

  grub_memcpy (p, s, len);
  p[len] = '\0';
  return p;
}

/* clang detects that we're implementing here a memset so it decides to
   optimise and calls memset resulting in infinite recursion. With volatile
   we make it not optimise in this way.  */
#ifdef __clang__
#define VOLATILE_CLANG volatile
#else
#define VOLATILE_CLANG
#endif

void *
grub_memset (void *s, int c, grub_size_t len)
{
  void *p = s;
  grub_uint8_t pattern8 = c;

  if (len >= 3 * sizeof (unsigned long))
    {
      unsigned long patternl = 0;
      grub_size_t i;

      for (i = 0; i < sizeof (unsigned long); i++)
	patternl |= ((unsigned long) pattern8) << (8 * i);

      while (len > 0 && (((grub_addr_t) p) & (sizeof (unsigned long) - 1)))
	{
	  *(VOLATILE_CLANG grub_uint8_t *) p = pattern8;
	  p = (grub_uint8_t *) p + 1;
	  len--;
	}
      while (len >= sizeof (unsigned long))
	{
	  *(VOLATILE_CLANG unsigned long *) p = patternl;
	  p = (unsigned long *) p + 1;
	  len -= sizeof (unsigned long);
	}
    }

  while (len > 0)
    {
      *(VOLATILE_CLANG grub_uint8_t *) p = pattern8;
      p = (grub_uint8_t *) p + 1;
      len--;
    }

  return s;
}

grub_size_t
grub_strlen (const char *s)
{
  const char *p = s;

  while (*p)
    p++;

  return p - s;
}

static inline void
grub_reverse (char *str)
{
  char *p = str + grub_strlen (str) - 1;

  while (str < p)
    {
      char tmp;

      tmp = *str;
      *str = *p;
      *p = tmp;
      str++;
      p--;
    }
}

/* Divide N by D, return the quotient, and store the remainder in *R.  */
grub_uint64_t
grub_divmod64 (grub_uint64_t n, grub_uint64_t d, grub_uint64_t *r)
{
  /* This algorithm is typically implemented by hardware. The idea
     is to get the highest bit in N, 64 times, by keeping
     upper(N * 2^i) = (Q * D + M), where upper
     represents the high 64 bits in 128-bits space.  */
  unsigned bits = 64;
  grub_uint64_t q = 0;
  grub_uint64_t m = 0;

  /* ARM and IA64 don't have a fast 32-bit division.
     Using that code would just make us use software division routines, calling
     ourselves indirectly and hence getting infinite recursion.
  */
#if !GRUB_DIVISION_IN_SOFTWARE
  /* Skip the slow computation if 32-bit arithmetic is possible.  */
  if (n < 0xffffffff && d < 0xffffffff)
    {
      if (r)
	*r = ((grub_uint32_t) n) % (grub_uint32_t) d;

      return ((grub_uint32_t) n) / (grub_uint32_t) d;
    }
#endif

  while (bits--)
    {
      m <<= 1;

      if (n & (1ULL << 63))
	m |= 1;

      q <<= 1;
      n <<= 1;

      if (m >= d)
	{
	  q |= 1;
	  m -= d;
	}
    }

  if (r)
    *r = m;

  return q;
}

/* Convert a long long value to a string. This function avoids 64-bit
   modular arithmetic or divisions.  */
static inline char *
grub_lltoa (char *str, int c, unsigned long long n)
{
  unsigned base = ((c == 'x') || (c == 'X')) ? 16 : 10;
  char *p;

  if ((long long) n < 0 && c == 'd')
    {
      n = (unsigned long long) (-((long long) n));
      *str++ = '-';
    }

  p = str;

  if (base == 16)
    do
      {
	unsigned d = (unsigned) (n & 0xf);
	*p++ = (d > 9) ? d + ((c == 'x') ? 'a' : 'A') - 10 : d + '0';
      }
    while (n >>= 4);
  else
    /* BASE == 10 */
    do
      {
	grub_uint64_t m;

	n = grub_divmod64 (n, 10, &m);
	*p++ = m + '0';
      }
    while (n);

  *p = 0;

  grub_reverse (str);
  return p;
}

static void
parse_printf_args (const char *fmt0, struct printf_args *args,
		   va_list args_in)
{
  const char *fmt;
  char c;
  grub_size_t n = 0;

  args->count = 0;

  COMPILE_TIME_ASSERT (sizeof (int) == sizeof (grub_uint32_t));
  COMPILE_TIME_ASSERT (sizeof (int) <= sizeof (long long));
  COMPILE_TIME_ASSERT (sizeof (long) <= sizeof (long long));
  COMPILE_TIME_ASSERT (sizeof (long long) == sizeof (void *)
		       || sizeof (int) == sizeof (void *));

  fmt = fmt0;
  while ((c = *fmt++) != 0)
    {
      if (c != '%')
	continue;

      if (*fmt =='-')
	fmt++;

      while (grub_isdigit (*fmt))
	fmt++;

      if (*fmt == '$')
	fmt++;

      if (*fmt =='-')
	fmt++;

      while (grub_isdigit (*fmt))
	fmt++;

      if (*fmt =='.')
	fmt++;

      while (grub_isdigit (*fmt))
	fmt++;

      c = *fmt++;
      if (c == 'l' || c == 'h')
	{
	  char prev = c;

	  c = *fmt++;
	  if (c == prev)
	    c = *fmt++;
	}

      if (c == 'z' && grub_strchr("duxX", *fmt))
	c = *fmt++;

      switch (c)
	{
	case 'p':
	case 'x':
	case 'X':
	case 'u':
	case 'd':
	case 'c':
	case 'C':
	case 's':
#if defined(GRUB_MACHINE_EFI)
	  if ((c == 'u' && fmt[0] == 'r') ||
	      (c == 'l' && fmt[0] == 'u' && fmt[1] == 'r') ||
	      (c == 'p' && *fmt == 'D') ||
	      (c == 'p' && *fmt == 'G'))
	    fmt += fmt[0] == 'u' ? 2 : 1;
#endif
	  args->count++;
	  break;
	}
    }

  if (args->count <= ARRAY_SIZE (args->prealloc))
    args->ptr = args->prealloc;
  else
    {
      args->ptr = grub_calloc (args->count, sizeof (args->ptr[0]));
      if (!args->ptr)
	{
	  grub_errno = GRUB_ERR_NONE;
	  args->ptr = args->prealloc;
	  args->count = ARRAY_SIZE (args->prealloc);
	}
    }

  grub_memset (args->ptr, 0, args->count * sizeof (args->ptr[0]));

  fmt = fmt0;
  n = 0;
  while ((c = *fmt++) != 0)
    {
      int longfmt = 0;
      grub_size_t curn;
      const char *p;

      if (c != '%')
	continue;

      curn = n++;

      if (*fmt =='-')
	fmt++;

      p = fmt;

      while (grub_isdigit (*fmt))
	fmt++;

      if (*fmt == '$')
	{
	  curn = grub_strtoull (p, 0, 10) - 1;
	  fmt++;
	}

      if (*fmt =='-')
	fmt++;

      while (grub_isdigit (*fmt))
	fmt++;

      if (*fmt =='.')
	fmt++;

      while (grub_isdigit (*fmt))
	fmt++;

      c = *fmt++;
      if (c == '%')
	{
	  n--;
	  continue;
	}

      if (c == 'l' || c == 'h')
	{
	  char prev = c;

	  longfmt = c == 'h' ? -1 : 1;

	  c = *fmt++;
	  if (c == prev) {
	      longfmt *= 2;
	      c = *fmt++;
	  }
	}

      if (c == 'z' && grub_strchr("duxX", *fmt))
	{
	  c = *fmt++;
	  if (sizeof (grub_size_t) == 8)
	    longfmt = 2;
	  else
	    longfmt = 1;
	}

      if (curn >= args->count)
	continue;
      switch (c)
	{
	case 'x':
	case 'X':
	case 'u':
#if defined(GRUB_MACHINE_EFI)
	  if ((c == 'u' && fmt[0] == 'r') ||
	      (c == 'l' && fmt[0] == 'u' && fmt[1] == 'r'))
	    fmt += fmt[0] == 'u' ? 2 : 1;
#endif
	  args->ptr[curn].type = UNSIGNED_INT + longfmt;
	  break;
	case 'd':
	  args->ptr[curn].type = INT + longfmt;
	  break;
	case 'p':
#if defined(GRUB_MACHINE_EFI)
	  if (*fmt == 'D' || *fmt == 'G')
	    fmt++;
#endif
	  /* fall through. */
	case 's':
	  if (sizeof (void *) == sizeof (long long))
	    args->ptr[curn].type = UNSIGNED_LONGLONG;
	  else
	    args->ptr[curn].type = UNSIGNED_INT;
	  break;
	case 'C':
	case 'c':
	  args->ptr[curn].type = INT;
	  break;
	}
    }

  for (n = 0; n < args->count; n++)
    switch (args->ptr[n].type)
      {
	grub_uint8_t tmpu8;
	grub_uint16_t tmpu16;
	grub_int8_t tmpd8;
	grub_int16_t tmpd16;

      case INT8:
	tmpd8 = (grub_int8_t)va_arg (args_in, int);
	args->ptr[n].ll = tmpd8;
	break;
      case INT16:
	tmpd16 = (grub_int16_t)va_arg (args_in, int);
	args->ptr[n].ll = tmpd16;
	break;
      case INT:
	args->ptr[n].ll = va_arg (args_in, int);
	break;
      case LONG:
	args->ptr[n].ll = va_arg (args_in, long);
	break;
      case UINT8:
	tmpu8 = (grub_uint8_t)va_arg (args_in, unsigned int);
	args->ptr[n].ll = tmpu8;
	break;
      case UINT16:
	tmpu16 = (grub_uint16_t)va_arg (args_in, unsigned int);
	args->ptr[n].ll = tmpu16;
	break;
      case UNSIGNED_INT:
	args->ptr[n].ll = va_arg (args_in, unsigned int);
	break;
      case UNSIGNED_LONG:
	args->ptr[n].ll = va_arg (args_in, unsigned long);
	break;
      case LONGLONG:
      case UNSIGNED_LONGLONG:
	args->ptr[n].ll = va_arg (args_in, long long);
	break;
      }
}

static inline void __attribute__ ((always_inline))
write_char (char *str, grub_size_t *count, grub_size_t max_len, unsigned char ch)
{
  if (str && *count < max_len)
    str[*count] = ch;

  (*count)++;
}

static int
grub_vsnprintf_real (char *str, grub_size_t max_len, const char *fmt0,
		     struct printf_args *args)
{
  char c;
  grub_size_t n = 0;
  grub_size_t count = 0;
  const char *fmt;

  fmt = fmt0;

  while ((c = *fmt++) != 0)
    {
      unsigned int format1 = 0;
      unsigned int format2 = ~ 0U;
      char zerofill = ' ';
      char rightfill = 0;
      grub_size_t curn;

      if (c != '%')
	{
	  write_char (str, &count, max_len,c);
	  continue;
	}

      curn = n++;

    rescan:;

      if (*fmt =='-')
	{
	  rightfill = 1;
	  fmt++;
	}

      /* Read formatting parameters.  */
      if (grub_isdigit (*fmt))
	{
	  if (fmt[0] == '0')
	    zerofill = '0';
	  format1 = grub_strtoul (fmt, &fmt, 10);
	}

      if (*fmt == '.')
	fmt++;

      if (grub_isdigit (*fmt))
	format2 = grub_strtoul (fmt, &fmt, 10);

      if (*fmt == '$')
	{
	  curn = format1 - 1;
	  fmt++;
	  format1 = 0;
	  format2 = ~ 0U;
	  zerofill = ' ';
	  rightfill = 0;

	  goto rescan;
	}

      c = *fmt++;
      if (c == 'l' || c == 'h')
	{
	  char prev = c;

	  c = *fmt++;
	  if (c == prev)
	    c = *fmt++;
	}

      if (c == 'z' && grub_strchr("duxX", *fmt))
	c = *fmt++;

      if (c == '%')
	{
	  write_char (str, &count, max_len,c);
	  n--;
	  continue;
	}

      if (curn >= args->count)
	continue;

      long long curarg = args->ptr[curn].ll;

      switch (c)
	{
	case 'p':
#if defined(GRUB_MACHINE_EFI)
	  if (*fmt == 'G')
	    {
	      grub_ssize_t sz;
	      grub_efi_guid_t *guid = (void *)(grub_uintptr_t)curarg;

	      fmt++;
	      sz = grub_efi_fmt_guid (str ? str + count : str,
				      max_len ? max_len - count : max_len,
				      guid);
	      if (sz < 0)
		return sz;

	      count += sz;
	      continue;
	    }
#endif
	  write_char (str, &count, max_len, '0');
	  write_char (str, &count, max_len, 'x');
	  c = 'x';
	  /* Fall through. */
	case 'x':
	case 'X':
	case 'u':
	case 'd':
	  {
	    char tmp[32];
	    const char *p = tmp;
	    grub_size_t len;
	    grub_size_t fill;

#if defined(GRUB_MACHINE_EFI)
	    if ((c == 'u' && fmt[0] == 'r') ||
		(c == 'l' && fmt[0] == 'u' && fmt[1] == 'r'))
	      {
		grub_efi_status_t status;

		fmt += fmt[0] == 'u' ? 2 : 1;

		status = args->ptr[curn].ll;
		p = grub_efi_status_to_str (status);
		zerofill = ' ';

		if (p == NULL)
		  {
		    write_char (str, &count, max_len, '0');
		    write_char (str, &count, max_len, 'x');
		    c = 'x';
		    zerofill = '0';

		    if (sizeof (long) == 8)
		      args->ptr[curn].type = UNSIGNED_LONG;
		    else
		      args->ptr[curn].type = UNSIGNED_LONGLONG;

		    goto rescan;
		  }

		len = grub_strlen (p);

		while (len < format2 && p[len])
		  len++;

		fill = len < format1 ? format1 - len : 0;

		if (!rightfill)
		  while (fill--)
		    write_char (str, &count, max_len, zerofill);

		for (grub_size_t i = 0; i < len; i++)
		  write_char (str, &count, max_len,*p++);

		if (rightfill)
		  while (fill--)
		    write_char (str, &count, max_len, zerofill);

		break;
	      }
#endif
	    len = grub_lltoa (tmp, c, curarg) - tmp;
	    fill = len < format1 ? format1 - len : 0;
	    if (! rightfill)
	      while (fill--)
		write_char (str, &count, max_len, zerofill);
	    while (*p)
	      write_char (str, &count, max_len, *p++);
	    if (rightfill)
	      while (fill--)
		write_char (str, &count, max_len, zerofill);
	  }
	  break;

	case 'c':
	  write_char (str, &count, max_len,curarg & 0xff);
	  break;

	case 'C':
	  {
	    grub_uint32_t code = curarg;
	    int shift;
	    unsigned mask;

	    if (code <= 0x7f)
	      {
		shift = 0;
		mask = 0;
	      }
	    else if (code <= 0x7ff)
	      {
		shift = 6;
		mask = 0xc0;
	      }
	    else if (code <= 0xffff)
	      {
		shift = 12;
		mask = 0xe0;
	      }
	    else if (code <= 0x10ffff)
	      {
		shift = 18;
		mask = 0xf0;
	      }
	    else
	      {
		code = '?';
		shift = 0;
		mask = 0;
	      }

	    write_char (str, &count, max_len,mask | (code >> shift));

	    for (shift -= 6; shift >= 0; shift -= 6)
	      write_char (str, &count, max_len,0x80 | (0x3f & (code >> shift)));
	  }
	  break;

	case 's':
	  {
	    grub_size_t len = 0;
	    grub_size_t fill;
	    const char *p = ((char *) (grub_addr_t) curarg) ? : "(null)";
	    grub_size_t i;

	    while (len < format2 && p[len])
	      len++;

	    fill = len < format1 ? format1 - len : 0;

	    if (!rightfill)
	      while (fill--)
		write_char (str, &count, max_len, zerofill);

	    for (i = 0; i < len; i++)
	      write_char (str, &count, max_len,*p++);

	    if (rightfill)
	      while (fill--)
		write_char (str, &count, max_len, zerofill);
	  }

	  break;

	default:
	  write_char (str, &count, max_len,c);
	  break;
	}
    }

  if (str)
    {
      if (count < max_len)
	str[count] = '\0';
      else if (max_len > 0)
	str[max_len - 1] = '\0';
    }
  return count;
}

int
grub_vsnprintf (char *str, grub_size_t n, const char *fmt, va_list ap)
{
  grub_size_t ret;
  struct printf_args args;

  parse_printf_args (fmt, &args, ap);

  ret = grub_vsnprintf_real (str, n, fmt, &args);

  free_printf_args (&args);

  if (!str)
    return ret;

  return ret < n ? ret : n;
}

int
grub_snprintf (char *str, grub_size_t n, const char *fmt, ...)
{
  va_list ap;
  int ret;

  va_start (ap, fmt);
  ret = grub_vsnprintf (str, n, fmt, ap);
  va_end (ap);

  return ret;
}

char *
grub_xvasprintf (const char *fmt, va_list ap)
{
  grub_ssize_t s;
  char *ret = NULL;
  struct printf_args args;

  parse_printf_args (fmt, &args, ap);

  s = grub_vsnprintf_real (NULL, 0, fmt, &args);
  if (s < 0)
    goto err;

  s++;
  ret = grub_malloc (s);
  if (!ret)
    goto err;

  grub_vsnprintf_real (ret, s, fmt, &args);
err:
  if (ret == NULL)
    grub_error_pop ();

  free_printf_args (&args);
  return ret;
}

char *
grub_xasprintf (const char *fmt, ...)
{
  va_list ap;
  char *ret;

  va_start (ap, fmt);
  ret = grub_xvasprintf (fmt, ap);
  va_end (ap);

  return ret;
}

/* Abort GRUB. This function does not return.  */
static void __attribute__ ((noreturn))
grub_abort (void)
{
  grub_printf ("\nAborted.");
  
#ifndef GRUB_UTIL
  if (grub_term_inputs)
#endif
    {
      grub_printf (" Press any key to exit.");
      grub_getkey ();
    }

  grub_exit ();
}

void
grub_fatal (const char *fmt, ...)
{
  va_list ap;

  va_start (ap, fmt);
  grub_vprintf (_(fmt), ap);
  va_end (ap);

  grub_refresh ();

  grub_abort ();
}

#if BOOT_TIME_STATS

#include <grub/time.h>

struct grub_boot_time *grub_boot_time_head;
static struct grub_boot_time **boot_time_last = &grub_boot_time_head;

void
grub_real_boot_time (const char *file,
		     const int line,
		     const char *fmt, ...)
{
  struct grub_boot_time *n;
  va_list args;

  grub_error_push ();
  n = grub_malloc (sizeof (*n));
  if (!n)
    {
      grub_errno = 0;
      grub_error_pop ();
      return;
    }
  n->file = file;
  n->line = line;
  n->tp = grub_get_time_ms ();
  n->next = 0;

  va_start (args, fmt);
  n->msg = grub_xvasprintf (fmt, args);    
  va_end (args);

  *boot_time_last = n;
  boot_time_last = &n->next;

  grub_errno = 0;
  grub_error_pop ();
}
#endif
