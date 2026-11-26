/* The port's additions to the HUD, made on the game's own drawing: hooks of
 * calls from the translated DrawPanel, DrawMenu and Text3D (tools/ppc2c.py
 * HOOKS).
 *
 * - The frames-per-second counter (pause > OPTIONS > SHOW FPS) is text of the
 *   game's 3D font in the top-left corner, drawn in the HUD scene like the
 *   wumpa counter. The rate is measured on the render thread (gx_gl.c).
 *
 * - Widescreen pause screen. In widescreen the HUD keeps the original 4:3
 *   layout in the middle of the screen (the panel camera keeps its scale), so
 *   the pause screen's frame (panel objects 98 and 99, made for 4:3) left the
 *   sides of the screen uncovered. While that frame is up (DrawPanel draws it
 *   for menus 5-10), the frame is stretched to the whole width and what is
 *   placed on it (menus, stats, icons) moves out with it: the x of each thing
 *   the translated DrawPanel / DrawMenu place is multiplied by the wide factor,
 *   but text and models keep their shape. Nested calls (the glyphs of a text,
 *   what BigOutOf or DrawMenuEntry draw) are not moved again: g_hud_depth, and
 *   nurndr_forced_mtl_table, which Text3D sets while it draws its glyphs.
 *
 * - PS Vita buttons. The game's texts write the controller buttons as
 *   lowercase a, b, x, y, w and n ("APPUYER SUR a POUR CONTINUER"): Text3D
 *   draws those as the uppercase letter A, B, X, Y, R or L, flat (a quarter of
 *   the usual depth; the font's lowercase entries are the same objects as the
 *   uppercase ones). The flat A, B, X and Y are not drawn: the render thread
 *   draws the PS Vita button the port maps that GameCube button to in their
 *   place (port_pad.c: A = cross, B = square, X = circle, Y = triangle;
 *   gx_gl.c). The flat L and R stay: the Vita's are called the same.
 *   CWOC_GCBUTTONS=1 keeps the GameCube glyphs.
 *
 * The log gets, once, the font's glyph list and the game's texts that can
 * show buttons (those with lowercase letters, and the first of each table;
 * every one with CWOC_TEXTDUMP=1), and the first strings where a button
 * glyph was replaced. */

extern void port_log(const char *fmt, ...);
extern float port_wide_factor(void);
extern int port_frame_count(void);
extern int port_fps_overlay_on(void);
extern int port_fps_x10(void);
extern void port_gx_icon(float x, float y, float d, int icon);
extern void port_gx_icon_shade(float s);

extern s32 Paused;
extern s32 fadeval;
extern s32 disable_safearea_clamp;
extern struct ldata_s *LDATA;
extern struct nugscn_s *font3d_scene;
extern struct numtl_s **nurndr_forced_mtl_table;
extern s16 Font3DRemap[256];
extern float FONT3DSIZE;
extern float FONT3DYMUL;
extern float PANEL3DMULX;
extern float PANEL3DMULY;

extern char *tPRESSxTOCONTINUE[6];
extern char *tTCR_PRESSSTARTTORESUME[6];
extern char *tSUPERBELLYFLOP[6];
extern char *tSUPERBELLYFLOPTEXT[6][7];
extern char *tDEATHTORNADOSPIN[6];
extern char *tDEATHTORNADOSPINTEXT[6][7];
extern char *tDOUBLEJUMP[6];
extern char *tDOUBLEJUMPTEXT[6][7];
extern char *tFRUITBAZOOKA[6];
extern char *tFRUITBAZOOKATEXT[6][7];
extern char *tSPEEDSHOES[6];
extern char *tSPEEDSHOESTEXT[6][7];
extern char *tSNEAKSHOES[6];
extern char *tSNEAKSHOESTEXT[6][7];

