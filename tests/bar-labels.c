/* Exercise the actual title reader and bar renderer without an X server. */
#define XGetClassHint fixture_class_hint
#define XGetTextProperty fixture_text_property
#define drw_text capture_text
#define drw_rect skip_rect
#define drw_map skip_map
#define drw_fontset_getwidth fixture_text_width
#define main dwm_main
#include "../dwm.c"
#undef main
#include <assert.h>

static const char *fixture_class, *fixture_instance;
static const char private_title[] = "Private page or terminal title | example-user";
static char drawn[sizeof stext];
static int maps;

Status fixture_class_hint(Display *display, Window window, XClassHint *hint) {
  (void)display; (void)window;
  hint->res_class = fixture_class ? strdup(fixture_class) : NULL;
  hint->res_name = fixture_instance ? strdup(fixture_instance) : NULL;
  return fixture_class || fixture_instance;
}

Status fixture_text_property(Display *display, Window window,
                             XTextProperty *property, Atom atom) {
  (void)display; (void)window; (void)atom;
  property->value = (unsigned char *)strdup(private_title);
  property->encoding = XA_STRING;
  property->format = 8;
  property->nitems = strlen(private_title);
  return 1;
}

int capture_text(Drw *context, int x, int y, unsigned int w, unsigned int h,
                 unsigned int pad, const char *text, int invert) {
  (void)context; (void)y; (void)h; (void)pad; (void)invert;
  snprintf(drawn, sizeof drawn, "%s", text);
  return x + w;
}

void skip_rect(Drw *context, int x, int y, unsigned int w, unsigned int h,
               int filled, int invert) {
  (void)context; (void)x; (void)y; (void)w; (void)h;
  (void)filled; (void)invert;
}

void skip_map(Drw *context, Window win, int x, int y,
              unsigned int w, unsigned int h) {
  (void)context; (void)win; (void)x; (void)y; (void)w; (void)h;
  maps++;
}

unsigned int fixture_text_width(Drw *context, const char *text) {
  (void)context;
  return (unsigned int)strlen(text) * 8;
}

int main(void) {
  Fnt font = {.h = 20};
  Drw context = {.fonts = &font};
  Clr palette[3] = {0};
  Clr *palettes[] = {palette, palette, palette, palette};
  Monitor monitor = {.ww = 1200, .showbar = 1, .tagset = {1, 1}};
  Client client = {.win = 42, .tags = 1, .mon = &monitor};
  const struct { const char *class, *instance, *label; } cases[] = {
      {"URxvt", "scratchpad", "urxvt"},
      {"LibreWolf", "Navigator", "librewolf"},
      {"Thunar", "file-manager", "thunar"},
      {NULL, "URxvt", "urxvt"},
      {"", "Librewolf", "librewolf"},
      {NULL, NULL, "app"},
      {"", "", "app"},
  };

  drw = &context;
  scheme = palettes;
  mons = selmon = &monitor;
  monitor.clients = monitor.sel = &client;
  bh = 24;
  lrpad = 12;
  netatom[NetWMName] = 1234;
  strcpy(monitor.ltsymbol, "t");

  for (size_t i = 0; i < LENGTH(cases); i++) {
    fixture_class = cases[i].class;
    fixture_instance = cases[i].instance;
    updatetitle(&client);
    assert(strcmp(client.name, private_title) == 0); /* title rules still work */
    drawbar(&monitor);
    if (strcmp(drawn, cases[i].label)) {
      fprintf(stderr, "Expected bar label '%s', got '%s'\n", cases[i].label, drawn);
      return 1;
    }
  }

  /* A class arriving/changing after map must update even an unselected tab. */
  monitor.sel = NULL;
  XEvent event = {0};
  event.xproperty.window = client.win;
  event.xproperty.atom = XA_WM_CLASS;
  fixture_class = "LibreWolf";
  fixture_instance = "Navigator";
  maps = 0;
  propertynotify(&event);
  assert(maps > 0 && strcmp(drawn, "librewolf") == 0);

  event.xproperty.state = PropertyDelete;
  fixture_class = fixture_instance = NULL;
  maps = 0;
  propertynotify(&event);
  assert(maps > 0 && strcmp(drawn, "app") == 0);
  puts("PASS: app labels, private-title isolation, missing class, class changes/deletion");
  return 0;
}
