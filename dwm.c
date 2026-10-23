#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif
#include <X11/Xft/Xft.h>
#include <X11/extensions/XRes.h>

#include "drw.h"
#include "util.h"

#define BUTTONMASK (ButtonPressMask | ButtonReleaseMask)
#define CLEANMASK(mask)                                                        \
  ((mask) & ~(numlockmask | LockMask) &                                        \
   (ShiftMask | ControlMask | Mod1Mask | Mod2Mask | Mod3Mask | Mod4Mask |      \
    Mod5Mask))
#define INTERSECT(x, y, w, h, m)                                               \
  (MAX(0, MIN((x) + (w), (m)->wx + (m)->ww) - MAX((x), (m)->wx)) * \
   MAX(0, MIN((y) + (h), (m)->wy + (m)->wh) - MAX((y), (m)->wy)))
#define ISVISIBLE(C)                                                           \
  ((C)->swallowed == NULL &&                                                   \
   ((C)->tags & (C)->mon->tagset[(C)->mon->seltags]))
#define HIDDEN(C) ((C)->ishidden)
#define LENGTH(X) (sizeof(X) / sizeof((X)[0]))
#define MOUSEMASK (BUTTONMASK | PointerMotionMask)
#define WIDTH(X) ((X)->w + 2 * (X)->bw + gappx)
#define HEIGHT(X) ((X)->h + 2 * (X)->bw + gappx)
#define TAGMASK ((1U << LENGTH(tags)) - 1U)
#define SPTAG (1U << LENGTH(tags))
#define TEXTW(X) (drw_fontset_getwidth(drw, (X)) + lrpad)
#define TAGW(X) (drw_fontset_getwidth(drw, (X)) + lrpad / 2)
#define SYSTEM_TRAY_REQUEST_DOCK 0

#define XEMBED_EMBEDDED_NOTIFY 0
#define XEMBED_WINDOW_ACTIVATE 1
#define XEMBED_WINDOW_DEACTIVATE 2
#define XEMBED_MAPPED (1 << 0)
#define XEMBED_EMBEDDED_VERSION 0
enum { CurNormal, CurResize, CurMove, CurLast };
enum { SchemeNorm, SchemeSel, SchemeTitle, SchemeHid };
enum {
  NetSupported,
  NetWMName,
  NetWMState,
  NetWMCheck,
  NetWMFullscreen,
  NetActiveWindow,
  NetWMWindowType,
  NetWMWindowTypeDialog,
  NetWMWindowTypeNormal,
  NetClientList,
  NetSupportedCount,
  NetSystemTray = NetSupportedCount,
  NetSystemTrayOP,
  NetSystemTrayOrientation,
  NetLast
};
enum { Manager, Xembed, XembedInfo, XLast };
enum { WMProtocols, WMDelete, WMState, WMTakeFocus, WMLast };
enum {
  ClkTagBar,
  ClkLtSymbol,
  ClkStatusText,
  ClkWinTitle,
  ClkClientWin,
  ClkRootWin,
  ClkLast
};

typedef union {
  int i;
  unsigned int ui;
  float f;
  const void *v;
} Arg;

typedef struct {
  unsigned int click;
  unsigned int mask;
  unsigned int button;
  void (*func)(const Arg *arg);
  const Arg arg;
} Button;

typedef struct Monitor Monitor;
typedef struct Client Client;
struct Client {
  char name[256];
  char appname[256];
  float mina, maxa;
  int x, y, w, h;
  int oldx, oldy, oldw, oldh;
  int basew, baseh, incw, inch, maxw, maxh, minw, minh;
  int bw, oldbw, fsbw;
  unsigned int tags;
  int isfixed, isfloating, isurgent, ishidden, neverfocus, oldstate,
      isfullscreen, needresize;

  Client *swallower;
  Client *swallowed;
  Client *next_swallowed;

  Client *next;
  Client *snext;
  Monitor *mon;
  Window win;
};

typedef struct {
  unsigned int mod;
  KeySym keysym;
  void (*func)(const Arg *);
  const Arg arg;
} Key;

typedef struct Pertag Pertag;
typedef struct {
  const char *symbol;
  void (*arrange)(Monitor *);
} Layout;

struct Monitor {
  char ltsymbol[16];
  float mfact;
  int nmaster;
  int num;
  int by;
  int btw;
  int bt;
  int mx, my, mw, mh;
  int wx, wy, ww, wh;
  unsigned int seltags;
  unsigned int sellt;
  unsigned int tagset[2];
  int showbar;
  int topbar;
  int hidsel;
  Client *clients;
  Client *sel;
  Client *stack;
  Monitor *next;
  Window barwin;
  const Layout *lt[2];

  Pertag *pertag;
};

typedef struct {
  const char *class;
  const char *instance;
  const char *title;
  unsigned int tags;
  int isfloating;
  int monitor;
} Rule;

typedef struct Systray Systray;
struct Systray {
  Window win;
  Client *icons;
};

typedef struct SwallowDef {
  pid_t pid;
  Client *swallower;
  struct SwallowDef *next;
} SwallowDef;

static void applyrules(Client *c);
static int applysizehints(Client *c, int *x, int *y, int *w, int *h,
                          int interact);
static void arrange(Monitor *m);
static void arrangemon(Monitor *m);
static void attach(Client *c);
static void attachstack(Client *c);
static void buttonpress(XEvent *e);
static void cagepointer(Monitor *m, int x, int y);
static void checkotherwm(void);
static void cleanup(void);
static void cleanupmon(Monitor *mon);
static void clientmessage(XEvent *e);
static void configure(Client *c);
static void configurenotify(XEvent *e);
static void configurerequest(XEvent *e);
static Monitor *createmon(void);
static void destroynotify(XEvent *e);
static void defaultgaps(const Arg *arg);
static void enternotify(XEvent *e);
static void detach(Client *c);
static void detachstack(Client *c);
static Monitor *dirtomon(int dir);
static void drawbar(Monitor *m);
static void drawbars(void);
static int drawstatusbar(Monitor *m, int height, const char *text);
static void expose(XEvent *e);
static Client *findbefore(Client *c);
static void focus(Client *c);
static void focusin(XEvent *e);
static void focusmon(const Arg *arg);
static void focusmonitor(Monitor *m);
static void focusstackvis(const Arg *arg);
static void focusstackhid(const Arg *arg);
static void focusstack(int inc, int vis);
static unsigned long getxembedflags(Window w);
static Client *getclientundermouse(void);
static int getrootptr(int *x, int *y);
static long getstate(Window w);
static unsigned int getsystraywidth(void);
static int gettextprop(Window w, Atom atom, char *text, unsigned int size);
static void grabbuttons(Client *c, int focused);
static void grabkeys(void);
static int hasatomprop(Window w, Atom prop, Atom target);
static int isxembedicon(Window w);
static void hide(const Arg *arg);
static void hidewin(Client *c);
static void incnmaster(const Arg *arg);
static void keypress(XEvent *e);
static void killclient(const Arg *arg);
static void manage(Window w, XWindowAttributes *wa);
static void mappingnotify(XEvent *e);
static void maprequest(XEvent *e);
static void monocle(Monitor *m);
static void motionnotify(XEvent *e);
static void movemouse(const Arg *arg);
static Client *nexttiled(Client *c);
static void propertynotify(XEvent *e);
static void quit(const Arg *arg);
static void reloadtheme(void);
static Monitor *recttomon(int x, int y, int w, int h);
static void removesystrayicon(Client *i);
static void resize(Client *c, int x, int y, int w, int h, int interact);
static void resizebarwin(Monitor *m);
static void resizeclient(Client *c, int x, int y, int w, int h);
static void resizefullscreen(Client *c, int x, int y, int w, int h);
static void resizemouse(const Arg *arg);
static void resizerequest(XEvent *e);
static void restack(Monitor *m);
static void run(void);
static void scan(void);
static int sendevent(Window w, Atom proto, int m, long d0, long d1, long d2,
                     long d3, long d4);
static void sendxembed(Window w, long message);
static void sendmon(Client *c, Monitor *m);
static void setatomprop(Client *c, Atom prop, Atom target, int enabled);
static void setbarvisible(Monitor *m, int visible);
static void setclientstate(Client *c, long state);
static void setfocus(Client *c);
static void setfullscreen(Client *c, int fullscreen);
static void setlayout(const Arg *arg);
static void setgaps(const Arg *arg);
static void setmfact(const Arg *arg);
static void shiftview(const Arg *arg);
static void setup(void);
static Monitor *systraytomon(Monitor *m);
static void seturgent(Client *c, int urg);
static void showall(const Arg *arg);
static void showwin(Client *c);
static void showhide(Client *c);
static void sigchld(int unused);
static void spawn(const Arg *arg);
static void statusclick(const Arg *arg);
static int statusmoduleat(Monitor *m, int clickx);
static unsigned int statusbarwidth(void);
static unsigned int displayedstatuswidth(Monitor *m);
static void status2dtext(char *dst, size_t size, const char *src);
static unsigned int statustraywidth(Monitor *m);
static void tag(const Arg *arg);
static void tagmon(const Arg *arg);
static void tile(Monitor *);
static void togglebar(const Arg *arg);
static void togglefloating(const Arg *arg);
static void togglegaps(const Arg *arg);
static void togglescratch(const Arg *arg);
static void toggletag(const Arg *arg);
static void toggleview(const Arg *arg);
static void togglewin(const Arg *arg);
static void unfocus(Client *c, int setfocus);
static void unmanage(Client *c, int destroyed);
static void unmapnotify(XEvent *e);
static void updatebarpos(Monitor *m);
static void updatebars(void);
static void updateclientlist(void);
static int updategeom(void);
static void updatenumlockmask(void);
static void updatesizehints(Client *c);
static void updatestatus(void);
static void updatesystray(void);
static void updatesystrayicongeom(Client *i, int w, int h);
static void updatesystrayiconidentity(Client *i);
static void updatesystrayiconstate(Client *i, XPropertyEvent *ev);
static void updatetitle(Client *c);
static void updatewindowtype(Client *c);
static void updatewmhints(Client *c);
static void view(const Arg *arg);
static Client *wintoclient(Window w);
static Monitor *wintomon(Window w);
static Client *wintosystrayicon(Window w);
static int xerror(Display *dpy, XErrorEvent *ee);
static int xerrordummy(Display *dpy, XErrorEvent *ee);
static int xerrorstart(Display *dpy, XErrorEvent *ee);
static void zoom(const Arg *arg);

static Client *prevzoom = NULL;
static const char broken[] = "broken";
static char sessionfile[64];
static char stext[1024];
static int screen;
static int sw, sh;
static int bh, blw = 0;
static int lrpad;
static int xresclientids;
static int (*xerrorxlib)(Display *, XErrorEvent *);
static unsigned int numlockmask = 0;
static void (*handler[LASTEvent])(XEvent *) = {
    [ButtonPress] = buttonpress,
    [ClientMessage] = clientmessage,
    [ConfigureRequest] = configurerequest,
    [ConfigureNotify] = configurenotify,
    [DestroyNotify] = destroynotify,
    [EnterNotify] = enternotify,
    [Expose] = expose,
    [FocusIn] = focusin,
    [KeyPress] = keypress,
    [MappingNotify] = mappingnotify,
    [MapRequest] = maprequest,
    [MotionNotify] = motionnotify,
    [PropertyNotify] = propertynotify,
    [ResizeRequest] = resizerequest,
    [UnmapNotify] = unmapnotify};
static Atom swallow_atom, theme_atom;
static SwallowDef *swallowlist;
static Systray *systray = NULL;
static Atom wmatom[WMLast], netatom[NetLast], xatom[XLast];
static int running = 1;
static int restart = 0;
static Cur *cursor[CurLast];
static Clr **scheme;
static Display *dpy;
static Drw *drw;
static Monitor *mons, *selmon;
static Window root, wmcheckwin;

#include "config.h"

struct Pertag {
  unsigned int curtag, prevtag;
  int nmasters[LENGTH(tags) + 1];
  float mfacts[LENGTH(tags) + 1];
  unsigned int sellts[LENGTH(tags) + 1];
  const Layout *ltidxs[LENGTH(tags) + 1][2];
  int showbars[LENGTH(tags) + 1];
};

struct NumTags {
  char limitexceeded[LENGTH(tags) > 31 ? -1 : 1];
};

void applyrules(Client *c) {
  const char *class, *instance;
  unsigned int i;
  const Rule *r;
  Monitor *m;
  XClassHint ch = {NULL, NULL};

  c->isfloating = 0;
  c->tags = 0;
  XGetClassHint(dpy, c->win, &ch);
  class = ch.res_class ? ch.res_class : broken;
  instance = ch.res_name ? ch.res_name : broken;

  for (i = 0; i < LENGTH(rules); i++) {
    r = &rules[i];
    if ((!r->title || strstr(c->name, r->title)) &&
        (!r->class || strstr(class, r->class)) &&
        (!r->instance || strstr(instance, r->instance))) {
      c->isfloating = r->isfloating;
      c->tags |= r->tags;
      for (m = mons; m && m->num != r->monitor; m = m->next)
        ;
      if (m)
        c->mon = m;
    }
  }
  if (ch.res_class)
    XFree(ch.res_class);
  if (ch.res_name)
    XFree(ch.res_name);
  c->tags &= TAGMASK | SPTAG;
  if (!c->tags)
    c->tags = c->mon->tagset[c->mon->seltags] & TAGMASK;
}

int applysizehints(Client *c, int *x, int *y, int *w, int *h, int interact) {
  int baseismin;
  Monitor *m = c->mon;

  *w = MAX(1, *w);
  *h = MAX(1, *h);
  if (interact) {
    if (*x > sw)
      *x = sw - WIDTH(c);
    if (*y > sh)
      *y = sh - HEIGHT(c);
    if (*x + *w + 2 * c->bw < 0)
      *x = 0;
    if (*y + *h + 2 * c->bw < 0)
      *y = 0;
  } else {
    if (*x >= m->wx + m->ww)
      *x = m->wx + m->ww - WIDTH(c);
    if (*y >= m->wy + m->wh)
      *y = m->wy + m->wh - HEIGHT(c);
    if (*x + *w + 2 * c->bw <= m->wx)
      *x = m->wx;
    if (*y + *h + 2 * c->bw <= m->wy)
      *y = m->wy;
  }
  if (*h < bh)
    *h = bh;
  if (*w < bh)
    *w = bh;
  if (resizehints || c->isfloating || !c->mon->lt[c->mon->sellt]->arrange) {

    baseismin = c->basew == c->minw && c->baseh == c->minh;
    if (!baseismin) {
      *w -= c->basew;
      *h -= c->baseh;
    }

    if (c->mina > 0 && c->maxa > 0) {
      if (c->maxa < (float)*w / *h)
        *w = *h * c->maxa + 0.5;
      else if (c->mina < (float)*h / *w)
        *h = *w * c->mina + 0.5;
    }
    if (baseismin) {
      *w -= c->basew;
      *h -= c->baseh;
    }

    if (c->incw)
      *w -= *w % c->incw;
    if (c->inch)
      *h -= *h % c->inch;

    *w = MAX(*w + c->basew, c->minw);
    *h = MAX(*h + c->baseh, c->minh);
    if (c->maxw)
      *w = MIN(*w, c->maxw);
    if (c->maxh)
      *h = MIN(*h, c->maxh);
  }
  return *x != c->x || *y != c->y || *w != c->w || *h != c->h;
}

void arrange(Monitor *m) {
  if (m)
    showhide(m->stack);
  else
    for (m = mons; m; m = m->next)
      showhide(m->stack);
  if (m) {
    arrangemon(m);
    restack(m);
  } else
    for (m = mons; m; m = m->next)
      arrangemon(m);
}