void DrawPanel(void);
void DrawMenu(struct cursor_s *cursor, s32 paused);
void BigOutOf(float x, float y, float scale, int value, int total);
void DrawTimeTrialTimes(int level, float x, float y, float z);
void DrawMenuEntry2(struct cursor_s *cursor, char *txt0, char *txt1, float *x, float *y, s32 *i);

/* panel objects of the pause screen's frame (DrawPanel) */
#define PAUSE_FRAME_A 98
#define PAUSE_FRAME_B 99

/* ---------------------------------------------------------------- layout */
static float g_hud_wf = 1.0f; /* != 1: wide pause layout, x of placed things times this */
static int g_hud_depth;       /* hooked calls in progress (only the outermost one moves) */
static const char *g_text_cur; /* the text being drawn (log of replaced buttons) */

static int hud_moves(void) {
    return g_hud_wf != 1.0f && g_hud_depth == 0 && nurndr_forced_mtl_table == NULL;
}

/* used by every hook that places something: returns the x to use; *st is
 * the safe-area clamp to put back (-1: untouched). Text3D squeezes text
 * that starts left of -0.81: not wanted once the layout is wider. */
float port_hud_enter(float x, int *st) {
    *st = -1;
    if (hud_moves()) {
        x *= g_hud_wf;
        *st = disable_safearea_clamp;
        disable_safearea_clamp = 1;
    }
    g_hud_depth++;
    return x;
}

void port_hud_leave(int st) {
    g_hud_depth--;
    if (st >= 0) disable_safearea_clamp = st;
}

/* port_hooks.c (DrawMenuEntry) */
void port_hud_text(const char *txt) { g_text_cur = txt; }

static int wide_pause_wanted(void) {
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("CWOC_WIDEPAUSE");
        on = !(e && e[0] == '0');
    }
    return on;
}

/* ---------------------------------------------------------------- buttons */
enum { ICON_CROSS, ICON_CIRCLE, ICON_SQUARE, ICON_TRIANGLE }; /* atlas order (gx_gl.c) */
static const char k_btn_char[4] = { 'a', 'b', 'x', 'y' };
static const int k_btn_icon[4] = { ICON_CROSS, ICON_SQUARE, ICON_CIRCLE, ICON_TRIANGLE };
static const char *const k_icon_name[4] = { "cross", "circle", "square", "triangle" };

static int vita_buttons_on(void) {
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("CWOC_GCBUTTONS");
        on = !(e && e[0] == '1');
        port_log("hud: %s button glyphs", on ? "PS Vita" : "GameCube");
    }
    return on;
}

/* z / y scale of the text being drawn (Text3D / Text3D2 hooks; 0: not known) */
static float g_text_zr;

/* which GameCube button glyph Text3D is drawing (-1: none). The font's
 * lowercase a, b, x, y entries are the same objects as the uppercase letters
 * (Font3DTab names them "a", "b", "x", "y" too): a button is the letter drawn
 * flat, a quarter of the depth Text3D gives every other glyph. So: flat, and
 * then which letter. (w and n, the flat R and L, are the PS Vita's L and R
 * too: they stay.) */
static int button_of(const struct nuspecial_s *obj, float scaley, float scalez) {
    /* a text drawn from native code (DrawMenuEntry: "PRESS a TO CONTINUE")
     * does not go through the Text3D hook: the game's texts have z = y
     * scale (1.2 at most), so a glyph is flat below 2 */
    float zr = g_text_zr > 0.0f ? g_text_zr : 1.0f;
    int b;
    if (scaley == 0.0f || fabsf(scalez / scaley) >= 2.0f * zr) return -1;
    for (b = 0; b < 4; b++) {
        int i = Font3DRemap[(u8)k_btn_char[b]];
        if (i >= 0 && i < 62 && Font3DTab[i].obj.special == obj) return b;
    }
    return -1;
}

static float text_zr(float scaley, float scalez) { return scaley != 0.0f ? fabsf(scalez / scaley) : 0.0f; }

static void log_text(const char *label, const char *s);

