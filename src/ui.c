#define _XOPEN_SOURCE 700
#include "app.h"
#include "image_terminal.h"
#include <errno.h>
#include <ctype.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <sys/ioctl.h>
#include <unistd.h>

enum { INK = 1, MUTED, ACCENT, BLUE, GREEN, CYAN, RED, SELECTED, PANEL, BAR, STRING, COMMENT };
enum { HOME_BASE = INK, HOME_MUTED = MUTED, HOME_ACCENT = ACCENT, HOME_SELECTED = SELECTED,
       HOME_BLUE = BLUE, HOME_GOOD = BLUE, HOME_WARN = ACCENT, HOME_BAD = RED, HOME_TRACK = PANEL };
static int color(App *a, int pair) { return a->no_color ? 0 : COLOR_PAIR(pair); }
static void pane_border(App *a, WINDOW *w) {
    if (a->ascii) { wborder(w, '|', '|', '-', '-', '+', '+', '+', '+'); return; }
    cchar_t v, h, tl, tr, bl, br;
    setcchar(&v, L"│", 0, 0, NULL); setcchar(&h, L"─", 0, 0, NULL);
    setcchar(&tl, L"╭", 0, 0, NULL); setcchar(&tr, L"╮", 0, 0, NULL);
    setcchar(&bl, L"╰", 0, 0, NULL); setcchar(&br, L"╯", 0, 0, NULL);
    wborder_set(w, &v, &v, &h, &h, &tl, &tr, &bl, &br);
}
/* Never pass untrusted bytes directly to curses: decode UTF-8, strip controls,
   account for display-cell widths, and replace invalid sequences. */