void arrangemon(Monitor *m) {
  snprintf(m->ltsymbol, sizeof m->ltsymbol, "%s", m->lt[m->sellt]->symbol);
  if (m->lt[m->sellt]->arrange)
    m->lt[m->sellt]->arrange(m);
}

pid_t wintopid(Window window) {
  XResClientIdSpec spec = {
      .client = window,
      .mask = XRES_CLIENT_ID_PID_MASK,
  };
  XResClientIdValue *ids = NULL;
  long count = 0;
  pid_t pid = -1;

  if (!xresclientids)
    return -1;
  if (!XResQueryClientIds(dpy, 1, &spec, &count, &ids))
    return -1;
  for (long i = 0; i < count; i++) {
    pid = XResGetClientPid(&ids[i]);
    if (pid >= 0)
      break;
  }
  XResClientIdsDestroy(count, ids);
  return pid;
}

void copyclientpos(Client *dst, Client *src) {
  dst->mon = src->mon;
  dst->tags = src->tags;
  dst->bw = src->bw;
  dst->isfloating = src->isfloating;
  resizeclient(dst, src->x, src->y, src->w, src->h);
  dst->oldx = src->oldx;
  dst->oldy = src->oldy;
  dst->oldw = src->oldw;
  dst->oldh = src->oldh;
  dst->fsbw = src->fsbw;
  dst->oldstate = src->oldstate;
  dst->isfullscreen = src->isfullscreen;
}

void checkswallowed(Client *c) {
  SwallowDef **link;
  pid_t pid = wintopid(c->win);

  if (pid < 0)
    return;
  for (link = &swallowlist; *link; link = &(*link)->next) {
    SwallowDef *sd = *link;

    if (pid == sd->pid) {
      c->swallower = sd->swallower;
      *link = sd->next;
      free(sd);
      copyclientpos(c, c->swallower);

      c->next_swallowed = c->swallower->swallowed;
      c->swallower->swallowed = c;

      c->next = c->swallower->next;
      c->swallower->next = c;

      c->snext = c->swallower->snext;
      c->swallower->snext = c;

      return;
    }
  }
}

void attach(Client *c) {
  c->next = c->mon->clients;
  c->mon->clients = c;
}

void attachstack(Client *c) {
  c->snext = c->mon->stack;
  c->mon->stack = c;
}

void buttonpress(XEvent *e) {
  unsigned int i, click;
  int x;
  Arg arg = {0};
  Client *c;
  Monitor *m;
  XButtonPressedEvent *ev = &e->xbutton;

  click = ClkRootWin;

  if ((m = wintomon(ev->window)) && m != selmon) {
    if (focusmonOnEnter)
      focusmonitor(m);
    else {
      /* An unfocused client's passive grab freezes pointer AND keyboard.
       * This rejected click bypasses ReplayPointer below: release the grab
       * before returning, or neither device can deliver further input. */
      XUngrabPointer(dpy, CurrentTime);
      XWarpPointer(dpy, None, root, 0, 0, 0, 0,
                   selmon->mx + (selmon->mw / 2),
                   selmon->my + (selmon->mh / 2));
      return;
    }
  }

  if (m && ev->window == m->barwin) {
    i = x = 0;
    do
      x += (int)TAGW(tags[i]);
    while (ev->x >= x && ++i < LENGTH(tags));
    if (i < LENGTH(tags)) {
      click = ClkTagBar;
      arg.ui = 1U << i;
    } else if (ev->x < x + blw)
      click = ClkLtSymbol;
    else if (ev->x >= (int)m->ww - (int)statustraywidth(m) -
                              (int)displayedstatuswidth(m)) {
      click = ClkStatusText;
      arg.ui = statusmoduleat(m, ev->x);
    }
    else {
      unsigned int base, extra, index = 0;

      x += TEXTW(m->ltsymbol);
      if (m->bt > 0 && m->btw > 0) {
        base = (unsigned int)m->btw / (unsigned int)m->bt;
        extra = (unsigned int)m->btw % (unsigned int)m->bt;
        for (c = m->clients; c; c = c->next) {
          unsigned int width;

          if (!ISVISIBLE(c))
            continue;
          width = base + (index++ < extra);
          if (ev->x < (int)(x + width))
            break;
          x += width;
        }
        if (c) {
          click = ClkWinTitle;
          arg.v = c;
        }
      }
    }
  } else if ((c = wintoclient(ev->window))) {
    focus(c);
    restack(selmon);
    XAllowEvents(dpy, ReplayPointer, CurrentTime);
    click = ClkClientWin;
  }
  for (i = 0; i < LENGTH(buttons); i++)
    if (click == buttons[i].click && buttons[i].func &&
        buttons[i].button == ev->button &&
        CLEANMASK(buttons[i].mask) == CLEANMASK(ev->state))
      buttons[i].func((click == ClkTagBar || click == ClkWinTitle ||
                       click == ClkStatusText) &&
                              buttons[i].arg.i == 0
                          ? &arg
                          : &buttons[i].arg);
}

void cagepointer(Monitor *m, int x, int y) {
  const int buffer = 3;

  if (!m)
    return;
  x = MAX(m->mx + buffer, MIN(x, m->mx + m->mw - 1 - buffer));
  y = MAX(m->my + buffer, MIN(y, m->my + m->mh - 1 - buffer));
  XWarpPointer(dpy, None, root, 0, 0, 0, 0, x, y);
}

void checkotherwm(void) {
  xerrorxlib = XSetErrorHandler(xerrorstart);

  XSelectInput(dpy, DefaultRootWindow(dpy), SubstructureRedirectMask);
  XSync(dpy, False);
  XSetErrorHandler(xerror);
  XSync(dpy, False);
}

void cleanup(void) {
  Client *c;
  Layout foo = {"", NULL};
  Monitor *m;
  size_t i;

  for (m = mons; m; m = m->next) {
    m->tagset[m->seltags] = TAGMASK | SPTAG;
    m->lt[m->sellt] = &foo;
    for (c = m->clients; c; c = c->next)
      if (HIDDEN(c))
        showwin(c);
    showhide(m->stack);
  }
  selmon = NULL;
  for (m = mons; m; m = m->next)
    while ((c = m->clients))
      unmanage(c, 0);
  XUngrabKey(dpy, AnyKey, AnyModifier, root);
  while (mons)
    cleanupmon(mons);

  if (showsystray && systray) {
    XUnmapWindow(dpy, systray->win);
    XDestroyWindow(dpy, systray->win);
    free(systray);
  }

  for (i = 0; i < CurLast; i++)
    drw_cur_free(drw, cursor[i]);
  for (i = 0; i < LENGTH(colors); i++)
    drw_scm_free(drw, scheme[i], 3);
  free(scheme);
  XDestroyWindow(dpy, wmcheckwin);
  drw_free(drw);
  XSync(dpy, False);
  XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
  XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
}

void cleanupmon(Monitor *mon) {
  Monitor *m;

  if (mon == mons)
    mons = mons->next;
  else {
    for (m = mons; m && m->next != mon; m = m->next)
      ;
    m->next = mon->next;
  }

  free(mon->pertag);
  XUnmapWindow(dpy, mon->barwin);
  XDestroyWindow(dpy, mon->barwin);
  free(mon);
}

void clientmessage(XEvent *e) {
  XWindowAttributes wa;
  XSetWindowAttributes swa;
  XClientMessageEvent *cme = &e->xclient;
  Client *c = wintoclient(cme->window);

  if (showsystray && systray && cme->window == systray->win &&
      cme->message_type == netatom[NetSystemTrayOP]) {
    if (cme->data.l[1] == SYSTEM_TRAY_REQUEST_DOCK) {
      Window icon;

      if (cme->data.l[2] <= 0)
        return;
      icon = (Window)cme->data.l[2];
      if (wintosystrayicon(icon))
        return;
      if ((c = wintoclient(icon)))
        unmanage(c, 0);
      c = ecalloc(1, sizeof(Client));
      c->win = icon;
      if (!XGetWindowAttributes(dpy, c->win, &wa)) {
        free(c);
        return;
      }
      c->mon = selmon;
      c->next = systray->icons;
      systray->icons = c;
      c->x = c->oldx = c->y = c->oldy = 0;
      c->w = c->oldw = wa.width;
      c->h = c->oldh = wa.height;
      c->oldbw = wa.border_width;
      c->bw = 0;
      c->isfloating = True;
      c->tags = (getxembedflags(c->win) & XEMBED_MAPPED) != 0;

      updatesizehints(c);
      updatesystrayicongeom(c, wa.width, wa.height);
      XAddToSaveSet(dpy, c->win);
      XSelectInput(dpy, c->win,
                   StructureNotifyMask | PropertyChangeMask |
                       ResizeRedirectMask);
      updatesystrayiconidentity(c);
      XReparentWindow(dpy, c->win, systray->win, 0, 0);
      swa.background_pixel = scheme[SchemeNorm][ColBg].pixel;
      XChangeWindowAttributes(dpy, c->win, CWBackPixel, &swa);

      sendevent(c->win, xatom[Xembed], NoEventMask, CurrentTime,
                XEMBED_EMBEDDED_NOTIFY, 0, systray->win,
                XEMBED_EMBEDDED_VERSION);
      sendxembed(c->win, XEMBED_WINDOW_ACTIVATE);
      XSync(dpy, False);
      updatesystray();
      setclientstate(c, c->tags ? NormalState : WithdrawnState);
    }
    return;
  }

  if (!c)
    return;
  if (cme->message_type == netatom[NetWMState]) {
    if (cme->data.l[0] >= 0 && cme->data.l[0] <= 2 &&
        ((Atom)cme->data.l[1] == netatom[NetWMFullscreen] ||
         (Atom)cme->data.l[2] == netatom[NetWMFullscreen]))
      setfullscreen(c, (cme->data.l[0] == 1 ||
                        (cme->data.l[0] == 2 && !c->isfullscreen)));
  } else if (cme->message_type == netatom[NetActiveWindow]) {
    if (c != selmon->sel && !c->isurgent)
      seturgent(c, 1);
  } else if (cme->message_type == swallow_atom) {
    SwallowDef *node;

    if (cme->data.l[0] <= 0 || cme->data.l[0] > INT_MAX)
      return;
    node = ecalloc(1, sizeof(SwallowDef));
    node->pid = cme->data.l[0];
    node->swallower = c;
    node->next = swallowlist;
    swallowlist = node;
    return;
  }
}

void configure(Client *c) {
  XConfigureEvent ce;

  ce.type = ConfigureNotify;
  ce.display = dpy;
  ce.event = c->win;
  ce.window = c->win;
  ce.x = c->x;
  ce.y = c->y;
  ce.width = c->w;
  ce.height = c->h;
  ce.border_width = c->bw;
  ce.above = None;
  ce.override_redirect = False;
  XSendEvent(dpy, c->win, False, StructureNotifyMask, (XEvent *)&ce);
}

void configurenotify(XEvent *e) {
  Monitor *m;
  Client *c;
  XConfigureEvent *ev = &e->xconfigure;
  int dirty;

  if (ev->window == root) {
    dirty = (sw != ev->width || sh != ev->height);
    sw = ev->width;
    sh = ev->height;
    if (updategeom() || dirty) {
      drw_resize(drw, sw, bh);
      updatebars();
      for (m = mons; m; m = m->next) {
        for (c = m->clients; c; c = c->next)
          if (c->isfullscreen)
            resizefullscreen(c, m->mx, m->my, m->mw, m->mh);
        resizebarwin(m);
      }
      arrange(NULL);
      focus(NULL);
      updatesystray();
    }
  }
}

void configurerequest(XEvent *e) {
  Client *c;
  Monitor *m;
  XConfigureRequestEvent *ev = &e->xconfigurerequest;
  XWindowChanges wc;

  if ((c = wintoclient(ev->window))) {
    if (c->isfullscreen) {
      configure(c);
      return;
    }
    if (ev->value_mask & CWBorderWidth) {
      c->bw = ev->border_width;
      XSetWindowBorderWidth(dpy, c->win, (unsigned int)c->bw);
    }
    if (c->isfloating || !c->mon->lt[c->mon->sellt]->arrange) {
      m = c->mon;
      if (ev->value_mask & CWX) {
        c->oldx = c->x;
        c->x = m->mx + ev->x;
      }
      if (ev->value_mask & CWY) {
        c->oldy = c->y;
        c->y = m->my + ev->y;
      }
      if (ev->value_mask & CWWidth) {
        c->oldw = c->w;
        c->w = ev->width;
      }
      if (ev->value_mask & CWHeight) {
        c->oldh = c->h;
        c->h = ev->height;
      }
      if ((c->x + c->w) > m->mx + m->mw && c->isfloating)
        c->x = m->mx + (m->mw / 2 - WIDTH(c) / 2);
      if ((c->y + c->h) > m->my + m->mh && c->isfloating)
        c->y = m->my + (m->mh / 2 - HEIGHT(c) / 2);
      if ((ev->value_mask & (CWX | CWY)) &&
          !(ev->value_mask & (CWWidth | CWHeight)))
        configure(c);
      if (ISVISIBLE(c))
        XMoveResizeWindow(dpy, c->win, c->x, c->y, c->w, c->h);
      else
        c->needresize = 1;
    } else {
      configure(c);
      if (ev->value_mask & CWBorderWidth)
        arrange(c->mon);
    }
  } else {
    wc.x = ev->x;
    wc.y = ev->y;
    wc.width = ev->width;
    wc.height = ev->height;
    wc.border_width = ev->border_width;
    wc.sibling = ev->above;
    wc.stack_mode = ev->detail;
    XConfigureWindow(dpy, ev->window, ev->value_mask, &wc);
  }
  XFlush(dpy);
}

Monitor *createmon(void) {
  Monitor *m;

  unsigned int i;

  m = ecalloc(1, sizeof(Monitor));
  m->tagset[0] = m->tagset[1] = 1;
  m->mfact = mfact;
  m->nmaster = nmaster;
  m->showbar = showbar;
  m->topbar = topbar;
  m->lt[0] = &layouts[0];
  m->lt[1] = &layouts[1 % LENGTH(layouts)];

  m->pertag = ecalloc(1, sizeof(Pertag));
  m->pertag->curtag = m->pertag->prevtag = 1;

  for (i = 0; i <= LENGTH(tags); i++) {
    m->pertag->nmasters[i] = m->nmaster;
    m->pertag->mfacts[i] = m->mfact;

    m->pertag->ltidxs[i][0] = m->lt[0];
    m->pertag->ltidxs[i][1] = m->lt[1];
    m->pertag->sellts[i] = m->sellt;

    m->pertag->showbars[i] = m->showbar;
  }
  snprintf(m->ltsymbol, sizeof m->ltsymbol, "%s", layouts[0].symbol);
  return m;
}

void destroynotify(XEvent *e) {
  Client *c;
  XDestroyWindowEvent *ev = &e->xdestroywindow;

  if ((c = wintoclient(ev->window)))
    unmanage(c, 1);
  else if ((c = wintosystrayicon(ev->window))) {
    removesystrayicon(c);
    updatesystray();
  }
}

void defaultgaps(const Arg *arg) {
  (void)arg;
  gappx = defaultgappx;
  arrange(NULL);
}

void enternotify(XEvent *e) {
  Client *c;
  Monitor *m;
  XCrossingEvent *ev = &e->xcrossing;

  if ((ev->mode != NotifyNormal || ev->detail == NotifyInferior) &&
      ev->window != root)
    return;
  c = wintoclient(ev->window);
  m = c ? c->mon : wintomon(ev->window);

  if (!focusmonOnEnter) {
    if (m != selmon) {
      cagepointer(selmon, ev->x_root, ev->y_root);
      return;
    }
  } else if (m && m != selmon)
    focusmonitor(m);

  if (!c || c == selmon->sel)
    return;
  focus(c);
}

