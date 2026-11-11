/* fos_dark: the darkroom.
 *
 * Rolls that have been shot and rewound wait here to be developed. The
 * camera develops them itself (fos_dev.c), a slice at a time between the
 * frames of a darkroom scene under the red safelight: the latent image
 * comes up on the paper in the tray, the print darkens to its full depth,
 * and goes up on the line to dry. The prints are looked at here as well,
 * each on its own palette, and PLAY comes here too: nothing undeveloped is
 * ever shown.
 */

#include "fos_ui.h"
#include "fos_dev.h"
#include <string.h>

#define MAX_ROLLS 48

/* The darkroom's lists live in memory taken while the film OS is up, not
 * in autoexec.bin: on the EOS M every byte of that is taken from Canon's
 * own memory at boot. */
static struct dark_mem
{
    struct fos_lab_roll rolls[MAX_ROLLS];
    uint8_t marked[MAX_ROLLS];
    uint8_t newonly[MAX_ROLLS];         /* queued for "develop new": the roll is still in the camera, its newest frames first, no contact sheet */
    int queue[MAX_ROLLS];
    struct fos_lab_frame frames[FOS_MAX_FRAMES], meta[FOS_MAX_FRAMES];
    char paths[FOS_MAX_FRAMES][64];
    struct fos_print print;
} *dm;
#define rolls   (dm->rolls)
#define marked  (dm->marked)
#define queue   (dm->queue)
#define newonly (dm->newonly)
#define pv_meta (dm->meta)
#define pv_print (dm->print)

static int nrolls;

static int dark_mem()
{
    if (!dm && (dm = fos_port_alloc(sizeof(*dm)))) memset(dm, 0, sizeof(*dm));
    return dm != 0;
}

void dark_free()
{
    fos_port_free(dm);
    dm = 0;
}

static void rescan()
{
    nrolls = dm ? fos_lab_scan(rolls, MAX_ROLLS) : 0;
}

static int roll_index(int number)
{
    for (int i = 0; i < nrolls; i++)
        if (rolls[i].number == number) return i;
    return -1;
}

/* developed: every frame has its print and the contact sheet is there;
 * waiting: shot, out of the camera, and a print or the sheet missing
 * (developing again skips the prints already made) */
/* developed: every frame printed, and every print in DCIM as a JPEG */
static int is_developed(const struct fos_lab_roll *r) { return r->sheet && r->developed >= r->shot && r->jpegs >= r->developed; }
/* Basic mode: a finished roll still in the camera waits too; developing it finishes it (fos_port_action) */
static int finished_in_camera(const struct fos_lab_roll *r) { return r->in_camera && !ui_realistic() && r->shot >= r->exposures; }
static int is_waiting(const struct fos_lab_roll *r)
{
    return !is_developed(r) && (!r->in_camera || finished_in_camera(r)) && r->shot > 0;
}
static int has_prints(const struct fos_lab_roll *r) { return r->sheet || r->developed > 0; }

/* about how long a frame takes at each print size, measured while developing (a guess for the 6D Mark II's
 * 26 MP until one has been made at that size) */
static int ms_per_frame[FOS_SIZES] = { 11000, 22000, 36000 };     /* measured with LiveView off, 2026-11-10 (medium a guess) */
static int dv_size;                     /* FOS_SIZE_* asked for: the setting, or less once the camera hadn't the memory */
static int dv_made = -1, dv_w, dv_h;    /* what the frame in the tray comes to (-1: not known yet) */
static int cur_size() { return fos_clamp(fos_port_get(FS_SIZE), 0, FOS_SIZES - 1); }
static int eta_ms() { return ms_per_frame[fos_clamp(dv_made >= 0 ? dv_made : dv_size, 0, FOS_SIZES - 1)]; }

static void format_eta(char *buf, int size, int frames, int extra_ms)
{
    int s = (int) (((int64_t) frames * eta_ms() + extra_ms) / 1000);
    if (s < 60) snprintf(buf, size, "under a minute");
    else if (s < 90 * 60) snprintf(buf, size, "about %d min", (s + 30) / 60);
    else snprintf(buf, size, "about %d h %02d min", s / 3600, (s % 3600 + 30) / 60);
}

