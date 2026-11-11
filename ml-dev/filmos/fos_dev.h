/* fos_dev: developing film in the camera.
 *
 * A frame's CR2 RAW becomes a print for the screen: 720 x 480 in its own
 * palette of FOS_PRINT_COLOURS colours. Developing runs in steps of a few
 * tens of ms, so the film OS keeps animating on the same task while the
 * camera works through a roll.
 */

#ifndef _fos_dev_h_
#define _fos_dev_h_

#include <stdint.h>

/* ---------------------------------------------------------------- looks */

/* a film's look, from films/films.py (see fos_looks.c; Darkrooms on the desktop uses the same) */
struct fos_hue_shift { int8_t from, to; float amount, chroma; };
struct fos_look
{
    const char *key;            /* matches fos_films[].key */
    int balance;                /* K the film is balanced for */
    int slide;                  /* reversal film: no scanner exposure */
    float ev, sat;
    float contrast[3], pivot[3], black[3], white[3];
    int nhues;
    struct fos_hue_shift hues[4];
    float halation, hal_thr, hal_tint[3];
    float grain, grain_size;
    int mono;
    float mono_mix[3];
};
extern const struct fos_look fos_looks[];
extern const int fos_nlooks;
/* the looks of the films from ML/DATA/FOSFILMS.BIN (fos_films_load) */
extern const struct fos_look *fos_more_looks;
extern int fos_nmore_looks;
const struct fos_look *fos_look_of(const char *key);

/* the film's colour for one scene-linear sRGB value (0.18 = mid grey):
 * the print, display sRGB 0..1. Same model as film_color() in the lab. */
void fos_film_color(const struct fos_look *f, const float lin[3], float out[3]);

/* ---------------------------------------------------------------- prints */

/* the camera's screen; the web darkroom builds bigger prints with -D */
#ifndef FOS_PRINT_W
#define FOS_PRINT_W         720
#define FOS_PRINT_H         480
#endif
#define FOS_PRINT_COLOURS   240         /* palette indices 1..240 */
#define FOS_FIXED_BASE      241         /* 241..254: the same in every palette */
#define FOS_FIXED_N         14          /* 241..254 */
#define FOS_THUMB_W         120
#define FOS_THUMB_H         80

struct fos_print
{
    int frame;                  /* 0 = the roll's contact sheet */
    int film;                   /* index into fos_films */
    char when[20];              /* "2026-10-22 17:45" */
    char fstop[8];
    int x0, w;                  /* the picture's columns (a portrait frame is narrower) */
    uint8_t pal[FOS_PRINT_COLOURS][3];
    uint8_t *px;                /* FOS_PRINT_W x FOS_PRINT_H */
};

/* while a photograph is up, the overlay draws with these */
enum { PX_BLACK = FOS_FIXED_BASE, PX_PANEL, PX_G1, PX_G2, PX_G3, PX_G4, PX_G5, PX_WHITE,
       PX_AMBER, PX_AMBER2, PX_RED, PX_LINE, PX_DIM, PX_SPARE };
extern const uint8_t fos_fixed_rgb[FOS_FIXED_N][3];

/* A print is an 8-bit BMP, so it also opens on a PC: the file and info
 * headers, 256 palette entries (1..240 the print's), our own header with
 * the frame's details, the pixels top row first, then the thumbnail. */
#define FOS_BMP_PAL     (14 + 40)
#define FOS_BMP_META    (FOS_BMP_PAL + 256 * 4)
#define FOS_META_SIZE   64
#define FOS_BMP_PIXELS  (FOS_BMP_META + FOS_META_SIZE)
#define FOS_BMP_THUMB   (FOS_BMP_PIXELS + FOS_PRINT_W * FOS_PRINT_H)
#define FOS_THUMB_SIZE  (FOS_THUMB_W * FOS_THUMB_H * 3)
#define FOS_BMP_SIZE    (FOS_BMP_THUMB + FOS_THUMB_SIZE)

/* A zoom file: a print at the size it was developed (fos_dev_big), for the
 * screen to look into. "FOZ1", its width and height (16 bits each, low byte
 * first), 8 bytes to spare; then its pixels, top row first, as indices in
 * the palette of the print it belongs to (the BMP beside it). */
#define FOS_ZOOM_HEAD   16
struct fos_zoom { void *f; int w, h; };
int  fos_zoom_open(const char *path, struct fos_zoom *z);       /* 0: no such file, or not one */
/* ow x oh pixels into out (rows pitch apart), from (x, y) of the picture,
 * every step-th pixel each way; rows: room for oh * step rows of the file,
 * read in one go. 0 if it can't be read */
int  fos_zoom_read(struct fos_zoom *z, int x, int y, int step, uint8_t *out, int pitch, int ow, int oh, uint8_t *rows);
void fos_zoom_close(struct fos_zoom *z);

/* in fos_print.c, which every build has (the shelf's samples are prints too) */
/* a print's headers while it is read or written: one for the readers here and
 * the darkroom's writer, which never use it at the same time (all on the film
 * OS's task, each done with it before it returns), so the boot memory holds
 * it once and not three times */
extern uint8_t fos_bmp_head[FOS_BMP_PIXELS];
int  fos_print_load(const char *path, struct fos_print *p);     /* allocates p->px; 0 on failure */
void fos_print_free(struct fos_print *p);
int  fos_print_thumb(const char *path, uint8_t *thumb);         /* FOS_THUMB_W x FOS_THUMB_H RGB */

