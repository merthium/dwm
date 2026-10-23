// DWM CONFIG
#include "push.c"
#include <X11/XF86keysym.h>
#include <X11/keysym.h>

static const int borderpx = 2;
static const int focusmonOnEnter = 0;
static const int defaultgappx = 0;
static int gappx = 0;
static const int snap = 20;
static const unsigned int systraypinning = 0;
static const unsigned int systrayspacing = 2;
static const int systraypinningfailfirst = 1;
static const int showsystray = 1;
static const int showbar = 1;
static const int topbar = 0;
static const Bool viewontag = True;
static const char *fonts[] = {
    "Fragment Mono:style=Regular:size=14:antialias=true:hinting=true:embolden=true",
    "Lilex Nerd Font Mono:style=Regular:size=13:antialias=true:hinting=true"
};
static const char dmenufont[] =
    "Noto Sans Mono:style=Regular:size=11:antialias=true:hinting=true";

/* eyes.nvim dark: black background, neutral grayscale surfaces and text. */
static char col_bg[8]          = "#000000";
static char col_fg_norm[8]     = "#a0a0a0";
static char col_border_norm[8] = "#202020";
static char col_bg_sel[8]      = "#202020";
static char col_fg_sel[8]      = "#a0a0a0";
static char col_border_sel[8]  = "#808080";
static char col_bg_tab[8]      = "#101010";
static char col_bg_back[8]     = "#000000";
static char col_fg_tab[8]      = "#a0a0a0";
static char col_fg_hid[8]      = "#606060";
static const char *colors[][3] = {
    [SchemeNorm]  = {col_fg_norm, col_bg, col_border_norm},
    [SchemeSel]   = {col_fg_sel, col_bg_sel, col_border_sel},
    [SchemeTitle] = {col_fg_tab, col_bg_tab, col_bg_tab},
    [SchemeHid]   = {col_fg_hid, col_bg_back, col_bg_back},
};