static const char *short_date(const char *when)
{
    /* "2026-10-22 17:45" -> "22 Oct 2026" */
    static const char *const mon[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
    static char b[16];
    int m = (when[5] - '0') * 10 + (when[6] - '0');
    if (strlen(when) < 10 || m < 1 || m > 12) return "";
    /* put together by hand: the camera's snprintf has no %c (the list showed a row of 1s for the date) */
    int i = 0;
    if (when[8] != '0') b[i++] = when[8];
    b[i++] = when[9]; b[i++] = ' ';
    memcpy(b + i, mon[m - 1], 3); i += 3;
    b[i++] = ' ';
    memcpy(b + i, when, 4); i += 4;
    b[i] = 0;
    return b;
}

/* ================================================================ the list */

static int dk_sel;
static struct spring dk_hl, dk_scroll;
static int dk_hints_for = -1;           /* the hints show what SET does on this row */
static int dk_del;                      /* Trash asked to delete the marked rolls, until the next key */

#define DK_X    24
#define DK_W    672
#define DK_Y    124
#define DK_ROW  60
#define DK_ROWS 5

static int count_waiting(int *frames)
{
    int n = 0, f = 0;
    for (int i = 0; i < nrolls; i++)
        if (is_waiting(&rolls[i])) { n++; f += rolls[i].shot - (rolls[i].jpegs < rolls[i].developed ? rolls[i].jpegs : rolls[i].developed); }
    if (frames) *frames = f;
    return n;
}

static int count_marked_any()           /* marked for deleting too, developed or not */
{
    int n = 0;
    for (int i = 0; i < nrolls; i++) n += marked[i];
    return n;
}

/* what the hints below the list depend on: the row, how many are marked, whether Trash was asked */
static int hint_key() { return (dk_sel * 64 + count_marked_any()) * 2 + (dk_del != 0); }

static int count_marked()
{
    int n = 0;
    for (int i = 0; i < nrolls; i++) n += marked[i] && is_waiting(&rolls[i]);
    return n;
}

static void dark_bake()
{
    ui_bg_plain();
    ui_to_bg(1);
    ui_status_bar("DARKROOM");
    fos_text(cv, &fos_font_lg, DK_X, 82, "Darkroom", C_TEXT, OPAQUE, 0);
    char b[96], eta[32];
    int frames, waiting = count_waiting(&frames);
    if (!nrolls)
        snprintf(b, sizeof(b), ui_realistic() ? "No rolls yet " T_MDASH " finish one, rewind it, and it waits here."
                                              : "No rolls yet " T_MDASH " finish one, and it waits here.");
    else if (waiting)
    {
        format_eta(eta, sizeof(eta), frames, 0);
        snprintf(b, sizeof(b), "%d roll%s to develop " T_DOT " %d frames " T_DOT " %s", waiting, waiting > 1 ? "s" : "", frames, eta);
    }
    else
        snprintf(b, sizeof(b), "Everything is developed " T_DOT " %d roll%s of prints", nrolls, nrolls > 1 ? "s" : "");
    int w = fos_text(cv, &fos_font_sm, DK_X, 106, b, C_TEXT3, OPAQUE, 0);
    /* the camera takes its time; a computer doesn't */
    if (waiting && w + 260 < DK_W)
        fos_text_r(cv, &fos_font_sm, DK_X + DK_W, 106, "Faster: Darkrooms on a computer", C_ACCENT2, OPAQUE, 0);
    ui_to_bg(0);
}

static void dark_hints()
{
    const struct fos_lab_roll *r = nrolls ? &rolls[dk_sel] : 0;
    int nm = count_marked();
    char dev[32];
    if (nm) snprintf(dev, sizeof(dev), "Develop %d", nm);
    else snprintf(dev, sizeof(dev), "Develop all");
    struct hint h[5];
    int n = 0;
    const int any = count_marked_any();
    char del[24];
    snprintf(del, sizeof(del), "Delete %d", any);
    if (r && !(r->in_camera && !finished_in_camera(r))) h[n++] = (struct hint) { "SET", marked[dk_sel] ? "Unmark" : "Mark" };
    if (count_waiting(0) && !(any && !nm)) h[n++] = (struct hint) { "INFO", dev };
    h[n++] = (struct hint) { "PLAY", "Prints" };
    if (any) h[n++] = (struct hint) { "TRASH", dk_del ? "Delete?" : del };
    h[n++] = (struct hint) { "MENU", "Back" };
    ui_bg_restore(0, HINTS_Y, FOS_W, FOS_H - HINTS_Y);
    ui_hints(h, n);
    dk_hints_for = hint_key();
}

void dark_enter(int arg)
{
    fos_port_roll(&ui_roll);
    dark_mem();
    rescan();
    if (dm) memset(marked, 0, sizeof(marked));
    int i = arg ? roll_index(arg) : -1;
    if (i < 0)
    {
        /* the newest roll waiting, else the newest */
        i = 0;
        for (int k = 0; k < nrolls; k++)
            if (is_waiting(&rolls[k])) { i = k; break; }
    }
    dk_sel = i;
    dk_hl.x = (float) dk_sel; dk_hl.v = 0;
    dk_scroll.x = (float) fos_clamp(dk_sel - DK_ROWS / 2, 0, nrolls > DK_ROWS ? nrolls - DK_ROWS : 0);
    dk_scroll.v = 0;
    dk_hints_for = -1;
    dk_del = 0;
    dark_bake();
}

void dark_queue_clear();
void dark_queue_add(int roll);

static void dark_develop()
{
    /* the marked rolls, or every one waiting; oldest first */
    int nm = count_marked();
    if (count_marked_any() && !nm) { ui_toast("The marked rolls are developed already"); return; }
    dark_queue_clear();
    for (int i = nrolls - 1; i >= 0; i--)
        if (is_waiting(&rolls[i]) && (!nm || marked[i]))
        {
            if (finished_in_camera(&rolls[i])) fos_port_action(FA_REWIND, 0, 0);    /* out of the camera first */
            dark_queue_add(rolls[i].number);
        }
    ui_go(SCR_DEVELOP, 0, GO_FADE);
}

void dark_key(int k)
{
    if (!dm)
    {
        if (k == FK_MENU) ui_go(SCR_HOME, 2, GO_POP);
        return;
    }
    const struct fos_lab_roll *r = nrolls ? &rolls[dk_sel] : 0;
    const int asked = dk_del;
    if (k != FK_HOME && dk_del) { dk_del = 0; dk_hints_for = -1; }      /* any other key calls it off */
    switch (k)
    {
        case FK_HOME:                   /* the Trash button: delete the marked rolls, asked twice */
        {
            int nd = count_marked_any();
            if (!nd)
            {
                if (r) ui_toast("Mark rolls with SET first, then press Trash");
                break;
            }
            char msg[112];
            if (!asked)
            {
                int dv = 0;
                for (int j = 0; j < nrolls; j++) dv += marked[j] && rolls[j].developed;
                if (dv) snprintf(msg, sizeof(msg), "Press Trash again to delete %d roll%s, %d developed " T_MDASH " the RAW files stay", nd, nd > 1 ? "s" : "", dv);
                else snprintf(msg, sizeof(msg), "Press Trash again to delete %d roll%s " T_MDASH " the RAW files stay", nd, nd > 1 ? "s" : "");
                dk_del = 1;
                dk_hints_for = -1;
                ui_toast(msg);
                break;
            }
            int nums[MAX_ROLLS], nn = 0, gone = 0;
            for (int j = 0; j < nrolls; j++) if (marked[j]) nums[nn++] = rolls[j].number;
            for (int j = 0; j < nn; j++) gone += fos_lab_delete_roll(nums[j], dm->frames, FOS_MAX_FRAMES);
            snprintf(msg, sizeof(msg), "darkroom: %d rolls deleted, %d files", nn, gone);
            fos_port_log(msg);
            dk_del = 0;
            memset(marked, 0, sizeof(marked));
            rescan();
            dk_sel = fos_clamp(dk_sel, 0, nrolls ? nrolls - 1 : 0);
            dk_hl.x = (float) dk_sel; dk_hl.v = 0;
            dk_scroll.x = (float) fos_clamp(dk_sel - DK_ROWS / 2, 0, nrolls > DK_ROWS ? nrolls - DK_ROWS : 0);
            dk_scroll.v = 0;
            dk_hints_for = -1;
            dark_bake();
            snprintf(msg, sizeof(msg), "%d roll%s deleted " T_MDASH " the RAW files stay", nn, nn > 1 ? "s" : "");
            ui_toast(msg);
            break;
        }
        case FK_UP: case FK_WHEEL_L:
            if (dk_sel > 0) dk_sel--;
            break;
        case FK_DOWN: case FK_WHEEL_R:
            if (dk_sel < nrolls - 1) dk_sel++;
            break;
        case FK_SET: case FK_RIGHT:
            if (!r) break;
            if (r->in_camera && !finished_in_camera(r))
                ui_toast(ui_realistic() ? "Still in the camera " T_MDASH " rewind it first"
                                        : "Still in the camera " T_MDASH " finish it on the Roll page first");
            else if (k == FK_SET) marked[dk_sel] ^= 1;                  /* any roll: to develop it, or to delete it */
            else if (has_prints(r)) ui_go(SCR_PRINTS, r->number, GO_FADE);
            break;
        case FK_INFO:
            if (count_waiting(0)) dark_develop();
            else ui_toast(nrolls ? "Nothing to develop" : "No rolls yet");
            break;
        case FK_PLAY:
            ui_go(SCR_PRINTS, r && has_prints(r) ? r->number : 0, GO_FADE);
            break;
        case FK_MENU:
            ui_go(SCR_HOME, 2, GO_POP);
            break;
    }
    ui_damage(DK_Y - 4, FOS_H);
}

int dark_update(int dt)
{
    int moving = 0;
    float top = (float) fos_clamp(dk_sel - DK_ROWS / 2, 0, nrolls > DK_ROWS ? nrolls - DK_ROWS : 0);
    moving |= spring_step(&dk_hl, (float) dk_sel, dt, 3.4f, 0.82f);
    moving |= spring_step(&dk_scroll, top, dt, 2.6f, 0.9f);
    if (moving) ui_damage(DK_Y - 4, DK_Y + DK_ROWS * DK_ROW + 4);
    return moving;
}

/* a round mark: empty, or filled with a tick */
static void mark_dot(int cx, int cy, int on)
{
    if (on)
    {
        fos_disc(cv, cx * 256, cy * 256, 10 * 256, C_ACCENT, OPAQUE);
        fos_line(cv, (cx - 5) * 256, cy * 256, (cx - 1) * 256, (cy + 4) * 256, 2 * 256, FC(RAMP_GRAY, 12), OPAQUE);
        fos_line(cv, (cx - 1) * 256, (cy + 4) * 256, (cx + 6) * 256, (cy - 4) * 256, 2 * 256, FC(RAMP_GRAY, 12), OPAQUE);
    }
    else fos_ring(cv, cx * 256, cy * 256, 10 * 256, 2 * 256, FC(RAMP_GRAY, 90), OPAQUE);
}

static void dark_row(int i, int y)
{
    const struct fos_lab_roll *r = &rolls[i];
    int sel = i == dk_sel;
    int x = DK_X + 20;
    if (is_waiting(r) || marked[i]) mark_dot(x + 12, y + DK_ROW / 2, marked[i]);
    else ui_icon(r->in_camera ? ICON_FILM : ICON_PRINTS, x + 12, y + DK_ROW / 2, sel ? C_TEXT2 : C_TEXT3, OPAQUE);
    char b[96];
    fos_text(cv, &fos_font_mdb, x + 42, y + 27, fos_films[r->film].name, sel ? C_TEXT : C_TEXT2, OPAQUE, 0);
    const char *when = r->last[0] ? r->last : r->loaded;
    snprintf(b, sizeof(b), "Roll %03d " T_DOT " %d frame%s " T_DOT " %s", r->number, r->shot, r->shot == 1 ? "" : "s", short_date(when));
    fos_text(cv, &fos_font_sm, x + 42, y + 48, b, C_TEXT3, OPAQUE, 0);
    fcol c;
    if (r->in_camera && !finished_in_camera(r)) { snprintf(b, sizeof(b), "IN CAMERA"); c = C_TEXT3; }
    else if (is_developed(r)) { snprintf(b, sizeof(b), "DEVELOPED"); c = C_TEXT3; }
    else if (r->developed) { snprintf(b, sizeof(b), "%d OF %d DEVELOPED", r->developed, r->shot); c = C_ACCENT; }
    else { snprintf(b, sizeof(b), "TO DEVELOP"); c = C_ACCENT; }
    fos_text_r(cv, &fos_font_xs, DK_X + DK_W - 24, y + 36, b, c, OPAQUE, 48);
}

void dark_draw()
{
    if (ui_clobbered)
    {
        ui_clobbered = 0;
        ui_bg_restore(0, 0, FOS_W, FOS_H);
        ui_damage_all();
        dk_hints_for = -1;
    }
    if (!dm)
    {
        fos_text_c(cv, &fos_font_md, 360, DK_Y + 120, "Not enough memory for the darkroom.", C_TEXT3, OPAQUE, 0);
        return;
    }
    if (dk_hints_for != hint_key()) dark_hints();

    int y0 = DK_Y - 4, y1 = DK_Y + DK_ROWS * DK_ROW + 4;
    struct fos_canvas save = *cv;
    ui_bg_restore(0, y0, FOS_W, y1 - y0);
    fos_clip(cv, 0, DK_Y, FOS_W, DK_Y + DK_ROWS * DK_ROW);
    if (!nrolls)
    {
        fos_text_c(cv, &fos_font_md, 360, DK_Y + 120, ui_realistic() ? "Shot rolls appear here once they are rewound." : "Shot rolls appear here once they are finished.", C_TEXT3, OPAQUE, 0);
        *cv = save;
        return;
    }
    float top = dk_scroll.x;
    int hy = DK_Y + (int) ((dk_hl.x - top) * DK_ROW + 0.5f);
    fos_rrect(cv, DK_X, hy, DK_W, DK_ROW, 12, FC(RAMP_GRAY, 27), OPAQUE);
    fos_rrect(cv, DK_X, hy + 10, 3, DK_ROW - 20, 1, C_ACCENT, OPAQUE);
    for (int i = (int) top; i < nrolls && i <= (int) top + DK_ROWS; i++)
        dark_row(i, DK_Y + (int) ((i - top) * DK_ROW + 0.5f));
    *cv = save;
    /* the list goes on: a hint at the edge */
    if (top > 0.05f) fos_tri(cv, DK_X + DK_W - 14, DK_Y - 8, 8, 3, C_TEXT3, OPAQUE);
    if (top < nrolls - DK_ROWS - 0.05f) fos_tri(cv, DK_X + DK_W - 14, DK_Y + DK_ROWS * DK_ROW + 6, 8, 1, C_TEXT3, OPAQUE);
    ui_damage(y0, y1 + 8);
}

/* ================================================================ developing */

/* the scene, under the safelight */
#define SL(level)       FC(RAMP_RED, level)
#define SL_TEXT         SL(222)
#define SL_TEXT2        SL(160)
#define SL_TEXT3        SL(104)
#define SL_ACCENT       FC(RAMP_ORANGE, 200)

/* the tray, the paper in it and the picture on the paper */
#define TRAY_X   24
#define TRAY_Y   128
#define TRAY_W   400
#define TRAY_H   284
#define BATH_X   (TRAY_X + 12)
#define BATH_Y   (TRAY_Y + 12)
#define BATH_W   (TRAY_W - 24)
#define BATH_H   (TRAY_H - 24)
#define PAPER_W  348
#define PAPER_H  240
#define PAPER_X  (TRAY_X + (TRAY_W - PAPER_W) / 2)
#define PAPER_Y  (TRAY_Y + (TRAY_H - PAPER_H) / 2)
#define IMG_W    324
#define IMG_H    216
#define IMG_X    (PAPER_X + (PAPER_W - IMG_W) / 2)
#define IMG_Y    (PAPER_Y + (PAPER_H - IMG_H) / 2)
#define GHOST_W  (IMG_W / 2)
#define GHOST_H  (IMG_H / 2)
#define PAPER_LV 124                    /* white paper under the safelight */
#define BLACK_LV 9

/* the line the prints dry on */
#define LINE_Y   52
#define DRY_W    66
#define DRY_H    44
#define DRY_STEP 92
#define DRY_X0   36
#define DRY_N    7

/* the panel and its timer */
#define PANEL_CX 572
#define DIAL_CY2 282
#define DIAL_R2  58

#define EMERGE_MS 1200
#define HOLD_MS   400
#define LIFT_MS   450
#define SLIDE_MS  450
#define STEP_MS   100                   /* developing per frame while the scene moves (60 left the next frame at a little over half speed while a print came up and went to the line) */
#define CALM_MS   400                   /* and while it is still: the dial's hand ticks */

static int nqueue, qi;
static char dcim_dir[128];              /* the roll's JPEG folder, or "" */
static char cur_jpeg[160], cur_copy[160];   /* this frame's JPEG, and its twin beside the RAW (Canon's own folder) */
static void twin_path(char *out, int size, const char *cr2);
static void twin_copy(const char *from, const char *to);
static int nframes, fi, roll_ready;
static int cur_roll, cur_film, cur_frame;
static struct fos_dev *dev;
static int frame_t0;
/* The timer's hand is a second hand: a tick a second from when the developing began, as a darkroom timer
 * goes. (It followed the work's progress, whose stages are of very different lengths, and moved whenever
 * the screen happened to be drawn: fast, then still, then a jump.) */
static int dial_t0, dial_sec = -1;
static int total_frames, done_frames, failed_frames, rolls_done;
static int finished, asking_stop;
static struct spring stop_sheet;
static int dv_t;

/* the tray: what the paper shows */
enum { TRAY_LATENT, TRAY_EMERGE, TRAY_HOLD, TRAY_LIFT, TRAY_IN, TRAY_EMPTY };
static int tray, tray_t;
static uint8_t *ghost;                  /* GHOST_W x GHOST_H: the latent image */
static uint8_t *img;                    /* IMG_W x IMG_H: the finished print */
static int ghost_t;                     /* ms since the latent image was read */
static int ghost_final;                 /* read once more when the RAW is through */
static int pending_frame;               /* a print waiting for the tray, 0 = none */
static char pending_path[64];
static int tray_dirty = 1, line_dirty = 1, panel_dirty = 1, ui_damaged_dial = 1;
static int last_doing = -1;
static char last_error[72];             /* why the last frame that failed did */

/* the line: newest first */
static uint8_t *dry;                    /* DRY_N x DRY_W x DRY_H */
static int ndry, dry_t = 100000;        /* dry_t: ms since the newest went up */

static int from_roll_screen;            /* this run was started by "develop new" on the roll screen: it ends there */
static int cur_new, new_from;           /* the roll being developed is a "develop new" one; its first new frame */
void dark_queue_clear() { nqueue = 0; from_roll_screen = 0; if (dm) memset(newonly, 0, sizeof(newonly)); }
void dark_queue_add(int roll) { if (dark_mem() && nqueue < MAX_ROLLS) { newonly[nqueue] = 0; queue[nqueue++] = roll; } }
/* the roll still in the camera: what has no print yet, newest first */
void dark_queue_add_new(int roll) { if (dark_mem() && nqueue < MAX_ROLLS) { newonly[nqueue] = 1; queue[nqueue++] = roll; from_roll_screen = 1; } }

static int file_exists(const char *path);
/* "new" = shot after the newest frame that has a print; with no print yet, only the last frame (older ones are not "new") */
static int new_start(int roll, int n)
{
    char path[64];
    for (int i = n - 1; i >= 0; i--)
    {
        fos_lab_print_path(path, sizeof(path), roll, dm->frames[i].no);
        if (file_exists(path)) return i + 1;
    }
    return n > 0 ? n - 1 : 0;
}

/* how many frames "develop new" would do (frames loaded into dm->frames) */
int dark_pending(int roll)
{
    if (!dark_mem()) return 0;
    int n = fos_lab_frames(roll, dm->frames, FOS_MAX_FRAMES), todo = 0;
    for (int i = new_start(roll, n); i < n; i++) todo += !dm->frames[i].missing;
    return todo;
}

static int file_exists(const char *path)
{
    void *f = fos_port_open(path, 0);
    if (f) fos_port_close(f);
    return f != 0;
}

/* the print's picture, small and grey, for the tray */
static void load_tray_image(const char *path)
{
    struct fos_print p;
    memset(img, 255, IMG_W * IMG_H);
    if (!fos_print_load(path, &p)) return;
    uint8_t lum[FOS_PRINT_COLOURS + 1];
    lum[0] = 0;
    for (int i = 0; i < FOS_PRINT_COLOURS; i++)
        lum[i + 1] = (uint8_t) ((p.pal[i][0] * 77 + p.pal[i][1] * 150 + p.pal[i][2] * 29) >> 8);
    for (int y = 0; y < IMG_H; y++)
    {
        int sy = (2 * y + 1) * FOS_PRINT_H / (2 * IMG_H);
        for (int x = 0; x < IMG_W; x++)
        {
            int sx = (2 * x + 1) * FOS_PRINT_W / (2 * IMG_W);
            /* the borders of a portrait frame stay white paper */
            if (sx < p.x0 || sx >= p.x0 + p.w) continue;
            const uint8_t *s = p.px + sy * FOS_PRINT_W + sx;
            int k0 = s[0], k1 = s[1], k2 = s[FOS_PRINT_W], k3 = s[FOS_PRINT_W + 1];
            img[y * IMG_W + x] = (uint8_t) ((lum[k0 > 240 ? 0 : k0] + lum[k1 > 240 ? 0 : k1] + lum[k2 > 240 ? 0 : k2]
                                             + lum[k3 > 240 ? 0 : k3]) >> 2);
        }
    }
    fos_print_free(&p);
}

/* a finished frame's thumbnail onto the line; a portrait frame's sides
 * are white paper */
static void hang(const uint8_t *thumb, int x0, int w)
{
    if (!dry || !thumb) return;
    memmove(dry + DRY_W * DRY_H, dry, (DRY_N - 1) * DRY_W * DRY_H);
    for (int y = 0; y < DRY_H; y++)
        for (int x = 0; x < DRY_W; x++)
        {
            int px = (2 * x + 1) * FOS_PRINT_W / (2 * DRY_W);
            const uint8_t *s = thumb + 3 * ((y * FOS_THUMB_H / DRY_H) * FOS_THUMB_W + x * FOS_THUMB_W / DRY_W);
            dry[y * DRY_W + x] = px < x0 || px >= x0 + w ? 255 : (uint8_t) ((s[0] * 77 + s[1] * 150 + s[2] * 29) >> 8);
        }
    if (ndry < DRY_N) ndry++;
    dry_t = 0;
    line_dirty = 1;
}

/* the next frame to develop, or the next roll's; the contact sheet when a
 * roll is through */
static void next_job()
{
    char path[64];
    while (qi < nqueue)
    {
        if (!roll_ready)
        {
            cur_roll = queue[qi];
            cur_new = newonly[qi];
            int ri = roll_index(cur_roll);
            cur_film = ri >= 0 ? rolls[ri].film : 0;
            nframes = fos_lab_frames(cur_roll, dm->frames, FOS_MAX_FRAMES);
            {
                char msg[96];
                int raws = 0;
                for (int i = 0; i < nframes; i++) raws += !dm->frames[i].missing;
                snprintf(msg, sizeof(msg), "darkroom: roll %03d (%s): %d frames, %d RAW files", cur_roll,
                         fos_films[cur_film].key, nframes, raws);
                fos_port_log(msg);
            }
            fos_lab_prepare(cur_roll);
            /* the roll's JPEGs go in DCIM, in a folder named for the day
             * its first frame was shot and the film */
            const char *first = 0;
            for (int i = 0; i < nframes; i++)
                if (dm->frames[i].when[0] && (!first || strcmp(dm->frames[i].when, first) < 0)) first = dm->frames[i].when;
            if (fos_lab_dcim_dir(dcim_dir, sizeof(dcim_dir), cur_roll, cur_film, first) < 0)
            {
                dcim_dir[0] = 0;
                fos_port_log("darkroom: no folder for the JPEGs in DCIM");
            }
            else
            {
                char msg[160];
                snprintf(msg, sizeof(msg), "darkroom: JPEGs in %s", dcim_dir);
                fos_port_log(msg);
            }
            fi = 0;
            roll_ready = 1;
            panel_dirty = 1;
        }
        while (fi < nframes)
        {
            if (cur_new && fi == 0) { new_from = new_start(cur_roll, nframes); }
            const int idx = cur_new ? nframes - 1 - fi : fi;     /* "develop new": the newest first */
            fi++;
            if (cur_new && idx < new_from) { fi = nframes; break; }
            const struct fos_lab_frame *f = &dm->frames[idx];
            fos_lab_print_path(path, sizeof(path), cur_roll, f->no);
            /* its JPEG in DCIM: IMG_0102.CR2 -> IMG_0102.JPG */
            char jpeg[160] = "";
            if (dcim_dir[0])
            {
                const char *base = strrchr(f->cr2, '/');
                char name[9];
                snprintf(jpeg, sizeof(jpeg), "%s/%s.JPG", dcim_dir, fos_copy(name, sizeof(name), base ? base + 1 : f->cr2, 8));
            }
            char twin[160];
            twin_path(twin, sizeof(twin), f->cr2);
            /* developed before (and, by this version, written to DCIM too) */
            if (file_exists(path) && (!jpeg[0] || f->missing || file_exists(jpeg)))
            {
                if (jpeg[0] && twin[0] && !f->missing && !file_exists(twin) && file_exists(jpeg)) twin_copy(jpeg, twin);
                continue;
            }
            snprintf(cur_jpeg, sizeof(cur_jpeg), "%s", jpeg);
            snprintf(cur_copy, sizeof(cur_copy), "%s", twin);
            if (f->missing && cur_new) continue;       /* "develop new": a frame with no RAW (shot as JPEG) is not an error */
            if (f->missing)
            {
                char msg[120];
                snprintf(msg, sizeof(msg), "darkroom: roll %03d frame %d: no RAW file %s", cur_roll, f->no, f->cr2);
                fos_port_log(msg);
                {
                    const char *base = strrchr(f->cr2, '/');
                    if (f->cr2[0])
                        snprintf(last_error, sizeof(last_error), "%s isn't on the card " T_MDASH " was it shot in RAW?",
                                 base ? base + 1 : f->cr2);
                    else
                        snprintf(last_error, sizeof(last_error), "No RAW file " T_MDASH " shoot with Image quality RAW");
                }
                failed_frames++;
                done_frames++;
                continue;
            }
            cur_frame = f->no;
            dev = fos_dev_start(f->cr2, path, cur_film, f->no, f->when, f->fstop);
            fos_dev_stamp(dev, fos_port_get(FS_STAMP));
            if (dev && dv_size > FOS_SIZE_S)
            {
                /* the bigger print (M-Film 0.2): the JPEG at that size, and a file to zoom into beside the print */
                char zoom[64];
                fos_lab_zoom_path(zoom, sizeof(zoom), cur_roll, f->no);
                fos_dev_big(dev, dv_size, zoom);
            }
            dv_made = -1;
            if (dev && jpeg[0]) fos_dev_jpeg(dev, jpeg);
            frame_t0 = ui_now;
            ghost_t = 100000;
            ghost_final = 0;
            memset(ghost, 255, GHOST_W * GHOST_H);      /* fresh paper */
            panel_dirty = 1;
            if (dev) return;
            snprintf(last_error, sizeof(last_error), "Not enough memory");
            failed_frames++;
            done_frames++;
        }
        /* the roll is through: its contact sheet (not for a roll still in the camera: it is not through) */
        if (cur_new) { rolls_done++; qi++; roll_ready = 0; continue; }
        const char *pp[FOS_MAX_FRAMES];
        for (int i = 0; i < nframes; i++)
        {
            fos_lab_print_path(dm->paths[i], sizeof(dm->paths[i]), cur_roll, i + 1);
            pp[i] = dm->paths[i];
        }
        fos_lab_print_path(path, sizeof(path), cur_roll, 0);
        if (!fos_dev_sheet(path, cur_film, pp, nframes)) fos_port_log("darkroom: can't write the contact sheet");
        rolls_done++;
        qi++;
        roll_ready = 0;
    }
    finished = 1;
    cur_frame = 0;
    panel_dirty = 1;
    fos_dev_release();
    fos_port_action(FA_BUSY, 0, 0);
}

/* IMG_0102.JPG beside IMG_0102.CR2, in Canon's folder: the camera and its phone app list it */
static void twin_path(char *out, int size, const char *cr2)
{
    int n = (int) strlen(cr2);
    out[0] = 0;
    if (n < 5 || n >= size || strcmp(cr2 + n - 4, ".CR2")) return;
    memcpy(out, cr2, n - 3);
    strcpy(out + n - 3, "JPG");
}

static void twin_copy(const char *from, const char *to)
{
    /* a big buffer when there is one (fewer trips to the card), else the small one that is always there */
    static uint8_t small[4096];
    int size = 64 * 1024;
    uint8_t *buf = fos_port_alloc(size);
    if (!buf) { buf = small; size = sizeof(small); }
    void *in = fos_port_open(from, 0), *out = in ? fos_port_open(to, 1) : 0;
    int n, ok = out != 0;
    while (ok && (n = fos_port_read(in, buf, size)) > 0) ok = fos_port_write(out, buf, n) == n;
    if (in) fos_port_close(in);
    if (out) fos_port_close(out);
    if (buf != small) fos_port_free(buf);
    if (!ok) fos_port_log("darkroom: can't copy the JPEG beside the RAW");
}

static void frame_finished(int ok)
{
    char msg[160];
    int took = ui_now - frame_t0;
    if (ok)
    {
        /* a running mean for the size it was made at, the first frames counting fully */
        int size = fos_clamp(dv_made, 0, FOS_SIZES - 1);
        ms_per_frame[size] = done_frames - failed_frames < 3 ? took : (ms_per_frame[size] * 3 + took) / 4;
        snprintf(msg, sizeof(msg), "darkroom: roll %03d frame %d developed in %d ms, %d x %d", cur_roll, cur_frame, took, dv_w, dv_h);
        fos_port_log(msg);
        if (cur_jpeg[0] && cur_copy[0]) twin_copy(cur_jpeg, cur_copy);
        if (!pending_frame)
        {
            pending_frame = cur_frame;
            fos_lab_print_path(pending_path, sizeof(pending_path), cur_roll, cur_frame);
        }
        int x0, w;
        fos_dev_picture(dev, &x0, &w);
        hang(fos_dev_thumb(dev), x0, w);
    }
    else
    {
        const char *why = fos_dev_error(dev) && fos_dev_error(dev)[0] ? fos_dev_error(dev) : "stopped without a reason";
        snprintf(msg, sizeof(msg), "darkroom: roll %03d frame %d: %s", cur_roll, cur_frame, why);
        fos_port_log(msg);
        snprintf(last_error, sizeof(last_error), "Frame %d: %s", cur_frame, why);
        failed_frames++;
    }
    done_frames++;
    fos_dev_free(dev);
    dev = 0;
    panel_dirty = 1;
}

static void develop_bake()
{
    ui_to_bg(1);
    /* the room: dark, the safelight from above */
    fos_vgrad(cv, 0, 0, FOS_W, FOS_H, RAMP_RED, LV12(30), LV12(7));
    fos_text(cv, &fos_font_xs, 24, 26, "DARKROOM", SL_TEXT2, OPAQUE, 64);
    /* the line */
    fos_fill(cv, 12, LINE_Y, FOS_W - 24, 2, SL(70));
    /* the tray and the developer in it */
    fos_rrect(cv, TRAY_X, TRAY_Y, TRAY_W, TRAY_H, 16, SL(26), OPAQUE);
    fos_rrect_line(cv, TRAY_X, TRAY_Y, TRAY_W, TRAY_H, 16, 2, SL(48), OPAQUE);
    fos_rrect(cv, BATH_X, BATH_Y, BATH_W, BATH_H, 10, SL(15), OPAQUE);
    ui_to_bg(0);
}

void develop_enter(int arg)
{
    if (!dark_mem()) nqueue = 0;
    rescan();
    total_frames = done_frames = failed_frames = rolls_done = 0;
    for (int i = 0; i < nqueue; i++)
    {
        int ri = roll_index(queue[i]);
        if (ri >= 0) total_frames += newonly[i] ? dark_pending(queue[i]) : rolls[ri].shot - rolls[ri].developed;
    }
    qi = 0; roll_ready = 0; dev = 0;
    dial_t0 = ui_now; dial_sec = -1;
    dv_size = cur_size();
    dv_made = -1;
    last_error[0] = 0;
    finished = nqueue == 0;
    asking_stop = 0;
    stop_sheet.x = stop_sheet.v = 0;
    cur_frame = 0;
    dv_t = 0;
    tray = TRAY_LATENT; tray_t = 0;
    pending_frame = 0;
    ndry = 0; dry_t = 100000;
    ghost = fos_port_alloc(GHOST_W * GHOST_H);
    img = fos_port_alloc(IMG_W * IMG_H);
    dry = fos_port_alloc(DRY_N * DRY_W * DRY_H);
    if (ghost) memset(ghost, 255, GHOST_W * GHOST_H);
    tray_dirty = line_dirty = panel_dirty = 1;
    last_doing = -1;
    char dir[64];
    fos_lab_cache_dir(dir, sizeof(dir));
    fos_dev_cache(dir);
    if (!finished) fos_port_action(FA_BUSY, 1, 0);
    develop_bake();
}

void develop_leave()
{
    if (dev) { fos_dev_free(dev); dev = 0; }
    if (!finished) fos_port_action(FA_BUSY, 0, 0);
    finished = 1;
    fos_dev_release();
    fos_port_free(ghost); ghost = 0;
    fos_port_free(img); img = 0;
    fos_port_free(dry); dry = 0;
}

void develop_key(int k)
{
    if (asking_stop)
    {
        if (k == FK_SET) { asking_stop = 0; ui_go(from_roll_screen ? SCR_ROLL : SCR_DARK, from_roll_screen ? 0 : cur_roll, GO_FADE); }
        else if (k == FK_MENU) asking_stop = 0;
        ui_clobbered = 1;
        return;
    }
    switch (k)
    {
        case FK_MENU:
            if (finished) ui_go(from_roll_screen ? SCR_ROLL : SCR_DARK, from_roll_screen ? 0 : cur_roll, GO_FADE);
            else asking_stop = 1;
            ui_clobbered = 1;
            break;
        case FK_SET: case FK_PLAY:
            if (finished) ui_go(from_roll_screen ? SCR_ROLL : SCR_PRINTS, from_roll_screen ? 0 : (nqueue ? queue[0] : 0), GO_FADE);   /* a roll still in the camera has no contact sheet to open */
            break;
        case FK_HOME:
            ui_leave();
            break;
    }
}

int develop_update(int dt)
{
    /* the tray's and the line's clock is the real one: dt stops at 60 ms a frame, and a frame here takes 150 to
     * 450 ms (the developing in it), so a print took 6 s to come up and leave instead of 2.5 */
    static int real_last;
    int real = real_last ? fos_clamp(ui_now - real_last, 0, 500) : dt;
    real_last = ui_now;
    dv_t += dt;
    if (!ghost || !img || !dry || !dm)
    {
        /* no memory for the scene: say so and stop */
        if (!finished) { finished = 1; fos_port_action(FA_BUSY, 0, 0); ui_toast("Not enough memory to develop"); }
        return 0;
    }
    /* the work: only once the screen has settled, and not while asking */
    if (!finished && !asking_stop && ui_settled())
    {
        if (!dev) next_job();
        if (dev)
        {
            /* the work takes nearly all of a frame when nothing on the
             * screen moves but the dial's hand */
            int calm = (tray == TRAY_LATENT || tray == TRAY_EMPTY) && dry_t >= SLIDE_MS + 100
                       && stop_sheet.x < 0.002f;
            /* the work stops for the screen right at the next second, so the hand ticks on time */
            int budget = calm ? CALM_MS : STEP_MS, to_tick = 1000 - (ui_now - dial_t0) % 1000;
            if (to_tick < budget) budget = to_tick > 15 ? to_tick : 15;
            int p = fos_dev_step(dev, budget);
            if (dv_made < 0 && p > 0)
            {
                /* the size it comes to: the one asked for, or what the camera had the memory for */
                dv_made = fos_dev_made(dev, &dv_w, &dv_h);
                if (dv_made < dv_size)
                {
                    char msg[96];
                    snprintf(msg, sizeof(msg), "Not enough memory for %s prints: %s", dv_size == FOS_SIZE_L ? "large" : "medium",
                             dv_made == FOS_SIZE_M ? "medium" : "small");
                    ui_toast(msg);
                    dv_size = dv_made;                  /* said once: the rest of the run the same */
                }
                panel_dirty = 1;
            }
            if (p >= 0 && p < 1000 && !ghost_final && fos_dev_doing(dev) == FOS_DEV_FIXING)
            {
                /* the whole latent image, while it can still be read */
                ghost_final = 1;
                if (fos_dev_preview(dev, ghost, GHOST_W, GHOST_H) && (tray == TRAY_LATENT || tray == TRAY_IN)) tray_dirty = 1;
            }
            else if (p >= 0 && p < 1000 && ghost_t > 600 && fos_dev_preview(dev, ghost, GHOST_W, GHOST_H))
            {
                /* the latent image so far; the paper shows it when it is in the bath */
                ghost_t = 0;
                if (tray == TRAY_LATENT || tray == TRAY_IN) tray_dirty = 1;
            }
            if (p >= 1000) frame_finished(1);
            else if (p < 0) frame_finished(0);
        }
    }

    /* the tray */
    tray_t += real;
    ghost_t += real;
    switch (tray)
    {
        case TRAY_LATENT:
            if (pending_frame)
            {
                load_tray_image(pending_path);
                tray = TRAY_EMERGE; tray_t = 0;
                tray_dirty = 1;
            }
            break;
        case TRAY_EMERGE:
            tray_dirty = 1;
            if (tray_t > EMERGE_MS) { tray = TRAY_HOLD; tray_t = 0; }
            break;
        case TRAY_HOLD:
            if (tray_t > HOLD_MS) { tray = TRAY_LIFT; tray_t = 0; }
            break;
        case TRAY_LIFT:
            tray_dirty = 1;
            if (tray_t > LIFT_MS)
            {
                pending_frame = 0;
                tray = finished ? TRAY_EMPTY : TRAY_IN;
                tray_t = 0;
                ghost_t = 100000;
            }
            break;
        case TRAY_IN:
            tray_dirty = 1;
            if (tray_t > SLIDE_MS) { tray = TRAY_LATENT; tray_t = 0; }
            break;
        case TRAY_EMPTY:
            break;
    }
    if (finished && tray == TRAY_LATENT && !pending_frame) { tray = TRAY_EMPTY; tray_dirty = 1; }
    if (dry_t < SLIDE_MS + 100) { dry_t += real; line_dirty = 1; }
    else dry_t += real;
    int moving = spring_step(&stop_sheet, asking_stop ? 1 : 0, dt, 3.0f, 0.95f);
    if (moving) ui_clobbered = 1;
    return 1;           /* the timer's hand keeps going */
}

/* grey levels (paper 255 .. black 0) at density k/256 of their own, as
 * dithered positions on the red ramp */
static void density_table(int16_t *pos, int k)
{
    for (int l = 0; l < 256; l++)
    {
        int d = (255 - l) * k >> 8;                     /* density 0..255 */
        int lv = LV12(PAPER_LV) - (LV12(PAPER_LV) - LV12(BLACK_LV)) * d / 255;
        pos[l] = (int16_t) fos_ramp_pos(RAMP_RED, lv);
    }
}

/* the paper and what is on it, its top edge dy below where it rests */
static void draw_paper(int dy)
{
    struct fos_canvas save = *cv;
    fos_clip(cv, BATH_X, BATH_Y, BATH_X + BATH_W, BATH_Y + BATH_H);
    ui_bg_restore(BATH_X, BATH_Y, BATH_W, BATH_H);
    int px = PAPER_X, py = PAPER_Y + dy;
    /* a soft shadow under it in the bath */
    fos_rrect(cv, px + 3, py + 4, PAPER_W, PAPER_H, 3, SL(10), 160);
    fos_fill(cv, px, py, PAPER_W, PAPER_H, SL(PAPER_LV));
    int16_t pos[256];
    const uint8_t *src = 0;
    int half = 0;
    if (tray == TRAY_LATENT || tray == TRAY_IN)
    {
        /* the latent image, faint */
        density_table(pos, 90);
        src = ghost;
        half = 1;
    }
    else
    {
        /* the darks first, then the rest, to the print's full depth */
        float t = tray == TRAY_EMERGE ? clampf((float) tray_t / EMERGE_MS, 0, 1) : 1;
        for (int l = 0; l < 256; l++)
        {
            float dfull = (255 - l) / 255.0f;
            float k = t >= 1 ? 1 : ease_out(clampf(t * (0.7f + dfull) * 1.3f, 0, 1));
            int d = (int) ((255 - l) * (0.35f + 0.65f * k));
            int lv = LV12(PAPER_LV) - (LV12(PAPER_LV) - LV12(BLACK_LV)) * d / 255;
            pos[l] = (int16_t) fos_ramp_pos(RAMP_RED, lv);
        }
        src = img;
    }
    int y0 = IMG_Y + dy;
    int ya = fos_clamp(y0, cv->cy0, cv->cy1), yb = fos_clamp(y0 + IMG_H, cv->cy0, cv->cy1);
    const int base = fos_ramp_base[RAMP_RED], last = fos_ramp_last[RAMP_RED];
    for (int y = ya; y < yb; y++)
    {
        int sy = y - y0;
        const uint8_t *s = half ? src + (sy >> 1) * GHOST_W : src + sy * IMG_W;
        uint8_t *o = &cv->px[y * cv->pitch + IMG_X];
        const uint8_t *bay = fos_bayer[y & 3];
        for (int x = 0; x < IMG_W; x++)
        {
            int p = (pos[s[half ? x >> 1 : x]] + bay[x & 3]) >> 4;
            o[x] = (uint8_t) (base + (p > last ? last : p));
        }
    }
    *cv = save;
    ui_damage(BATH_Y, BATH_Y + BATH_H);
}

static void draw_tray()
{
    switch (tray)
    {
        case TRAY_LIFT:
        {
            /* lifted out of the bath: up and away */
            float t = clampf((float) tray_t / LIFT_MS, 0, 1);
            draw_paper(-(int) (t * t * (PAPER_H + 40)));
            break;
        }
        case TRAY_IN:
        {
            /* fresh paper slides in */
            float t = ease_out(clampf((float) tray_t / SLIDE_MS, 0, 1));
            draw_paper((int) ((1 - t) * (BATH_H + 10)));
            break;
        }
        case TRAY_EMPTY:
            ui_bg_restore(BATH_X, BATH_Y, BATH_W, BATH_H);
            ui_damage(BATH_Y, BATH_Y + BATH_H);
            break;
        default:
            draw_paper(0);
    }
}

/* the prints drying, newest on the left; a new one drops in */
static void draw_line()
{
    int y0 = LINE_Y - 8, y1 = LINE_Y + DRY_H + 22;
    ui_bg_restore(0, y0, FOS_W, y1 - y0);
    int16_t pos[256];
    density_table(pos, 256);
    const int base = fos_ramp_base[RAMP_RED], last = fos_ramp_last[RAMP_RED];
    struct fos_canvas save = *cv;
    fos_clip(cv, 0, y0, FOS_W, y1);
    for (int i = 0; i < ndry; i++)
    {
        /* the older ones move along a place, then the new one comes */
        float shift = ease_out(clampf((float) dry_t / (SLIDE_MS / 2), 0, 1));
        float drop = ease_out(clampf((float) (dry_t - SLIDE_MS / 2) / (SLIDE_MS / 2), 0, 1));
        if (i == 0 && drop <= 0) continue;
        float slot = i == 0 ? 0 : i - (1 - shift);
        int x = DRY_X0 + (int) (slot * DRY_STEP);
        int y = LINE_Y + 8 - (i == 0 ? (int) ((1 - drop) * 70) : 0);
        if (x > FOS_W) continue;
        fos_fill(cv, x - 3, y - 3, DRY_W + 6, DRY_H + 6, SL(PAPER_LV));
        int ya = fos_clamp(y, cv->cy0, cv->cy1), yb = fos_clamp(y + DRY_H, cv->cy0, cv->cy1);
        const uint8_t *src = dry + i * DRY_W * DRY_H;
        for (int yy = ya; yy < yb; yy++)
        {
            const uint8_t *s = src + (yy - y) * DRY_W;
            uint8_t *o = &cv->px[yy * cv->pitch + x];
            const uint8_t *bay = fos_bayer[yy & 3];
            for (int k = 0; k < DRY_W && x + k < FOS_W; k++)
            {
                int p = (pos[s[k]] + bay[k & 3]) >> 4;
                o[k] = (uint8_t) (base + (p > last ? last : p));
            }
        }
        /* the peg */
        fos_rrect(cv, x + DRY_W / 2 - 4, y - 12, 8, 14, 2, FC(RAMP_METAL, 60), OPAQUE);
    }
    *cv = save;
    ui_damage(y0, y1);
}

/* the darkroom timer: a frame a turn, the roll's frames round its face */
static void draw_dial(float frac)
{
    int cx = PANEL_CX, cy = DIAL_CY2, R = DIAL_R2;
    ui_bg_restore(cx - R - 6, cy - R - 6, 2 * R + 12, 2 * R + 12);
    fos_disc(cv, cx * 256, cy * 256, (R + 4) * 256, SL(20), OPAQUE);
    fos_ring(cv, cx * 256, cy * 256, (R + 4) * 256, 2 * 256, SL(56), OPAQUE);
    for (int i = 0; i < 60; i++)
    {
        int a = i * 65536 / 60 - 16384, r0 = i % 5 ? R - 6 : R - 11;
        fos_line(cv, cx * 256 + fos_cos(a) * r0 / 16, cy * 256 + fos_sin(a) * r0 / 16,
                 cx * 256 + fos_cos(a) * (R - 1) / 16, cy * 256 + fos_sin(a) * (R - 1) / 16, i % 5 ? 200 : 360, SL(i % 5 ? 70 : 120), OPAQUE);
    }
    /* the roll's frames as dots: developed, this one, to come */
    int n = nframes > 0 ? nframes : 1;
    for (int i = 0; i < n && nframes; i++)
    {
        int a = i * 65536 / n - 16384, r = R - 22;
        int no = i + 1;
        fcol c = finished || no < cur_frame ? SL(190) : no == cur_frame ? SL_ACCENT : SL(64);
        fos_disc(cv, cx * 256 + fos_cos(a) * r / 16, cy * 256 + fos_sin(a) * r / 16, n > 24 ? 480 : 640, c, OPAQUE);
    }
    /* the hand */
    int a = (int) (frac * 65536) - 16384;
    fos_line(cv, cx * 256 - fos_cos(a) * 12 / 16, cy * 256 - fos_sin(a) * 12 / 16,
             cx * 256 + fos_cos(a) * (R - 8) / 16, cy * 256 + fos_sin(a) * (R - 8) / 16, 2 * 256, SL_ACCENT, OPAQUE);
    fos_disc(cv, cx * 256, cy * 256, 4 * 256, SL_ACCENT, OPAQUE);
    ui_damage(cy - R - 6, cy + R + 6);
}

static const char *stage_name()
{
    switch (fos_dev_doing(dev))
    {
        case FOS_DEV_MIXING:     return "Mixing the chemistry";
        case FOS_DEV_DEVELOPING: return "Exposing the paper";
        case FOS_DEV_FIXING:     return "Developer";
        case FOS_DEV_DRYING:     return "Fixer " T_DOT " wash";
        case FOS_DEV_ENLARGING:  return "Enlarging";
    }
    return "";
}

static void draw_panel()
{
    int x0 = 440, x1 = FOS_W - 12;
    char b[96], eta[32];
    ui_bg_restore(x0, 120, x1 - x0, 112);
    ui_bg_restore(x0, 346, x1 - x0, 72);
    if (finished)
    {
        fos_text_c(cv, &fos_font_xs, PANEL_CX, 150, "DONE", SL_ACCENT, OPAQUE, 64);
        snprintf(b, sizeof(b), "%d roll%s", rolls_done, rolls_done == 1 ? "" : "s");
        fos_text_c(cv, &fos_font_lg, PANEL_CX, 182, b, SL_TEXT, OPAQUE, 0);
        snprintf(b, sizeof(b), "%d print%s", done_frames - failed_frames, done_frames - failed_frames == 1 ? "" : "s");
        fos_text_c(cv, &fos_font_sm, PANEL_CX, 206, b, SL_TEXT2, OPAQUE, 0);
        if (failed_frames)
        {
            snprintf(b, sizeof(b), "%d frame%s could not be developed", failed_frames, failed_frames == 1 ? "" : "s");
            fos_text_c(cv, &fos_font_sm, PANEL_CX, 372, b, SL_TEXT2, OPAQUE, 0);
            fos_text_c(cv, &fos_font_xs, PANEL_CX, 394, last_error, SL_TEXT3, OPAQUE, 0);
        }
    }
    else
    {
        snprintf(b, sizeof(b), "ROLL %03d", cur_roll);
        fos_text_c(cv, &fos_font_xs, PANEL_CX, 150, b, SL_ACCENT, OPAQUE, 64);
        fos_text_c(cv, &fos_font_lg, PANEL_CX, 182, fos_films[cur_film].name, SL_TEXT, OPAQUE, 0);
        static const char *const names[FOS_SIZES] = { "small", "medium", "large" };
        if (cur_frame) snprintf(b, sizeof(b), "Frame %d of %d " T_DOT " %s", cur_frame, nframes, names[fos_clamp(dv_made >= 0 ? dv_made : dv_size, 0, FOS_SIZES - 1)]);
        else b[0] = 0;
        fos_text_c(cv, &fos_font_sm, PANEL_CX, 206, b, SL_TEXT2, OPAQUE, 0);
        fos_text_c(cv, &fos_font_md, PANEL_CX, 376, dev ? stage_name() : "", SL_TEXT, OPAQUE, 0);
        int left = total_frames - done_frames;
        int spent = dev ? ui_now - frame_t0 : 0;
        format_eta(eta, sizeof(eta), left, -(spent < eta_ms() ? spent : eta_ms()));
        snprintf(b, sizeof(b), "%d to go " T_DOT " %s", left, eta);
        fos_text_c(cv, &fos_font_sm, PANEL_CX, 400, b, SL_TEXT3, OPAQUE, 0);
    }
    ui_damage(120, 232);
    ui_damage(346, 418);
}

static void develop_hints()
{
    ui_bg_restore(0, HINTS_Y, FOS_W, FOS_H - HINTS_Y);
    int base = HINTS_Y + 23, x = 24;
    if (finished)
    {
        x += ui_keycap(x, base, "SET") + 8;
        x += fos_text(cv, &fos_font_sm, x, base - 1, "See the prints", SL_TEXT2, OPAQUE, 0) + 22;
        x += ui_keycap(x, base, "MENU") + 8;
        fos_text(cv, &fos_font_sm, x, base - 1, "Darkroom", SL_TEXT2, OPAQUE, 0);
    }
    else
    {
        x += ui_keycap(x, base, "MENU") + 8;
        x += fos_text(cv, &fos_font_sm, x, base - 1, "Stop", SL_TEXT2, OPAQUE, 0) + 22;
        fos_text_r(cv, &fos_font_sm, FOS_W - 24, base - 1, "Keep the camera on " T_DOT " faster with Darkrooms on a computer", SL_TEXT3, OPAQUE, 0);
    }
    ui_damage(HINTS_Y, FOS_H);
}

void develop_draw()
{
    static int was_finished = -1;
    if (ui_clobbered)
    {
        ui_clobbered = 0;
        ui_bg_restore(0, 0, FOS_W, FOS_H);
        ui_damage_all();
        tray_dirty = line_dirty = panel_dirty = 1;
        ui_damaged_dial = 1;
        was_finished = -1;
    }
    if (tray_dirty) { draw_tray(); tray_dirty = 0; }
    if (line_dirty) { draw_line(); line_dirty = 0; }
    if (panel_dirty) { draw_panel(); panel_dirty = 0; ui_damaged_dial = 1; }      /* the panel's top strip is the dial's, and its dots follow the frame */
    if (was_finished != finished) { develop_hints(); was_finished = finished; }
    /* the panel's stage name follows the work */
    int doing = fos_dev_doing(dev);
    if (dev && doing != last_doing) { last_doing = doing; draw_panel(); ui_damaged_dial = 1; }
    /* the second hand: drawn when its second changes (or the dial was drawn over) */
    int sec = finished ? 0 : (ui_now - dial_t0) / 1000;
    if (sec != dial_sec || ui_damaged_dial)
    {
        dial_sec = sec;
        ui_damaged_dial = 0;
        draw_dial((sec % 60) / 60.0f);
    }
    if (stop_sheet.x > 0.002f)
        ui_sheet(stop_sheet.x, "Stop developing?", "The prints made so far are kept; the rest wait in the darkroom.", "Stop", "Keep going");
}

/* ================================================================ the prints */

/* While a print is up the palette is the print's own: entries 1..240 its
 * colours, the overlay drawn with the fixed ones (PX_*). Changing what is
 * shown goes through black, so the screen never shows one picture in
 * another's colours. */

enum { PV_WAIT, PV_IN, PV_SHOW, PV_OUT, PV_NONE };
enum { NEXT_LOAD, NEXT_BACK };

static int pv_roll;                     /* index into rolls */
static int pv_alone;
static int pv_mode;                     /* 0 the contact sheet, 1 a frame */
static int pv_cur;                      /* the frame (1..), also the sheet's cursor */
static int pv_state, pv_t, pv_next;
static int pv_info, pv_info_t;
static int pv_frames;                   /* frames on the roll */
static int pv_have;                     /* pv_print holds a picture */
static int pv_dirty;                    /* the picture or the overlay changed */

#define FADE_MS 180
#define INFO_H  44

static int prints_roll_at(int from, int dir)
{
    for (int i = from; i >= 0 && i < nrolls; i += dir)
        if (has_prints(&rolls[i])) return i;
    return -1;
}

/* Looking into a print (M-Film 0.2): a frame developed medium or large has a zoom file, the picture at
 * that size in the print's own palette. The wheel goes in and out, the arrows move about. The part on
 * the screen is read from the card each time, so a print of any size takes no more memory than its rows
 * on show. */
static struct fos_zoom pv_zf;           /* the frame's zoom file, open while its print is up */
static int pv_zsteps[4], pv_znsteps;    /* the file's pixels to a screen pixel at each level in, widest first */
static int pv_zlevel;                   /* 0: the whole print; 1 .. pv_znsteps: into it */
static int pv_zx, pv_zy;                /* the middle of the view, in the file's pixels */
static uint8_t *pv_zrows;               /* the file's rows on show, as read */
static char pv_note[40];                /* said in the details bar, in place of the hints */

static void pv_zoom_close()
{
    fos_zoom_close(&pv_zf);
    fos_port_free(pv_zrows);
    pv_zrows = 0;
    pv_znsteps = pv_zlevel = 0;
}

/* the frame's zoom file, if it has one, and the levels it gives */
static void pv_zoom_open()
{
    char path[64];
    pv_zoom_close();
    if (!pv_mode || pv_roll < 0) return;
    fos_lab_zoom_path(path, sizeof(path), rolls[pv_roll].number, pv_cur);
    if (!fos_zoom_open(path, &pv_zf)) return;
    /* a level where the picture is at least half as big again as it was on the level before, down to
     * the file's own pixels */
    int most = 0;
    for (int step = 4; step >= 1; step--)
    {
        if (step > 1 && (pv_zf.h / step) * 2 < FOS_PRINT_H * 3) continue;
        if (pv_zf.h / step <= FOS_PRINT_H) continue;
        pv_zsteps[pv_znsteps++] = step;
        int vh = pv_zf.h / step < FOS_PRINT_H ? pv_zf.h / step : FOS_PRINT_H;
        if (vh * step * pv_zf.w > most) most = vh * step * pv_zf.w;
    }
    if (pv_znsteps) pv_zrows = fos_port_alloc(most);
    if (!pv_zrows)
    {
        if (pv_znsteps) fos_port_log("prints: no memory to zoom in");
        pv_zoom_close();
    }
}

/* the view at the level it is on: the part of the file (fx, fy, every step-th pixel) and where it goes
 * on the screen (vw x vh, centred) */
static void pv_zoom_view(int *fx, int *fy, int *step, int *vw, int *vh)
{
    int s = pv_zsteps[pv_zlevel - 1];
    int w = pv_zf.w / s < FOS_PRINT_W ? pv_zf.w / s : FOS_PRINT_W;
    int h = pv_zf.h / s < FOS_PRINT_H ? pv_zf.h / s : FOS_PRINT_H;
    *fx = fos_clamp(pv_zx - w * s / 2, 0, pv_zf.w - w * s);
    *fy = fos_clamp(pv_zy - h * s / 2, 0, pv_zf.h - h * s);
    pv_zx = *fx + w * s / 2;
    pv_zy = *fy + h * s / 2;
    *step = s; *vw = w; *vh = h;
}

static void pv_load()
{
    char path[64];
    if (pv_have) fos_print_free(&pv_print);
    pv_have = 0;
    if (pv_roll < 0) return;
    const struct fos_lab_roll *r = &rolls[pv_roll];
    fos_lab_print_path(path, sizeof(path), r->number, pv_mode ? pv_cur : 0);
    pv_have = fos_print_load(path, &pv_print);
    if (!pv_have)
    {
        char msg[96];
        snprintf(msg, sizeof(msg), "prints: can't load %s", path);
        fos_port_log(msg);
    }
    pv_note[0] = 0;
    if (pv_have) pv_zoom_open();
    else pv_zoom_close();
    pv_dirty = 1;
}

static void pv_palette(int k)
{
    static const uint8_t black[1][3] = { { 0, 0, 0 } };
    if (pv_have) fos_port_photo((const uint8_t (*)[3]) pv_print.pal, FOS_PRINT_COLOURS, k);
    else fos_port_photo(black, 1, k);
}

void prints_enter(int arg)
{
    pv_alone = (arg & PRINTS_ALONE) != 0;
    arg &= ~PRINTS_ALONE;
    pv_have = 0;
    dark_mem();
    rescan();
    int i = arg ? roll_index(arg) : -1;
    if (i < 0 || !has_prints(&rolls[i])) i = prints_roll_at(0, 1);
    pv_roll = i;
    pv_mode = 0;
    pv_cur = 1;
    pv_frames = i >= 0 ? rolls[i].shot : 0;
    if (i >= 0) fos_lab_frames(rolls[i].number, pv_meta, FOS_MAX_FRAMES);
    pv_have = 0;
    pv_state = i >= 0 ? PV_WAIT : PV_NONE;
    pv_t = 0;
    pv_info = 1;
    pv_info_t = 0;
    pv_dirty = 1;
    if (pv_state == PV_NONE)
    {
        ui_bg_plain();
        ui_to_bg(1);
        ui_status_bar("PRINTS");
        fos_text_c(cv, &fos_font_lg, 360, 220, "No prints yet", C_TEXT, OPAQUE, 0);
        fos_text_c(cv, &fos_font_sm, 360, 248, "Develop a roll in the darkroom and its prints show here.", C_TEXT3, OPAQUE, 0);
        struct hint h[] = { { "SET", "Darkroom" }, { "MENU", "Back" } };
        ui_hints(h, 2);
        ui_to_bg(0);
    }
    else memset(ui_bg, fos_ix(FC(RAMP_GRAY, 0)), FOS_W * FOS_H);
}

void prints_leave()
{
    pv_zoom_close();
    if (pv_have) fos_print_free(&pv_print);
    pv_have = 0;
    /* not when the shutter is taking us out: the print is still on the screen, and the menus' palette under it
     * turned it into coloured noise that stayed in the middle of Canon's screen (closing sets Canon's palette) */
    if (pv_state != PV_NONE && !fos_port_abort()) fos_port_palette();
}

static void pv_go(int next)
{
    if (pv_state == PV_OUT) return;
    pv_next = next;
    pv_state = PV_OUT;
    pv_t = 0;
}

static void pv_back()
{
    if (pv_alone) ui_leave();
    else if (ui_from() == SCR_DARK || ui_from() == SCR_DEVELOP)
        ui_go(SCR_DARK, pv_roll >= 0 ? rolls[pv_roll].number : 0, GO_FADE);
    else if (ui_from() == SCR_ROLL) ui_go(SCR_ROLL, 0, GO_FADE);       /* PLAY on the roll screen: back to it */
    else ui_go(SCR_HOME, 2, GO_FADE);
}

void prints_key(int k)
{
    if (pv_state == PV_NONE)
    {
        if (k == FK_SET) ui_go(SCR_DARK, 0, GO_FADE);
        else if (k == FK_MENU || k == FK_PLAY) { if (pv_alone) ui_leave(); else ui_go(SCR_HOME, 2, GO_FADE); }
        else if (k == FK_HOME) ui_leave();
        return;
    }
    if (pv_state != PV_SHOW) return;
    if (pv_mode && pv_zlevel > 0)
    {
        /* into the print: the arrows move a quarter of the screen, the wheel goes in and out, the rest come back out */
        int s = pv_zsteps[pv_zlevel - 1];
        switch (k)
        {
            case FK_LEFT:    pv_zx -= FOS_PRINT_W * s / 4; break;
            case FK_RIGHT:   pv_zx += FOS_PRINT_W * s / 4; break;
            case FK_UP:      pv_zy -= FOS_PRINT_H * s / 4; break;
            case FK_DOWN:    pv_zy += FOS_PRINT_H * s / 4; break;
            case FK_WHEEL_R: if (pv_zlevel < pv_znsteps) pv_zlevel++; break;
            case FK_WHEEL_L: pv_zlevel--; break;
            case FK_INFO:    pv_info = !pv_info; pv_info_t = 0; break;
            case FK_SET: case FK_MENU: case FK_PLAY: pv_zlevel = 0; break;
            case FK_HOME:    ui_leave(); return;
            default: return;
        }
        pv_dirty = 1;
        ui_damage_all();
        return;
    }
    int n = pv_frames;
    switch (k)
    {
        case FK_LEFT: case FK_RIGHT: case FK_UP: case FK_DOWN:
        {
            int d = k == FK_LEFT ? -1 : k == FK_RIGHT ? 1 : k == FK_UP ? -6 : 6;
            if (pv_mode && (k == FK_UP || k == FK_DOWN)) break;
            int to = pv_cur + d;
            if (to < 1 || to > n) break;
            pv_cur = to;
            if (pv_mode) pv_go(NEXT_LOAD);
            else pv_dirty = 1;
            break;
        }
        case FK_WHEEL_L: case FK_WHEEL_R:
            if (pv_mode && k == FK_WHEEL_R && pv_have && pv_znsteps)
            {
                /* a print developed medium or large: the wheel goes into it, from its middle */
                pv_zlevel = 1;
                pv_zx = pv_zf.w / 2;
                pv_zy = pv_zf.h / 2;
                pv_dirty = 1;
            }
            else if (pv_mode)
            {
                int to = pv_cur + (k == FK_WHEEL_L ? -1 : 1);
                if (to >= 1 && to <= n) { pv_cur = to; pv_go(NEXT_LOAD); }
            }
            else
            {
                /* the other rolls: newer to the left */
                int to = prints_roll_at(pv_roll + (k == FK_WHEEL_L ? -1 : 1), k == FK_WHEEL_L ? -1 : 1);
                if (to >= 0)
                {
                    pv_roll = to;
                    pv_frames = rolls[to].shot;
                    pv_cur = 1;
                    fos_lab_frames(rolls[to].number, pv_meta, FOS_MAX_FRAMES);
                    pv_go(NEXT_LOAD);
                }
            }
            break;
        case FK_SET:
            pv_mode = !pv_mode;
            pv_go(NEXT_LOAD);
            break;
        case FK_INFO:
            pv_info = !pv_info;
            pv_info_t = 0;
            pv_dirty = 1;
            break;
        case FK_MENU:
            if (pv_mode) { pv_mode = 0; pv_go(NEXT_LOAD); }
            else pv_go(NEXT_BACK);
            break;
        case FK_PLAY:
            pv_go(NEXT_BACK);
            break;
        case FK_HOME:                   /* Trash: back to the camera, as on every screen but the darkroom's list */
            ui_leave();
            break;
    }
    if (pv_state == PV_SHOW && (k == FK_SET || k == FK_LEFT || k == FK_RIGHT)) pv_info_t = 0;
    if (pv_dirty) ui_damage_all();
}

int prints_update(int dt)
{
    if (pv_state == PV_NONE) return 0;
    pv_t += dt;
    switch (pv_state)
    {
        case PV_WAIT:
            if (!ui_settled()) return 1;
            pv_load();
            pv_palette(0);
            pv_state = PV_IN;
            pv_t = 0;
            pv_info_t = 0;
            return 1;
        case PV_IN:
            pv_palette(fos_clamp(pv_t * 256 / FADE_MS, 0, 256));
            if (pv_t >= FADE_MS) pv_state = PV_SHOW;
            return 1;
        case PV_OUT:
            pv_palette(fos_clamp(256 - pv_t * 256 / FADE_MS, 0, 256));
            if (pv_t < FADE_MS) return 1;
            if (pv_next == NEXT_LOAD)
            {
                pv_load();
                pv_state = PV_IN;
                pv_t = 0;
                return 1;
            }
            /* back to the film OS's colours, on black */
            pv_state = PV_NONE;
            if (pv_have) fos_print_free(&pv_print);
            pv_have = 0;
            pv_zoom_close();
            memset(cv->px, fos_ix(FC(RAMP_GRAY, 0)), FOS_W * FOS_H);
            fos_port_present(cv->px, FOS_W, 0, FOS_H);
            fos_port_palette();
            memset(ui_bg, fos_ix(FC(RAMP_GRAY, 0)), FOS_W * FOS_H);
            pv_back();
            return 1;
        case PV_SHOW:
            /* the details go after a while on their own */
            if (pv_info && pv_info_t < 3000)
            {
                pv_info_t += dt;
                if (pv_info_t >= 3000) { pv_info = 0; pv_note[0] = 0; pv_dirty = 1; ui_damage_all(); }
            }
            return 0;
    }
    return 0;
}

static void sheet_cell(int i, int *x, int *y)
{
    int rows = (pv_frames + 5) / 6;
    int y0 = (FOS_PRINT_H - rows * FOS_THUMB_H) / 2;
    *x = (i % 6) * FOS_THUMB_W + FOS_SHEET_FX;
    *y = y0 + (i / 6) * FOS_THUMB_H + FOS_SHEET_FY;
}

static void frame_rect(int x, int y, int w, int h, uint8_t ix)
{
    for (int k = 0; k < 2; k++)
    {
        memset(&cv->px[(y + k) * cv->pitch + x], ix, w);
        memset(&cv->px[(y + h - 1 - k) * cv->pitch + x], ix, w);
    }
    for (int j = y; j < y + h; j++)
    {
        cv->px[j * cv->pitch + x] = cv->px[j * cv->pitch + x + 1] = ix;
        cv->px[j * cv->pitch + x + w - 1] = cv->px[j * cv->pitch + x + w - 2] = ix;
    }
}

void prints_draw()
{
    if (pv_state == PV_NONE)
    {
        if (ui_clobbered) { ui_clobbered = 0; ui_bg_restore(0, 0, FOS_W, FOS_H); ui_damage_all(); }
        return;
    }
    if (pv_state == PV_WAIT)
    {
        /* black, in the film OS's colours, until the screen has settled */
        memset(cv->px, fos_ix(FC(RAMP_GRAY, 0)), FOS_W * FOS_H);
        ui_clobbered = 0;
        return;
    }
    if (pv_state == PV_OUT) return;        /* what is fading out stays as it was */
    if (!pv_dirty && !ui_clobbered) return;
    pv_dirty = 0;
    ui_clobbered = 0;
    /* the picture: the part of it the zoom is on, or all of it */
    int zoomed = 0;
    if (pv_have && pv_mode && pv_zlevel > 0)
    {
        int fx, fy, step, vw, vh;
        pv_zoom_view(&fx, &fy, &step, &vw, &vh);
        memset(cv->px, PX_BLACK, FOS_W * FOS_H);
        zoomed = fos_zoom_read(&pv_zf, fx, fy, step, cv->px + ((FOS_H - vh) / 2) * cv->pitch + (FOS_W - vw) / 2, cv->pitch, vw, vh, pv_zrows);
        if (zoomed)
        {
            /* where that is in the print: a little map of it, top right, and how far in */
            int mh = 44, mw = mh * pv_zf.w / pv_zf.h, mx = FOS_W - 16 - mw, my = 16;
            for (int y = my - 4; y < my + mh + 4; y++) memset(&cv->px[y * cv->pitch + mx - 4], PX_PANEL, mw + 8);
            frame_rect(mx - 1, my - 1, mw + 2, mh + 2, PX_G2);
            int rx = mx + fx * mw / pv_zf.w, ry = my + fy * mh / pv_zf.h;
            int rw = vw * step * mw / pv_zf.w, rh = vh * step * mh / pv_zf.h;
            frame_rect(rx, ry, rw < 5 ? 5 : rw, rh < 5 ? 5 : rh, PX_AMBER);
            char z[16];
            int f10 = pv_zf.h * 10 / (FOS_PRINT_H * step);
            snprintf(z, sizeof(z), T_TIMES "%d.%d", f10 / 10, f10 % 10);
            int tw = fos_text_w(&fos_font_xs, z, 0);
            for (int y = my + mh + 8; y < my + mh + 28; y++) memset(&cv->px[y * cv->pitch + FOS_W - 26 - tw], PX_PANEL, tw + 14);
            ui_photo_text(&fos_font_xs, FOS_W - 19, my + mh + 23, z, 0, 2);
        }
        else
        {
            fos_port_log("prints: the zoom file can't be read");
            pv_zlevel = 0;
        }
    }
    if (zoomed) {}
    else if (pv_have) memcpy(cv->px, pv_print.px, FOS_W * FOS_H);
    else
    {
        memset(cv->px, PX_BLACK, FOS_W * FOS_H);
        char b[64];
        snprintf(b, sizeof(b), pv_mode ? "Frame %d was not developed" : "No contact sheet", pv_cur);
        ui_photo_text(&fos_font_md, 360, 244, b, 0, 1);
    }
    const struct fos_lab_roll *r = &rolls[pv_roll];
    if (!pv_mode)
    {
        int x, y;
        sheet_cell(pv_cur - 1, &x, &y);
        frame_rect(x - 4, y - 4, FOS_SHEET_FW + 8, FOS_SHEET_FH + 8, PX_AMBER);
    }
    if (pv_info)
    {
        char b[96], c[64];
        int y = FOS_H - INFO_H;
        memset(&cv->px[y * cv->pitch], PX_PANEL, FOS_W * INFO_H);
        memset(&cv->px[y * cv->pitch], PX_LINE, FOS_W);
        const struct fos_lab_frame *m = pv_cur >= 1 && pv_cur <= FOS_MAX_FRAMES ? &pv_meta[pv_cur - 1] : 0;
        snprintf(b, sizeof(b), "ROLL %03d", r->number);
        int x = 20 + ui_photo_text(&fos_font_xs, 20, y + 28, b, 1, 0);
        snprintf(b, sizeof(b), "%s " T_DOT " frame %d of %d", fos_films[r->film].name, pv_cur, pv_frames);
        x += 14 + ui_photo_text(&fos_font_sm, x + 14, y + 28, b, 0, 0);
        if (m && m->when[0])
        {
            snprintf(c, sizeof(c), "%s%s%s", m->when, m->fstop[0] ? "  f/" : "", m->fstop);
            ui_photo_text(&fos_font_sm, x + 14, y + 28, c, 0, 0);
        }
        ui_photo_text(&fos_font_xs, FOS_W - 20, y + 28,
                      pv_note[0] ? pv_note : zoomed ? "ARROWS MOVE   SET OUT" :
                      !pv_mode ? "SET OPEN   WHEEL ROLLS" : pv_znsteps ? "WHEEL ZOOM   SET SHEET" : "SET SHEET   INFO HIDE", pv_note[0] != 0, 2);
    }
    ui_damage_all();
}
