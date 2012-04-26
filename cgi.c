/*
 * This software has been authored by an employee or employees of Los
 * Alamos National Security, LLC, operator of the Los Alamos National
 * Laboratory (LANL) under Contract No. DE-AC52-06NA25396 with the U.S.
 * Department of Energy.  The U.S. Government has rights to use,
 * reproduce, and distribute this software.  The public may copy,
 * distribute, prepare derivative works and publicly display this
 * software without charge, provided that this Notice and any statement
 * of authorship are reproduced on all copies.  Neither the Government
 * nor LANS makes any warranty, express or implied, or assumes any
 * liability or responsibility for the use of this software.  If
 * software is modified to produce derivative works, such modified
 * software should be clearly marked, so as not to confuse it with the
 * version available from LANL.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "cgi.h"

static int is_cgi  = 0;
static char **argv = NULL;

static int
read_char_argv()
{
  static int   arg = 0;
  static char *p;

  if (NULL == argv) {
    return EOF;
  }

  if (0 == arg) {
    arg = 1;
    p = argv[1];
  }

  if (! p) {
    return EOF;
  } else if (! *p) {
    arg += 1;
    p = argv[arg];
    return '&';
  }

  return *(p++);
}

static int
read_char_stdin()
{
  static long inlen = -1;

  if (-1 == inlen) {
    char *p = getenv("CONTENT_LENGTH");
    if (p) {
      inlen = atoi(p);
      if (inlen > POST_MAX) {
        inlen = POST_MAX;
      }
      if (inlen < 0) {
        inlen = 0;
      }
    } else {
      inlen = 0;
    }
  }

  if (inlen) {
    inlen -= 1;
    return getchar();
  }
  return EOF;
}

static int
read_char_query_string()
{
  static char *p = (char *)-1;

  if ((char *)-1 == p) {
    p = getenv("QUERY_STRING");
  }

  if (! p) {
    return EOF;
  } else if (! *p) {
    return EOF;
  } else {
    return *(p++);
  }
}

static int (* read_char)() = read_char_argv;

int
cgi_init(char *global_argv[])
{
  char *rm = getenv("REQUEST_METHOD");

  if (! rm) {
    read_char = read_char_argv;
    argv = global_argv;
  } else if (0 == strcmp(rm, "POST")) {
    read_char = read_char_stdin;
    is_cgi = 1;
  } else if (0 == strcmp(rm, "GET")) {
    read_char = read_char_query_string;
    is_cgi = 1;
  } else {
    printf(("405 Method not allowed\r\n"
            "Allow: GET, POST\r\n"
            "Content-type: text/plain\r\n"
            "\r\n"
            "%s is not allowed.\n"),
           rm);
    return -1;
  }

  return 0;
}

static char
tonum(int c)
{
  if ((c >= '0') && (c <= '9')) {
    return c - '0';
  }
  if ((c >= 'a') && (c <= 'f')) {
    return 10 + c - 'a';
  }
  if ((c >= 'A') && (c <= 'F')) {
    return 10 + c - 'A';
  }
  return 0;
}

static char
read_hex()
{
  int a = read_char();
  int b = read_char();

  return tonum(a)*16 + tonum(b);
}

/* Read a key or a value.  Since & and = aren't supposed to appear
   outside of boundaries, we can use the same function for both.
*/
size_t
cgi_item(char *str, size_t maxlen)
{
  int    c;
  size_t pos = 0;

  while (1) {
    c = read_char();
    switch (c) {
      case EOF:
      case '=':
      case '&':
        str[pos] = '\0';
        return pos;
      case '%':
        c = read_hex();
        break;
      case '+':
        c = ' ';
        break;
    }
    if (pos < maxlen - 1) {
      str[pos] = c;
      pos += 1;
    }
  }
}

void
cgi_header(char *content_type)
{
    if (is_cgi) {
        printf("Content-type: %s\r\n\r\n", content_type);
    }
}