void detach(Client *c) {
  Client **tc;

  for (tc = &c->mon->clients; *tc && *tc != c; tc = &(*tc)->next)
    ;
  if (*tc)
    *tc = c->next;
}

void detachstack(Client *c) {
  Client **tc, *t;

  for (tc = &c->mon->stack; *tc && *tc != c; tc = &(*tc)->snext)
    ;
  if (!*tc)
    return;
  *tc = c->snext;

  if (c == c->mon->sel) {
    for (t = c->mon->stack; t && (!ISVISIBLE(t) || HIDDEN(t)); t = t->snext)
      ;
    c->mon->sel = t;
  }
}

Monitor *dirtomon(int dir) {
  Monitor *m = NULL;

  if (dir > 0) {
    if (!(m = selmon->next))
      m = mons;
  } else if (selmon == mons)
    for (m = mons; m->next; m = m->next)
      ;
  else
    for (m = mons; m->next != selmon; m = m->next)
      ;
  return m;
}

void status2dtext(char *dst, size_t size, const char *src) {
  size_t n = 0;
  int code = 0;

  if (!size)
    return;
  while (*src && n + 1 < size) {
    if (*src == '^') {
      code = !code;
    } else if (!code) {
      dst[n++] = *src;
    }
    src++;
  }
  dst[n] = '\0';
}

static unsigned int statussegmentwidth(const char *text, size_t length) {
  char segment[sizeof stext];

  length = MIN(length, sizeof segment - 1);
  memcpy(segment, text, length);
  segment[length] = '\0';
  return drw_fontset_getwidth(drw, segment);
}

static unsigned int status2dwidth(const char *status) {
  const char *close, *code, *open, *p = status;
  long width = 0;
  unsigned int segment;

  while ((open = strchr(p, '^'))) {
    segment = statussegmentwidth(p, (size_t)(open - p));
    width = segment > (unsigned long)(INT_MAX - width) ? INT_MAX
                                                       : width + segment;
    if (!(close = strchr(open + 1, '^')))
      return (unsigned int)width;
    for (code = open + 1; code < close;) {
      if (*code++ == 'f') {
        char *end;
        long offset;

        errno = 0;
        offset = strtol(code, &end, 10);
        if (end > code && end <= close && errno != ERANGE) {
          if (offset > INT_MAX - width)
            width = INT_MAX;
          else if (offset < -width)
            width = 0;
          else
            width += offset;
          code = end;
        }
      }
    }
    p = close + 1;
  }
  segment = statussegmentwidth(p, strlen(p));
  width = segment > (unsigned long)(INT_MAX - width) ? INT_MAX
                                                     : width + segment;
  return (unsigned int)width;
}

static int statuscolor(const char *text, const char *end, char color[8]) {
  size_t i;

  if (end - text < 7 || text[0] != '#')
    return 0;
  for (i = 1; i < 7; i++)
    if (!isxdigit((unsigned char)text[i]))
      return 0;
  memcpy(color, text, 7);
  color[7] = '\0';
  return 1;
}

static int statusnumber(const char **text, const char *end, int *number) {
  char *after;
  long value;

  errno = 0;
  value = strtol(*text, &after, 10);
  if (after == *text || after > end || errno == ERANGE || value < INT_MIN ||
      value > INT_MAX)
    return 0;
  *text = after;
  *number = (int)value;
  return 1;
}

static int statusadd(int value, int change) {
  if (change > 0 && value > INT_MAX - change)
    return INT_MAX;
  if (change < 0 && value < INT_MIN - change)
    return INT_MIN;
  return value + change;
}

static unsigned int drawstatussegment(int x, int height, const char *text,
                                      size_t length) {
  char segment[sizeof stext];
  unsigned int width;

  length = MIN(length, sizeof segment - 1);
  memcpy(segment, text, length);
  segment[length] = '\0';
  width = drw_fontset_getwidth(drw, segment);
  if (width)
    drw_text(drw, x, 0, width, height, 0, segment, 0);
  return width;
}

unsigned int statusbarwidth(void) {
  return MIN(status2dwidth(stext), (unsigned int)INT_MAX - 2U) + 2U;
}

unsigned int displayedstatuswidth(Monitor *m) {
  unsigned int occupied = statustraywidth(m);
  unsigned int available = m->ww > (int)occupied ? m->ww - occupied : 0;

  return MIN(statusbarwidth(), available);
}

unsigned int statustraywidth(Monitor *m) {
  return showsystray && systray && m == systraytomon(m) ? getsystraywidth()
                                                        : 0;
}

int statusmoduleat(Monitor *m, int clickx) {
  static const char separator[] = "  ";
  char text[sizeof stext];
  char *segment, *sep;
  int i = 0, pos = 0;
  int start = m->ww - (int)statustraywidth(m) -
              (int)displayedstatuswidth(m);
  int rel = MAX(clickx - start - 1, 0);
  int sepwidth = drw_fontset_getwidth(drw, separator);

  status2dtext(text, sizeof text, stext);
  segment = text;
  while (i + 1 < (int)LENGTH(statuscmds) &&
         (sep = strstr(segment, separator))) {
    *sep = '\0';
    if (rel < pos + (int)drw_fontset_getwidth(drw, segment) + sepwidth / 2)
      return i;
    pos += drw_fontset_getwidth(drw, segment) + sepwidth;
    segment = sep + strlen(separator);
    i++;
  }
  return MIN(i, (int)LENGTH(statuscmds) - 1);
}

int drawstatusbar(Monitor *m, int height, const char *status) {
  const char *close, *code, *open, *p = status;
  int custombg = 0, customfg = 0;
  int trayw = (int)statustraywidth(m);
  int available = MAX(m->ww - trayw, 0);
  int total = (int)MIN(status2dwidth(status) + 2U, (unsigned int)available);
  int ret = m->ww - total;
  int x = ret - trayw;
  Clr status_scheme[3];

  memcpy(status_scheme, scheme[SchemeNorm], sizeof status_scheme);
  drw_setscheme(drw, status_scheme);
  drw_rect(drw, x, 0, (unsigned int)total, (unsigned int)height, 1, 1);
  x++;

  while ((open = strchr(p, '^'))) {
    x = statusadd(x,
                  (int)drawstatussegment(x, height, p, (size_t)(open - p)));
    if (!(close = strchr(open + 1, '^'))) {
      p = NULL;
      break;
    }
    for (code = open + 1; code < close;) {
      if (*code == 'c' || *code == 'b') {
        int *custom = *code == 'c' ? &customfg : &custombg;
        int index = *code == 'c' ? ColFg : ColBg;
        char color[8];

        if (statuscolor(code + 1, close, color)) {
          if (*custom)
            drw_clr_free(drw, &status_scheme[index]);
          drw_clr_create(drw, &status_scheme[index], color);
          *custom = 1;
          code += 8;
        } else
          code++;
      } else if (*code == 'd') {
        if (customfg)
          drw_clr_free(drw, &status_scheme[ColFg]);
        if (custombg)
          drw_clr_free(drw, &status_scheme[ColBg]);
        status_scheme[ColFg] = scheme[SchemeNorm][ColFg];
        status_scheme[ColBg] = scheme[SchemeNorm][ColBg];
        customfg = custombg = 0;
        code++;
      } else if (*code == 'f') {
        const char *number = code + 1;
        int offset;

        if (statusnumber(&number, close, &offset)) {
          x = MAX(0, MIN(statusadd(x, offset), m->ww));
          code = number;
        } else
          code++;
      } else if (*code == 'r') {
        const char *number = code + 1;
        int rh, rw, rx, ry;

        if (statusnumber(&number, close, &rx) && number < close &&
            *number++ == ',' && statusnumber(&number, close, &ry) &&
            number < close && *number++ == ',' &&
            statusnumber(&number, close, &rw) && number < close &&
            *number++ == ',' && statusnumber(&number, close, &rh)) {
          if (rw > 0 && rh > 0)
            drw_rect(drw, statusadd(x, rx), ry, (unsigned int)rw,
                     (unsigned int)rh, 1, 0);
          code = number;
        } else
          code++;
      } else
        code++;
    }
    p = close + 1;
  }
  if (p)
    drawstatussegment(x, height, p, strlen(p));

  if (customfg)
    drw_clr_free(drw, &status_scheme[ColFg]);
  if (custombg)
    drw_clr_free(drw, &status_scheme[ColBg]);
  drw_setscheme(drw, scheme[SchemeNorm]);
  return ret;
}

void drawbar(Monitor *m) {
  int x, w, tw = 0, stw = 0, n = 0, scm;
  int boxs = drw->fonts->h / 9;
  int boxw = drw->fonts->h / 6 + 2;
  unsigned int i, occ = 0, urg = 0;
  Client *c;

  if (!m->showbar)
    return;

  stw = statustraywidth(m);
  tw = m->ww - drawstatusbar(m, bh, stext);

  for (c = m->clients; c; c = c->next) {
    if (ISVISIBLE(c))
      n++;
    occ |= c->tags;
    if (c->isurgent)
      urg |= c->tags;
  }
  x = 0;
  for (i = 0; i < LENGTH(tags); i++) {
    w = TAGW(tags[i]);
    drw_setscheme(
        drw,
        scheme[m->tagset[m->seltags] & 1U << i ? SchemeSel
               : (occ & 1U << i ? SchemeNorm : SchemeHid)]);
    drw_text(drw, x, 0, w, bh, lrpad / 4, tags[i], urg & 1U << i);
    if (occ & 1U << i)
      drw_rect(drw, x + boxs, boxs, boxw, boxw,
               m == selmon && selmon->sel && selmon->sel->tags & 1U << i,
               urg & 1U << i);
    x += w;
  }
  w = blw = TEXTW(m->ltsymbol);
  drw_setscheme(drw, scheme[SchemeNorm]);
  x = drw_text(drw, x, 0, w, bh, lrpad / 2, m->ltsymbol, 0);

  if ((w = m->ww - tw - stw - x) > bh) {
    if (n > 0 && w >= n) {
      int base = w / n;
      int extra = w % n;
      int index = 0;

      for (c = m->clients; c; c = c->next) {
        int tabw;

        if (!ISVISIBLE(c))
          continue;
        if (m->sel == c)
          scm = (n > 1) ? SchemeTitle : SchemeNorm;
        else if (HIDDEN(c))
          scm = SchemeHid;
        else
          scm = SchemeNorm;
        drw_setscheme(drw, scheme[scm]);
        tabw = base + (index++ < extra);
        drw_text(drw, x, 0, (unsigned int)tabw, bh, lrpad / 2, c->appname, 0);
        x += tabw;
      }
    } else {
      drw_setscheme(drw, scheme[SchemeNorm]);
      drw_rect(drw, x, 0, w, bh, 1, 1);
    }
  }
  m->bt = w > bh && w >= n ? n : 0;
  m->btw = m->bt ? w : 0;
  drw_map(drw, m->barwin, 0, 0, m->ww, bh);
}

void drawbars(void) {
  Monitor *m;

  for (m = mons; m; m = m->next)
    drawbar(m);
}

void expose(XEvent *e) {
  Monitor *m;
  XExposeEvent *ev = &e->xexpose;

  if (ev->count != 0)
    return;
  if (showsystray && systray && ev->window == systray->win) {
    updatesystray();
    return;
  }
  for (m = mons; m && ev->window != m->barwin; m = m->next)
    ;
  if (m)
    drawbar(m);
}

Client *findbefore(Client *c) {
  Client *tmp;

  if (!c || c == selmon->clients)
    return NULL;
  for (tmp = selmon->clients; tmp && tmp->next != c; tmp = tmp->next)
    ;
  return tmp && tmp->next == c ? tmp : NULL;
}

void focus(Client *c) {
  Monitor *oldmon = selmon;

  if (!c || !ISVISIBLE(c))
    for (c = selmon->stack; c && (!ISVISIBLE(c) || HIDDEN(c)); c = c->snext)
      ;
  if (selmon->sel && selmon->sel != c) {
    unfocus(selmon->sel, 0);
    if (selmon->hidsel) {
      hidewin(selmon->sel);
      selmon->hidsel = 0;
      oldmon->sel = NULL;
      arrange(oldmon);
    }
  }
  if (c) {
    if (c->mon != selmon)
      selmon = c->mon;
    if (c->isurgent)
      seturgent(c, 0);
    detachstack(c);
    attachstack(c);
    grabbuttons(c, 1);
    XSetWindowBorder(dpy, c->win, scheme[SchemeSel][ColBorder].pixel);
    setfocus(c);
  } else {
    XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
    XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
  }
  selmon->sel = c;
  if (oldmon)
    drawbar(oldmon);
  if (selmon != oldmon)
    drawbar(selmon);
}

void focusin(XEvent *e) {
  XFocusChangeEvent *ev = &e->xfocus;

  if (selmon->sel && ev->window != selmon->sel->win)
    setfocus(selmon->sel);
}

void focusmon(const Arg *arg) {
  Monitor *m;

  if (!mons->next)
    return;
  if ((m = dirtomon(arg->i)) == selmon)
    return;
  focusmonitor(m);

  if (selmon->sel) {
    XWarpPointer(dpy, None, selmon->sel->win, 0, 0, 0, 0, selmon->sel->w / 2,
                 selmon->sel->h / 2);
  } else {
    XWarpPointer(dpy, None, root, 0, 0, 0, 0, selmon->mx + (selmon->mw / 2),
                 selmon->my + (selmon->mh / 2));
  }
}

void focusmonitor(Monitor *m) {
  Monitor *old = selmon;

  if (!m || m == old)
    return;
  if (!old) {
    selmon = m;
    focus(NULL);
    return;
  }
  unfocus(old->sel, 1);
  if (old->hidsel && old->sel) {
    Client *c;

    hidewin(old->sel);
    old->hidsel = 0;
    for (c = old->stack; c && (!ISVISIBLE(c) || HIDDEN(c)); c = c->snext)
      ;
    old->sel = c;
    arrange(old);
  }
  selmon = m;
  focus(NULL);
}

void focusstackvis(const Arg *arg) {
  focusstack(arg->i, 0);
}

void focusstackhid(const Arg *arg) {
  focusstack(arg->i, 1);
}

void focusstack(int inc, int hid) {
  Client *c = NULL, *i;
  int washidden;

  if (selmon->sel && selmon->sel->isfullscreen && lockfullscreen)
    return;
  if (!selmon->clients)
    return;

  if (inc > 0) {
    int after = selmon->sel == NULL;

    for (i = selmon->clients; i; i = i->next) {
      if (i == selmon->sel) {
        after = 1;
        continue;
      }
      if (after && ISVISIBLE(i) && (hid || !HIDDEN(i))) {
        c = i;
        break;
      }
    }
    if (!c)
      for (c = selmon->clients;
           c && (!ISVISIBLE(c) || (!hid && HIDDEN(c))); c = c->next)
        ;
  } else {
    for (i = selmon->clients; i && i != selmon->sel; i = i->next)
      if (ISVISIBLE(i) && (hid || !HIDDEN(i)))
        c = i;
    if (!c)
      for (i = selmon->clients; i; i = i->next)
        if (ISVISIBLE(i) && (hid || !HIDDEN(i)))
          c = i;
  }
  if (c) {
    washidden = HIDDEN(c);
    if (washidden)
      showwin(c);
    focus(c);
    if (washidden) {
      c->mon->hidsel = 1;
      arrange(c->mon);
    } else
      restack(selmon);
    XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->w / 2, c->h / 2);
  }
}

unsigned long getxembedflags(Window w) {
  Atom actual;
  int format;
  unsigned char *value = NULL;
  unsigned long bytes_after, flags = 0, nitems;

  if (XGetWindowProperty(dpy, w, xatom[XembedInfo], 0L, 2L, False,
                         xatom[XembedInfo], &actual, &format, &nitems,
                         &bytes_after, &value) == Success &&
      actual == xatom[XembedInfo] && format == 32 && nitems >= 2)
    flags = ((unsigned long *)value)[1];
  if (value)
    XFree(value);
  return flags;
}

