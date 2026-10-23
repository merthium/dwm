/*
 * Copyright © 2013 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

/* Minimal public declarations from libXpresent 1.0.2 (x.org).
 * The installed runtime library is used; no separate compositor or service. */
#ifndef DWM_PRESENT_H
#define DWM_PRESENT_H
#include <stdint.h>
#include <X11/Xlib.h>
#include <X11/extensions/presenttokens.h>
typedef struct {
  int type;
  unsigned long serial;
  Bool send_event;
  Display *display;
  int extension, evtype;
  uint32_t eid;
  Window window;
  uint32_t serial_number;
  uint64_t ust, msc;
  uint8_t kind, mode;
} XPresentCompleteNotifyEvent;
Bool XPresentQueryExtension(Display *, int *, int *, int *);
XID XPresentSelectInput(Display *, Window, unsigned);
void XPresentNotifyMSC(Display *, Window, uint32_t, uint64_t, uint64_t, uint64_t);
#endif
