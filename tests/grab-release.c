/* Use an isolated X server: this test temporarily grabs its input devices. */
#define main dwm_main
#include "../dwm.c"
#undef main

int main(void) {
  Monitor selected = {.mw = 400, .mh = 600};
  Monitor other = {.mx = 400, .mw = 400, .mh = 600};
  Client client = {.mon = &other};
  Display *probe;
  XEvent event = {0};
  int before, after;

  if (!getenv("DWM_ISOLATED_X_TEST"))
    die("Refusing to grab input outside an isolated test server.");
  dpy = XOpenDisplay(NULL);
  probe = XOpenDisplay(NULL);
  if (!dpy || !probe)
    die("Cannot open test display.");
  root = DefaultRootWindow(dpy);
  client.win = XCreateSimpleWindow(dpy, root, 410, 10, 100, 100, 0, 0, 0);
  XMapWindow(dpy, client.win);
  mons = selmon = &selected;
  selected.next = &other;
  other.clients = &client;
  XSync(dpy, False);

  /* The passive grab on an unfocused window freezes BOTH devices. */
  if (XGrabPointer(dpy, root, False, BUTTONMASK, GrabModeSync,
                   GrabModeSync, None, None, CurrentTime) != GrabSuccess)
    die("Could not establish the test grab.");
  before = XGrabPointer(probe, root, False, BUTTONMASK, GrabModeAsync,
                        GrabModeAsync, None, None, CurrentTime);
  if (before != AlreadyGrabbed)
    die("Test precondition: another client should own the pointer.");

  event.xbutton.type = ButtonPress;
  event.xbutton.window = client.win;
  event.xbutton.button = Button1;
  buttonpress(&event);
  XSync(dpy, False);
  after = XGrabPointer(probe, root, False, BUTTONMASK, GrabModeAsync,
                       GrabModeAsync, None, None, CurrentTime);
  XUngrabPointer(probe, CurrentTime);
  XUngrabPointer(dpy, CurrentTime);
  XDestroyWindow(dpy, client.win);
  XCloseDisplay(probe);
  XCloseDisplay(dpy);
  if (after != GrabSuccess) {
    fprintf(stderr, "FAIL: rejected monitor click leaves input grabbed (%d)\n", after);
    return 1;
  }
  puts("PASS: rejected monitor click releases the input grab");
  return 0;
}