int hasatomprop(Window w, Atom prop, Atom target) {
  Atom actual;
  int format, found = 0;
  unsigned char *value = NULL;
  unsigned long bytes_after, i, nitems, offset = 0;

  do {
    if (XGetWindowProperty(dpy, w, prop, (long)offset, 32L, False, XA_ATOM,
                           &actual, &format, &nitems, &bytes_after, &value) !=
            Success ||
        actual != XA_ATOM || format != 32)
      break;
    for (i = 0; i < nitems; i++)
      if (((Atom *)value)[i] == target) {
        found = 1;
        break;
      }
    offset += nitems;
    XFree(value);
    value = NULL;
  } while (!found && bytes_after && nitems);

  if (value)
    XFree(value);
  return found;
}

int isxembedicon(Window w) {
  Atom actual;
  int format, result;
  unsigned char *value = NULL;
  unsigned long bytes_after, nitems;

  result = XGetWindowProperty(dpy, w, xatom[XembedInfo], 0L, 2L, False,
                              xatom[XembedInfo], &actual, &format, &nitems,
                              &bytes_after, &value) == Success &&
           actual == xatom[XembedInfo] && format == 32 && nitems >= 2;
  if (value)
    XFree(value);
  return result &&
         !hasatomprop(w, netatom[NetWMWindowType],
                      netatom[NetWMWindowTypeNormal]);
}

void setatomprop(Client *c, Atom prop, Atom target, int enabled) {
  Atom actual;
  int format, found = 0;
  unsigned char *value = NULL;
  unsigned long bytes_after, i, nitems;

  if (XGetWindowProperty(dpy, c->win, prop, 0L, 64L, False, XA_ATOM, &actual,
                         &format, &nitems, &bytes_after, &value) != Success)
    return;
  if (actual == XA_ATOM && format == 32)
    for (i = 0; i < nitems; i++)
      if (((Atom *)value)[i] == target) {
        found = 1;
        break;
      }

  if (bytes_after) {
    if (value)
      XFree(value);
    return;
  }
  if (enabled && !found) {
    XChangeProperty(dpy, c->win, prop, XA_ATOM, 32,
                    actual == XA_ATOM && format == 32 ? PropModeAppend
                                                      : PropModeReplace,
                    (unsigned char *)&target, 1);
  } else if (!enabled && found) {
    memmove(&((Atom *)value)[i], &((Atom *)value)[i + 1],
            (nitems - i - 1) * sizeof(Atom));
    XChangeProperty(dpy, c->win, prop, XA_ATOM, 32, PropModeReplace, value,
                    (int)nitems - 1);
  }
  if (value)
    XFree(value);
}

unsigned int getsystraywidth(void) {
  unsigned int w = 0;
  Client *i;

  if (showsystray && systray)
    for (i = systray->icons; i; i = i->next)
      if (i->tags)
        w += i->w + systrayspacing;
  return w ? w + systrayspacing : 1;
}

Client *getclientundermouse(void) {
  int ret, di;
  unsigned int dui;
  Window child, dummy;

  ret = XQueryPointer(dpy, root, &dummy, &child, &di, &di, &di, &di, &dui);
  if (!ret)
    return NULL;

  return wintoclient(child);
}

int getrootptr(int *x, int *y) {
  int di;
  unsigned int dui;
  Window dummy;

  return XQueryPointer(dpy, root, &dummy, &dummy, x, y, &di, &di, &dui);
}

long getstate(Window w) {
  int format;
  long result = -1;
  unsigned char *p = NULL;
  unsigned long n, extra;
  Atom real;

  if (XGetWindowProperty(dpy, w, wmatom[WMState], 0L, 2L, False,
                         wmatom[WMState], &real, &format, &n, &extra,
                         (unsigned char **)&p) != Success ||
      !p)
    return -1;
  if (real == wmatom[WMState] && format == 32 && n > 0)
    result = *(long *)p;
  XFree(p);
  return result;
}

int gettextprop(Window w, Atom atom, char *text, unsigned int size) {
  char **list = NULL;
  int n, result;
  XTextProperty name;

  if (!text || size == 0)
    return 0;
  text[0] = '\0';
  if (!XGetTextProperty(dpy, w, &name, atom) || !name.nitems)
    return 0;
  if (name.encoding == XA_STRING)
    strncpy(text, (char *)name.value, size - 1);
  else {
    result = XmbTextPropertyToTextList(dpy, &name, &list, &n);
    if (result >= Success && list && n > 0 && *list)
      strncpy(text, *list, size - 1);
    if (list)
      XFreeStringList(list);
  }
  text[size - 1] = '\0';
  XFree(name.value);
  return text[0] != '\0';
}

void grabbuttons(Client *c, int focused) {
  unsigned int i, j;
  unsigned int modifiers[] = {0, LockMask, numlockmask,
                              numlockmask | LockMask};

  XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
  if (!focused)
    XGrabButton(dpy, AnyButton, AnyModifier, c->win, False, BUTTONMASK,
                GrabModeSync, GrabModeSync, None, None);
  for (i = 0; i < LENGTH(buttons); i++)
    if (buttons[i].click == ClkClientWin)
      for (j = 0; j < LENGTH(modifiers); j++)
        XGrabButton(dpy, buttons[i].button, buttons[i].mask | modifiers[j],
                    c->win, False, BUTTONMASK, GrabModeAsync, GrabModeSync,
                    None, None);
}

void grabkeys(void) {
  updatenumlockmask();
  {
    unsigned int i, j;
    unsigned int modifiers[] = {0, LockMask, numlockmask,
                                numlockmask | LockMask};
    KeyCode code;

    XUngrabKey(dpy, AnyKey, AnyModifier, root);
    for (i = 0; i < LENGTH(keys); i++)
      if ((code = XKeysymToKeycode(dpy, keys[i].keysym)))
        for (j = 0; j < LENGTH(modifiers); j++)
          XGrabKey(dpy, code, keys[i].mod | modifiers[j], root, True,
                   GrabModeAsync, GrabModeAsync);
  }
}

void hide(const Arg *arg) {
  (void)arg;
  if (!selmon->sel)
    return;
  hidewin(selmon->sel);
  focus(NULL);
  arrange(selmon);
}

void hidewin(Client *c) {
  XWindowAttributes clientattrs, rootattrs;

  if (!c || HIDDEN(c))
    return;
  XGrabServer(dpy);
  if (!XGetWindowAttributes(dpy, root, &rootattrs) ||
      !XGetWindowAttributes(dpy, c->win, &clientattrs)) {
    XUngrabServer(dpy);
    return;
  }
  XSelectInput(dpy, root,
               rootattrs.your_event_mask & ~SubstructureNotifyMask);
  XSelectInput(dpy, c->win,
               clientattrs.your_event_mask & ~StructureNotifyMask);
  XUnmapWindow(dpy, c->win);
  setclientstate(c, IconicState);
  XSelectInput(dpy, root, rootattrs.your_event_mask);
  XSelectInput(dpy, c->win, clientattrs.your_event_mask);
  XUngrabServer(dpy);
}

void incnmaster(const Arg *arg) {
  selmon->nmaster = selmon->pertag->nmasters[selmon->pertag->curtag] =
      MAX(selmon->nmaster + arg->i, 0);
  arrange(selmon);
}

#ifdef XINERAMA
static int isuniquegeom(XineramaScreenInfo *unique, size_t n,
                        XineramaScreenInfo *info) {
  while (n--)
    if (unique[n].x_org == info->x_org && unique[n].y_org == info->y_org &&
        unique[n].width == info->width && unique[n].height == info->height)
      return 0;
  return 1;
}
#endif

void keypress(XEvent *e) {
  unsigned int i;
  KeySym keysym;
  XKeyEvent *ev;

  ev = &e->xkey;
  keysym = XKeycodeToKeysym(dpy, (KeyCode)ev->keycode, 0);
  for (i = 0; i < LENGTH(keys); i++)
    if (keysym == keys[i].keysym &&
        CLEANMASK(keys[i].mod) == CLEANMASK(ev->state) && keys[i].func)
      keys[i].func(&(keys[i].arg));
}

void killclient(const Arg *arg) {
  (void)arg;
  if (!selmon->sel)
    return;

  if (!sendevent(selmon->sel->win, wmatom[WMDelete], NoEventMask,
                 wmatom[WMDelete], CurrentTime, 0, 0, 0)) {
    XGrabServer(dpy);
    XSetErrorHandler(xerrordummy);
    XSetCloseDownMode(dpy, DestroyAll);
    XKillClient(dpy, selmon->sel->win);
    XSync(dpy, False);
    XSetErrorHandler(xerror);
    XUngrabServer(dpy);
  }
}

void manage(Window w, XWindowAttributes *wa) {
  Client *c, *t = NULL;
  Window trans = None;
  XWindowChanges wc;

  c = ecalloc(1, sizeof(Client));
  c->win = w;
  c->ishidden = getstate(w) == IconicState;

  c->x = c->oldx = wa->x;
  c->y = c->oldy = wa->y;
  c->w = c->oldw = wa->width;
  c->h = c->oldh = wa->height;
  c->bw = borderpx;
  c->oldbw = wa->border_width;

  updatetitle(c);
  if (XGetTransientForHint(dpy, w, &trans) && (t = wintoclient(trans))) {
    c->mon = t->mon;
    c->tags = t->tags;
  } else {
    c->mon = selmon;
    applyrules(c);
  }

  if (c->x + WIDTH(c) > c->mon->mx + c->mon->mw)
    c->x = c->mon->mx + c->mon->mw - WIDTH(c);
  if (c->y + HEIGHT(c) > c->mon->my + c->mon->mh)
    c->y = c->mon->my + c->mon->mh - HEIGHT(c);
  c->x = MAX(c->x, c->mon->mx);

  c->y = MAX(c->y,
             ((c->mon->by == c->mon->my) && (c->x + (c->w / 2) >= c->mon->wx) &&
              (c->x + (c->w / 2) < c->mon->wx + c->mon->ww))
                 ? c->mon->my + bh
                 : c->mon->my);
  checkswallowed(c);
  wc.border_width = c->bw;
  XConfigureWindow(dpy, w, CWBorderWidth, &wc);
  XSetWindowBorder(dpy, w, scheme[SchemeNorm][ColBorder].pixel);
  configure(c);
  updatewindowtype(c);
  updatesizehints(c);
  if (c->tags & SPTAG) {
    c->x = c->mon->wx + (c->mon->ww - WIDTH(c)) / 2;
    c->y = c->mon->wy + (c->mon->wh - HEIGHT(c)) / 2;
  }
  updatewmhints(c);
  XSelectInput(dpy, w,
               EnterWindowMask | FocusChangeMask | PropertyChangeMask |
                   StructureNotifyMask);
  grabbuttons(c, 0);
  if (!c->isfloating)
    c->isfloating = c->oldstate = trans != None || c->isfixed;
  if (c->isfloating)
    XRaiseWindow(dpy, c->win);
  if (!c->swallower) {
    attach(c);
    attachstack(c);
  }
  XChangeProperty(dpy, root, netatom[NetClientList], XA_WINDOW, 32,
                  PropModeAppend, (unsigned char *)&(c->win), 1);
  XMoveResizeWindow(dpy, c->win, c->x + 2 * sw, c->y, c->w, c->h);
  if (!HIDDEN(c))
    setclientstate(c, NormalState);
  if (c->mon == selmon)
    unfocus(selmon->sel, 0);
  c->mon->sel = c;
  arrange(c->mon);
  if (!HIDDEN(c))
    XMapWindow(dpy, c->win);

  if (c->mon == selmon && ISVISIBLE(c) && !HIDDEN(c))
    XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->w / 2, c->h / 2);
  focus(NULL);
}

void mappingnotify(XEvent *e) {
  Client *c;
  Monitor *m;
  XMappingEvent *ev = &e->xmapping;

  XRefreshKeyboardMapping(ev);
  if (ev->request == MappingKeyboard || ev->request == MappingModifier) {
    grabkeys();
    if (ev->request == MappingModifier)
      for (m = mons; m; m = m->next)
        for (c = m->clients; c; c = c->next)
          grabbuttons(c, c == m->sel);
  }
}

void maprequest(XEvent *e) {
  static XWindowAttributes wa;
  XMapRequestEvent *ev = &e->xmaprequest;

  Client *i;
  if ((i = wintosystrayicon(ev->window))) {
    sendxembed(i->win, XEMBED_WINDOW_ACTIVATE);
    updatesystray();
    return;
  }
  if (showsystray && systray && isxembedicon(ev->window))
    return;

  if (!XGetWindowAttributes(dpy, ev->window, &wa))
    return;
  if (wa.override_redirect)
    return;
  if (!wintoclient(ev->window))
    manage(ev->window, &wa);
}

void monocle(Monitor *m) {
  unsigned int n = 0;
  Client *c;

  for (c = m->clients; c; c = c->next)
    if (ISVISIBLE(c))
      n++;
  if (n > 0)
    snprintf(m->ltsymbol, sizeof m->ltsymbol, "[%u]", n);
  for (c = nexttiled(m->clients); c; c = nexttiled(c->next))
    resize(c, m->wx, m->wy, m->ww - 2 * c->bw, m->wh - 2 * c->bw, 0);
}

void motionnotify(XEvent *e) {
  static Monitor *mon = NULL;
  Monitor *m;
  XMotionEvent *ev = &e->xmotion;

  if (ev->window != root)
    return;

  if (!focusmonOnEnter && selmon) {
    if (ev->x_root < selmon->mx || ev->x_root >= selmon->mx + selmon->mw ||
        ev->y_root < selmon->my || ev->y_root >= selmon->my + selmon->mh)
      cagepointer(selmon, ev->x_root, ev->y_root);
    return;
  }

  if (!mon)
    mon = selmon;
  if ((m = recttomon(ev->x_root, ev->y_root, 1, 1)) != mon)
    focusmonitor(m);
  mon = m;
}

void movemouse(const Arg *arg) {
  int x, y, ocx, ocy, nx, ny;
  Client *c;
  Monitor *m;
  XEvent ev;
  Time lasttime = 0;

  (void)arg;
  if (!(c = selmon->sel))
    return;
  if (c->isfullscreen)
    return;
  restack(selmon);
  ocx = c->x;
  ocy = c->y;
  if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
                   None, cursor[CurMove]->cursor, CurrentTime) != GrabSuccess)
    return;
  if (!getrootptr(&x, &y)) {
    XUngrabPointer(dpy, CurrentTime);
    return;
  }
  do {
    XMaskEvent(dpy, MOUSEMASK | ExposureMask | SubstructureRedirectMask, &ev);
    switch (ev.type) {
    case ConfigureRequest:
    case Expose:
    case MapRequest:
      handler[ev.type](&ev);

      if (c->swallowed)
        c = c->swallowed;
      break;
    case MotionNotify:
      if ((ev.xmotion.time - lasttime) <= (1000 / 60))
        continue;
      lasttime = ev.xmotion.time;

      nx = ocx + (ev.xmotion.x - x);
      ny = ocy + (ev.xmotion.y - y);
      if (abs(selmon->wx - nx) < snap)
        nx = selmon->wx;
      else if (abs((selmon->wx + selmon->ww) - (nx + WIDTH(c))) < snap)
        nx = selmon->wx + selmon->ww - WIDTH(c);
      if (abs(selmon->wy - ny) < snap)
        ny = selmon->wy;
      else if (abs((selmon->wy + selmon->wh) - (ny + HEIGHT(c))) < snap)
        ny = selmon->wy + selmon->wh - HEIGHT(c);
      if (!c->isfloating && selmon->lt[selmon->sellt]->arrange &&
          (abs(nx - c->x) > snap || abs(ny - c->y) > snap))
        togglefloating(NULL);
      if (!selmon->lt[selmon->sellt]->arrange || c->isfloating)
        resize(c, nx, ny, c->w, c->h, 1);
      break;
    }
  } while (ev.type != ButtonRelease);
  XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1,
               c->h + c->bw - 1);
  XUngrabPointer(dpy, CurrentTime);
  if ((m = recttomon(c->x, c->y, c->w, c->h)) != selmon) {
    sendmon(c, m);
    focusmonitor(m);
  }
}

