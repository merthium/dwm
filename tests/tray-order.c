/* Exercise real tray ordering/property handling without opening an X server. */
#define XGetClassHint fixture_class_hint
#define XGetTextProperty fixture_text_property
#define main dwm_main
#include "../dwm.c"
#undef main
#include <assert.h>

static struct {
  const char *class, *instance, *title, *nettitle;
} identities[] = {
    {"Nm-Applet", "nm-applet", "NetworkManager Applet", NULL},
    {"snixembed", "snixembed", "bLuEmAn", NULL},
    {NULL, "CLIPIT", "Clipboard", NULL},
    {"Alpha", "alpha", "Changing status", NULL},
    {"snixembed", "snixembed", "Ferdium", NULL},
    {NULL, NULL, NULL, NULL},
};
static Client icons[LENGTH(identities)];
static Systray tray;
static unsigned int permutations;

Status fixture_class_hint(Display *display, Window window, XClassHint *hint) {
  (void)display;
  assert(window >= 1 && window <= LENGTH(identities));
  const size_t n = window - 1;
  hint->res_class = identities[n].class ? strdup(identities[n].class) : NULL;
  hint->res_name = identities[n].instance ? strdup(identities[n].instance) : NULL;
  return hint->res_class || hint->res_name;
}

Status fixture_text_property(Display *display, Window window,
                             XTextProperty *property, Atom atom) {
  (void)display;
  assert(window >= 1 && window <= LENGTH(identities));
  const char *title = atom == netatom[NetWMName] ?
      identities[window - 1].nettitle : identities[window - 1].title;
  if (!title)
    return 0;
  property->value = (unsigned char *)strdup(title);
  property->encoding = XA_STRING;
  property->format = 8;
  property->nitems = strlen(title);
  return 1;
}

static void change(size_t n, Atom atom, int state) {
  XEvent event = {0};
  event.xproperty.window = n + 1;
  event.xproperty.atom = atom;
  event.xproperty.state = state;
  propertynotify(&event);
}

static void expect(const Window *order, size_t n) {
  Client *icon = tray.icons;
  for (size_t i = 0; i < n; i++, icon = icon->next) {
    if (!icon || icon->win != order[i]) {
      fprintf(stderr, "tray position %zu: expected window %lu, got %lu\n",
              i, order[i], icon ? icon->win : 0);
      exit(1);
    }
  }
  assert(!icon);
}

static void permute(size_t *order, size_t start) {
  if (start == LENGTH(identities)) {
    tray.icons = NULL;
    memset(icons, 0, sizeof icons);
    for (size_t j = 0; j < LENGTH(identities); j++) {
      size_t n = order[j];
      icons[n].win = n + 1;
      icons[n].next = tray.icons;
      tray.icons = &icons[n];
      change(n, XA_WM_CLASS, PropertyNewValue);
    }
    const Window expected[] = {1, 2, 3, 4, 5, 6};
    expect(expected, LENGTH(expected));
    permutations++;
    return;
  }
  for (size_t i = start; i < LENGTH(identities); i++) {
    size_t tmp = order[start]; order[start] = order[i]; order[i] = tmp;
    permute(order, start + 1);
    tmp = order[start]; order[start] = order[i]; order[i] = tmp;
  }
}

int main(void) {
  size_t order[] = {0, 1, 2, 3, 4, 5};
  systray = &tray;
  netatom[NetWMName] = 1234;
  permute(order, 0);
  assert(permutations == 720);

  /* A late SNI title changes position; _NET_WM_NAME takes precedence. */
  identities[4].nettitle = "Aardvark";
  change(4, netatom[NetWMName], PropertyNewValue);
  const Window aardvark[] = {1, 2, 3, 5, 4, 6};
  expect(aardvark, LENGTH(aardvark));
  identities[4].nettitle = NULL;
  change(4, netatom[NetWMName], PropertyDelete);
  const Window standard[] = {1, 2, 3, 4, 5, 6};
  expect(standard, LENGTH(standard));

  /* Native Bluetooth classes work even with a localized title. */
  identities[1].class = "BLUEMAN-APPLET";
  identities[1].title = "Bluetooth devices";
  change(1, XA_WM_CLASS, PropertyNewValue);
  expect(standard, LENGTH(standard));

  /* Native popup frontend also keeps Bluetooth in the same slot. */
  identities[1].class = "Blueman-tray-menu";
  identities[1].instance = "blueman-tray-menu";
  identities[1].title = "blueman-tray-menu";
  change(1, XA_WM_CLASS, PropertyNewValue);
  expect(standard, LENGTH(standard));
  identities[1].instance = "snixembed";

  /* Deleted identity loses priority and sorts by the remaining app name. */
  identities[1].class = "snixembed";
  identities[1].title = "Zebra";
  change(1, XA_WM_NAME, PropertyNewValue);
  const Window zebra[] = {1, 3, 4, 5, 2, 6};
  expect(zebra, LENGTH(zebra));
  identities[1].title = NULL;
  change(1, XA_WM_NAME, PropertyDelete);
  expect(zebra, LENGTH(zebra));
  identities[2].instance = NULL;
  change(2, XA_WM_CLASS, PropertyDelete);
  const Window classdeleted[] = {1, 4, 3, 5, 2, 6};
  expect(classdeleted, LENGTH(classdeleted));

  /* Duplicate or absent identities cannot fall back to docking order. */
  for (size_t j = LENGTH(identities); j > 0; j--) {
    identities[j - 1].class = "SameApp";
    identities[j - 1].instance = "sameapp";
    identities[j - 1].title = "Same title";
    change(j - 1, XA_WM_CLASS, PropertyNewValue);
  }
  expect(standard, LENGTH(standard));
  for (size_t j = LENGTH(identities); j > 0; j--) {
    identities[j - 1].class = identities[j - 1].instance = NULL;
    identities[j - 1].title = NULL;
    change(j - 1, XA_WM_CLASS, PropertyDelete);
  }
  expect(standard, LENGTH(standard));
  puts("PASS: 720 dock permutations, native/SNI identities, property changes/deletion, duplicate/anonymous identities");
  return 0;
}