static void text(WINDOW *w, int row, int col, int width, const char *s, int attr) {
    if (!w || width <= 0 || row < 0 || row >= getmaxy(w) || col < 0 || col >= getmaxx(w)) return;
    if (width > getmaxx(w) - col) width = getmaxx(w) - col;
    wmove(w, row, col); wattrset(w, attr);
    mbstate_t state = {0}; int used = 0;
    while (*s && used < width) {
        wchar_t ch; size_t n = mbrtowc(&ch, s, MB_CUR_MAX, &state);
        if (n == (size_t)-1 || n == (size_t)-2) { ch = L'?'; n = 1; memset(&state, 0, sizeof(state)); }
        if (!n) break;
        int cells = wcwidth(ch);
        if (cells < 0 || ch == 0x202e || ch == 0x202d || (ch >= 0x2066 && ch <= 0x2069)) { ch = L'?'; cells = 1; }
        if (used + cells > width) break;
        wchar_t chars[2] = {ch, 0}; waddnwstr(w, chars, 1); used += cells; s += n;
    }
    wattrset(w, 0);
}
static const char *glyph(App *a, const Entry *e) {
    if (a->ascii) return e->symlink ? "@" : e->directory ? "/" : e->st.st_mode & 0111 ? "*" : "-";
    return e->symlink ? "↗" : e->directory ? "▸" : e->st.st_mode & 0111 ? "◆" : "·";
}
static int entry_color(App *a, const Entry *e) { return color(a, e->symlink ? CYAN : e->directory ? BLUE : e->st.st_mode & 0111 ? GREEN : INK); }
static void destroy_windows(App *a) {
    if (a->left) delwin(a->left);
    if (a->center) delwin(a->center);
    if (a->right) delwin(a->right);
    if (a->viewer) delwin(a->viewer);
    a->left = a->center = a->right = a->viewer = NULL;
}
static void layout(App *a) {
    destroy_windows(a); a->layout_rows = LINES; a->layout_cols = COLS;
    a->layout_preview = a->preview_full;
    a->layout_panel = a->panel;
    if (LINES < 14 || COLS < 38) return;
    int h = LINES - 7;
    if (a->preview_full || a->panel != PANEL_NONE) { a->viewer = newwin(h, COLS, 3, 0); return; }
    if (COLS >= 108) {
        int left = COLS * 22 / 100, center = COLS * 38 / 100;
        a->left = newwin(h, left, 3, 0); a->center = newwin(h, center, 3, left);
        a->right = newwin(h, COLS - left - center, 3, left + center);
    } else if (COLS >= 76) {
        int center = COLS / 2; a->center = newwin(h, center, 3, 0); a->right = newwin(h, COLS - center, 3, center);
    } else a->center = newwin(h, COLS, 3, 0);
}
int ui_init(App *a) {
    const char *term = getenv("TERM");
    if (!term || !strcmp(term, "dumb")) { fputs("termnav: set TERM to your terminal type (for example xterm-256color)\n", stderr); return -1; }
    if (!initscr()) return -1;
    cbreak(); noecho(); keypad(stdscr, TRUE); curs_set(0); timeout(80); set_escdelay(25);
    if (!a->no_color && !a->ascii) a->sixel=image_terminal_probe(&a->cell_width,&a->cell_height);
    if (!has_colors()) a->no_color = true;
    if (!a->no_color) {
        start_color(); use_default_colors();
        if (COLORS >= 256) {
            init_pair(INK, 252, 234); init_pair(MUTED, 244, 234); init_pair(ACCENT, 180, 234);
            init_pair(BLUE, 110, 234); init_pair(GREEN, 150, 234); init_pair(CYAN, 116, 234); init_pair(RED, 174, 234);
            init_pair(SELECTED, 223, 238); init_pair(PANEL, 240, 234); init_pair(BAR, 234, 180);
            init_pair(STRING, 150, 234); init_pair(COMMENT, 244, 234);
            if (COLOR_PAIRS >= 256) for (int i=16;i<256;++i) init_pair((short)i,(short)i,(short)i);
        } else {
            init_pair(INK, COLOR_WHITE, -1); init_pair(MUTED, COLOR_WHITE, -1); init_pair(ACCENT, COLOR_YELLOW, -1);
            init_pair(BLUE, COLOR_BLUE, -1); init_pair(GREEN, COLOR_GREEN, -1); init_pair(CYAN, COLOR_CYAN, -1); init_pair(RED, COLOR_RED, -1);
            init_pair(SELECTED, COLOR_BLACK, COLOR_WHITE); init_pair(PANEL, COLOR_WHITE, -1); init_pair(BAR, COLOR_BLACK, COLOR_YELLOW);
            init_pair(STRING, COLOR_GREEN, -1); init_pair(COMMENT, COLOR_WHITE, -1);
        }
    }
    bkgd(color(a, INK)); layout(a); app_preview(a,true); return 0;
}
void ui_shutdown(App *a) { destroy_windows(a); endwin(); }
static void frame(App *a, WINDOW *w, const char *label, const char *subtitle, bool active) {
    if (!w) return;
    wbkgd(w, color(a, INK)); werase(w); touchwin(w);
    wattrset(w, color(a, PANEL));
    pane_border(a, w);
    int width = getmaxx(w);
    text(w, 1, 2, width - 4, label, color(a, active ? ACCENT : MUTED) | A_BOLD);
    text(w, 2, 2, width - 4, subtitle, color(a, MUTED));
}
static void row_entry(App *a, WINDOW *w, int row, const Entry *e, bool selected, bool sizes) {
    int width = getmaxx(w); char size[24];
    int attr = selected ? color(a, SELECTED) | (int)A_BOLD : entry_color(a, e);
    if (selected && a->no_color) attr = A_REVERSE | A_BOLD;
    if (selected) { wattrset(w, attr); mvwhline(w, row, 1, ' ', width - 2); }
    int name_width = width - 8 - (sizes && width > 32 ? 10 : 0);
    text(w, row, 2, 1, e->marked ? (a->ascii ? "*" : "●") : " ", e->marked && !selected ? color(a, ACCENT) : attr);
    text(w, row, 4, 2, glyph(a, e), attr);
    text(w, row, 6, name_width, e->name, attr);
    if (sizes && width > 32) {
        if (e->directory) snprintf(size, sizeof(size), "%s", e->symlink ? "link/" : "dir/"); else fs_size(size, sizeof(size), e->st.st_size);
        text(w, row, width - 11, 9, size, selected ? attr : color(a, MUTED));
    }
}
static void parent_pane(App *a) {
    if (!a->left) return;
    frame(a, a->left, "01  PARENT", fs_basename(a->parent.path), false);
    int h = getmaxy(a->left) - 5; size_t selected = 0;
    const char *name = fs_basename(a->current.path);
    for (size_t i = 0; i < a->parent.count; ++i) if (!strcmp(a->parent.entries[i].name, name)) { selected = i; break; }
    size_t start = selected > (size_t)(h / 2) ? selected - (size_t)(h / 2) : 0;
    for (int i = 0; i < h && start + (size_t)i < a->parent.count; ++i) row_entry(a, a->left, i + 4, &a->parent.entries[start + (size_t)i], start + (size_t)i == selected, false);
    wnoutrefresh(a->left);
}
static void current_pane(App *a) {
    if (!a->center) return;
    char subtitle[128]; snprintf(subtitle, sizeof(subtitle), "%zu items%s  /  %s", a->visible_count, a->query[0] ? " matching" : "", a->sort == 1 ? "size" : a->sort == 2 ? "modified" : "name");
    frame(a, a->center, a->left ? "02  FILES" : "FILES", subtitle, true);
    int h = getmaxy(a->center) - 5;
    if (a->cursor < a->scroll) a->scroll = a->cursor;
    if (a->cursor >= a->scroll + (size_t)h) a->scroll = a->cursor - (size_t)h + 1;
    if (!a->visible_count) {
        text(a->center, 5, 3, getmaxx(a->center) - 6, a->query[0] ? "No matches." : "A little room to begin.", color(a, MUTED));
        if (getmaxy(a->center) > 7) text(a->center, 7, 3, getmaxx(a->center) - 6, a->query[0] ? "Esc clears the filter" : "a  create a directory", color(a, ACCENT));
    }
    for (int i = 0; i < h && a->scroll + (size_t)i < a->visible_count; ++i) {
        size_t index = a->scroll + (size_t)i;
        row_entry(a, a->center, i + 4, &a->current.entries[a->visible[index]], index == a->cursor, true);
    }
    if (a->visible_count > (size_t)h) {
        int y = 4 + (int)(a->cursor * (size_t)(h - 1) / (a->visible_count - 1));
        text(a->center, y, getmaxx(a->center) - 1, 1, a->ascii ? "#" : "┃", color(a, ACCENT));
    }
    wnoutrefresh(a->center);
}
static bool keyword(const char *s, size_t n) {
    const char *words[] = {"if", "else", "for", "while", "return", "break", "continue", "switch", "case", "const", "static", "void", "int", "char", "size_t", "struct", "typedef", "enum", "bool", "true", "false", "null", "NULL", "def", "class", "import", "from", "as", "try", "except", "with", "in", "is", "not", "and", "or", "None", "True", "False", "function", "let", "var", "export", "async", "await", "fn", "pub", "use", "impl", "mut", "package", "func"};
    for (size_t i = 0; i < sizeof(words) / sizeof(*words); ++i) if (strlen(words[i]) == n && !memcmp(s, words[i], n)) return true;
    return false;
}
static bool word_byte(unsigned char ch) { return ch == '_' || (ch < 128 && isalnum(ch)); }
static void code_line(App *a, WINDOW *w, int row, const char *s, size_t length, int width) {
    size_t cells = 0, token_end = 0, skip = a->preview_column;
    bool comment = false, escaped = false; char quote = 0; int token_pair = INK;
    PreviewSyntax mode = a->preview.syntax;
    const char *trim = s; while ((size_t)(trim - s) < length && (*trim == ' ' || *trim == '\t')) ++trim;
    bool heading = mode == SYNTAX_MARKDOWN && (size_t)(trim - s) < length && *trim == '#';
    mbstate_t state = {0};
    for (size_t i = 0; i < length && cells < skip + (size_t)width;) {
        unsigned char byte = (unsigned char)s[i]; int pair = heading ? ACCENT : INK;
        if (mode == SYNTAX_CODE || mode == SYNTAX_HASH || mode == SYNTAX_JSON) {
            if (comment) pair = COMMENT;
            else if (quote) {
                pair = STRING;
                if (escaped) escaped = false;
                else if (byte == '\\') escaped = true;
                else if (byte == (unsigned char)quote) quote = 0;
            } else if (byte == '"' || (byte == '\'' && mode != SYNTAX_JSON)) { quote = (char)byte; pair = STRING; }
            else if ((byte == '#' && mode == SYNTAX_HASH) || (byte == '/' && mode == SYNTAX_CODE && i + 1 < length && (s[i + 1] == '/' || s[i + 1] == '*'))) { comment = true; pair = COMMENT; }
            else if (i < token_end) pair = token_pair;
            else if (word_byte(byte)) {
                token_end = i + 1; while (token_end < length && word_byte((unsigned char)s[token_end])) ++token_end;
                token_pair = isdigit(byte) ? CYAN : keyword(s + i, token_end - i) ? ACCENT : INK;
                pair = token_pair;
            }
        }
        wchar_t ch; size_t n = mbrtowc(&ch, s + i, length - i, &state);
        if (n == (size_t)-1 || n == (size_t)-2 || !n) { ch = L'?'; n = 1; memset(&state, 0, sizeof(state)); }
        if (ch == L'\t') {
            size_t spaces = 4 - cells % 4;
            for (size_t j = 0; j < spaces; ++j, ++cells) if (cells >= skip && cells < skip + (size_t)width) text(w, row, 8 + (int)(cells - skip), 1, " ", color(a, pair));
        } else {
            int count = wcwidth(ch);
            if (count < 0 || ch == 0x202e || ch == 0x202d || (ch >= 0x2066 && ch <= 0x2069)) { ch = L'?'; count = 1; }
            if (!count && cells > skip && cells <= skip + (size_t)width) {
                wchar_t chars[2] = {ch, 0}; waddnwstr(w, chars, 1);
            } else if (count && cells >= skip && cells + (size_t)count <= skip + (size_t)width) {
                wchar_t chars[2] = {ch, 0};
                wattrset(w, color(a, pair)); wmove(w, row, 8 + (int)(cells - skip)); waddnwstr(w, chars, 1);
            }
            cells += (size_t)count;
        }
        i += n;
    }
    wattrset(w, 0);
}
static void preview_text(App *a, WINDOW *w, int height) {
    int width = getmaxx(w) - 10;
    for (int row = 0; row < height && a->preview_scroll + (size_t)row < a->preview.lines; ++row) {
        size_t line = a->preview_scroll + (size_t)row, length;
        const char *content = preview_line(&a->preview, line, &length);
        char number[24]; snprintf(number, sizeof(number), "%5zu", line + 1);
        text(w, row + 4, 2, 5, number, color(a, MUTED));
        if (content) code_line(a, w, row + 4, content, length, width);
    }
}
static void preview_hex(App *a, WINDOW *w, int height) {
    for (int row = 0; row < height && a->preview_scroll + (size_t)row < preview_rows(&a->preview); ++row) {
        size_t start = (a->preview_scroll + (size_t)row) * 16;
        char line[96], ascii[17]; size_t used = (size_t)snprintf(line, sizeof(line), "%08zx  ", start);
        for (size_t i = 0; i < 16; ++i) {
            if (start + i < a->preview.bytes) {
                unsigned char byte = (unsigned char)a->preview.data[start + i];
                used += (size_t)snprintf(line + used, sizeof(line) - used, "%02x ", byte);
                ascii[i] = byte >= 32 && byte <= 126 ? (char)byte : '.';
            } else { used += (size_t)snprintf(line + used, sizeof(line) - used, "   "); ascii[i] = ' '; }
            if (i == 7) line[used++] = ' ';
        }
        ascii[16] = 0; snprintf(line + used, sizeof(line) - used, " |%s|", ascii);
        size_t skip = a->preview_column < strlen(line) ? a->preview_column : strlen(line);
        text(w, row + 4, 3, getmaxx(w) - 6, line + skip, color(a, CYAN));
    }
}
static int image_color(const unsigned char *rgb) {
    static const int levels[]={0,95,135,175,215,255}; int components[3],distance=0;
    for (int component=0;component<3;++component) {
        int closest=0,best=65536;
        for (int level=0;level<6;++level) { int d=rgb[component]-levels[level]; if (d*d<best) { best=d*d; closest=level; } }
        components[component]=closest; distance+=best;
    }
    int index=16+components[0]*36+components[1]*6+components[2];
    for (int gray=0;gray<24;++gray) {
        int value=8+gray*10,d=0;
        for (int component=0;component<3;++component) { int delta=rgb[component]-value; d+=delta*delta; }
        if (d<distance) { distance=d; index=232+gray; }
    }
    return index;
}
static short image_pair(int top,int bottom) {
    static short pairs[256][256]; static int next=256;
    if (top==bottom) return (short)top;
    if (pairs[top][bottom]) return pairs[top][bottom];
    if (next<COLOR_PAIRS && next<32767) {
        short pair=(short)next++; init_pair(pair,(short)top,(short)bottom); pairs[top][bottom]=pair; return pair;
    }
    return (short)top;
}
static void draw_preview(App *a, WINDOW *w, bool expanded) {
    if (!w) return;
    Preview *p = &a->preview; int width = getmaxx(w), h = getmaxy(w) - 5;
    char subtitle[128];
    snprintf(subtitle, sizeof(subtitle), "%s  /  %s", preview_format(p), expanded ? "READ ONLY" : "Tab to expand");
    frame(a, w, expanded ? "READ-ONLY PREVIEW" : a->left ? "03  PREVIEW" : "PREVIEW", subtitle, expanded);
    if (p->kind == PREVIEW_IMAGE) {
        char dimensions[128]; snprintf(dimensions,sizeof(dimensions),"%u x %u pixels / first frame",p->original_width,p->original_height);
        text(w,3,2,width-4,dimensions,color(a,MUTED));
        if (p->image_sixel) { /* The raster is painted after the curses frame. */ }
        else if (a->no_color || COLORS < 256 || COLOR_PAIRS < 256) text(w,5,3,width-6,"Image colors require a 256-color terminal.",color(a,ACCENT));
        else {
            int step=a->ascii ? 2 : 1, vertical=a->ascii ? 1 : 2;
            int iw=(width-6)/step, ih=(getmaxy(w)-7)*vertical;
            double scale=(double)iw/p->image_width;
            if ((double)ih/p->image_height < scale) scale=(double)ih/p->image_height;
            iw=(int)(p->image_width*scale); ih=(int)(p->image_height*scale);
            for (int y=0;y<ih;y+=vertical) for (int x=0;x<iw;++x) {
                size_t offset=((size_t)(y*p->image_height/ih)*p->image_width+(unsigned)(x*p->image_width/iw))*3;
                const unsigned char *rgb=(const unsigned char*)p->data+offset;
                int top=image_color(rgb),bottom=234;
                if (y+1<ih) { size_t lower=((size_t)((y+1)*p->image_height/ih)*p->image_width+(unsigned)(x*p->image_width/iw))*3; bottom=image_color((const unsigned char*)p->data+lower); }
                short pair=a->ascii ? (short)top : image_pair(top,bottom);
                wattr_set(w,A_NORMAL,pair,NULL); wmove(w,y/vertical+5,(width-iw*step)/2+x*step);
                if (a->ascii) waddstr(w,"  "); else waddnwstr(w,L"▀",1);
            }
            wattrset(w,0);
        }
    } else if (p->kind == PREVIEW_TEXT) {
        preview_text(a, w, h);
        if (!p->bytes) text(w, 5, 3, width - 6, "Empty file. A clean slate.", color(a, MUTED));
        if (p->truncated) text(w, getmaxy(w) - 1, 2, width - 4, " first 64 KiB ", color(a, MUTED));
    } else if (p->kind == PREVIEW_DIRECTORY) {
        for (int i = 0; i < h && a->preview_scroll + (size_t)i < p->directory.count; ++i) row_entry(a, w, i + 4, &p->directory.entries[a->preview_scroll + (size_t)i], false, true);
        if (!p->directory.count) text(w, 5, 3, width - 6, "This directory is empty.", color(a, MUTED));
    } else if (p->kind == PREVIEW_BINARY && expanded) {
        preview_hex(a, w, h);
        if (p->truncated) text(w, getmaxy(w) - 1, 2, width - 4, " first 64 KiB ", color(a, MUTED));
    } else {
        const char *title = p->kind == PREVIEW_LOADING ? "Preparing rich preview..." : p->kind == PREVIEW_BINARY ? "Binary file" : p->kind == PREVIEW_ERROR ? "Preview unavailable" : p->kind == PREVIEW_SPECIAL ? "Special file" : p->kind == PREVIEW_LINK ? "Linked somewhere else" : "Nothing selected";
        text(w, 5, 3, width - 6, title, color(a, p->kind == PREVIEW_ERROR ? RED : ACCENT) | A_BOLD);
        if (getmaxy(w) > 8) text(w, 7, 3, width - 6, p->kind == PREVIEW_BINARY ? "Tab to inspect hexadecimal bytes." : p->kind == PREVIEW_LINK ? p->target : p->message, color(a, MUTED));
    }
    wnoutrefresh(w);
}
static void help(App *a) {
    int width = COLS > 82 ? 78 : COLS - 4, height = LINES > 28 ? 25 : LINES - 2;
    if (width < 34 || height < 10) return;
    WINDOW *w = newwin(height, width, (LINES - height) / 2, (COLS - width) / 2); if (!w) return;
    wbkgd(w, color(a, INK)); werase(w); wattrset(w, color(a, ACCENT)); pane_border(a, w);
    text(w, 1, 3, width - 6, "Find your way around", color(a, ACCENT) | A_BOLD);
    text(w, 2, 3, width - 6, "F1 Home / w toggle Home / Tab switch areas on Home", color(a, MUTED));
    const char *keys[] = {"j k / arrows", "h l / Enter", "gg / G", "J K / Tab", "/ / S / :", ". / s / R", "Space / v", "y p / c", "a / r", "d / u / T", "X", "t / D", "b f / H", "o ~ / e", "Esc", "q"};
    const char *desc[] = {"Move selection", "Parent / enter; file opens preview", "First / last item", "Scroll preview / expand to full screen", "Filter / search subfolders / commands", "Hidden files / sort / refresh", "Mark one / mark visible items", "Clipboard copy / copy to a directory", "Create directory / rename", "Trash / undo / browse trash", "Permanent delete; type delete to confirm", "Operations dashboard / disk usage", "Directory back/forward / history", "Go to directory/home / editor", "Clear filter & marks / cancel job", "Quit (confirms active or queued jobs)"};
    int available = height - 7;
    for (int i = 0; i < 16 && i < available; ++i) {
        text(w, i + 4, 3, 18, keys[i], color(a, ACCENT));
        text(w, i + 4, 23, width - 26, desc[i], color(a, INK));
    }
    text(w, height - 2, 3, width - 6, "? / Esc / Enter  close  ·  Full key reference in README", color(a, MUTED));
    wnoutrefresh(w); delwin(w);
}
static int home_color(App *a, int pair) {
    if (a->no_color) return pair == HOME_SELECTED ? A_REVERSE : 0;
    return color(a, pair);
}
static void home_fill(App *a, WINDOW *w, int y, int x, int height, int width, int pair) {
    wattrset(w, home_color(a, pair));
    for (int row = y; row < y + height && row < getmaxy(w); ++row) mvwhline(w, row, x, ' ', width);
}
static void home_bar(App *a, WINDOW *w, int y, int x, int width, double used) {
    int filled = (int)(used * width);
    int pair = used >= .95 ? HOME_BAD : used >= .85 ? HOME_WARN : HOME_GOOD;
    for (int i = 0; i < width; ++i)
        text(w,y,x+i,1,a->ascii || a->no_color ? (i < filled ? "#" : "-") : (i < filled ? "━" : "─"),home_color(a,i < filled ? pair : HOME_TRACK));
}
static void home_folder(App *a, WINDOW *w, int y, int x, int width, size_t index) {
    bool selected = a->home_focus == 0 && a->home_folder == index;
    int attr = home_color(a,selected ? HOME_ACCENT : HOME_BLUE) | (selected ? A_BOLD : 0);
    int tab = width / 3;
    text(w,y,x,1,a->ascii ? "+" : "╭",attr);
    for(int i=1;i<tab;++i) text(w,y,x+i,1,a->ascii ? "-" : "─",attr);
    text(w,y,x+tab,1,a->ascii ? "+" : "╮",attr);
    text(w,y+1,x,1,a->ascii ? "|" : "│",attr);
    text(w,y+1,x+tab,1,a->ascii ? "+" : "╰",attr);
    for(int i=tab+1;i<width-1;++i) text(w,y+1,x+i,1,a->ascii ? "-" : "─",attr);
    text(w,y+1,x+width-1,1,a->ascii ? "+" : "╮",attr);
    for(int row=2;row<4;++row) {
        text(w,y+row,x,1,a->ascii ? "|" : "│",attr);
        text(w,y+row,x+width-1,1,a->ascii ? "|" : "│",attr);
    }
    text(w,y+2,x+2,width-4,app_quick_folder(index),attr|A_BOLD);
    text(w,y+3,x+2,width-4,selected ? "> Open" : "Folder",home_color(a,selected ? HOME_ACCENT : HOME_MUTED));
    text(w,y+4,x,1,a->ascii ? "+" : "╰",attr);
    for(int i=1;i<width-1;++i) text(w,y+4,x+i,1,a->ascii ? "-" : "─",attr);
    text(w,y+4,x+width-1,1,a->ascii ? "+" : "╯",attr);
}
static void home_dashboard(App *a) {
    WINDOW *w = a->viewer; if (!w) return;
    int width = getmaxx(w), height = getmaxy(w);
    int nav = width >= 90 && height >= 15 ? 20 : 0, detail = width >= 120 && height >= 20 ? 27 : 0;
    int x = nav + 2, content = width - nav - detail - 4;
    wbkgd(w, home_color(a, HOME_BASE)); werase(w); touchwin(w);
    if (height < 12) {
        text(w,0,2,width-4,"HOME / Tab: folders, files, drives",home_color(a,HOME_ACCENT)|A_BOLD);
        const char *selected=a->home_focus==0 ? app_quick_folder(a->home_folder) : a->home_focus==1 ? (app_selected(a) ? app_selected(a)->name : "No files") : "Drive selected";
        text(w,1,2,width-4,selected,home_color(a,HOME_BASE)|A_BOLD);
        if(a->drives.count) {
            Drive *d=&a->drives.items[a->home_drive]; char total[24],available[24],label[PATH_MAX+80];
            fs_size(total,sizeof(total),(off_t)d->total); fs_size(available,sizeof(available),(off_t)d->available);
            text(w,3,2,width-4,d->path,home_color(a,HOME_ACCENT));
            home_bar(a,w,4,2,width-4,1.0-(double)d->available/(double)d->total);
            snprintf(label,sizeof(label),"%s free of %s",available,total); text(w,5,2,width-4,label,home_color(a,HOME_MUTED));
        }
        wnoutrefresh(w); return;
    }
    if (nav) {
        text(w,1,2,17,"TermNav",home_color(a,HOME_ACCENT)|A_BOLD);
        text(w,3,2,17,"WORKSPACE",home_color(a,HOME_MUTED));
        const char *labels[] = {"w  Home", "o  Open location", "S  Search files", "H  Recent folders", "t  Operations", "T  Trash", "D  Disk explorer"};
        for (int i=0;i<7 && i+5<height;++i) text(w,i+5,2,17,labels[i],home_color(a,i ? HOME_BASE : HOME_ACCENT)|(!i ? A_BOLD : 0));
        text(w,height-3,2,17,":  Commands",home_color(a,HOME_ACCENT));
        text(w,height-2,2,17,"?  Keyboard help",home_color(a,HOME_MUTED));
        for(int row=0;row<height;++row) text(w,row,nav-1,1,a->ascii ? "|" : "│",home_color(a,HOME_MUTED));
    }
    text(w,1,x,content,"Home / Your workspace",home_color(a,HOME_BASE)|A_BOLD);
    char line[PATH_MAX+64]; snprintf(line,sizeof(line),"%zu items   %s   a New folder   / Filter   S Search",a->visible_count,a->sort == 1 ? "Size" : a->sort == 2 ? "Modified" : "Name");
    text(w,2,x,content,line,home_color(a,HOME_MUTED));
    bool cards = height >= 22 && content >= 52;
    text(w,4,x,content,a->home_focus == 0 ? "> QUICK ACCESS" : "QUICK ACCESS",home_color(a,HOME_ACCENT)|A_BOLD);
    int card_width = content/4;
    for (size_t i=0;i<4;++i) {
        int cx = x+(int)i*card_width, cw = card_width-1;
        if (cards) {
            home_folder(a,w,5,cx,cw,i);
        } else text(w,5,cx,cw,app_quick_folder(i),home_color(a,a->home_focus == 0 && a->home_folder == i ? HOME_SELECTED : HOME_BASE)|A_BOLD);
    }
    /* Small terminals prioritize drives; larger ones retain the full table. */
    int table_y = cards ? 11 : 7;
    int drive_rows = (height-table_y-4)/2;
    if (drive_rows < 1) drive_rows = 1;
    if (a->drives.count && (size_t)drive_rows > a->drives.count) drive_rows = (int)a->drives.count;
    int drive_y = height - drive_rows*2 - 1;
    int rows = drive_y-table_y-2;
    if (rows > 0) {
        text(w,table_y,x,content,a->home_focus == 1 ? "> FILES / Enter open   p Paste   s Sort" : "FILES / Enter open   p Paste   s Sort",home_color(a,HOME_ACCENT)|A_BOLD);
        text(w,table_y+1,x,content,"Name",home_color(a,HOME_MUTED));
        if(content>45) text(w,table_y+1,x+content-22,22,"Type       Size",home_color(a,HOME_MUTED));
        if (a->cursor<a->scroll) a->scroll=a->cursor;
        if (a->cursor>=a->scroll+(size_t)rows) a->scroll=a->cursor-(size_t)rows+1;
        for (int i=0;i<rows && a->scroll+(size_t)i<a->visible_count;++i) {
            size_t index=a->scroll+(size_t)i; Entry *e=&a->current.entries[a->visible[index]];
            int y=table_y+2+i, pair=index==a->cursor && a->home_focus==1 ? HOME_SELECTED : e->directory ? HOME_BLUE : HOME_BASE;
            home_fill(a,w,y,x,1,content,pair);
            snprintf(line,sizeof(line),"%s %s %s",e->marked ? "*" : " ",glyph(a,e),e->name);
            text(w,y,x,content>45 ? content-24 : content,line,home_color(a,pair));
            if(content>45) {
                char size[24]; fs_size(size,sizeof(size),e->st.st_size);
                snprintf(line,sizeof(line),"%-10s %s",e->directory ? "Folder" : e->symlink ? "Link" : "File",e->directory ? "--" : size);
                text(w,y,x+content-22,22,line,home_color(a,pair));
            }
        }
        if(!a->visible_count) text(w,table_y+2,x,content,"No files here. a creates a folder.",home_color(a,HOME_MUTED));
    }
    if (drive_y < 7) drive_y = 7;
    text(w,drive_y,x,content,a->home_focus == 2 ? "> THIS PC / Drives" : "THIS PC / Drives",home_color(a,HOME_ACCENT)|A_BOLD);
    size_t start=a->home_drive>=(size_t)drive_rows ? a->home_drive-(size_t)drive_rows+1 : 0;
    for(int i=0;i<drive_rows && start+(size_t)i<a->drives.count;++i) {
        size_t index=start+(size_t)i; Drive *d=&a->drives.items[index]; int y=drive_y+1+i*2;
        double used=d->total ? 1.0-(double)d->available/(double)d->total : 0;
        char total[24],free_space[24]; fs_size(total,sizeof(total),(off_t)d->total); fs_size(free_space,sizeof(free_space),(off_t)d->available);
        snprintf(line,sizeof(line),"%s %s   %.0f%% used",a->home_focus == 2 && a->home_drive == index ? ">" : " ",!strcmp(d->path,"/") ? "Linux /" : d->path,used*100);
        text(w,y,x,content,line,home_color(a,used>=.95 ? HOME_BAD : HOME_BASE)|A_BOLD);
        int bar_width=content>55 ? content/2 : content/4;
        home_bar(a,w,y+1,x,bar_width,used);
        snprintf(line,sizeof(line),"%s free of %s",free_space,total);
        text(w,y+1,x+bar_width+2,content-bar_width-2,line,home_color(a,HOME_MUTED));
    }
    if(!a->drives.count) text(w,drive_y+1,x,content,"No local drive capacity available",home_color(a,HOME_MUTED));
    if(detail) {
        int dx=width-detail;
        for(int row=0;row<height;++row) text(w,row,dx-1,1,a->ascii ? "|" : "│",home_color(a,HOME_MUTED));
        text(w,1,dx+2,detail-4,"DETAILS",home_color(a,HOME_ACCENT)|A_BOLD);
        Entry *e=app_selected(a); char size[24];
        text(w,3,dx+2,detail-4,"Current location",home_color(a,HOME_MUTED));
        text(w,4,dx+2,detail-4,fs_basename(a->current.path),home_color(a,HOME_BASE)|A_BOLD);
        text(w,6,dx+2,detail-4,"Selected file",home_color(a,HOME_MUTED));
        text(w,7,dx+2,detail-4,e ? e->name : "Nothing selected",home_color(a,HOME_BASE)|A_BOLD);
        if(e) {
            fs_size(size,sizeof(size),e->st.st_size);
            text(w,9,dx+2,detail-4,e->directory ? "Folder" : e->symlink ? "Symbolic link" : "File",home_color(a,HOME_MUTED));
            text(w,10,dx+2,detail-4,e->directory ? "Enter to browse" : size,home_color(a,HOME_BASE));
            struct tm modified; localtime_r(&e->st.st_mtime,&modified); strftime(line,sizeof(line),"%d %b %Y, %H:%M",&modified);
            text(w,12,dx+2,detail-4,"Last modified",home_color(a,HOME_MUTED));
            text(w,13,dx+2,detail-4,line,home_color(a,HOME_BASE));
        }
        text(w,height-5,dx+2,detail-4,"STORAGE",home_color(a,HOME_ACCENT)|A_BOLD);
        snprintf(line,sizeof(line),"%zu mounted drives",a->drives.count); text(w,height-3,dx+2,detail-4,line,home_color(a,HOME_BASE));
        text(w,height-2,dx+2,detail-4,"Updates every 5 seconds",home_color(a,HOME_MUTED));
    }
    wnoutrefresh(w);
}
static size_t panel_count(App *a) {
    if (a->panel == PANEL_SEARCH) { pthread_mutex_lock(&a->search.mutex); size_t n=a->search.count; pthread_mutex_unlock(&a->search.mutex); return n; }
    if (a->panel == PANEL_HISTORY) return a->history.count;
    if (a->panel == PANEL_TRASH) return a->trash.count;
    if (a->panel == PANEL_USAGE) { pthread_mutex_lock(&a->usage.mutex); size_t n = a->usage.count; pthread_mutex_unlock(&a->usage.mutex); return n; }
    return (a->job.started ? 1 : 0) + a->queue_count + a->operation_count;
}
static void feature_panel(App *a) {
    WINDOW *w = a->viewer; if (!w) return;
    int width = getmaxx(w), height = getmaxy(w) - 5; char info[512], line[1024];
    size_t count = panel_count(a);
    const char *title = a->panel == PANEL_SEARCH ? "SEARCH RESULTS" : a->panel == PANEL_HISTORY ? "DIRECTORY HISTORY" : a->panel == PANEL_TRASH ? "TRASH / RECOVER YOUR FILES" : a->panel == PANEL_USAGE ? "DISK USAGE EXPLORER" : "OPERATIONS DASHBOARD";
    if (a->panel == PANEL_OPERATIONS) snprintf(info, sizeof(info), "%zu active / %zu queued / %zu recent", a->job.started ? (size_t)1 : 0, a->queue_count, a->operation_count);
    else if (a->panel == PANEL_USAGE) {
        pthread_mutex_lock(&a->usage.mutex);
        char size[32], apparent[32]; fs_size(size, sizeof(size), (off_t)a->usage.total.allocated); fs_size(apparent,sizeof(apparent),(off_t)a->usage.total.bytes);
        snprintf(info, sizeof(info), "%s / %zu of %zu / %s allocated / %s apparent / %llu skipped", atomic_load(&a->usage.done) ? "COMPLETE" : "SCANNING", a->usage.completed, a->usage.count, size, apparent, (unsigned long long)a->usage.total.errors);
        if (atomic_load(&a->usage.done) && a->usage.error) snprintf(info, sizeof(info), "Scan failed: %s", strerror(a->usage.error));
        pthread_mutex_unlock(&a->usage.mutex);
    } else if(a->panel==PANEL_SEARCH) {
        pthread_mutex_lock(&a->search.mutex);
        bool done=atomic_load(&a->search.done);
        snprintf(info,sizeof(info),"%s / %zu matches / %zu skipped%s / %.255s",!done ? "SEARCHING" : atomic_load(&a->search.cancel) ? "STOPPED" : "COMPLETE",count,a->search.skipped,a->search.limited ? " / first 1000 results" : "",a->search.query);
        if(done && a->search.error) snprintf(info,sizeof(info),"Search failed: %s",strerror(a->search.error));
        pthread_mutex_unlock(&a->search.mutex);
    } else snprintf(info, sizeof(info), "%zu %s / Enter %s", count, a->panel == PANEL_HISTORY ? "locations" : "recoverable items", a->panel == PANEL_HISTORY ? "jump" : "restore");
    frame(a, w, title, info, true);
    if (a->panel_cursor >= count) a->panel_cursor = count ? count - 1 : 0;
    if (a->panel_cursor < a->panel_scroll) a->panel_scroll = a->panel_cursor;
    if (a->panel_cursor >= a->panel_scroll + (size_t)height) a->panel_scroll = a->panel_cursor - (size_t)height + 1;
    for (int row = 0; row < height && a->panel_scroll + (size_t)row < count; ++row) {
        size_t index = a->panel_scroll + (size_t)row;
        bool selected = index == a->panel_cursor; int attr = color(a, INK);
        if (a->panel == PANEL_SEARCH) {
            pthread_mutex_lock(&a->search.mutex);
            SearchItem *item=&a->search.items[index];
            const char *relative=item->path+strlen(a->search.path); if(*relative=='/') ++relative;
            snprintf(line,sizeof(line),"%s %.990s",item->directory ? "dir/" : "file",relative);
            attr=color(a,item->directory ? BLUE : INK);
            pthread_mutex_unlock(&a->search.mutex);
        } else if (a->panel == PANEL_HISTORY) {
            snprintf(line, sizeof(line), "%c %2zu  %.900s", index == a->history.position ? '*' : ' ', index + 1, a->history.entries[index].path);
        } else if (a->panel == PANEL_TRASH) {
            time_t when = (time_t)(a->trash.entries[index].when / 1000000000u); struct tm tm; localtime_r(&when, &tm); char date[32]; strftime(date, sizeof(date), "%b %d %H:%M", &tm);
            snprintf(line, sizeof(line), "%s  %.900s", date, a->trash.entries[index].original); attr = color(a, ACCENT);
        } else if (a->panel == PANEL_USAGE) {
            pthread_mutex_lock(&a->usage.mutex); UsageItem item = a->usage.items[index]; uint64_t total = a->usage.total.allocated; pthread_mutex_unlock(&a->usage.mutex);
            char size[32], bar[15]; fs_size(size, sizeof(size), (off_t)item.usage.allocated);
            double fraction = total ? (double)item.usage.allocated / (double)total : 0;
            int filled = (int)(fraction * 12); if (filled > 12) filled = 12;
            for (int j = 0; j < 12; ++j) bar[j] = j < filled ? '#' : '.';
            bar[12] = 0;
            snprintf(line, sizeof(line), "%12s  %5.1f%%  [%s]  %s%s%s", item.complete ? size : "scanning", fraction * 100, bar, item.name, item.directory ? "/" : "", item.usage.errors ? "  !" : "");
            attr = color(a, item.directory ? BLUE : INK);
        } else {
            size_t active = a->job.started ? 1 : 0;
            if (active && !index) {
                uint64_t total = a->job.kind == JOB_COPY ? atomic_load(&a->job.total_bytes) : atomic_load(&a->job.total_files);
                uint64_t done = a->job.kind == JOB_COPY ? atomic_load(&a->job.bytes) : atomic_load(&a->job.files);
                double percent = total ? (double)done * 100 / (double)total : 0; if (percent > 100) percent = 100;
                snprintf(line, sizeof(line), "#%-3u %-8s %-10s %5.1f%%  %s", a->active_op, job_name(a->job.kind), atomic_load(&a->job.paused) ? "PAUSED" : atomic_load(&a->job.planning) ? "PREPARING" : "RUNNING", percent, a->active_label); attr = color(a, ACCENT);
            } else if (index - active < a->queue_count) {
                PendingOp *op = &a->queue[index - active]; snprintf(line, sizeof(line), "#%-3u %-8s QUEUED      %zu items  %s", op->id, job_name(op->kind), op->count, op->label); attr = color(a, MUTED);
            } else {
                size_t completed = index - active - a->queue_count; OperationLog *op = &a->operations[a->operation_count - completed - 1];
                snprintf(line, sizeof(line), "#%-3u %-8s %-10s %.1fs  %s", op->id, job_name(op->kind), op->error == ECANCELED ? "CANCELLED" : op->error ? "FAILED" : "COMPLETE", op->seconds, op->label); attr = color(a, op->error ? RED : GREEN);
            }
        }
        if (selected) { attr = a->no_color ? (int)A_REVERSE : color(a, SELECTED) | (int)A_BOLD; wattrset(w, attr); mvwhline(w, row + 4, 1, ' ', width - 2); }
        text(w, row + 4, 2, width - 4, line, attr);
    }
    if (!count) text(w, 5, 3, width - 6, a->panel == PANEL_SEARCH ? "No matches yet. S changes the search; Esc closes." : a->panel == PANEL_TRASH ? "Your trash is empty." : a->panel == PANEL_OPERATIONS ? "No operations yet. Copy or trash a file to begin." : "No entries to show.", color(a, MUTED));
    wnoutrefresh(w);
}
static void palette(App *a) {
    int width = COLS > 94 ? 90 : COLS - 4, height = LINES > 24 ? 22 : LINES - 2;
    if (width < 30 || height < 10) return;
    WINDOW *w = newwin(height, width, (LINES - height) / 2, (COLS - width) / 2); if (!w) return;
    frame(a, w, "COMMAND PALETTE", "Type to find an action", true);
    text(w, 3, 2, width - 4, a->input[0] ? a->input : ":", color(a, ACCENT) | A_BOLD);
    size_t matches[64], count = command_matches(a->input, matches, 64);
    if (a->palette_cursor >= count) a->palette_cursor = count ? count - 1 : 0;
    int available = height - 8; size_t start = a->palette_cursor >= (size_t)available ? a->palette_cursor - (size_t)available + 1 : 0;
    for (int row = 0; row < available && start + (size_t)row < count; ++row) {
        const Command *command = &commands[matches[start + (size_t)row]]; bool selected = start + (size_t)row == a->palette_cursor;
        int attr = selected ? a->no_color ? (int)A_REVERSE : color(a, SELECTED) | (int)A_BOLD : color(a, INK);
        if (selected) { wattrset(w, attr); mvwhline(w, row + 5, 1, ' ', width - 2); }
        text(w, row + 5, 3, width - 10, command->name, attr);
        char key[8]; if (command->key == '\t') strcpy(key, "Tab"); else snprintf(key, sizeof(key), "%c", command->key);
        text(w, row + 5, width - 6, 4, key, attr);
    }
    if (count) text(w, height - 3, 2, width - 4, commands[matches[a->palette_cursor]].description, color(a, MUTED));
    else text(w, 5, 3, width - 6, "No matching actions", color(a, MUTED));
    text(w, height - 2, 2, width - 4, "Up/Down choose   Enter run   Esc close", color(a, ACCENT)); wnoutrefresh(w); delwin(w);
}
static void bottom(App *a) {
    int row = LINES - 4; Entry *e = app_selected(a); char buffer[1024];
    if (row < 0) return;
    if (e && a->panel == PANEL_NONE) {
        char perms[11], size[24], date[40]; fs_permissions(perms, e->st.st_mode); fs_size(size, sizeof(size), e->st.st_size);
        struct tm tm; localtime_r(&e->st.st_mtime, &tm); strftime(date, sizeof(date), "%b %d %Y  %H:%M", &tm);
        snprintf(buffer, sizeof(buffer), "%s  %s  %s", perms, size, date);
        text(stdscr, row, 2, COLS - 4, buffer, color(a, MUTED));
    }
    size_t marks = 0; for (size_t i = 0; i < a->current.count; ++i) if (a->current.entries[i].marked) ++marks;
    if (a->job.started) {
        char size[24], speed[24]; uint64_t bytes=atomic_load(&a->job.bytes); double seconds=app_job_seconds(a);
        fs_size(size, sizeof(size), (off_t)bytes); fs_size(speed,sizeof(speed),(off_t)(seconds>0 ? bytes/seconds : 0));
        snprintf(buffer, sizeof(buffer), "%c %s%s  %llu items  %s  %s/s  %.1fs / %zu queued / t dashboard", "|/-\\"[(a->ticks / 2) % 4], job_name(a->job.kind), atomic_load(&a->job.paused) ? " PAUSED" : "", (unsigned long long)atomic_load(&a->job.files), size, speed, seconds, a->queue_count);
        text(stdscr, row + 1, 2, COLS - 4, buffer, color(a, ACCENT));
    } else if (!a->preview_full && a->message_until >= time(NULL)) text(stdscr, row + 1, 2, COLS - 4, a->message, color(a, a->error ? RED : ACCENT));
    else if (!a->preview_full) {
        snprintf(buffer, sizeof(buffer), "%zu marked  /  %zu clipboard%s%s", marks, a->clipboard_count, a->query[0] ? "  /  filter: " : "", a->query);
        text(stdscr, row + 1, 2, COLS - 4, buffer, color(a, MUTED));
    }
    if (a->panel == PANEL_HOME && a->mode == NORMAL) {
        text(stdscr,row+2,2,COLS-4,"S search   Tab areas   Enter/Right open   Backspace parent   w browser   F1 Home",color(a,MUTED));
        text(stdscr,row+3,1,8," HOME ",color(a,BAR)|A_BOLD);
    } else if (a->panel != PANEL_NONE && a->panel != PANEL_HOME && a->mode == NORMAL) {
        if (a->panel == PANEL_OPERATIONS) {
            size_t offset=(a->job.started ? 1 : 0)+a->queue_count;
            if (a->panel_cursor>=offset && a->panel_cursor-offset<a->operation_count) {
                OperationLog *op=&a->operations[a->operation_count-1-(a->panel_cursor-offset)];
                if (op->error) { snprintf(buffer,sizeof(buffer),"%s: %s / %s",job_name(op->kind),strerror(op->error),op->failed); text(stdscr,row,2,COLS-4,buffer,color(a,RED)); }
            }
        }
        const char *hint = a->panel == PANEL_SEARCH ? "Enter open   o reveal   j k choose   S new search   R rescan   x stop   Esc close" : a->panel == PANEL_OPERATIONS ? "Space pause/resume   x cancel selected job   j k choose   Esc close" : a->panel == PANEL_USAGE ? "Enter drill down   h parent   o reveal   R rescan   Esc close" : a->panel == PANEL_TRASH ? "Enter restore selected   u undo latest batch   R refresh   Esc close" : "Enter jump   j k choose   Esc close";
        text(stdscr, row + 2, 2, COLS - 4, hint, color(a, MUTED));
        text(stdscr, row + 3, 1, 12, " EXPLORER ", color(a, BAR) | A_BOLD);
    } else if (a->preview_full && a->mode != PALETTE_INPUT) {
        if (!a->job.started) {
            if (a->preview.kind==PREVIEW_IMAGE) snprintf(buffer,sizeof(buffer),"IMAGE / %u x %u / %s",a->preview.original_width,a->preview.original_height,a->preview.image_sixel ? "native Sixel raster" : "half-block fallback");
            else snprintf(buffer, sizeof(buffer), "%s  /  %zu %s%s  /  column %zu", preview_format(&a->preview), preview_rows(&a->preview), a->preview.kind == PREVIEW_TEXT ? "lines" : a->preview.kind == PREVIEW_DIRECTORY ? "entries" : "rows", a->preview.truncated ? "  /  first 64 KiB" : "", a->preview_column + 1);
            text(stdscr, row + 1, 2, COLS - 4, buffer, color(a, MUTED));
        }
        text(stdscr, row + 2, 2, COLS - 4, a->preview.kind==PREVIEW_IMAGE ? "Esc/Tab close   R refresh   : commands" : "Esc/Tab close   j k scroll   h l pan   PgUp PgDn page   gg G ends   R refresh", color(a, MUTED));
        text(stdscr, row + 3, 1, 10, " PREVIEW ", color(a, BAR) | A_BOLD);
        size_t rows = preview_rows(&a->preview);
        if (a->preview.kind==PREVIEW_IMAGE) snprintf(buffer,sizeof(buffer),"%s",a->preview.image_sixel ? "RASTER" : "THUMBNAIL");
        else snprintf(buffer, sizeof(buffer), "%zu / %zu", rows ? a->preview_scroll + 1 : 0, rows);
        text(stdscr, row + 3, COLS - (int)strlen(buffer) - 2, (int)strlen(buffer), buffer, color(a, MUTED));
    } else if (a->mode != NORMAL) {
        const char *label = a->mode == SEARCH_INPUT ? " SEARCH SUBFOLDERS " : a->mode == FILTER ? " FILTER / " : a->mode == MKDIR_INPUT ? " NEW DIRECTORY " : a->mode == RENAME_INPUT ? " RENAME " : a->mode == COPY_INPUT ? " COPY TO DIR " : a->mode == GOTO_INPUT ? " GO TO " : a->mode == QUIT_INPUT ? " QUIT JOB? type q " : a->mode == TRASH_INPUT ? " TRASH? type trash " : a->mode == PALETTE_INPUT ? " COMMAND " : " DELETE? type delete ";
        int len = (int)strlen(label); text(stdscr, row + 2, 1, len, label, color(a, BAR) | A_BOLD);
        text(stdscr, row + 2, len + 2, COLS - len - 4, a->mode == FILTER ? a->query : a->input, color(a, INK) | A_BOLD);
        if (a->mode == DELETE_INPUT || a->mode == TRASH_INPUT) {
            snprintf(buffer, sizeof(buffer), "%s %s%zu %s. Enter confirms; Esc cancels.", a->mode == TRASH_INPUT ? "Move to trash" : "Permanently delete", marks ? "marked " : "", marks ? marks : (e ? (size_t)1 : 0), marks == 1 || !marks ? "item" : "items");
            text(stdscr, row + 1, 2, COLS - 4, buffer, color(a, RED));
        }
        text(stdscr, row + 3, 2, COLS - 4, "Enter accept   Esc cancel   Ctrl-u clear", color(a, MUTED));
    } else {
        text(stdscr, row + 2, 2, COLS - 4, "F1 Home   S search   / filter   Tab preview   : commands   ? help", color(a, MUTED));
        snprintf(buffer, sizeof(buffer), " %s ", a->query[0] ? "FILTERED" : "NORMAL");
        text(stdscr, row + 3, 1, (int)strlen(buffer), buffer, color(a, BAR) | A_BOLD);
        snprintf(buffer, sizeof(buffer), "%zu / %zu", a->visible_count ? a->cursor + 1 : 0, a->visible_count);
        text(stdscr, row + 3, COLS - (int)strlen(buffer) - 2, (int)strlen(buffer), buffer, color(a, MUTED));
    }
}
void ui_render(App *a) {
    bool changed=a->layout_rows != LINES || a->layout_cols != COLS || a->layout_preview != a->preview_full || a->layout_panel != a->panel;
    bool native=a->preview.kind==PREVIEW_IMAGE && a->preview.image_sixel && a->panel==PANEL_NONE && !a->help && a->mode==NORMAL && LINES>=14 && COLS>=38;
    if (a->native_image_visible && (!native || changed || a->image_shown_revision!=a->image_revision)) { clearok(stdscr,TRUE); a->native_image_visible=false; }
    if (changed) { layout(a); app_preview(a,true); }
    erase();
    if (LINES < 14 || COLS < 38 || (!a->center && !a->viewer)) {
        text(stdscr, 0, 0, COLS, "TermNav · enlarge terminal", color(a, ACCENT));
        if (LINES > 2) text(stdscr, 2, 0, COLS, a->preview_full ? "Enlarge or Esc to close preview." : "Minimum 38 x 14. q to quit.", color(a, MUTED));
        refresh(); return;
    }
    text(stdscr, 0, 2, 16, "TERM / NAV", color(a, ACCENT) | A_BOLD);
    text(stdscr, 0, 16, COLS - 35, "a quiet place for your files", color(a, MUTED));
    text(stdscr, 0, COLS - 16, 14, a->hidden ? "HIDDEN  ON" : "HIDDEN  OFF", color(a, MUTED));
    const char *home = getenv("HOME"); char path[PATH_MAX + 8];
    const char *shown_path = a->panel == PANEL_USAGE ? a->usage.path : a->preview_full ? a->preview.path : a->current.path;
    if (home && !strncmp(shown_path, home, strlen(home)) && (!shown_path[strlen(home)] || shown_path[strlen(home)] == '/')) snprintf(path, sizeof(path), "~%s", shown_path + strlen(home));
    else snprintf(path, sizeof(path), "%s", shown_path);
    if (strlen(path) > (size_t)(COLS - 5)) { size_t offset = strlen(path) - (size_t)(COLS - 8); while (((unsigned char)path[offset] & 0xc0) == 0x80) ++offset; text(stdscr, 2, 2, 3, "...", color(a, ACCENT)); text(stdscr, 2, 5, COLS - 7, path + offset, color(a, INK)); }
    else text(stdscr, 2, 2, COLS - 4, path, color(a, INK) | A_BOLD);
    bottom(a); wnoutrefresh(stdscr);
    if (a->panel == PANEL_HOME) home_dashboard(a);
    else if (a->panel != PANEL_NONE) feature_panel(a);
    else if (a->preview_full) draw_preview(a, a->viewer, true);
    else { parent_pane(a); current_pane(a); draw_preview(a, a->right, false); }
    if (a->help) help(a);
    if (a->mode == PALETTE_INPUT) palette(a);
    doupdate();
    WINDOW *image_window=a->preview_full ? a->viewer : a->right;
    if (image_window && a->preview.kind==PREVIEW_IMAGE && a->preview.image_sixel && a->panel==PANEL_NONE && !a->help && a->mode==NORMAL && !a->native_image_visible) {
        int cells=(int)((a->preview.image_width+a->cell_width-1)/a->cell_width);
        int column=getbegx(image_window)+(getmaxx(image_window)-cells)/2;
        fprintf(stdout,"\0337\033[%d;%dH",getbegy(image_window)+6,column+1);
        fwrite(a->preview.data,1,a->preview.bytes,stdout); fputs("\0338",stdout); fflush(stdout);
        a->native_image_visible=true; a->image_shown_revision=a->image_revision;
    }
}