Client *nexttiled(Client *c) {
  for (; c && (c->isfloating || !ISVISIBLE(c) || HIDDEN(c)); c = c->next)
    ;
  return c;
}

void propertynotify(XEvent *e) {
  Client *c;
  Window trans;
  XPropertyEvent *ev = &e->xproperty;

  if ((c = wintosystrayicon(ev->window))) {
    if (ev->atom == XA_WM_CLASS || ev->atom == XA_WM_NAME ||
        ev->atom == netatom[NetWMName]) {
      updatesystrayiconidentity(c);
      updatesystray();
    } else if (ev->atom == XA_WM_NORMAL_HINTS) {
      updatesizehints(c);
      updatesystrayicongeom(c, c->w, c->h);
      updatesystray();
    } else if (ev->atom == xatom[XembedInfo]) {
      updatesystrayiconstate(c, ev);
      updatesystray();
    }
    return;
  }

  if ((ev->window == root) && (ev->atom == XA_WM_NAME))
    updatestatus();
  else if ((ev->window == root) && (ev->atom == theme_atom))
    reloadtheme();
  else if ((c = wintoclient(ev->window))) {
    if (ev->atom == XA_WM_CLASS) {
      updatetitle(c);
      drawbar(c->mon);
      return;
    }
    if (ev->state == PropertyDelete) {
      if (ev->atom == netatom[NetWMState])
        setfullscreen(c, 0);
      else if (ev->atom == XA_WM_HINTS) {
        c->isurgent = c->neverfocus = 0;
        drawbar(c->mon);
      } else if (ev->atom == XA_WM_NAME || ev->atom == netatom[NetWMName]) {
        updatetitle(c);
        if (c == c->mon->sel)
          drawbar(c->mon);
      }
      return;
    }
    switch (ev->atom) {
    default:
      break;
    case XA_WM_TRANSIENT_FOR:
      if (!c->isfloating && (XGetTransientForHint(dpy, c->win, &trans)) &&
          (c->isfloating = (wintoclient(trans)) != NULL))
        arrange(c->mon);
      break;
    case XA_WM_NORMAL_HINTS:
      updatesizehints(c);
      break;
    case XA_WM_HINTS:
      updatewmhints(c);
      drawbar(c->mon);
      break;
    }
    if (ev->atom == XA_WM_NAME || ev->atom == netatom[NetWMName]) {
      updatetitle(c);
      if (c == c->mon->sel)
        drawbar(c->mon);
    }
    if (ev->atom == netatom[NetWMWindowType]) {
      int wasfloating = c->isfloating;

      updatewindowtype(c);
      if (c->isfloating != wasfloating)
        arrange(c->mon);
    } else if (ev->atom == netatom[NetWMState])
      updatewindowtype(c);
  }
}

void reloadtheme(void) {
  enum { ThemeColorCount = 10 };
  Atom actual_type;
  int actual_format;
  unsigned long bytes_after, nitems;
  unsigned char *value = NULL;
  char property[256], parsed[ThemeColorCount][8];
  char *saveptr = NULL, *token;
  char *theme_colors[ThemeColorCount] = {
      col_bg,      col_fg_norm, col_border_norm, col_bg_sel, col_fg_sel,
      col_border_sel,  col_bg_tab,  col_bg_back,     col_fg_tab, col_fg_hid};
  Client *c;
  Monitor *m;
  size_t i, j;

  if (XGetWindowProperty(dpy, root, theme_atom, 0L,
                         (long)(sizeof(property) / 4), False, AnyPropertyType,
                         &actual_type, &actual_format, &nitems, &bytes_after,
                         &value) != Success ||
      !value || actual_type != XA_STRING || actual_format != 8 || bytes_after ||
      nitems >= sizeof(property)) {
    if (value)
      XFree(value);
    return;
  }

  memcpy(property, value, nitems);
  property[nitems] = '\0';
  XFree(value);

  token = strtok_r(property, " \t\r\n", &saveptr);
  for (i = 0; i < ThemeColorCount; i++) {
    if (!token || strlen(token) != 7 || token[0] != '#')
      return;
    for (j = 1; j < 7; j++)
      if (!isxdigit((unsigned char)token[j]))
        return;
    memcpy(parsed[i], token, 8);
    token = strtok_r(NULL, " \t\r\n", &saveptr);
  }
  if (token)
    return;

  for (i = 0; i < ThemeColorCount; i++)
    memcpy(theme_colors[i], parsed[i], 8);

  if (!scheme)
    return;
  for (i = 0; i < LENGTH(colors); i++)
    drw_scm_free(drw, scheme[i], 3);
  for (i = 0; i < LENGTH(colors); i++)
    scheme[i] = drw_scm_create(drw, colors[i], 3);
  drw_setscheme(drw, scheme[SchemeNorm]);

  for (m = mons; m; m = m->next)
    for (c = m->clients; c; c = c->next)
      XSetWindowBorder(
          dpy, c->win,
          scheme[m == selmon && c == m->sel ? SchemeSel : SchemeNorm]
                [ColBorder]
                    .pixel);

  if (showsystray && systray) {
    XSetWindowBackground(dpy, systray->win, scheme[SchemeNorm][ColBg].pixel);
    XClearWindow(dpy, systray->win);
    updatesystray();
  }
  if (mons && mons->barwin)
    drawbars();
  XSync(dpy, False);
}

void saveSession(void) {
  FILE *fw = fopen(sessionfile, "w");
  Monitor *m;
  Client *c;

  if (!fw) {
    fprintf(stderr, "dwm: cannot save session: ");
    perror(sessionfile);
    return;
  }

  fprintf(fw, "S %d\n", selmon ? selmon->num : 0);
  for (m = mons; m; m = m->next) {
    fprintf(fw, "M %d %u %u %u\n", m->num, m->tagset[0], m->tagset[1],
            m->seltags);
    for (c = m->clients; c; c = c->next)
      fprintf(fw, "C %lu %u %d\n", c->win, c->tags, m->num);
  }
  if (fclose(fw) == EOF)
    fprintf(stderr, "dwm: cannot finish saving session\n");
}

void restoreSession(void) {
  const char *sessionpath = sessionfile;
  FILE *fr = fopen(sessionpath, "r");
  Monitor *m, *target;
  Client *c, *found;
  char line[128];

  if (!fr) {
    sessionpath = "/tmp/dwm-session";
    fr = fopen(sessionpath, "r");
    if (!fr)
      return;
  }

  while (fgets(line, sizeof line, fr)) {
    if (line[0] == 'S') {
      int monnum;

      if (sscanf(line + 1, "%d", &monnum) != 1)
        continue;
      for (m = mons; m && m->num != monnum; m = m->next)
        ;
      if (m)
        selmon = m;
    } else if (line[0] == 'M') {
      int monnum;
      unsigned int tagset0, tagset1, seltags;
      unsigned int active;
      int i;

      if (sscanf(line + 1, "%d %u %u %u", &monnum, &tagset0, &tagset1,
                 &seltags) != 4)
        continue;
      for (m = mons; m && m->num != monnum; m = m->next)
        ;
      if (!m)
        continue;
      m->tagset[0] = tagset0 & (TAGMASK | SPTAG);
      m->tagset[1] = tagset1 & (TAGMASK | SPTAG);
      if (!(m->tagset[0] & TAGMASK))
        m->tagset[0] |= 1U;
      if (!(m->tagset[1] & TAGMASK))
        m->tagset[1] |= 1U;
      m->seltags = seltags & 1U;
      active = m->tagset[m->seltags] & TAGMASK;
      m->pertag->curtag = active == TAGMASK ? 0 : 1;
      if (active != TAGMASK)
        for (i = 0; i < (int)LENGTH(tags); i++)
          if (active & (1U << i)) {
            m->pertag->curtag = i + 1;
            break;
          }
      m->pertag->prevtag = m->pertag->curtag;
      m->nmaster = m->pertag->nmasters[m->pertag->curtag];
      m->mfact = m->pertag->mfacts[m->pertag->curtag];
      m->sellt = m->pertag->sellts[m->pertag->curtag];
      m->lt[0] = m->pertag->ltidxs[m->pertag->curtag][0];
      m->lt[1] = m->pertag->ltidxs[m->pertag->curtag][1];
    } else if (line[0] == 'C') {
      unsigned long winid;
      unsigned int clienttags;
      int monnum;

      if (sscanf(line + 1, "%lu %u %d", &winid, &clienttags, &monnum) != 3)
        continue;
      found = NULL;
      for (m = mons; m && !found; m = m->next)
        for (c = m->clients; c; c = c->next)
          if (c->win == winid) {
            found = c;
            break;
          }
      if (!found)
        continue;
      for (target = mons; target && target->num != monnum;
           target = target->next)
        ;
      if (target && found->mon != target) {
        detach(found);
        detachstack(found);
        found->mon = target;
        attach(found);
        attachstack(found);
      }
      found->tags = clienttags & (TAGMASK | SPTAG);
      if (!found->tags)
        found->tags = found->mon->tagset[found->mon->seltags] & TAGMASK;
    }
  }

  fclose(fr);
  remove(sessionpath);
  focus(NULL);
  arrange(NULL);
  drawbars();
}

void quit(const Arg *arg) {
  restart = arg && arg->i;
  if (restart)
    saveSession();
  else {
    remove(sessionfile);
    remove("/tmp/dwm-session");
  }

  running = 0;
}

Monitor *recttomon(int x, int y, int w, int h) {
  Monitor *m, *r = selmon;
  int a, area = 0;

  for (m = mons; m; m = m->next)
    if ((a = INTERSECT(x, y, w, h, m)) > area) {
      area = a;
      r = m;
    }
  return r;
}

void removesystrayicon(Client *i) {
  Client **ii;

  if (!showsystray || !systray || !i)
    return;
  for (ii = &systray->icons; *ii && *ii != i; ii = &(*ii)->next)
    ;
  if (*ii) {
    *ii = i->next;
    free(i);
  }
}

void resize(Client *c, int x, int y, int w, int h, int interact) {
  if (applysizehints(c, &x, &y, &w, &h, interact))
    resizeclient(c, x, y, w, h);
}

void resizebarwin(Monitor *m) {
  XMoveResizeWindow(dpy, m->barwin, m->wx, m->by, m->ww, bh);
}

void resizeclient(Client *c, int x, int y, int w, int h) {
  XWindowChanges wc;
  int gapoffset;
  int gapincr;

  wc.border_width = c->bw;

  if (c->isfloating || c->mon->lt[c->mon->sellt]->arrange == NULL) {
    gapincr = gapoffset = 0;
  } else {
    if (c->mon->lt[c->mon->sellt]->arrange == monocle ||
        (nexttiled(c->mon->clients) == c && !nexttiled(c->next))) {
      gapoffset = 0;
      wc.border_width = 0;
      gapincr = -2 * borderpx;
    } else {
      gapoffset = gappx;
      gapincr = 2 * gappx;
    }
  }

  c->oldx = c->x;
  c->x = wc.x = x + gapoffset;
  c->oldy = c->y;
  c->y = wc.y = y + gapoffset;
  c->oldw = c->w;
  c->w = wc.width = MAX(w - gapincr, 1);
  c->oldh = c->h;
  c->h = wc.height = MAX(h - gapincr, 1);

  XConfigureWindow(dpy, c->win, CWX | CWY | CWWidth | CWHeight | CWBorderWidth,
                   &wc);
  configure(c);
  XFlush(dpy);
}

void resizefullscreen(Client *c, int x, int y, int w, int h) {
  int oldx = c->oldx, oldy = c->oldy;
  int oldw = c->oldw, oldh = c->oldh;

  resizeclient(c, x, y, w, h);
  c->oldx = oldx;
  c->oldy = oldy;
  c->oldw = oldw;
  c->oldh = oldh;
}

void resizerequest(XEvent *e) {
  XResizeRequestEvent *ev = &e->xresizerequest;
  Client *i;

  if ((i = wintosystrayicon(ev->window))) {
    updatesystrayicongeom(i, ev->width, ev->height);
    updatesystray();
  }
}

void resizemouse(const Arg *arg) {
  int ocx, ocy, nw, nh;
  int ocx2, ocy2, nx, ny;
  Client *c;
  Monitor *m;
  XEvent ev;

  int horizcorner = 0, vertcorner = 0;
  int di;
  unsigned int dui;
  Window dummy;
  Time lasttime = 0;

  (void)arg;
  if (!(c = selmon->sel))
    return;
  if (c->isfullscreen)
    return;
  restack(selmon);
  ocx = c->x;
  ocy = c->y;
  ocx2 = c->x + c->w;
  ocy2 = c->y + c->h;
  if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
                   None, cursor[CurResize]->cursor, CurrentTime) != GrabSuccess)
    return;

  if (c->isfloating || NULL == c->mon->lt[c->mon->sellt]->arrange) {
    if (!XQueryPointer(dpy, c->win, &dummy, &dummy, &di, &di, &nx, &ny,
                       &dui)) {
      XUngrabPointer(dpy, CurrentTime);
      return;
    }
    horizcorner = nx < c->w / 2;
    vertcorner = ny < c->h / 2;
    XWarpPointer(dpy, None, c->win, 0, 0, 0, 0,
                 horizcorner ? (-c->bw) : (c->w + c->bw - 1),
                 vertcorner ? (-c->bw) : (c->h + c->bw - 1));
  } else {
    XWarpPointer(dpy, None, root, 0, 0, 0, 0,
                 selmon->mx + (selmon->ww * selmon->mfact),
                 selmon->my + (selmon->wh / 2));
  }

  do {
    XMaskEvent(dpy, MOUSEMASK | ExposureMask | SubstructureRedirectMask, &ev);
    switch (ev.type) {
    case ConfigureRequest:
    case Expose:
    case MapRequest:
      handler[ev.type](&ev);

      if (c->swallowed)
        c = c->swallowed;
      break;
    case MotionNotify:
      if ((ev.xmotion.time - lasttime) <= (1000 / 60))
        continue;
      lasttime = ev.xmotion.time;

      if (!selmon->lt[selmon->sellt]->arrange || c->isfloating) {
        nx =
            horizcorner && ocx2 - ev.xmotion.x >= c->minw ? ev.xmotion.x : c->x;
        ny = vertcorner && ocy2 - ev.xmotion.y >= c->minh ? ev.xmotion.y : c->y;
        nw = MAX(horizcorner ? (ocx2 - nx)
                             : (ev.xmotion.x - ocx - 2 * c->bw + 1),
                 1);
        nh = MAX(
            vertcorner ? (ocy2 - ny) : (ev.xmotion.y - ocy - 2 * c->bw + 1), 1);

        if (horizcorner && ev.xmotion.x > ocx2)
          nx = ocx2 - (nw = c->minw);
        if (vertcorner && ev.xmotion.y > ocy2)
          ny = ocy2 - (nh = c->minh);

        resize(c, nx, ny, nw, nh, 1);
      } else {
        float fact = (float)(ev.xmotion.x_root - selmon->mx) /
                     (float)selmon->ww;

        if (fact >= 0.05f && fact <= 0.95f) {
          selmon->mfact = fact;
          selmon->pertag->mfacts[selmon->pertag->curtag] = fact;
          arrange(selmon);
        }
      }
      break;
    }
  } while (ev.type != ButtonRelease);

  XUngrabPointer(dpy, CurrentTime);
  while (XCheckMaskEvent(dpy, EnterWindowMask, &ev))
    ;
  if ((m = recttomon(c->x, c->y, c->w, c->h)) != selmon) {
    sendmon(c, m);
    focusmonitor(m);
  }
}