static const char *tags[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9"};
static const Rule rules[] = {
    {"Emacs", NULL, NULL, 1 << 1, 0, -1},
    {"popup", NULL, NULL, 0, 1, -1},
    {"Blueman-manager", NULL, NULL, 0, 1, -1},
    {"URxvt", "scratchpad", NULL, SPTAG, 1, -1},
};
static const float mfact = 0.55;
static const int nmaster = 1;
static const int resizehints = 0;
static const int lockfullscreen = 1;
static const Layout layouts[] = {
    {"t", tile},
    {"f", NULL},
    {"m", monocle},
};

#define MODKEY Mod1Mask
#define TAGKEYS(KEY, TAG)                                                      \
  {MODKEY, KEY, view, {.ui = 1 << TAG}},                                       \
      {MODKEY | ControlMask, KEY, toggleview, {.ui = 1 << TAG}},               \
      {MODKEY | ShiftMask, KEY, tag, {.ui = 1 << TAG}},                        \
      {MODKEY | ControlMask | ShiftMask, KEY, toggletag, {.ui = 1 << TAG}},
#define SHCMD(cmd)                                                             \
  {                                                                            \
    .v = (const char *[]) { "/bin/sh", "-c", cmd, NULL }                     \
  }

static char dmenumon[12] = "0";
static const char *dmenucmd[] = {
    "dmenu_run", "-m", dmenumon, "-nb", col_bg, "-nf", col_fg_norm,
    "-sb", col_bg_sel, "-sf", col_fg_sel, "-p", ">>>", NULL};
static const char *termcmd[] = {"/bin/sh", "-c", "urxvtc +j || urxvt +j", NULL};
static const char *filetuicmd[] = {"urxvt", "-e", "mc", NULL};
static const char *scratchpadcmd[] = {
    "/bin/sh", "-c", "urxvtc +j -name scratchpad -geometry 100x28 || urxvt +j -name scratchpad -geometry 100x28", NULL};
static const char *brightnessup[] = {"brightnessctl", "set", "5%+", NULL};
static const char *brightnessdown[] = {"brightnessctl", "set", "5%-", NULL};
static const char *pdfcmd[] = {"zathura", NULL};
static const char *browsercmd[] = {"xdg-open", "https://duckduckgo.com", NULL};
static const char *volumecmd[] = {"pavucontrol", NULL};
static const char *taskcmd[] = {"urxvt", "-e", "htop", NULL};
static const char *flameshotcmd[] = {"sh", "-c", "QT_QPA_PLATFORM=xcb flameshot gui", NULL};
static const char *statusbatcmd[] = {
    "urxvtc", "+j", "-e", "watch", "-n", "5", "upower", "-i",
    "/org/freedesktop/UPower/devices/battery_BAT0", NULL};
static const char *statusclockcmd[] = {
    "/bin/sh", "-c", "urxvtc +j -e sh -c 'cal -m; printf \"\\nKapatmak icin Enter\\n\"; read _'", NULL};
static const char **statuscmds[] = {statusbatcmd, statusclockcmd};

static Key keys[] = {
    {0, XK_Print, spawn, {.v = flameshotcmd}},
    {MODKEY, XK_Print, spawn, SHCMD("flameshot full -p ~/Pictures/Screenshots")},
    {MODKEY | ShiftMask, XK_p, spawn, {.v = pdfcmd}},
    {MODKEY, XK_y, spawn, SHCMD("networkmanager_dmenu")},
    {0, XF86XK_MonBrightnessUp, spawn, {.v = brightnessup}},
    {0, XF86XK_MonBrightnessDown, spawn, {.v = brightnessdown}},
    {MODKEY, XK_e, spawn, {.v = filetuicmd}},
    {MODKEY, XK_d, spawn, {.v = pdfcmd}},
    {MODKEY, XK_w, spawn, {.v = volumecmd}},
    {MODKEY, XK_r, spawn, {.v = taskcmd}},
    {MODKEY | ShiftMask, XK_q, quit, {.i = 0}},
    {MODKEY | ControlMask | ShiftMask, XK_q, quit, {.i = 1}},
    {0, XF86XK_AudioLowerVolume, spawn, SHCMD("pactl set-sink-volume @DEFAULT_SINK@ -5%")},
    {0, XF86XK_AudioRaiseVolume, spawn, SHCMD("pactl set-sink-volume @DEFAULT_SINK@ +5%")},
    {0, XF86XK_AudioMute, spawn, SHCMD("pactl set-sink-mute @DEFAULT_SINK@ toggle")},
    {MODKEY, XK_n, spawn, SHCMD("urxvtc +j -e nvim || urxvt +j -e nvim")},
    {MODKEY, XK_p, spawn, {.v = dmenucmd}},
    {MODKEY, XK_a, spawn, {.v = termcmd}},
    {MODKEY, XK_s, spawn, {.v = browsercmd}},
    {MODKEY, XK_b, togglebar, {0}},
    {MODKEY, XK_j, focusstackvis, {.i = +1}},
    {MODKEY, XK_k, focusstackvis, {.i = -1}},
    {MODKEY | ShiftMask, XK_j, focusstackhid, {.i = +1}},
    {MODKEY | ShiftMask, XK_k, focusstackhid, {.i = -1}},
    {MODKEY | ShiftMask, XK_Down, pushdown, {0}},
    {MODKEY | ShiftMask, XK_Up, pushup, {0}},
    {MODKEY, XK_Right, incnmaster, {.i = -1}},
    {MODKEY, XK_Left, incnmaster, {.i = +1}},
    {MODKEY, XK_l, setmfact, {.f = -0.03}},
    {MODKEY, XK_i, setmfact, {.f = +0.03}},
    {MODKEY, XK_Return, zoom, {0}},
    {MODKEY, XK_Tab, view, {0}},
    {MODKEY, XK_q, killclient, {0}},
    {MODKEY, XK_t, setlayout, {.v = &layouts[0]}},
    {MODKEY, XK_f, setlayout, {.v = &layouts[1]}},
    {MODKEY, XK_m, setlayout, {.v = &layouts[2]}},
    {MODKEY, XK_space, setlayout, {0}},
    {MODKEY | ShiftMask, XK_space, togglefloating, {0}},
    {MODKEY, XK_0, view, {.ui = ~0}},
    {MODKEY | ShiftMask, XK_0, tag, {.ui = ~0}},
    {MODKEY, XK_comma, focusmon, {.i = -1}},
    {MODKEY, XK_period, focusmon, {.i = +1}},
    {MODKEY | ShiftMask, XK_comma, tagmon, {.i = -1}},
    {MODKEY | ShiftMask, XK_period, tagmon, {.i = +1}},
    {MODKEY | ShiftMask, XK_s, showall, {0}},
    {MODKEY, XK_h, hide, {0}},
    {MODKEY, XK_grave, togglescratch, {.v = scratchpadcmd}},
    TAGKEYS(XK_1, 0) TAGKEYS(XK_2, 1) TAGKEYS(XK_3, 2) TAGKEYS(XK_4, 3)
        TAGKEYS(XK_5, 4) TAGKEYS(XK_6, 5) TAGKEYS(XK_7, 6) TAGKEYS(XK_8, 7)
            TAGKEYS(XK_9, 8)};

static Button buttons[] = {
    {ClkLtSymbol, 0, Button1, setlayout, {0}},
    {ClkLtSymbol, 0, Button3, setlayout, {.v = &layouts[2]}},
    {ClkWinTitle, 0, Button1, togglewin, {0}},
    {ClkWinTitle, 0, Button2, zoom, {0}},
    {ClkStatusText, 0, Button1, statusclick, {0}},
    {ClkStatusText, 0, Button2, spawn, {.v = termcmd}},
    {ClkStatusText, 0, Button3, statusclick, {0}},
    {ClkClientWin, MODKEY, Button1, movemouse, {0}},
    {ClkClientWin, MODKEY, Button2, togglefloating, {0}},
    {ClkClientWin, MODKEY, Button3, resizemouse, {0}},
    {ClkTagBar, 0, Button1, view, {0}},
    {ClkTagBar, 0, Button3, toggleview, {0}},
    {ClkTagBar, MODKEY, Button1, tag, {0}},
    {ClkTagBar, MODKEY, Button3, toggletag, {0}},
};
