#ifndef _filmroll_h_
#define _filmroll_h_

struct event;

/* button hook, called from handle_common_events_by_feature; 0 = handled */
int handle_filmroll_keys(struct event *event);

/* called instead of opening the Magic Lantern menu (menu.c) */
int filmroll_menu_requested();      /* 0: no film OS (the module isn't loaded), ML's menu opens */

/* ML's own LiveView overlays stay out of the way of the film OS and the
 * viewfinder HUD (always, in this fork) */
int filmroll_overlays_active();

#endif