void restack(Monitor *m) {
  Client *c;
  XEvent ev;
  XWindowChanges wc;

  drawbar(m);
  if (!m->sel)
    return;
  if (m->sel->isfloating || !m->lt[m->sellt]->arrange)
    XRaiseWindow(dpy, m->sel->win);
  if (m->lt[m->sellt]->arrange) {
    wc.stack_mode = Below;
    wc.sibling = m->barwin;
    for (c = m->stack; c; c = c->snext)
      if (!c->isfloating && ISVISIBLE(c)) {
        XConfigureWindow(dpy, c->win, CWSibling | CWStackMode, &wc);
        wc.sibling = c->win;
      }
  }
  XSync(dpy, False);
  while (XCheckMaskEvent(dpy, EnterWindowMask, &ev))
    ;
}

void run(void) {
  XEvent ev;

  XSync(dpy, False);
  while (running && !XNextEvent(dpy, &ev))
    if (ev.type >= 0 && ev.type < LASTEvent && handler[ev.type])
      handler[ev.type](&ev);
}

void scan(void) {
  unsigned int i, num;
  Window d1, d2, *wins = NULL;
  XWindowAttributes wa;

  if (XQueryTree(dpy, root, &d1, &d2, &wins, &num)) {
    for (i = 0; i < num; i++) {
      if (!XGetWindowAttributes(dpy, wins[i], &wa) || wa.override_redirect ||
          (showsystray && systray && isxembedicon(wins[i])) ||
          XGetTransientForHint(dpy, wins[i], &d1))
        continue;
      if (wa.map_state == IsViewable || getstate(wins[i]) == IconicState)
        manage(wins[i], &wa);
    }
    for (i = 0; i < num; i++) {
      if (!XGetWindowAttributes(dpy, wins[i], &wa))
        continue;
      if (!(showsystray && systray && isxembedicon(wins[i])) &&
          XGetTransientForHint(dpy, wins[i], &d1) &&
          (wa.map_state == IsViewable || getstate(wins[i]) == IconicState))
        manage(wins[i], &wa);
    }
    if (wins)
      XFree(wins);
  }
}

void sendmon(Client *c, Monitor *m) {
  Monitor *old = c->mon;

  if (c->mon == m)
    return;
  if (c == old->sel)
    old->hidsel = 0;
  unfocus(c, 1);
  detach(c);
  detachstack(c);
  if (c->isfullscreen) {
    c->oldx += m->mx - old->mx;
    c->oldy += m->my - old->my;
  }
  c->mon = m;
  c->tags = m->tagset[m->seltags] & TAGMASK;
  if (!c->tags)
    c->tags = 1U;
  if (c == prevzoom)
    prevzoom = NULL;
  attach(c);
  attachstack(c);
  if (c->isfullscreen)
    resizefullscreen(c, m->mx, m->my, m->mw, m->mh);
  arrange(NULL);
  focus(NULL);
}

void setbarvisible(Monitor *m, int visible) {
  if (m->showbar == visible)
    return;
  m->showbar = visible;
  updatebarpos(m);
  resizebarwin(m);
  if (showsystray && systray && systraytomon(NULL) == m)
    updatesystray();
}

void setclientstate(Client *c, long state) {
  long data[] = {state, None};

  c->ishidden = state == IconicState;
  XChangeProperty(dpy, c->win, wmatom[WMState], wmatom[WMState], 32,
                  PropModeReplace, (unsigned char *)data, 2);
}

int sendevent(Window w, Atom proto, int mask, long d0, long d1, long d2,
              long d3, long d4) {
  int n;
  Atom *protocols, mt;
  int exists = 0;
  XEvent ev;

  if (proto == wmatom[WMTakeFocus] || proto == wmatom[WMDelete]) {
    mt = wmatom[WMProtocols];
    if (XGetWMProtocols(dpy, w, &protocols, &n)) {
      while (!exists && n--)
        exists = protocols[n] == proto;
      XFree(protocols);
    }
  } else {
    exists = True;
    mt = proto;
  }
  if (exists) {
    ev.type = ClientMessage;
    ev.xclient.window = w;
    ev.xclient.message_type = mt;
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = d0;
    ev.xclient.data.l[1] = d1;
    ev.xclient.data.l[2] = d2;
    ev.xclient.data.l[3] = d3;
    ev.xclient.data.l[4] = d4;
    XSendEvent(dpy, w, False, mask, &ev);
  }
  return exists;
}

void sendxembed(Window w, long message) {
  sendevent(w, xatom[Xembed], NoEventMask, CurrentTime, message, 0, 0, 0);
}

void setfocus(Client *c) {
  if (!c->neverfocus) {
    XSetInputFocus(dpy, c->win, RevertToPointerRoot, CurrentTime);
    XChangeProperty(dpy, root, netatom[NetActiveWindow], XA_WINDOW, 32,
                    PropModeReplace, (unsigned char *)&(c->win), 1);
  }
  sendevent(c->win, wmatom[WMTakeFocus], NoEventMask, wmatom[WMTakeFocus],
            CurrentTime, 0, 0, 0);
}

void setfullscreen(Client *c, int fullscreen) {
  if (fullscreen && !c->isfullscreen) {
    setatomprop(c, netatom[NetWMState], netatom[NetWMFullscreen], 1);
    c->isfullscreen = 1;
    c->oldstate = c->isfloating;
    c->fsbw = c->bw;
    c->bw = 0;
    c->isfloating = 1;
    resizeclient(c, c->mon->mx, c->mon->my, c->mon->mw, c->mon->mh);
    XRaiseWindow(dpy, c->win);
  } else if (!fullscreen && c->isfullscreen) {
    setatomprop(c, netatom[NetWMState], netatom[NetWMFullscreen], 0);
    c->isfullscreen = 0;
    c->isfloating = c->oldstate;
    c->bw = c->fsbw;
    c->x = c->oldx;
    c->y = c->oldy;
    c->w = c->oldw;
    c->h = c->oldh;
    resizeclient(c, c->x, c->y, c->w, c->h);
    arrange(c->mon);
  }
}

void setlayout(const Arg *arg) {

  if (!arg || !arg->v || arg->v != selmon->lt[selmon->sellt])
    selmon->sellt = selmon->pertag->sellts[selmon->pertag->curtag] ^= 1;

  if (arg && arg->v)
    selmon->lt[selmon->sellt] =
        selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt] =
            (Layout *)arg->v;

  snprintf(selmon->ltsymbol, sizeof selmon->ltsymbol, "%s",
           selmon->lt[selmon->sellt]->symbol);
  if (selmon->sel)
    arrange(selmon);
  else
    drawbar(selmon);
}

void setgaps(const Arg *arg) {
  int next = gappx + arg->i;

  gappx = MAX(next, 0);
  arrange(NULL);
}

void togglegaps(const Arg *arg) {
  static int savedgaps;

  (void)arg;
  if (gappx) {
    savedgaps = gappx;
    gappx = 0;
  } else {
    gappx = savedgaps ? savedgaps : defaultgappx;
  }
  arrange(NULL);
}

void shiftview(const Arg *arg) {
  unsigned int current = selmon->tagset[selmon->seltags] & TAGMASK;
  unsigned int shifted;
  int amount = arg->i % (int)LENGTH(tags);

  if (!current || !amount)
    return;
  if (amount < 0)
    amount += (int)LENGTH(tags);
  shifted = ((current << amount) |
             (current >> ((int)LENGTH(tags) - amount))) & TAGMASK;
  view(&(Arg){.ui = shifted});
}

void setmfact(const Arg *arg) {
  float f;

  if (!arg || !selmon->lt[selmon->sellt]->arrange)
    return;
  f = arg->f < 1.0 ? arg->f + selmon->mfact : arg->f - 1.0;
  if (f < 0.05 || f > 0.95)
    return;

  selmon->mfact = selmon->pertag->mfacts[selmon->pertag->curtag] = f;
  arrange(selmon);
}

void setup(void) {
  const unsigned char *displayname;
  unsigned long displayhash = 5381;
  int xresmajor, xresminor;
  size_t i;
  struct sigaction sa = {0};
  XSetWindowAttributes wa;
  Atom utf8string;
  char trayselection[32];

  sa.sa_handler = sigchld;
  sa.sa_flags = SA_NOCLDSTOP | SA_RESTART;
  sigemptyset(&sa.sa_mask);
  if (sigaction(SIGCHLD, &sa, NULL) == -1)
    die("can't install SIGCHLD handler:");

  screen = DefaultScreen(dpy);
  xresclientids = XResQueryVersion(dpy, &xresmajor, &xresminor) &&
                  (xresmajor > 1 || (xresmajor == 1 && xresminor >= 2));
  for (displayname = (const unsigned char *)DisplayString(dpy); *displayname;
       displayname++)
    displayhash = ((displayhash << 5) + displayhash) ^ *displayname;
  snprintf(sessionfile, sizeof sessionfile, "/tmp/dwm-session-%lu-%lu",
           (unsigned long)getuid(), displayhash);
  sw = DisplayWidth(dpy, screen);
  sh = DisplayHeight(dpy, screen);
  root = RootWindow(dpy, screen);
  drw = drw_create(dpy, screen, root, sw, sh);
  if (!drw_fontset_create(drw, fonts, LENGTH(fonts)))
    die("no fonts could be loaded.");
  lrpad = drw->fonts->h;
  bh = drw->fonts->h + 0;
  updategeom();

  utf8string = XInternAtom(dpy, "UTF8_STRING", False);
  wmatom[WMProtocols] = XInternAtom(dpy, "WM_PROTOCOLS", False);
  wmatom[WMDelete] = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
  wmatom[WMState] = XInternAtom(dpy, "WM_STATE", False);
  wmatom[WMTakeFocus] = XInternAtom(dpy, "WM_TAKE_FOCUS", False);
  netatom[NetActiveWindow] = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
  netatom[NetSupported] = XInternAtom(dpy, "_NET_SUPPORTED", False);
  snprintf(trayselection, sizeof trayselection, "_NET_SYSTEM_TRAY_S%d", screen);
  netatom[NetSystemTray] = XInternAtom(dpy, trayselection, False);
  netatom[NetSystemTrayOP] = XInternAtom(dpy, "_NET_SYSTEM_TRAY_OPCODE", False);
  netatom[NetSystemTrayOrientation] =
      XInternAtom(dpy, "_NET_SYSTEM_TRAY_ORIENTATION", False);
  netatom[NetWMName] = XInternAtom(dpy, "_NET_WM_NAME", False);
  netatom[NetWMState] = XInternAtom(dpy, "_NET_WM_STATE", False);
  netatom[NetWMCheck] = XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", False);
  netatom[NetWMFullscreen] =
      XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
  netatom[NetWMWindowType] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
  netatom[NetWMWindowTypeDialog] =
      XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
  netatom[NetWMWindowTypeNormal] =
      XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_NORMAL", False);
  netatom[NetClientList] = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
  xatom[Manager] = XInternAtom(dpy, "MANAGER", False);
  xatom[Xembed] = XInternAtom(dpy, "_XEMBED", False);
  xatom[XembedInfo] = XInternAtom(dpy, "_XEMBED_INFO", False);
  swallow_atom = XInternAtom(dpy, "_BETTER_SWALLOW", False);
  theme_atom = XInternAtom(dpy, "_DWM_THEME", False);

  cursor[CurNormal] = drw_cur_create(drw, XC_left_ptr);
  cursor[CurResize] = drw_cur_create(drw, XC_sizing);
  cursor[CurMove] = drw_cur_create(drw, XC_fleur);

  scheme = ecalloc(LENGTH(colors), sizeof(Clr *));
  for (i = 0; i < LENGTH(colors); i++)
    scheme[i] = drw_scm_create(drw, colors[i], 3);

  updatebars();
  updatesystray();
  updatestatus();
  reloadtheme();

  wmcheckwin = XCreateSimpleWindow(dpy, root, 0, 0, 1, 1, 0, 0, 0);
  XChangeProperty(dpy, wmcheckwin, netatom[NetWMCheck], XA_WINDOW, 32,
                  PropModeReplace, (unsigned char *)&wmcheckwin, 1);
  XChangeProperty(dpy, wmcheckwin, netatom[NetWMName], utf8string, 8,
                  PropModeReplace, (unsigned char *)"dwm", 3);
  XChangeProperty(dpy, root, swallow_atom, utf8string, 8, PropModeReplace,
                  (unsigned char *)"supported", 9);
  XChangeProperty(dpy, root, netatom[NetWMCheck], XA_WINDOW, 32,
                  PropModeReplace, (unsigned char *)&wmcheckwin, 1);

  XChangeProperty(dpy, root, netatom[NetSupported], XA_ATOM, 32,
                  PropModeReplace, (unsigned char *)netatom,
                  NetSupportedCount);
  XDeleteProperty(dpy, root, netatom[NetClientList]);

  wa.cursor = cursor[CurNormal]->cursor;
  wa.event_mask = SubstructureRedirectMask | SubstructureNotifyMask |
                  ButtonPressMask | PointerMotionMask | EnterWindowMask |
                  LeaveWindowMask | StructureNotifyMask | PropertyChangeMask;
  XChangeWindowAttributes(dpy, root, CWEventMask | CWCursor, &wa);
  grabkeys();
  focus(NULL);
}

void seturgent(Client *c, int urg) {
  XWMHints *wmh;

  c->isurgent = urg;
  if (!(wmh = XGetWMHints(dpy, c->win)))
    return;
  wmh->flags = urg ? (wmh->flags | XUrgencyHint) : (wmh->flags & ~XUrgencyHint);
  XSetWMHints(dpy, c->win, wmh);
  XFree(wmh);
}

void showall(const Arg *arg) {
  Client *c;

  (void)arg;
  selmon->hidsel = 0;
  for (c = selmon->clients; c; c = c->next)
    if (ISVISIBLE(c))
      showwin(c);
  if (!selmon->sel) {
    for (c = selmon->clients; c && !ISVISIBLE(c); c = c->next)
      ;
    if (c)
      focus(c);
  }
  arrange(selmon);
}

void showwin(Client *c) {
  if (!c || !HIDDEN(c))
    return;

  XMapWindow(dpy, c->win);
  setclientstate(c, NormalState);
}


void showhide(Client *c) {
  if (!c)
    return;
  if (ISVISIBLE(c)) {
    if (c->needresize) {
      c->needresize = 0;
      XMoveResizeWindow(dpy, c->win, c->x, c->y, c->w, c->h);
    } else {
      XMoveWindow(dpy, c->win, c->x, c->y);
    }
    if ((!c->mon->lt[c->mon->sellt]->arrange || c->isfloating) &&
        !c->isfullscreen)
      resize(c, c->x, c->y, c->w, c->h, 0);
    showhide(c->snext);
  } else {

    showhide(c->snext);
    XMoveWindow(dpy, c->win, WIDTH(c) * -2, c->y);
  }
}

void sigchld(int unused) {
  int saved_errno = errno;

  (void)unused;
  while (0 < waitpid(-1, NULL, WNOHANG))
    ;
  errno = saved_errno;
}

