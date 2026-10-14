/* -*-c-*- */
/*
 * ParseComand.c was originally part of FvwmForm.c.
 *
 * Turned into separate file 02/27/99, Dan Espen.
 *
 * FvwmForm is original work of Thomas Zuwei Feng.
 *
 * Copyright Feb 1995, Thomas Zuwei Feng.  No guarantees or warantees are
 * provided or implied in any way whatsoever.  Use this program at your own
 * risk.  Permission to use, modify, and redistribute this program is hereby
 * given, provided that this copyright is kept intact.
 */

/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see: <http://www.gnu.org/licenses/>
 */

#include "config.h"
#include "libs/fvwmlib.h"
#include "libs/fvwmsignal.h"
#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <inttypes.h>
#include <sys/types.h>
#include <FvwmForm.h>                   /* common FvwmForm stuff */
#include "form_input.h"

static char *buf;
static int N = 8;
static uint64_t form_command_serial;
static char form_command_token[17] = "0000000000000000";

bool form_command_begin(void)
{
	if (form_command_serial == UINT64_MAX)
		return false;
	form_command_serial++;
	snprintf(form_command_token, sizeof(form_command_token), "%016" PRIx64,
		form_command_serial);
	return true;
}

/* Macro used in following functions */
#define AddChar(chr) \
  do { \
    if (isTerminated) \
      goto invalid; \
    if (dn >= N) { \
      int next; \
      if (!form_array_capacity(N, dn, 1, 8, sizeof(*buf), &next)) \
        goto invalid; \
      buf = fxrealloc(buf, next, sizeof(*buf)); \
      N = next; \
    } \
    buf[dn++] = (chr); \
  } while (0)

/* Match the legacy normalized spelling without copying an identifier. */
static bool form_variable_matches(const char *begin, const char *end,
	const char *name)
{
	bool escaped = false;

	for (const char *p = begin; p < end; p++)
	{
		if (!escaped && isspace((unsigned char)*p))
		{
			continue;
		}
		if (*name == '\0' || *name++ != *p)
		{
			return false;
		}
		escaped = !escaped && *p == '\\';
	}
	return *name == '\0';
}

/* This explicit modifier transports field bytes as one data argument. Ordinary
 * variables, including names ending in :Base64, retain lookup priority. */