/* the glyph Text3D was going to draw at (x, y, z), with its y scale sy
 * (FONT3DSIZE x the text's y scale): the icon at the same place, a fifth
 * taller than the text's line (font3d_dy), so the symbol reads at the Vita's
 * resolution (the glyph sits between spaces: there is room) */
static void button_icon(int b, float x, float y, float z, float sy) {
    /* GS_SetProjectionMatrix: 40 degree vertical field of view, aspect
     * 1.428571 times the wide factor; a panel object sits at
     * (x * PANEL3DMULX, y * PANEL3DMULY, z) in front of the camera */
    const float t = 0.36397023f; /* tan(20 deg) */
    float aspect = 1.428571f * port_wide_factor();
    float scaley = FONT3DSIZE > 0.0f ? sy / FONT3DSIZE : sy;
    float d = 0.1f * scaley * (FONT3DYMUL > 0.0f ? FONT3DYMUL : 1.0f) * 1.2f;
    static int logged;
    if (z <= 0.01f) return;
    port_gx_icon(x * PANEL3DMULX / (z * t * aspect), y * PANEL3DMULY / (z * t), d * PANEL3DMULY / (z * t),
                 k_btn_icon[b]);
    if (logged < 24 && g_text_cur) {
        static const char *seen[24];
        int i;
        for (i = 0; i < logged; i++)
            if (seen[i] == g_text_cur) break;
        if (i == logged) {
            char label[64];
            seen[logged++] = g_text_cur;
            snprintf(label, sizeof(label), "button '%c' -> %s in", k_btn_char[b], k_icon_name[k_btn_icon[b]]);
            log_text(label, g_text_cur);
        }
    }
}

/* ---------------------------------------------------------------- hooks */
s32 port_hook_DrawPanel3DObject(s32 object, float x, float y, float z, float scalex, float scaley, float scalez,
                                u16 xrot, u16 yrot, u16 zrot, struct nugscn_s *scn, struct nuspecial_s *obj, s32 rot) {
    s32 r;
    int st;
    if (scn != NULL && scn == font3d_scene && obj != NULL && vita_buttons_on()) {
        int b = button_of(obj, scaley, scalez);
        if (b >= 0) {
            button_icon(b, x, y, z, scaley);
            return 1;
        }
    }
    if ((object == PAUSE_FRAME_A || object == PAUSE_FRAME_B) && hud_moves())
        scalex *= g_hud_wf; /* the frame itself: stretched to the whole width */
    x = port_hud_enter(x, &st);
    r = DrawPanel3DObject(object, x, y, z, scalex, scaley, scalez, xrot, yrot, zrot, scn, obj, rot);
    port_hud_leave(st);
    return r;
}

s32 port_hook_DrawPanel3DCharacter(s32 character, float x, float y, float z, float scalex, float scaley, float scalez,
                                   u16 xrot, u16 yrot, u16 zrot, s32 action, float anim_time, s32 rot) {
    s32 r;
    int st;
    x = port_hud_enter(x, &st);
    r = DrawPanel3DCharacter(character, x, y, z, scalex, scaley, scalez, xrot, yrot, zrot, action, anim_time, rot);
    port_hud_leave(st);
    return r;
}

void port_hook_Text3D(char *txt, float x, float y, float z, float scalex, float scaley, float scalez, s32 align,
                      s32 colour) {
    const char *prev = g_text_cur;
    float prev_zr = g_text_zr;
    int st;
    g_text_cur = txt;
    g_text_zr = text_zr(scaley, scalez);
    x = port_hud_enter(x, &st);
    Text3D(txt, x, y, z, scalex, scaley, scalez, align, colour);
    port_hud_leave(st);
    g_text_cur = prev;
    g_text_zr = prev_zr;
}

