/* Exercise real movement/state logic without an X server or wall-clock sleeps. */
#define XMoveWindow capture_move
#define XFlush ignore_flush
#define XSync ignore_sync
#define XCheckMaskEvent no_crossing
#define XPresentNotifyMSC capture_notify
#define main dwm_main
#include "../dwm.c"
#undef main
#include <assert.h>

static int moves, lastx, lasty, requests;
static Window lastwin;
static uint32_t lastserial;
static uint64_t target;
int capture_move(Display *display, Window window, int x, int y) {
  (void)display;
  moves++; lastwin = window; lastx = x; lasty = y;
  return 1;
}
int ignore_flush(Display *display) { (void)display; return 1; }
int ignore_sync(Display *display, Bool discard) { (void)display; (void)discard; return 1; }
Bool no_crossing(Display *display, long mask, XEvent *event) {
  (void)display; (void)mask; (void)event; return False;
}
void capture_notify(Display *display, Window window, uint32_t serial,
                    uint64_t msc, uint64_t divisor, uint64_t remainder) {
  (void)display; (void)window;
  assert(!divisor && !remainder);
  requests++; lastserial = serial; target = msc;
}

int main(void) {
  Monitor m = {.mw = 1920, .ww = 1920, .tagset = {2, 2}};
  Monitor other = {.mw = 1920};
  Client incoming = {.win = 42, .tags = 2, .mon = &m, .x = 100, .y = 50, .w = 600, .h = 400};
  Client shared = {.win = 43, .tags = 3, .mon = &m, .x = 800, .y = 50};
  Client hidden = {.win = 44, .tags = 2, .mon = &m, .ishidden = 1};
  Client outgoing = {.win = 45, .tags = 1, .mon = &m};
  incoming.next = &shared; shared.next = &hidden; hidden.next = &outgoing;
  mons = selmon = &m; m.clients = m.sel = &incoming;
  present_opcode = 1;

  assert(!taganim_can_start(&m, 1, 1));
  assert(!taganim_can_start(&m, 0, 2));
  assert(!taganim_can_start(&m, 3, 4));
  assert(!taganim_can_start(&m, 1, TAGMASK));
  assert(taganim_can_start(&m, 1, 2) == 1);
  taganim_begin(&m, 1, 2);
  assert(moves == 1 && lastwin == 42 && lastx == 132 && lasty == 50);
  assert(incoming.x == 100 && incoming.y == 50);
  assert(requests == 1 && target == 0);
  taganim_step(1000); /* Initial notification establishes the frame epoch. */
  assert(moves == 1 && target == 1001);
  int previousx = lastx;
  for (int frame = 1; frame <= (int)taganimframes; frame++) {
    taganim_step(1000 + frame);
    assert(moves == frame + 1 && lastwin == 42);
    assert(previousx - lastx >= taganimpx / (int)taganimframes &&
           previousx - lastx <= (taganimpx + (int)taganimframes - 1) / (int)taganimframes);
    previousx = lastx;
  }
  assert(lastx == 100 && !taganim_dir && requests == (int)taganimframes + 1);
  int completed = moves, notified = requests;
  taganim_step(999999);
  taganim_finish();
  assert(moves == completed && requests == notified); /* No idle refresh request. */

  shared.tags = 6;
  taganim_begin(&m, 4, 2);
  assert(lastx == 68); /* Left and right offsets must use signed arithmetic. */
  taganim_step(2000);
  taganim_step(2000 + taganimframes / 2);
  assert(lastx == 84); /* Delayed notification skips directly to current frame. */
  uint32_t canceled = lastserial;
  taganim_finish();
  assert(lastx == 100 && !taganim_dir && taganim_serial != canceled);
  taganim_step(2004);
  assert(requests == notified + 3);
  shared.tags = 3;
  m.next = &other;
  assert(!taganim_can_start(&m, 1, 2));
  m.next = NULL;
  incoming.isfullscreen = 1;
  assert(!taganim_can_start(&m, 1, 2));
  incoming.isfullscreen = 0;
  incoming.swallowed = &shared;
  assert(!taganim_can_start(&m, 1, 2));
  incoming.swallowed = NULL;
  incoming.ishidden = 1;
  assert(!taganim_can_start(&m, 1, 2));
  incoming.ishidden = 0;
  present_opcode = -1;
  completed = moves; notified = requests;
  taganim_begin(&m, 1, 2);
  assert(moves == completed && requests == notified && !taganim_dir);
  present_opcode = 1;

  /* Empty destinations animate the departing window in either direction. */
  m.clients = &incoming; incoming.next = NULL;
  for (unsigned int destination = 1; destination <= 4; destination += 3) {
    m.tagset[0] = destination;
    assert(taganim_can_start(&m, 2, destination) == -1);
    taganim_begin(&m, 2, destination);
    assert(lastx == 100);
    taganim_step(3000);
    previousx = lastx;
    for (int frame = 1; frame < (int)taganimframes; frame++) {
      taganim_step(3000 + frame);
      int distance = (destination == 4 ? 1 : -1) * (previousx - lastx);
      assert(distance >= taganimpx / (int)taganimframes &&
             distance <= (taganimpx + (int)taganimframes - 1) / (int)taganimframes);
      previousx = lastx;
    }
    assert(incoming.x == 100);
    taganim_step(3000 + taganimframes);
    assert(lastx == -2 * WIDTH(&incoming) && !taganim_dir);
  }
  completed = moves; notified = requests;
  taganim_begin(&m, 8, 4); /* Empty to empty: no phantom departure. */
  assert(moves == completed && requests == notified);
  m.tagset[0] = 2; m.clients = &shared;
  assert(!taganim_can_start(&m, 1, 2)); /* Shared client already visible. */
  m.clients = &incoming; incoming.next = &shared;
  taganim_begin(&m, 1, 2);
  completed = moves;
  m.clients = &shared; /* Unmanaged client leaves no retained Client pointer. */
  taganim_step(4000); taganim_step(4000 + taganimframes + 4);
  assert(moves == completed && !taganim_dir);
  m.clients = &incoming;
  taganim_begin(&m, 1, 2);
  taganim_step(0); /* MSC zero is valid on a freshly started display. */
  taganim_step(1);
  assert(lastx < 132 && lastx >= 132 - (taganimpx + (int)taganimframes - 1) / (int)taganimframes);
  taganim_finish();
  puts("PASS: refresh pacing, signed directions, skips, exclusions, cancellation, no idle requests");
  return 0;
}