void spawn(const Arg *arg) {
  pid_t pid;

  if (arg->v == dmenucmd)
    snprintf(dmenumon, sizeof dmenumon, "%d", selmon->num);
  pid = fork();
  if (pid < 0) {
    perror("dwm: fork failed");
    return;
  }
  if (pid == 0) {
    if (dpy)
      close(ConnectionNumber(dpy));
    setsid();
    execvp(((char **)arg->v)[0], (char **)arg->v);
    fprintf(stderr, "dwm: execvp %s", ((char **)arg->v)[0]);
    perror(" failed");
    _exit(EXIT_FAILURE);
  }
}

void statusclick(const Arg *arg) {
  unsigned int module = arg->ui & 0xffU;

  if (module >= LENGTH(statuscmds))
    return;
  spawn(&(Arg){.v = statuscmds[module]});
}

void tag(const Arg *arg) {
  if (selmon->sel && arg->ui & TAGMASK) {
    selmon->sel->tags = arg->ui & TAGMASK;
    focus(NULL);
    arrange(selmon);
    if (viewontag)
      view(arg);
  }
}

void tagmon(const Arg *arg) {
  if (!selmon->sel || !mons->next)
    return;
  sendmon(selmon->sel, dirtomon(arg->i));
}

void tile(Monitor *m) {
  unsigned int i, masters, n;
  int h, mw, my, ty;
  Client *c;

  for (n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++)
    ;
  if (n == 0)
    return;

  masters = m->nmaster > 0 ? MIN(n, (unsigned int)m->nmaster) : 0;
  if (n > masters)
    mw = masters ? (int)(m->ww * m->mfact) : 0;
  else
    mw = m->ww;
  for (i = my = ty = 0, c = nexttiled(m->clients); c;
       c = nexttiled(c->next), i++) {
    if (i >= n)
      break;
    if (i < masters) {
      h = (m->wh - my) / (int)(masters - i);
      resize(c, m->wx, m->wy + my, mw - (2 * c->bw), h - (2 * c->bw), 0);
      if (my + HEIGHT(c) < m->wh)
        my += HEIGHT(c);
    } else {
      h = (m->wh - ty) / (int)(n - i);
      resize(c, m->wx + mw, m->wy + ty, m->ww - mw - (2 * c->bw),
             h - (2 * c->bw), 0);
      if (ty + HEIGHT(c) < m->wh)
        ty += HEIGHT(c);
    }
  }
}

void togglebar(const Arg *arg) {
  int visible = !selmon->showbar;

  (void)arg;
  selmon->pertag->showbars[selmon->pertag->curtag] = visible;
  setbarvisible(selmon, visible);
  arrange(selmon);
}

void togglefloating(const Arg *arg) {
  (void)arg;
  if (!selmon->sel)
    return;
  if (selmon->sel->isfullscreen)
    return;
  selmon->sel->isfloating = !selmon->sel->isfloating || selmon->sel->isfixed;
  if (selmon->sel->isfloating)
    resize(selmon->sel, selmon->sel->x, selmon->sel->y, selmon->sel->w,
           selmon->sel->h, 0);
  arrange(selmon);
}

void togglescratch(const Arg *arg) {
  Monitor *m, *oldmon;
  Client *c, *found = NULL;

  for (m = mons; m && !found; m = m->next)
    for (c = m->clients; c; c = c->next)
      if (c->tags & SPTAG) {
        found = c;
        break;
      }

  if (!found) {
    selmon->tagset[selmon->seltags] |= SPTAG;
    spawn(arg);
    return;
  }

  if (found->mon != selmon) {
    oldmon = found->mon;
    oldmon->tagset[oldmon->seltags] &= ~SPTAG;
    detach(found);
    detachstack(found);
    found->mon = selmon;
    attach(found);
    attachstack(found);
    found->x = selmon->wx + (selmon->ww - WIDTH(found)) / 2;
    found->y = selmon->wy + (selmon->wh - HEIGHT(found)) / 2;
    resize(found, found->x, found->y, found->w, found->h, 0);
    arrange(oldmon);
  }

  if ((selmon->tagset[selmon->seltags] & SPTAG) && !HIDDEN(found)) {
    selmon->tagset[selmon->seltags] &= ~SPTAG;
    focus(NULL);
  } else {
    selmon->tagset[selmon->seltags] |= SPTAG;
    if (HIDDEN(found))
      showwin(found);
    focus(found);
  }
  arrange(selmon);
}

void toggletag(const Arg *arg) {
  unsigned int newtags;

  if (!selmon->sel)
    return;
  newtags = selmon->sel->tags ^ (arg->ui & TAGMASK);
  if (newtags) {
    selmon->sel->tags = newtags;
    focus(NULL);
    arrange(selmon);
  }
}

void toggleview(const Arg *arg) {
  unsigned int scratchtag = selmon->tagset[selmon->seltags] & SPTAG;
  unsigned int newtagset =
      (selmon->tagset[selmon->seltags] & TAGMASK) ^ (arg->ui & TAGMASK);

  int i;

  if (newtagset) {
    selmon->tagset[selmon->seltags] = newtagset | scratchtag;

    if (newtagset == TAGMASK) {
      selmon->pertag->prevtag = selmon->pertag->curtag;
      selmon->pertag->curtag = 0;
    } else if (selmon->pertag->curtag == 0 ||
               !(newtagset & 1U << (selmon->pertag->curtag - 1))) {
      selmon->pertag->prevtag = selmon->pertag->curtag;
      for (i = 0; !(newtagset & 1U << i); i++)
        ;
      selmon->pertag->curtag = i + 1;
    }

    selmon->nmaster = selmon->pertag->nmasters[selmon->pertag->curtag];
    selmon->mfact = selmon->pertag->mfacts[selmon->pertag->curtag];
    selmon->sellt = selmon->pertag->sellts[selmon->pertag->curtag];
    selmon->lt[selmon->sellt] =
        selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt];
    selmon->lt[selmon->sellt ^ 1] =
        selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt ^ 1];

    setbarvisible(selmon,
                  selmon->pertag->showbars[selmon->pertag->curtag]);

    focus(NULL);
    arrange(selmon);
  }
}

void togglewin(const Arg *arg) {
  Client *c = (Client *)arg->v;
  int washidden;

  if (!c)
    return;
  if (c == selmon->sel) {
    hidewin(c);
    focus(NULL);
    arrange(c->mon);
  } else {
    washidden = HIDDEN(c);
    if (washidden)
      showwin(c);
    focus(c);
    if (washidden)
      arrange(c->mon);
    else
      restack(selmon);
  }
}

void unfocus(Client *c, int setfocus) {
  if (!c)
    return;
  grabbuttons(c, 0);
  XSetWindowBorder(dpy, c->win, scheme[SchemeNorm][ColBorder].pixel);
  if (setfocus) {
    XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
    XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
  }
}

void deleteswallower(Client *c) {
  SwallowDef **prevnext = &swallowlist;
  for (SwallowDef *sd = swallowlist; sd != NULL;) {
    if (sd->swallower == c) {
      SwallowDef *next = sd->next;
      *prevnext = next;
      free(sd);
      sd = next;
    } else {
      prevnext = &sd->next;
      sd = sd->next;
    }
  }

  Client *child = c->swallowed;
  while (child) {
    Client *next = child->next_swallowed;

    child->swallower = NULL;
    child->next_swallowed = NULL;
    child = next;
  }
}

void unmanage(Client *c, int destroyed) {
  Monitor *m = c->mon;
  Client *nextfocus = c->swallower;
  int refocus = m == selmon && c == m->sel;
  XWindowChanges wc;

  if (c->swallower) {
    Client **prev = &c->swallower->swallowed;
    for (; *prev && *prev != c; prev = &(*prev)->next_swallowed)
      ;
    if (*prev)
      *prev = c->next_swallowed;
    c->next_swallowed = NULL;

    if (c->swallower->swallowed == NULL) {
      detach(c->swallower);
      detachstack(c->swallower);

      c->swallower->next = c->next;
      c->next = c->swallower;
      c->swallower->snext = c->snext;
      c->snext = c->swallower;

      copyclientpos(c->swallower, c);
    }
  }

  detach(c);
  detachstack(c);
  if (!destroyed) {
    wc.border_width = c->oldbw;
    XGrabServer(dpy);
    XSetErrorHandler(xerrordummy);
    XConfigureWindow(dpy, c->win, CWBorderWidth, &wc);
    XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
    setclientstate(c, WithdrawnState);
    XSync(dpy, False);
    XSetErrorHandler(xerror);
    XUngrabServer(dpy);
  }
  deleteswallower(c);
  if (c == prevzoom)
    prevzoom = NULL;
  free(c);
  updateclientlist();
  arrange(m);

  if (refocus) {
    if (!nextfocus)
      nextfocus = getclientundermouse();
    focus(nextfocus);
  } else
    drawbar(m);

  if (refocus && m == selmon && m->sel)
    XWarpPointer(dpy, None, m->sel->win, 0, 0, 0, 0, m->sel->w / 2,
                 m->sel->h / 2);
}

void unmapnotify(XEvent *e) {
  Client *c;
  XUnmapEvent *ev = &e->xunmap;

  if ((c = wintoclient(ev->window))) {
    if (ev->send_event)
      setclientstate(c, WithdrawnState);
    else
      unmanage(c, 0);
  } else if ((c = wintosystrayicon(ev->window))) {
    if (c->tags)
      XMapRaised(dpy, c->win);
    updatesystray();
  }
}

void updatebars(void) {
  unsigned int w;
  Monitor *m;
  XSetWindowAttributes wa = {.override_redirect = True,
                             .background_pixmap = ParentRelative,
                             .event_mask = ButtonPressMask | ExposureMask |
                                           PointerMotionMask};
  XClassHint ch = {"dwm", "dwm"};
  for (m = mons; m; m = m->next) {
    if (m->barwin)
      continue;
    w = m->ww;
    m->barwin = XCreateWindow(
        dpy, root, m->wx, m->by, w, bh, 0, DefaultDepth(dpy, screen),
        CopyFromParent, DefaultVisual(dpy, screen),
        CWOverrideRedirect | CWBackPixmap | CWEventMask, &wa);
    XDefineCursor(dpy, m->barwin, cursor[CurNormal]->cursor);
    if (showsystray && systray && m == systraytomon(m))
      XMapWindow(dpy, systray->win);
    XMapRaised(dpy, m->barwin);
    XSetClassHint(dpy, m->barwin, &ch);
  }
}

void updatebarpos(Monitor *m) {
  m->wy = m->my;
  m->wh = m->mh;
  if (m->showbar) {
    m->wh -= bh;
    m->by = m->topbar ? m->wy : m->wy + m->wh;
    m->wy = m->topbar ? m->wy + bh : m->wy;
  } else
    m->by = -bh;
}

void updateclientlist(void) {
  Client *c;
  Monitor *m;
  Window *windows;
  size_t count = 0, i = 0;

  for (m = mons; m; m = m->next)
    for (c = m->clients; c; c = c->next)
      count++;
  if (!count) {
    XDeleteProperty(dpy, root, netatom[NetClientList]);
    return;
  }

  windows = ecalloc(count, sizeof(*windows));
  for (m = mons; m; m = m->next)
    for (c = m->clients; c; c = c->next)
      windows[i++] = c->win;
  XChangeProperty(dpy, root, netatom[NetClientList], XA_WINDOW, 32,
                  PropModeReplace, (unsigned char *)windows, (int)count);
  free(windows);
}

int updategeom(void) {
  int dirty = 0;

#ifdef XINERAMA
  if (XineramaIsActive(dpy)) {
    int i, j, n, nn;
    Client *c;
    Monitor *m;
    XineramaScreenInfo *info = XineramaQueryScreens(dpy, &nn);
    XineramaScreenInfo *unique = NULL;

    if (!info || nn <= 0) {
      if (info)
        XFree(info);
      if (!mons) {
        mons = createmon();
        mons->mw = mons->ww = sw;
        mons->mh = mons->wh = sh;
        updatebarpos(mons);
        dirty = 1;
      }
      if (!selmon)
        selmon = mons;
      return dirty;
    }

    for (n = 0, m = mons; m; m = m->next, n++)
      ;

    unique = ecalloc(nn, sizeof(XineramaScreenInfo));
    for (i = 0, j = 0; i < nn; i++)
      if (isuniquegeom(unique, j, &info[i]))
        memcpy(&unique[j++], &info[i], sizeof(XineramaScreenInfo));
    XFree(info);
    nn = j;
    if (n <= nn) {
      for (i = 0; i < (nn - n); i++) {
        for (m = mons; m && m->next; m = m->next)
          ;
        if (m)
          m->next = createmon();
        else
          mons = createmon();
      }
    } else {
      for (i = nn; i < n; i++) {
        for (m = mons; m && m->next; m = m->next)
          ;
        dirty = 1;
        while ((c = m->clients)) {
          m->clients = c->next;
          detachstack(c);
          if (c->isfullscreen) {
            c->oldx += mons->mx - m->mx;
            c->oldy += mons->my - m->my;
          }
          c->mon = mons;
          attach(c);
          attachstack(c);
        }
        if (m == selmon)
          selmon = mons;
        cleanupmon(m);
      }
    }
    for (i = 0, m = mons; i < nn && m; m = m->next, i++)
      if (i >= n || unique[i].x_org != m->mx || unique[i].y_org != m->my ||
          unique[i].width != m->mw || unique[i].height != m->mh) {
        int dx = unique[i].x_org - m->mx;
        int dy = unique[i].y_org - m->my;

        dirty = 1;
        if (dx || dy)
          for (c = m->clients; c; c = c->next)
            if (c->isfullscreen) {
              c->oldx += dx;
              c->oldy += dy;
            }
        m->num = i;
        m->mx = m->wx = unique[i].x_org;
        m->my = m->wy = unique[i].y_org;
        m->mw = m->ww = unique[i].width;
        m->mh = m->wh = unique[i].height;
        updatebarpos(m);
      }
    free(unique);
  } else
#endif
  {
    if (!mons)
      mons = createmon();
    if (mons->mw != sw || mons->mh != sh) {
      dirty = 1;
      mons->mw = mons->ww = sw;
      mons->mh = mons->wh = sh;
      updatebarpos(mons);
    }
  }
  if (dirty && !selmon)
    selmon = wintomon(root);
  return dirty;
}

void updatenumlockmask(void) {
  unsigned int i;
  int j;
  XModifierKeymap *modmap;

  numlockmask = 0;
  modmap = XGetModifierMapping(dpy);
  if (!modmap)
    return;
  for (i = 0; i < 8; i++)
    for (j = 0; j < modmap->max_keypermod; j++)
      if (modmap->modifiermap[i * modmap->max_keypermod + j] ==
          XKeysymToKeycode(dpy, XK_Num_Lock))
        numlockmask = (1U << i);
  XFreeModifiermap(modmap);
}

void updatesizehints(Client *c) {
  long msize;
  XSizeHints size;

  if (!XGetWMNormalHints(dpy, c->win, &size, &msize))
    size.flags = PSize;
  if (size.flags & PBaseSize) {
    c->basew = MAX(size.base_width, 0);
    c->baseh = MAX(size.base_height, 0);
  } else if (size.flags & PMinSize) {
    c->basew = MAX(size.min_width, 0);
    c->baseh = MAX(size.min_height, 0);
  } else
    c->basew = c->baseh = 0;
  if (size.flags & PResizeInc) {
    c->incw = MAX(size.width_inc, 0);
    c->inch = MAX(size.height_inc, 0);
  } else
    c->incw = c->inch = 0;
  if (size.flags & PMaxSize) {
    c->maxw = MAX(size.max_width, 0);
    c->maxh = MAX(size.max_height, 0);
  } else
    c->maxw = c->maxh = 0;
  if (size.flags & PMinSize) {
    c->minw = MAX(size.min_width, 0);
    c->minh = MAX(size.min_height, 0);
  } else if (size.flags & PBaseSize) {
    c->minw = MAX(size.base_width, 0);
    c->minh = MAX(size.base_height, 0);
  } else
    c->minw = c->minh = 0;
  if (c->maxw && c->maxw < c->minw)
    c->maxw = 0;
  if (c->maxh && c->maxh < c->minh)
    c->maxh = 0;
  if ((size.flags & PAspect) && size.min_aspect.x > 0 &&
      size.min_aspect.y > 0 && size.max_aspect.x > 0 &&
      size.max_aspect.y > 0) {
    c->mina = (float)size.min_aspect.y / size.min_aspect.x;
    c->maxa = (float)size.max_aspect.x / size.max_aspect.y;
  } else
    c->maxa = c->mina = 0.0;
  c->isfixed =
      c->maxw && c->maxh && c->maxw == c->minw && c->maxh == c->minh;
}