/* ---------------------------------------------------------------- developing */

struct fos_dev;

/* where the film tables are kept between sessions (NULL: not kept) */
void fos_dev_cache(const char *dir);
/* build film's table into the cache now (the build makes the card's copies) */
int fos_dev_table(int film);
/* also write the frame in full colour as a JPEG at path (before the first step) */
void fos_dev_jpeg(struct fos_dev *d, const char *path);
/* a bigger print as well (before the first step): FOS_SIZE_M, a third of the
 * sensor each way, or FOS_SIZE_L, half of it. The JPEG is then that size,
 * and zoom (NULL: none) gets the same picture in the print's own palette,
 * for the screen to zoom into (a zoom file, below). Where the camera hasn't
 * the memory, the next size down is made, to the screen's print alone:
 * fos_dev_made says which once the first step is through */
void fos_dev_big(struct fos_dev *d, int size, const char *zoom);
int  fos_dev_made(const struct fos_dev *d, int *w, int *h);     /* FOS_SIZE_*, and the JPEG's size */
/* the date in the corner of the print, from when (before the first step) */
void fos_dev_stamp(struct fos_dev *d, int on);
/* forget the film table (leaving the darkroom) */
void fos_dev_release();

/* develop one frame: cr2 in, a print file out */
struct fos_dev *fos_dev_start(const char *cr2, const char *print, int film, int frame,
                              const char *when, const char *fstop);
/* work for about budget_ms; returns progress 0..1000, 1000 when the print
 * is written, or < 0 when the frame can't be developed */
int fos_dev_step(struct fos_dev *d, int budget_ms);
/* the finished frame, small, RGB: for the contact sheet and the animation */
const uint8_t *fos_dev_thumb(const struct fos_dev *d);
const char *fos_dev_error(const struct fos_dev *d);
/* what it is doing, for the darkroom's animation */
enum { FOS_DEV_MIXING, FOS_DEV_DEVELOPING, FOS_DEV_FIXING, FOS_DEV_DRYING, FOS_DEV_DONE, FOS_DEV_FAILED,
       FOS_DEV_ENLARGING };         /* the bigger print */
int fos_dev_doing(const struct fos_dev *d);
int fos_dev_progress(const struct fos_dev *d);      /* 0..1000, as fos_dev_step returned */
void fos_dev_picture(const struct fos_dev *d, int *x0, int *w);   /* the picture's columns on the print */
void fos_dev_free(struct fos_dev *d);

/* the print in progress, w x h, as the latent image coming up: a rough
 * luminance, 255 (paper) where nothing has been read yet; 0 if there is
 * nothing to show (not reading the RAW) */
int fos_dev_preview(const struct fos_dev *d, uint8_t *lum, int w, int h);

/* the roll's contact sheet from its frames' prints (NULL: not
 * developed); written as a print with frame 0. Frame i sits in cell
 * (i % 6, i / 6) of FOS_THUMB_W x FOS_THUMB_H cells, rows centred */
#define FOS_SHEET_FX 6
#define FOS_SHEET_FY 4
#define FOS_SHEET_FW 108
#define FOS_SHEET_FH 72
int fos_dev_sheet(const char *sheet, int film, const char *const *prints, int frames);

/* ---------------------------------------------------------------- the rolls on the card (fos_lab.c) */

#define FOS_MAX_FRAMES 40

struct fos_lab_roll
{
    int number;
    int film;                   /* index into fos_films */
    int exposures;
    int shot;                   /* frames in its logs */
    int developed;              /* frames with a print */
    int sheet;                  /* its contact sheet is there */
    int jpegs;                  /* JPEGs in its folder in DCIM */
    int in_camera;              /* still loaded: rewind it before developing */
    char loaded[17];            /* "2026-10-22 17:45" */
    char last[17];              /* its last frame */
};

struct fos_lab_frame
{
    int no;
    char cr2[64];
    int missing;                /* the file isn't there (deleted, or shot as JPEG) */
    int jpeg;                   /* missing, and shot as JPEG: cr2 names its .JPG */
    char when[17];
    char fstop[6];
};

int  fos_lab_scan(struct fos_lab_roll *rolls, int max);                 /* newest first; how many */
int  fos_lab_frames(int roll, struct fos_lab_frame *frames, int max);   /* frame order; how many */
void fos_lab_print_path(char *buf, int size, int roll, int frame);      /* frame 0: the contact sheet */
void fos_lab_zoom_path(char *buf, int size, int roll, int frame);       /* the frame's zoom file, beside its print */
int  fos_lab_prepare(int roll);                                         /* its print folder; 0 = ok */
void fos_lab_cache_dir(char *buf, int size);                            /* where film tables are kept */
/* the roll's folder of JPEGs in DCIM, made if need be: "Sep0906 Portra400"
 * (month, month number, day of its first frame; when: "2026-09-06 10:15")
 * or SEP0906 where the card takes no long names. 0 = ok */
int  fos_lab_dcim_dir(char *buf, int size, int roll, int film, const char *when);
/* take a roll off the card: its logs, prints, the folder of JPEGs and the copies of them beside the RAWs.
 * Never a RAW file. buf: room for the roll's frames (it reads them to find the copies). The files removed. */
int  fos_lab_delete_roll(int roll, struct fos_lab_frame *buf, int max);

#endif
