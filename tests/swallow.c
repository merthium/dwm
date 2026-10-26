/* Run on a separate X server. Real XRes PIDs and inherited WINDOWID;
 * two terminal windows deliberately share a PID, as with urxvtd. */
#define main dwm_main
#include "../dwm.c"
#undef main

static int failures;

static void expect(int ok, const char *message) {
  if (!ok) {
    fprintf(stderr, "FAIL: %s\n", message);
    failures++;
  }
}

static Window newwindow(Display *display, const char *class) {
  Window window = XCreateSimpleWindow(display, DefaultRootWindow(display),
                                      10, 10, 320, 240, 0, 0, 0);
  XClassHint hint = {.res_name = (char *)class, .res_class = (char *)class};
  XSetClassHint(display, window, &hint);
  XStoreName(display, window, class);
  return window;
}

static Client *managewindow(Window window) {
  XWindowAttributes attributes;
  XSync(dpy, False);
  XGetWindowAttributes(dpy, window, &attributes);
  manage(window, &attributes);
  XSync(dpy, False);
  return wintoclient(window);
}

int main(int argc, char **argv) {
  Window first, second, image;
  Client *terminal, *other, *application;
  int channel[2], x, y, width, height;
  pid_t child;
  char windowid[32];
  FILE *stream;

  if (argc == 2 && strcmp(argv[1], "--child") == 0) {
    Display *display = XOpenDisplay(NULL);
    if (!display)
      return 2;
    image = newwindow(display, "feh");
    XSync(display, False);
    printf("%lu\n", image);
    fflush(stdout);
    while (1)
      pause();
  }
  if (!getenv("DWM_ISOLATED_X_TEST"))
    die("Refusing to run outside an isolated test server.");
  if (!(dpy = XOpenDisplay(NULL)))
    die("Cannot open test display.");
  checkotherwm();
  setup();
  first = newwindow(dpy, "URxvt");
  second = newwindow(dpy, "URxvt");
  terminal = managewindow(first);
  other = managewindow(second);
  x = terminal->x;
  y = terminal->y;
  width = terminal->w;
  height = terminal->h;
  expect(wintopid(first) == wintopid(second), "shared-daemon fixture");

  if (pipe(channel) < 0)
    die("Cannot create child channel:");
  child = fork();
  if (child < 0)
    die("Cannot start child client:");
  if (child == 0) {
    close(channel[0]);
    dup2(channel[1], STDOUT_FILENO);
    close(channel[1]);
    close(ConnectionNumber(dpy));
    snprintf(windowid, sizeof windowid, "%lu", first);
    setenv("WINDOWID", windowid, 1);
    execl(argv[0], argv[0], "--child", (char *)NULL);
    _exit(2);
  }
  close(channel[1]);
  stream = fdopen(channel[0], "r");
  if (!stream || fscanf(stream, "%lu", &image) != 1)
    die("Child did not create its X window.");
  fclose(stream);
  expect(wintopid(image) == child, "XRes resolves the real application PID");
  application = managewindow(image);
  expect(application->swallower == terminal, "GUI automatically replaces its terminal");
  expect(terminal->swallowed == application, "terminal tracks its application");
  expect(!ISVISIBLE(terminal), "swallowed terminal leaves the layout");
  expect(ISVISIBLE(other) && !other->swallowed, "other urxvtd window stays visible");
  expect(application->x == x && application->y == y &&
         application->w == width && application->h == height,
         "application inherits the terminal tile");

  XDestroyWindow(dpy, image);
  unmanage(application, 1);
  expect(ISVISIBLE(terminal) && !terminal->swallowed,
         "closing the application restores its terminal");
  expect(terminal->x == x && terminal->y == y &&
         terminal->w == width && terminal->h == height,
         "restored terminal retains its tile");
  kill(child, SIGTERM);
  waitpid(child, NULL, 0);
  XDestroyWindow(dpy, first);
  unmanage(terminal, 1);
  XDestroyWindow(dpy, second);
  unmanage(other, 1);
  cleanup();
  XCloseDisplay(dpy);
  if (!failures)
    puts("PASS: automatic swallow, shared terminal daemon, tile geometry, terminal restoration");
  return failures ? 1 : 0;
}