void port_hook_Text3D2(char *txt, float x, float y, float z, float scalex, float scaley, float scalez, s32 align,
                       s32 colour) {
    const char *prev = g_text_cur;
    float prev_zr = g_text_zr;
    int st;
    g_text_cur = txt;
    g_text_zr = text_zr(scaley, scalez);
    x = port_hud_enter(x, &st);
    Text3D2(txt, x, y, z, scalex, scaley, scalez, align, colour);
    port_hud_leave(st);
    g_text_cur = prev;
    g_text_zr = prev_zr;
}

void port_hook_BigOutOf(float x, float y, float scale, int value, int total) {
    int st;
    x = port_hud_enter(x, &st);
    BigOutOf(x, y, scale, value, total);
    port_hud_leave(st);
}

void port_hook_DrawTimeTrialTimes(int level, float x, float y, float z) {
    int st;
    x = port_hud_enter(x, &st);
    DrawTimeTrialTimes(level, x, y, z);
    port_hud_leave(st);
}

void port_hook_DrawMenuEntry2(struct cursor_s *cursor, char *txt0, char *txt1, float *x, float *y, s32 *i) {
    float x0 = *x;
    int st;
    g_text_cur = txt0;
    *x = port_hud_enter(x0, &st);
    DrawMenuEntry2(cursor, txt0, txt1, x, y, i);
    port_hud_leave(st);
    *x = x0;
    g_text_cur = NULL;
}

/* ---------------------------------------------------------------- fps */
static void fps_draw(void) {
    static int last_frame = -1;
    char buf[40];
    int v = port_fps_x10(), clamp, f = port_frame_count();
    float x, y;
    if (!port_fps_overlay_on() || v < 0 || f == last_frame || PANEL3DMULX <= 0.0f || PANEL3DMULY <= 0.0f) return;
    last_frame = f;
    snprintf(buf, 16, "%d.%d FPS", v / 10, v % 10);
    if (Game.language == 0x63) AddSpacesIntoText(buf, 1);
    /* left-aligned near the top-left corner of the picture: the panel x, y
     * that the projection (button_icon) puts at 3.5 % / 8.5 % of the screen
     * from the edges. In widescreen that is past the HUD's 4:3 part. */
    x = -0.965f * (0.36397023f * 1.428571f * port_wide_factor()) / PANEL3DMULX;
    y = 0.915f * 0.36397023f / PANEL3DMULY;
    clamp = disable_safearea_clamp;
    disable_safearea_clamp = 1;
    g_hud_depth++; /* its glyphs are not moved by the wide pause layout */
    Text3D(buf, x, y, 1.0f, 0.5f, 0.5f, 0.5f, 4, 0);
    g_hud_depth--;
    disable_safearea_clamp = clamp;
}

/* ---------------------------------------------------------------- log */
static void log_text(const char *label, const char *s) {
    char buf[256];
    int n = 0;
    if (s == NULL) {
        port_log("hud: %s (null)", label);
        return;
    }
    for (; *s && n < 236; s++) {
        unsigned char c = (unsigned char)*s;
        if (c >= 0x20 && c < 0x7f && c != '\\') buf[n++] = (char)c;
        else n += snprintf(buf + n, sizeof(buf) - (size_t)n, "\\x%02x", c);
    }
    buf[n] = 0;
    port_log("hud: %s \"%s\"", label, buf);
}

/* lowercase letters are what the text uses for button glyphs and icons */
static int has_lower(const char *s) {
    for (; s != NULL && *s; s++)
        if (*s >= 'a' && *s <= 'z') return 1;
    return 0;
}

/* a power-up's name and description table (text.c: [6][7]); which index is
 * the language is not known for sure: the first row, and every entry with a
 * lowercase letter (all of them with CWOC_TEXTDUMP=1) */
static void dump_power(const char *name, char *title[6], char *text[6][7], int all) {
    int l, k;
    char label[64];
    for (l = 0; l < 6; l++) {
        if (all || l == 0 || has_lower(title[l])) {
            snprintf(label, sizeof(label), "%s[%d]", name, l);
            log_text(label, title[l]);
        }
        for (k = 0; k < 7; k++) {
            if (text[l][k] == NULL || text[l][k][0] == 0) continue;
            if (!all && l != 0 && !has_lower(text[l][k])) continue;
            snprintf(label, sizeof(label), "%sTEXT[%d][%d]", name, l, k);
            log_text(label, text[l][k]);
        }
    }
}

