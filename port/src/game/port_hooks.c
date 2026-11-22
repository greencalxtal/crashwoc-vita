/* Hooks called by translated code (tools/ppc2c.py HOOKS) where the port
 * deliberately deviates from the original flow. */
int port_skip_intro(void);
int port_skip_logos(void);
void firstscreens(void);
void PlayCutMovie(s32 movie);

int CopyFilesThreadProc(int a);

void port_hook_firstscreens(void) {
    if (!port_skip_logos())
        firstscreens();
    else
        CopyFilesThreadProc(0); /* firstscreens() also sets up the sfx tables */
}

/* the intro/title cut movie (.dat + gcutscene data) is not ported yet */
void port_hook_PlayCutMovie(s32 movie) {
    if (port_skip_intro())
        return;
    PlayCutMovie(movie);
}

/* Buggy race (level 3, a\west_a): the opponents' recorded runs
 * (WEST_A.AI0..AI3) are read raw from the disc, big-endian. The translated
 * LoadBuggyData only reads them; convert each 28-byte BUGSAVE record
 * (quaternion: 4 floats, x/y/z/FrontWheelAng: 4 shorts, WheelLength: 4 bytes)
 * to native order, or the other racers get garbage positions (invisible) and
 * the race position counter goes wrong. */
void *LoadBuggyData(void *data, int indx);
void *port_hook_LoadBuggyData(void *data, int indx) {
    unsigned char *p = data, *end = LoadBuggyData(data, indx);
    for (; p + 28 <= end; p += 28) {
        int i;
        for (i = 0; i < 16; i += 4) {
            unsigned char t = p[i]; p[i] = p[i + 3]; p[i + 3] = t;
            t = p[i + 1]; p[i + 1] = p[i + 2]; p[i + 2] = t;
        }
        for (i = 16; i < 24; i += 2) {
            unsigned char t = p[i]; p[i] = p[i + 1]; p[i + 1] = t;
        }
    }
    return end;
}

/* ---- Screen mode and frame lock in the pause menu's OPTIONS ----
 * They used to be SELECT + D-pad up/down, which clash with overlay plugins
 * (PSVshell). The options menu (menu 6: rumble, sound, [language], done) gets
 * two entries before DONE: GetMenuInfo (game.c) counts them, the translated
 * DrawMenu draws them through this DrawMenuEntry hook, and the ProcMenu hook
 * changes them (cross / D-pad right = next, D-pad left = previous). Both are
 * saved in the save directory (display.txt, framelock.txt) as before. */
#define PORT_MENU_OPTIONS 6
#define PORT_SFX_YES 0x36
extern char *tDONE[6];
extern char *tON[6];
extern char *tOFF[6];
extern float dme_sx;
extern s32 stick_bits;
extern s32 fadeval;
extern s32 Paused;
int port_display_mode(void);
void port_display_step(int dir);
int port_pace_lock_on(void);
void port_pace_lock_toggle(void);
void DrawMenuEntry(struct cursor_s *cursor, char *txt, float *x, float *y, s32 *i);
void ProcMenu(struct cursor_s *cursor, struct nupad_s *pad);

/* languages of the game's text tables: EN FR DE IT ES NL (no accents: the
 * font encodes them its own way) */
static const char *const k_screen[6] = { "SCREEN", "ECRAN", "BILD", "SCHERMO", "PANTALLA", "SCHERM" };
static const char *const k_mode[3][6] = {
    { "4:3", "4:3", "4:3", "4:3", "4:3", "4:3" },
    { "STRETCHED", "ETIRE", "GESTRECKT", "ALLUNGATO", "ESTIRADA", "UITGEREKT" },
    { "WIDESCREEN", "LARGE", "BREITBILD", "PANORAMICO", "PANORAMICA", "BREEDBEELD" },
};
static const char *const k_lock[6] = { "STEADY FPS", "FPS STABLES", "STABILE FPS", "FPS STABILI", "FPS ESTABLES",
                                        "STABIELE FPS" };

static int menu_lang(void) {
    return (Game.language >= 0 && Game.language < 6) ? Game.language : 0;
}

void port_hook_DrawMenuEntry(struct cursor_s *cursor, char *txt, float *x, float *y, s32 *i) {
    if (cursor->menu == PORT_MENU_OPTIONS && txt == tDONE[menu_lang()]) {
        static char line[2][64];
        int l = menu_lang(), m = port_display_mode();
        /* the width scale the (translated) DrawMenu set for DONE, used for
         * ours too; DrawMenuEntry resets it after each entry */
        float sx = dme_sx;
        if (m < 0 || m > 2) m = 0;
        snprintf(line[0], sizeof(line[0]), "%s: %s", k_screen[l], k_mode[m][l]);
        snprintf(line[1], sizeof(line[1]), "%s: %s", k_lock[l], port_pace_lock_on() ? tON[l] : tOFF[l]);
        dme_sx = sx;
        DrawMenuEntry(cursor, line[0], x, y, i);
        dme_sx = sx;
        DrawMenuEntry(cursor, line[1], x, y, i);
        dme_sx = sx;
    }
    DrawMenuEntry(cursor, txt, x, y, i);
}

void port_hook_ProcMenu(struct cursor_s *cursor, struct nupad_s *pad) {
    int menu0 = cursor->menu, y0 = cursor->y, lock0 = cursor->button_lock, item0 = cursor->item_frame;
    int wait0 = cursor->wait, busy = fadeval > 0 || (u32)(Paused - 1) < 0x1d;
    /* own edge detection on the held buttons: ProcMenu can run more than once
     * per pad read (one call per logic tick), and oldpaddata stays set for
     * every call of that frame */
    static u32 held_prev;
    u32 held = pad ? (u32)pad->paddata : 0, db;
    if (pad && (held & 0xf000) == 0) held |= (u32)stick_bits;
    db = held & ~held_prev;
    held_prev = held;
    ProcMenu(cursor, pad);
    /* act like ProcMenu would: same menu, cursor not moved this frame, no
     * button lock / wait / fade, and not on the frame the cursor arrived */
    if (menu0 != PORT_MENU_OPTIONS || cursor->menu != PORT_MENU_OPTIONS || cursor->y != y0 || pad == NULL)
        return;
    if (lock0 != 0 || wait0 != 0 || cursor->wait != 0 || item0 == 0 || busy) return;
    if (cursor->y != cursor->y_max - 2 && cursor->y != cursor->y_max - 1) return;
    if (cursor->y == cursor->y_max - 2) {
        if (db & 0x8000) port_display_step(-1);         /* D-pad left */
        else if (db & (0x2000 | 0x840)) port_display_step(1); /* right, cross, start */
        else return;
    } else {
        if (!(db & (0x8000 | 0x2000 | 0x840))) return;
        port_pace_lock_toggle();
    }
    GameSfx(PORT_SFX_YES, NULL);
}