void updatestatus(void) {
  Monitor *m;
  if (!gettextprop(root, XA_WM_NAME, stext, sizeof(stext)))
    strcpy(stext, "dwm-" VERSION);
  for (m = mons; m; m = m->next)
    drawbar(m);
}

/* Priority is independent of which app wins the race to dock at login. */
static int systraypriority(const char *identity) {
  if (!strcasecmp(identity, "nm-applet") ||
      !strcasecmp(identity, "NetworkManager Applet"))
    return 0;
  if (!strcasecmp(identity, "blueman") ||
      !strcasecmp(identity, "blueman-applet") ||
      !strcasecmp(identity, "blueman-tray") ||
      !strcasecmp(identity, "blueman-tray-menu"))
    return 1;
  if (!strcasecmp(identity, "clipit"))
    return 2;
  return 3;
}

void updatesystrayiconidentity(Client *i) {
  XClassHint hint = {0};
  char title[256] = "";
  const char *class, *instance, *app;
  Client **at;
  int priority, cmp;
  size_t n;

  XGetClassHint(dpy, i->win, &hint);
  class = hint.res_class ? hint.res_class : "";
  instance = hint.res_name ? hint.res_name : "";
  if (!gettextprop(i->win, netatom[NetWMName], title, sizeof title))
    gettextprop(i->win, XA_WM_NAME, title, sizeof title);
  priority = MIN(systraypriority(class), systraypriority(instance));
  priority = MIN(priority, systraypriority(title));
  app = *class ? class : (*instance ? instance : title);
  /* snixembed gives every bridged app the same class; its title identifies it. */
  if ((!strcasecmp(class, "snixembed") ||
       !strcasecmp(instance, "snixembed")) && *title)
    app = title;
  if (!*app)
    app = "~unknown";

  /* Tray-only Clients reuse these fields as cached sort keys.  No X property
   * reads are needed in the redraw/layout path. */
  snprintf(i->name, sizeof i->name, "%d:%.253s", priority, app);
  snprintf(i->appname, sizeof i->appname, "%.80s:%.80s:%.80s",
           class, instance, title);
  for (n = 0; i->name[n]; n++)
    i->name[n] = (char)tolower((unsigned char)i->name[n]);
  for (n = 0; i->appname[n]; n++)
    i->appname[n] = (char)tolower((unsigned char)i->appname[n]);
  if (hint.res_class)
    XFree(hint.res_class);
  if (hint.res_name)
    XFree(hint.res_name);

  for (at = &systray->icons; *at && *at != i; at = &(*at)->next)
    ;
  if (*at)
    *at = i->next;
  for (at = &systray->icons; *at; at = &(*at)->next) {
    cmp = strcmp(i->name, (*at)->name);
    if (!cmp)
      cmp = strcmp(i->appname, (*at)->appname);
    /* Identical/anonymous icons have no cross-login identity. XID provides a
     * repeatable tie-break for their lifetime, including a dwm restart. */
    if (cmp < 0 || (!cmp && i->win < (*at)->win))
      break;
  }
  i->next = *at;
  *at = i;
}

void updatesystrayicongeom(Client *i, int w, int h) {
  if (!i)
    return;

  w = MAX(w, 1);
  h = MAX(h, 1);
  i->h = bh;
  if (w == h)
    i->w = bh;
  else if (h == bh)
    i->w = w;
  else
    i->w = (int)((float)bh * ((float)w / (float)h));
  applysizehints(i, &i->x, &i->y, &i->w, &i->h, False);

  if (i->h > bh) {
    if (i->w == i->h)
      i->w = bh;
    else
      i->w = (int)((float)bh * ((float)i->w / (float)i->h));
    i->h = bh;
  }
}

void updatesystrayiconstate(Client *i, XPropertyEvent *ev) {
  unsigned long flags;
  long message;

  if (!showsystray || !i || ev->atom != xatom[XembedInfo])
    return;
  flags = getxembedflags(i->win);

  if ((flags & XEMBED_MAPPED) && !i->tags) {
    i->tags = 1;
    message = XEMBED_WINDOW_ACTIVATE;
    XMapRaised(dpy, i->win);
    setclientstate(i, NormalState);
  } else if (!(flags & XEMBED_MAPPED) && i->tags) {
    i->tags = 0;
    message = XEMBED_WINDOW_DEACTIVATE;
    XUnmapWindow(dpy, i->win);
    setclientstate(i, WithdrawnState);
  } else
    return;
  sendxembed(i->win, message);
}

void updatesystray(void) {
  XSetWindowAttributes wa;
  XWindowChanges wc;
  unsigned long orientation = 0;
  Client *i;
  Monitor *m;
  int w = 1, x;

  if (!showsystray || !(m = systraytomon(NULL)))
    return;
  x = m->mx + m->mw;
  if (!systray) {
    systray = ecalloc(1, sizeof(Systray));
    systray->win = XCreateSimpleWindow(dpy, root, x, m->by, w, bh, 0, 0,
                                       scheme[SchemeNorm][ColBg].pixel);
    wa.event_mask = ButtonPressMask | ExposureMask | PointerMotionMask |
                    EnterWindowMask | LeaveWindowMask |
                    SubstructureNotifyMask;
    wa.override_redirect = True;
    wa.background_pixel = scheme[SchemeNorm][ColBg].pixel;
    XChangeProperty(dpy, systray->win, netatom[NetSystemTrayOrientation],
                    XA_CARDINAL, 32, PropModeReplace,
                    (unsigned char *)&orientation, 1);
    XChangeWindowAttributes(
        dpy, systray->win, CWEventMask | CWOverrideRedirect | CWBackPixel, &wa);
    XSetSelectionOwner(dpy, netatom[NetSystemTray], systray->win, CurrentTime);
    if (XGetSelectionOwner(dpy, netatom[NetSystemTray]) == systray->win) {
      sendevent(root, xatom[Manager], StructureNotifyMask, CurrentTime,
                netatom[NetSystemTray], systray->win, 0, 0);
    } else {
      fprintf(stderr, "dwm: unable to obtain system tray.\n");
      XDestroyWindow(dpy, systray->win);
      free(systray);
      systray = NULL;
      return;
    }
  }

  for (w = 0, i = systray->icons; i; i = i->next) {
    wa.background_pixel = scheme[SchemeNorm][ColBg].pixel;
    XChangeWindowAttributes(dpy, i->win, CWBackPixel, &wa);
    if (!i->tags) {
      XUnmapWindow(dpy, i->win);
      continue;
    }
    XMapRaised(dpy, i->win);
    w += systrayspacing;
    i->x = w;
    XMoveResizeWindow(dpy, i->win, i->x, 0, i->w, i->h);
    w += i->w;
    i->mon = m;
  }
  w = w ? w + systrayspacing : 1;
  wc.x = x - w;
  wc.y = m->by;
  wc.width = w;
  wc.height = bh;
  wc.stack_mode = Above;
  wc.sibling = m->barwin;
  XConfigureWindow(dpy, systray->win,
                   CWX | CWY | CWWidth | CWHeight | CWSibling | CWStackMode,
                   &wc);
  /* Keep the tray just above the bar, beneath fullscreen windows and lockers. */
  XMapWindow(dpy, systray->win);
  XMapSubwindows(dpy, systray->win);
  XSetForeground(dpy, drw->gc, scheme[SchemeNorm][ColBg].pixel);
  XFillRectangle(dpy, systray->win, drw->gc, 0, 0, (unsigned int)w,
                 (unsigned int)bh);
  XSync(dpy, False);
}

void updatetitle(Client *c) {
  XClassHint hint = {0};
  const char *app = "app";
  size_t i;

  if (!gettextprop(c->win, netatom[NetWMName], c->name, sizeof c->name))
    gettextprop(c->win, XA_WM_NAME, c->name, sizeof c->name);
  if (c->name[0] == '\0')
    snprintf(c->name, sizeof c->name, "%s", broken);

  /* Keep the real title for window rules; only the app name goes in the bar. */
  XGetClassHint(dpy, c->win, &hint);
  if (hint.res_class && hint.res_class[0])
    app = hint.res_class;
  else if (hint.res_name && hint.res_name[0])
    app = hint.res_name;
  snprintf(c->appname, sizeof c->appname, "%s", app);
  for (i = 0; c->appname[i]; i++)
    if (c->appname[i] >= 'A' && c->appname[i] <= 'Z')
      c->appname[i] += 'a' - 'A';
  if (hint.res_class)
    XFree(hint.res_class);
  if (hint.res_name)
    XFree(hint.res_name);
}

void updatewindowtype(Client *c) {
  int fullscreen = hasatomprop(c->win, netatom[NetWMState],
                               netatom[NetWMFullscreen]);
  setfullscreen(c, fullscreen);
  if (hasatomprop(c->win, netatom[NetWMWindowType],
                  netatom[NetWMWindowTypeDialog]))
    c->isfloating = 1;
}

void updatewmhints(Client *c) {
  XWMHints *wmh;

  if ((wmh = XGetWMHints(dpy, c->win))) {
    if (c == selmon->sel && wmh->flags & XUrgencyHint) {
      wmh->flags &= ~XUrgencyHint;
      XSetWMHints(dpy, c->win, wmh);
      c->isurgent = 0;
    } else
      c->isurgent = (wmh->flags & XUrgencyHint) ? 1 : 0;
    if (wmh->flags & InputHint)
      c->neverfocus = !wmh->input;
    else
      c->neverfocus = 0;
    XFree(wmh);
  }
}

void view(const Arg *arg) {
  int i;
  unsigned int tmptag;

  if ((arg->ui & TAGMASK) ==
      (selmon->tagset[selmon->seltags] & TAGMASK))
    return;
  selmon->seltags ^= 1;

  if (arg->ui & TAGMASK) {
    selmon->tagset[selmon->seltags] = arg->ui & TAGMASK;
    selmon->pertag->prevtag = selmon->pertag->curtag;

    if (arg->ui == ~0U)
      selmon->pertag->curtag = 0;
    else {
      for (i = 0; !(arg->ui & 1U << i); i++)
        ;
      selmon->pertag->curtag = i + 1;
    }
  } else {
    tmptag = selmon->pertag->prevtag;
    selmon->pertag->prevtag = selmon->pertag->curtag;
    selmon->pertag->curtag = tmptag;
  }

  selmon->nmaster = selmon->pertag->nmasters[selmon->pertag->curtag];
  selmon->mfact = selmon->pertag->mfacts[selmon->pertag->curtag];
  selmon->sellt = selmon->pertag->sellts[selmon->pertag->curtag];
  selmon->lt[selmon->sellt] =
      selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt];
  selmon->lt[selmon->sellt ^ 1] =
      selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt ^ 1];

  setbarvisible(selmon, selmon->pertag->showbars[selmon->pertag->curtag]);

  focus(NULL);
  arrange(selmon);
}

Client *wintoclient(Window w) {
  Client *c;
  Monitor *m;

  for (m = mons; m; m = m->next)
    for (c = m->clients; c; c = c->next)
      if (c->win == w)
        return c;
  return NULL;
}

Client *wintosystrayicon(Window w) {
  Client *i = NULL;

  if (!showsystray || !systray || !w)
    return i;
  for (i = systray->icons; i && i->win != w; i = i->next)
    ;
  return i;
}

Monitor *wintomon(Window w) {
  int x, y;
  Client *c;
  Monitor *m;

  if (w == root && getrootptr(&x, &y))
    return recttomon(x, y, 1, 1);
  for (m = mons; m; m = m->next)
    if (w == m->barwin)
      return m;
  if ((c = wintoclient(w)))
    return c->mon;
  return selmon;
}

int xerror(Display *display, XErrorEvent *event) {
  if (event->error_code == BadWindow ||
      (event->request_code == X_SetInputFocus &&
       event->error_code == BadMatch) ||
      (event->request_code == X_PolyText8 &&
       event->error_code == BadDrawable) ||
      (event->request_code == X_PolyFillRectangle &&
       event->error_code == BadDrawable) ||
      (event->request_code == X_PolySegment &&
       event->error_code == BadDrawable) ||
      (event->request_code == X_ConfigureWindow &&
       event->error_code == BadMatch) ||
      (event->request_code == X_GrabButton &&
       event->error_code == BadAccess) ||
      (event->request_code == X_GrabKey && event->error_code == BadAccess) ||
      (event->request_code == X_CopyArea &&
       event->error_code == BadDrawable))
    return 0;
  fprintf(stderr, "dwm: fatal error: request code=%d, error code=%d\n",
          event->request_code, event->error_code);
  return xerrorxlib(display, event);
}

int xerrordummy(Display *display, XErrorEvent *event) {
  (void)display;
  (void)event;
  return 0;
}

int xerrorstart(Display *display, XErrorEvent *event) {
  (void)display;
  (void)event;
  die("dwm: another window manager is already running");
  return -1;
}

Monitor *systraytomon(Monitor *m) {
  Monitor *t;
  int i, n;

  if (!systraypinning) {
    if (!m)
      return selmon;
    return m == selmon ? m : NULL;
  }
  for (n = 1, t = mons; t && t->next; n++, t = t->next)
    ;
  for (i = 1, t = mons; t && t->next && i < (int)systraypinning;
       i++, t = t->next)
    ;
  if (systraypinningfailfirst && n < (int)systraypinning)
    return mons;
  return t;
}

void zoom(const Arg *arg) {
  Client *c = selmon->sel;
  Client *at = NULL, *cold, *cprevious = NULL;

  (void)arg;
  if (!c || !selmon->lt[selmon->sellt]->arrange || c->isfloating)
    return;
  if (c == nexttiled(selmon->clients)) {
    at = findbefore(prevzoom);
    if (at)
      cprevious = nexttiled(at->next);
    if (!cprevious || cprevious != prevzoom) {
      prevzoom = NULL;
      if (!c || !(c = nexttiled(c->next)))
        return;
    } else
      c = cprevious;
  }
  cold = nexttiled(selmon->clients);
  if (c != cold && !at)
    at = findbefore(c);
  detach(c);
  attach(c);

  if (c != cold && at) {
    prevzoom = cold;
    if (cold && at != cold) {
      detach(cold);
      cold->next = at->next;
      at->next = cold;
    }
  }
  focus(c);
  arrange(c->mon);
}

int main(int argc, char *argv[]) {
  if (argc == 2 && !strcmp("-v", argv[1]))
    die("dwm-" VERSION);
  else if (argc != 1)
    die("usage: dwm [-v]");
  if (!setlocale(LC_CTYPE, "") || !XSupportsLocale())
    fputs("warning: no locale support\n", stderr);
  if (!(dpy = XOpenDisplay(NULL)))
    die("dwm: cannot open display");
  checkotherwm();
  setup();
#ifdef __OpenBSD__
  if (pledge("stdio rpath proc exec", NULL) == -1)
    die("pledge");
#endif
  scan();
  restoreSession();
  run();

  if (restart) {
    execvp(argv[0], argv);
    perror("dwm: restart failed");
    restart = 0;
  }
  cleanup();
  XCloseDisplay(dpy);
  return EXIT_SUCCESS;
}
