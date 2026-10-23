/* Run only on an isolated X server; exercise actual X11 stacking. */
#define main dwm_main
#include "../dwm.c"
#undef main

static int failures;

static void expect_below(Window lower, Window upper, const char *scenario) {
  Window parent, returnedroot, *children = NULL;
  unsigned int count, n;
  int low = -1, high = -1;

  XSync(dpy, False);
  if (!XQueryTree(dpy, root, &returnedroot, &parent, &children, &count))
    die("Cannot query test stacking order.");
  for (n = 0; n < count; n++) {
    if (children[n] == lower)
      low = (int)n;
    if (children[n] == upper)
      high = (int)n;
  }
  XFree(children);
  if (low < 0 || high < 0 || low >= high) {
    fprintf(stderr, "FAIL: %s (lower=%d upper=%d)\n", scenario, low, high);
    failures++;
  }
}

static void update_icon(Client *icon) {
  XEvent event = {0};
  XStoreName(dpy, icon->win, "Changing tray icon");
  event.xproperty.window = icon->win;
  event.xproperty.atom = XA_WM_NAME;
  propertynotify(&event);
  event.xresizerequest.window = icon->win;
  event.xresizerequest.width = bh;
  event.xresizerequest.height = bh;
  resizerequest(&event);
  event.xexpose.window = systray->win;
  event.xexpose.count = 0;
  expose(&event);
}

int main(void) {
  Client icon = {0}, full = {0};
  XSetWindowAttributes attributes = {.override_redirect = True};
  XWindowAttributes actual;
  Window lock;

  if (!getenv("DWM_ISOLATED_X_TEST"))
    die("Refusing to run outside an isolated test server.");
  if (!(dpy = XOpenDisplay(NULL)))
    die("Cannot open test display.");
  checkotherwm();
  setup();
  icon.win = XCreateSimpleWindow(dpy, systray->win, 0, 0, bh, bh, 0, 0, 0);
  icon.w = icon.h = bh;
  icon.tags = 1;
  systray->icons = &icon;
  updatesystray();
  expect_below(mons->barwin, systray->win, "tray visible above the bar");

  full.mon = mons;
  full.win = XCreateSimpleWindow(dpy, root, 20, 20, 200, 100, 0, 0, 0);
  full.x = full.y = 20;
  full.w = 200;
  full.h = 100;
  XMapWindow(dpy, full.win);
  setfullscreen(&full, 1);
  expect_below(systray->win, full.win, "fullscreen initially covers tray");
  for (int n = 0; n < 3; n++) {
    update_icon(&icon);
    expect_below(systray->win, full.win, "icon updates stay below fullscreen");
  }

  lock = XCreateWindow(dpy, root, 0, 0, sw, sh, 0, CopyFromParent,
                       InputOutput, CopyFromParent, CWOverrideRedirect,
                       &attributes);
  XMapRaised(dpy, lock);
  for (int n = 0; n < 3; n++) {
    update_icon(&icon);
    expect_below(systray->win, lock, "icon updates stay below lock overlay");
  }
  XUnmapWindow(dpy, systray->win);
  updatesystray();
  expect_below(systray->win, lock, "remapping tray stays below lock overlay");
  expect_below(systray->win, full.win, "remapping tray stays below fullscreen");

  systray->icons = NULL;
  XDestroyWindow(dpy, systray->win);
  free(systray);
  systray = NULL;
  updatesystray();
  expect_below(systray->win, lock, "new tray stays below lock overlay");
  expect_below(systray->win, full.win, "new tray stays below fullscreen");

  XDestroyWindow(dpy, lock);
  XDestroyWindow(dpy, full.win);
  updatesystray();
  expect_below(mons->barwin, systray->win, "tray returns above the bar");
  XGetWindowAttributes(dpy, systray->win, &actual);
  if (actual.map_state != IsViewable) {
    fputs("FAIL: tray remains hidden after overlay closes\n", stderr);
    failures++;
  }
  cleanup();
  XCloseDisplay(dpy);
  if (failures)
    return 1;
  puts("PASS: tray updates, remapping and creation stay below fullscreen/lock; normal tray remains visible");
  return 0;
}