static char *form_encoded_value(const Item *item)
{
	static const char alphabet[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
	const char *value = "";
	char *joined = NULL, *result;
	size_t length = 0, used = 0;

	if (item->type == I_INPUT)
		value = item->input.value;
	else if (item->type == I_CHOICE)
		value = item->choice.value;
	else if (item->type == I_SELECT)
	{
		for (int i = 0; i < item->selection.n; i++)
		{
			if (item->selection.choices[i]->choice.on)
			{
				size_t n = strlen(item->selection.choices[i]->choice.value);
				size_t space = used != 0;
				if (space > 65536 - length ||
					n > 65536 - length - space)
					return NULL;
				length += n + (used++ != 0);
			}
		}
		joined = fxmalloc(length + 1);
		used = 0;
		bool added = false;
		for (int i = 0; i < item->selection.n; i++)
		{
			if (item->selection.choices[i]->choice.on)
			{
				const char *part = item->selection.choices[i]->choice.value;
				if (added)
					joined[used++] = ' ';
				added = true;
				memcpy(joined + used, part, strlen(part));
				used += strlen(part);
			}
		}
		joined[used] = '\0';
		value = joined;
	}
	length = strlen(value);
	if (length > 65536)
	{
		free(joined);
		return NULL;
	}
	result = fxmalloc((length * 4 + 2) / 3 + 1);
	used = 0;
	for (size_t i = 0; i < length; i += 3)
	{
		unsigned int bits = (unsigned char)value[i] << 16;
		if (i + 1 < length)
			bits |= (unsigned char)value[i + 1] << 8;
		if (i + 2 < length)
			bits |= (unsigned char)value[i + 2];
		result[used++] = alphabet[bits >> 18];
		result[used++] = alphabet[(bits >> 12) & 63];
		if (i + 1 < length)
			result[used++] = alphabet[(bits >> 6) & 63];
		if (i + 2 < length)
			result[used++] = alphabet[bits & 63];
	}
	result[used] = '\0';
	free(joined);
	return result;
}

/* do var substitution for command string */
char * ParseCommand (int dn, char *sp, char end, int *dn1, char **sp1)
{
  char c, x, *cp;
  int j, dn2;
  int added_sel;
  Item *item;

  *dn1 = 0;
  *sp1 = sp;
  if (dn < 0 || isTerminated)
    goto invalid;
  if (buf == 0) {                       /* if no buffer yet */
    buf = fxmalloc(N);
  }
  while (1) {
    if (isTerminated)
      goto invalid;
    c = *sp;
    if (c == '\0' || c == end) {  /* end of substitution */
      if (c == '\0' && end != '\0')
        goto invalid;
      sp++;
      *dn1 = dn;
      *sp1 = sp;
      if (end == 0) {                   /* if end of command reached */
	AddChar('\0');                  /* make sure theres a null */
      }
      return(buf);
    }
    sp++;
    if (c == '$') {  /* variable */
      if (*sp != '(')
	goto normal_char;
      ++sp;
      char *begin = sp;
      while (1) {
	if (isTerminated)
	  goto invalid;
	x = *sp;
	if (x == '\0')
	  goto invalid;
	if (x == '\\') {
	  if (sp[1] == '\0')
	    goto invalid;
	  sp += 2;
	}
	else if (x == ')' || x == '?' || x == '!') {
	  sp++;
	  break;
	}
	else
	  sp++;
      }
      for (item = root_item_ptr; item != 0;
	   item = item->header.next) {/* all items */
	if (isTerminated)
	  goto invalid;
	if (form_variable_matches(begin, sp - 1, item->header.name)) {
	  switch (item->type) {
	  case I_INPUT:
	    if (x == ')') {
	      for (cp = item->input.value; *cp != '\0'; cp++) {
		AddChar(*cp);
	      }
	    } else {
	      if (ParseCommand(dn, sp, ')', &dn2, &sp) == NULL)
	        goto invalid;
	      if ((x == '?' && strlen(item->input.value) > 0) ||
		  (x == '!' && strlen(item->input.value) == 0))
		dn = dn2;
	    }
	    break;
	  case I_CHOICE:
	    if (x == ')') {
	      for (cp = item->choice.value; *cp != '\0'; cp++)
		AddChar(*cp);
	    } else {
	      if (ParseCommand(dn, sp, ')', &dn2, &sp) == NULL)
	        goto invalid;
	      if ((x == '?' && item->choice.on) ||
		  (x == '!' && !item->choice.on))
		dn = dn2;
	    }
	    break;
	  case I_SELECT:
	    if (x != ')' && ParseCommand(dn, sp, ')', &dn2, &sp) == NULL)
	      goto invalid;
	    added_sel=0;
	    for (j = 0; j < item->selection.n; j++) {
	      if (item->selection.choices[j]->choice.on) {
		if (added_sel) {        /* if not first sel added */
		  AddChar(' ');         /* insert space before next value */
		}
		added_sel=1;
		for (cp = item->selection.choices[j]->choice.value;
		     *cp != '\0'; cp++) {
		  AddChar(*cp);
		}
	      }
	    }
	    break;
	  }
	  goto next_loop;
	}
      }
      /* Exact lookup above also covers pre-existing colon-bearing names. */
      if (x == ')' && form_variable_matches(begin, sp - 1, "FormJob:Token")) {
	for (cp = form_command_token; *cp != '\0'; cp++)
	  AddChar(*cp);
	goto next_loop;
      }
      if (x == ')' && sp - 1 - begin >= 7 &&
	  memcmp(sp - 8, ":Base64", 7) == 0) {
	for (item = root_item_ptr; item != NULL; item = item->header.next) {
	  if (form_variable_matches(begin, sp - 8, item->header.name)) {
	    char *encoded = form_encoded_value(item);
	    if (encoded == NULL)
	      goto invalid;
	    /* Reserve before copying so cancellation/refusal cannot leak storage. */
	    size_t length = strlen(encoded);
	    int next;
	    if (length > INT_MAX - dn - 1 ||
		!form_array_capacity(N, dn, (int)length + 1, 8,
		  sizeof(*buf), &next)) {
	      free(encoded);
	      goto invalid;
	    }
	    buf = fxrealloc(buf, next, sizeof(*buf));
	    N = next;
	    memcpy(buf + dn, encoded, length);
	    dn += (int)length;
	    free(encoded);
	    goto next_loop;
	  }
	}
      }
      goto next_loop;
    } /* end char is $, not followed by ( */
    /* if char is \, followed by ), want to pass thru the paren literally */
    if (c == '\\' && *sp == ')') {
      c = *(sp++);                      /* skip to the paren */
    }
  normal_char:
    AddChar(c);
  next_loop:
      ;
  }
invalid:
  *dn1 = 0;
  *sp1 = sp;
  return NULL;
}