/* once the font is loaded: its glyphs, and the texts that can show buttons */
static void hud_dump_once(void) {
    static int done;
    char glyphs[128];
    int i, n = 0, all = getenv("CWOC_TEXTDUMP") != NULL;
    if (done || !font3d_initialised || font3d_scene == NULL) return;
    done = 1;
    for (i = 0; i < 62 && Font3DTab[i].ascii != 0; i++) {
        char c = Font3DTab[i].ascii;
        if (Font3DRemap[(u8)c] == -1) continue;
        if (n < (int)sizeof(glyphs) - 1) glyphs[n++] = c;
        if (c >= 'a' && c <= 'z') {
            char label[64];
            snprintf(label, sizeof(label), "font glyph '%c' (lowercase) is", c);
            log_text(label, Font3DTab[i].name);
        }
    }
    glyphs[n] = 0;
    log_text("font glyphs:", glyphs);
    port_log("hud: language %d", (int)Game.language);
    for (i = 0; i < 6; i++) {
        char label[48];
        if (all || i == 0 || has_lower(tPRESSxTOCONTINUE[i])) {
            snprintf(label, sizeof(label), "tPRESSxTOCONTINUE[%d]", i);
            log_text(label, tPRESSxTOCONTINUE[i]);
        }
        if (all || i == 0 || has_lower(tTCR_PRESSSTARTTORESUME[i])) {
            snprintf(label, sizeof(label), "tTCR_PRESSSTARTTORESUME[%d]", i);
            log_text(label, tTCR_PRESSSTARTTORESUME[i]);
        }
    }
    dump_power("tSUPERBELLYFLOP", tSUPERBELLYFLOP, tSUPERBELLYFLOPTEXT, all);
    dump_power("tDEATHTORNADOSPIN", tDEATHTORNADOSPIN, tDEATHTORNADOSPINTEXT, all);
    dump_power("tDOUBLEJUMP", tDOUBLEJUMP, tDOUBLEJUMPTEXT, all);
    dump_power("tFRUITBAZOOKA", tFRUITBAZOOKA, tFRUITBAZOOKATEXT, all);
    dump_power("tSPEEDSHOES", tSPEEDSHOES, tSPEEDSHOESTEXT, all);
    dump_power("tSNEAKSHOES", tSNEAKSHOES, tSNEAKSHOESTEXT, all);
}

/* ---------------------------------------------------------------- panel */
void port_hook_DrawPanel(void) {
    float wf = port_wide_factor();
    int menu = Cursor.menu;
    hud_dump_once();
    g_hud_depth = 0;
    g_text_cur = NULL;
    g_text_zr = 0.0f;
    /* the pause screen's frame is up: DrawPanel draws it for these menus */
    g_hud_wf = (wf > 1.001f && wide_pause_wanted() && Paused != 0 && LDATA != NULL && (LDATA->flags & 1) != 0 &&
                menu >= 5 && menu <= 10) ? wf : 1.0f;
    DrawPanel();
    g_hud_wf = 1.0f;
    g_hud_depth = 0;
    /* DrawFade, after the HUD scene, multiplies the frame by 1 - fadeval/255
     * (not on menu 2): the icons, drawn later, get the same */
    port_gx_icon_shade((Cursor.menu != 2 && fadeval > 0) ? 1.0f - fadeval / 255.0f : 1.0f);
}

/* DrawPanel's menus: the fps counter goes in the same scene, with the same
 * set-up (DrawPanel calls DrawMenu before anything else it draws) */
void port_hook_DrawMenu(struct cursor_s *cursor, s32 paused) {
    DrawMenu(cursor, paused);
    fps_draw();
}
