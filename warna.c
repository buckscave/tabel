/* ============================================================
 * TABEL v3.0
 * Berkas: warna.c - Modul Pengelolaan Warna Terpisah
 * Standar: C89 + POSIX.1-2008
 * Penulis: Chandra Lesmana
 * ============================================================
 * 
 * Modul ini menyediakan pengelolaan warna yang terpisah antara
 * warna depan (foreground) dan warna latar (background).
 * Fungsi-fungsi menggambar tidak lagi menerima parameter warna,
 * melainkan menggunakan state warna global yang diatur melalui
 * fungsi-fungsi di modul ini.
 * ============================================================ */

#include "tabel.h"

/* ============================================================
 * DEFINISI KONSTANTA WARNA ANSI
 * ============================================================ */

/* Escape sequence untuk reset */
static const char *ESC_RESET = "\033[0m";

/* Warna depan (foreground) - teks */
static const char *FG_SEQ[] = {
    "\033[39m",          /* WARNAD_DEFAULT - default terminal */
    "\033[38;2;30;40;50m",  /* WARNAD_GELAP */
    "\033[38;2;80;90;100m", /* WARNAD_ABU */
    "\033[38;2;255;255;255m", /* WARNAD_PUTIH */
    "\033[38;2;100;200;100m", /* WARNAD_HIJAU */
    "\033[38;2;255;185;75m",  /* WARNAD_KUNING */
    "\033[38;2;50;185;185m",  /* WARNAD_BIRU */
    "\033[38;2;255;125;125m"  /* WARNAD_MERAH */
};

/* Warna latar (background) */
static const char *BG_SEQ[] = {
    "\033[49m",          /* WARNAL_DEFAULT - default terminal */
    "\033[48;2;5;10;15m",    /* WARNAL_HITAM */
    "\033[48;2;30;40;50m",   /* WARNAL_GELAP */
    "\033[48;2;80;90;100m",  /* WARNAL_ABU */
    "\033[48;2;100;200;100m", /* WARNAL_HIJAU */
    "\033[48;2;40;80;40m",    /* WARNAL_HIJAU_GELAP */
    "\033[48;2;20;40;40m",    /* WARNAL_HIJAU_TUA */
    "\033[48;2;255;185;75m",  /* WARNAL_KUNING */
    "\033[48;2;50;185;185m",  /* WARNAL_BIRU */
    "\033[48;2;255;125;125m"  /* WARNAL_MERAH */
};

/* Nama warna depan untuk tampilan */
static const char *NAMA_WARNAD[] = {
    "Default", "Gelap", "Abu", "Putih", "Hijau", "Kuning", "Biru", "Merah"
};

/* Nama warna latar untuk tampilan */
static const char *NAMA_WARNAL[] = {
    "Default", "Hitam", "Gelap", "Abu", "Hijau", "HijauGelap", 
    "HijauTua", "Kuning", "Biru", "Merah"
};

/* ============================================================
 * STATE WARNA GLOBAL
 * ============================================================ */

/* State warna saat ini untuk rendering */
static warna_depan g_warna_depan_saat_ini = WARNAD_DEFAULT;
static warna_latar g_warna_latar_saat_ini = WARNAL_DEFAULT;

/* State warna untuk buffer rendering */
static warna_depan g_warna_depan_buffer = WARNAD_DEFAULT;
static warna_latar g_warna_latar_buffer = WARNAL_DEFAULT;

/* State style untuk buffer rendering */
static font_style g_style_buffer = STYLE_NORMAL;

/* ============================================================
 * FUNGSI SET/GET WARNA DEPAN
 * ============================================================ */

/* Set warna depan aktif */
void warna_set_depan(warna_depan wd)
{
    if (wd >= WARNAD_DEFAULT && wd <= WARNAD_MERAH) {
        g_warna_depan_saat_ini = wd;
    }
}

/* Get warna depan aktif */
warna_depan warna_get_depan(void)
{
    return g_warna_depan_saat_ini;
}

/* Set warna depan untuk buffer */
void warna_set_depan_buffer(warna_depan wd)
{
    if (wd >= WARNAD_DEFAULT && wd <= WARNAD_MERAH) {
        g_warna_depan_buffer = wd;
    }
}

/* Get warna depan buffer */
warna_depan warna_get_depan_buffer(void)
{
    return g_warna_depan_buffer;
}

/* ============================================================
 * FUNGSI SET/GET WARNA LATAR
 * ============================================================ */

/* Set warna latar aktif */
void warna_set_latar(warna_latar wl)
{
    if (wl >= WARNAL_DEFAULT && wl <= WARNAL_MERAH) {
        g_warna_latar_saat_ini = wl;
    }
}

/* Get warna latar aktif */
warna_latar warna_get_latar(void)
{
    return g_warna_latar_saat_ini;
}

/* Set warna latar untuk buffer */
void warna_set_latar_buffer(warna_latar wl)
{
    if (wl >= WARNAL_DEFAULT && wl <= WARNAL_MERAH) {
        g_warna_latar_buffer = wl;
    }
}

/* Get warna latar buffer */
warna_latar warna_get_latar_buffer(void)
{
    return g_warna_latar_buffer;
}

/* ============================================================
 * FUNGSI SET/GET KOMBINASI
 * ============================================================ */

/* Set warna depan dan latar sekaligus */
void warna_set(warna_depan wd, warna_latar wl)
{
    warna_set_depan(wd);
    warna_set_latar(wl);
}

/* Set warna depan dan latar untuk buffer */
void warna_set_buffer(warna_depan wd, warna_latar wl)
{
    warna_set_depan_buffer(wd);
    warna_set_latar_buffer(wl);
}

/* Reset semua warna ke default */
void warna_reset(void)
{
    g_warna_depan_saat_ini = WARNAD_DEFAULT;
    g_warna_latar_saat_ini = WARNAL_DEFAULT;
}

/* Reset warna buffer ke default */
void warna_reset_buffer(void)
{
    g_warna_depan_buffer = WARNAD_DEFAULT;
    g_warna_latar_buffer = WARNAL_DEFAULT;
    g_style_buffer = STYLE_NORMAL;
}

/* ============================================================
 * FUNGSI SET/GET FONT STYLE
 * ============================================================ */

/* Set font style untuk buffer */
void warna_set_style_buffer(font_style style)
{
    g_style_buffer = style;
}

/* Get font style buffer */
font_style warna_get_style_buffer(void)
{
    return g_style_buffer;
}

/* Reset font style buffer ke normal */
void warna_reset_style_buffer(void)
{
    g_style_buffer = STYLE_NORMAL;
}

/* Dapatkan sequence ANSI untuk font style */
int warna_seq_style(font_style style, char *buf, size_t ukuran)
{
    int len = 0;
    
    if (!buf || ukuran < 16) return 0;
    
    /* Reset dulu jika style normal */
    if (style == STYLE_NORMAL) {
        len = snprintf(buf, ukuran, "\033[0m");
        return len;
    }
    
    /* Build style sequence */
    buf[0] = '\0';
    len = 0;
    
    /* Mulai dengan \033[ */
    strcpy(buf, "\033[");
    len = 2;
    
    /* Tambahkan kode style */
    if (style & STYLE_BOLD) {
        if (len > 2) { buf[len++] = ';'; }
        buf[len++] = '1';
    }
    if (style & STYLE_ITALIC) {
        if (len > 2) { buf[len++] = ';'; }
        buf[len++] = '3';
    }
    if (style & STYLE_UNDERLINE) {
        if (len > 2) { buf[len++] = ';'; }
        buf[len++] = '4';
    }
    if (style & STYLE_STRIKETHROUGH) {
        if (len > 2) { buf[len++] = ';'; }
        buf[len++] = '9';
    }
    
    /* Tutup dengan 'm' */
    buf[len++] = 'm';
    buf[len] = '\0';
    
    return len;
}

/* ============================================================
 * FUNGSI OUTPUT ANSI SEQUENCE
 * ============================================================ */

/* Dapatkan sequence ANSI untuk warna depan */
const char *warna_seq_depan(warna_depan wd)
{
    if (wd < WARNAD_DEFAULT || wd > WARNAD_MERAH) {
        wd = WARNAD_DEFAULT;
    }
    return FG_SEQ[wd];
}

/* Dapatkan sequence ANSI untuk warna latar */
const char *warna_seq_latar(warna_latar wl)
{
    if (wl < WARNAL_DEFAULT || wl > WARNAL_MERAH) {
        wl = WARNAL_DEFAULT;
    }
    return BG_SEQ[wl];
}

/* Dapatkan sequence ANSI lengkap (fg + bg) */
int warna_seq_gabung(warna_depan wd, warna_latar wl, char *buf, size_t ukuran)
{
    const char *fg_seq;
    const char *bg_seq;
    size_t fg_len;
    size_t bg_len;
    
    if (!buf || ukuran < 2) {
        return -1;
    }
    
    fg_seq = warna_seq_depan(wd);
    bg_seq = warna_seq_latar(wl);
    fg_len = strlen(fg_seq);
    bg_len = strlen(bg_seq);
    
    if (fg_len + bg_len + 1 > ukuran) {
        return -1;
    }
    
    memcpy(buf, fg_seq, fg_len);
    memcpy(buf + fg_len, bg_seq, bg_len + 1);
    
    return (int)(fg_len + bg_len);
}

/* Dapatkan sequence ANSI lengkap dengan style (style + fg + bg dalam satu sequence) */
int warna_seq_gabung_style(warna_depan wd, warna_latar wl, font_style style, 
                           char *buf, size_t ukuran)
{
    const char *fg_seq;
    const char *bg_seq;
    size_t fg_len;
    size_t bg_len;
    size_t total_len;
    char *p;
    int first = 1;
    
    if (!buf || ukuran < 64) {
        return -1;
    }
    
    fg_seq = warna_seq_depan(wd);
    bg_seq = warna_seq_latar(wl);
    fg_len = strlen(fg_seq);
    bg_len = strlen(bg_seq);
    
    /* Build combined sequence: \033[<style>;<fg_codes>;<bg_codes>m */
    /* Mulai dengan \033[ */
    buf[0] = '\033';
    buf[1] = '[';
    p = buf + 2;
    total_len = 2;
    
    /* Jika style NORMAL, reset semua style attributes */
    if (style == STYLE_NORMAL) {
        /* Reset codes: 22=bold off, 23=italic off, 24=underline off, 29=strikethrough off */
        strcpy(p, "22;23;24;29");
        p += 11;
        total_len += 11;
        first = 0;
    } else {
        /* Tambahkan style codes (SGR codes) */
        if (style & STYLE_BOLD) {
            if (!first) *p++ = ';';
            *p++ = '1';
            total_len += (first ? 1 : 2);
            first = 0;
        }
        if (style & STYLE_ITALIC) {
            if (!first) *p++ = ';';
            *p++ = '3';
            total_len += (first ? 1 : 2);
            first = 0;
        }
        if (style & STYLE_UNDERLINE) {
            if (!first) *p++ = ';';
            *p++ = '4';
            total_len += (first ? 1 : 2);
            first = 0;
        }
        if (style & STYLE_STRIKETHROUGH) {
            if (!first) *p++ = ';';
            *p++ = '9';
            total_len += (first ? 1 : 2);
            first = 0;
        }
    }
    
    /* Ekstrak dan tambahkan fg codes */
    /* fg_seq format: \033[XXm atau \033[38;2;R;G;Bm */
    /* Kita perlu mengambil bagian antara '[' dan 'm' */
    if (fg_len > 4) {
        /* Cari posisi 'm' dari belakang */
        size_t code_start = 2; /* skip \033[ */
        size_t code_end = fg_len - 1; /* posisi 'm' */
        size_t code_len = code_end - code_start;
        
        if (!first) *p++ = ';';
        memcpy(p, fg_seq + code_start, code_len);
        p += code_len;
        total_len += code_len + (first ? 0 : 1);
        first = 0;
    }
    
    /* Ekstrak dan tambahkan bg codes */
    if (bg_len > 4) {
        size_t code_start = 2;
        size_t code_end = bg_len - 1;
        size_t code_len = code_end - code_start;
        
        if (!first) *p++ = ';';
        memcpy(p, bg_seq + code_start, code_len);
        p += code_len;
        total_len += code_len + (first ? 0 : 1);
        first = 0;
    }
    
    /* Tutup dengan 'm' */
    *p++ = 'm';
    *p = '\0';
    total_len++;
    
    return (int)total_len;
}

/* Tulis sequence ANSI ke file descriptor */
int warna_tulis_seq(int fd, warna_depan wd, warna_latar wl)
{
    const char *fg_seq;
    const char *bg_seq;
    int ret = 0;
    
    fg_seq = warna_seq_depan(wd);
    bg_seq = warna_seq_latar(wl);
    
    ret += (int)write(fd, fg_seq, strlen(fg_seq));
    ret += (int)write(fd, bg_seq, strlen(bg_seq));
    
    return ret;
}

/* Tulis sequence ANSI untuk warna saat ini */
int warna_tulis_seq_saat_ini(int fd)
{
    return warna_tulis_seq(fd, g_warna_depan_saat_ini, g_warna_latar_saat_ini);
}

/* Tulis reset sequence */
int warna_tulis_reset(int fd)
{
    return (int)write(fd, ESC_RESET, strlen(ESC_RESET));
}

/* ============================================================
 * FUNGSI NAMA WARNA
 * ============================================================ */

/* Dapatkan nama warna depan */
const char *warna_nama_depan(warna_depan wd)
{
    if (wd < WARNAD_DEFAULT || wd > WARNAD_MERAH) {
        wd = WARNAD_DEFAULT;
    }
    return NAMA_WARNAD[wd];
}

/* Dapatkan nama warna latar */
const char *warna_nama_latar(warna_latar wl)
{
    if (wl < WARNAL_DEFAULT || wl > WARNAL_MERAH) {
        wl = WARNAL_DEFAULT;
    }
    return NAMA_WARNAL[wl];
}

/* ============================================================
 * FUNGSI UNTUK PROMPT WARNA
 * ============================================================ */

/* Jumlah pilihan warna depan */
int warna_jumlah_depan(void)
{
    return WARNAD_JUMLAH;
}

/* Jumlah pilihan warna latar */
int warna_jumlah_latar(void)
{
    return WARNAL_JUMLAH;
}

/* Dapatkan info warna depan untuk prompt */
int warna_info_depan(int indeks, warna_depan *wd, const char **nama)
{
    if (indeks < 0 || indeks >= WARNAD_JUMLAH) {
        return -1;
    }
    if (wd) *wd = (warna_depan)indeks;
    if (nama) *nama = NAMA_WARNAD[indeks];
    return 0;
}

/* Dapatkan info warna latar untuk prompt */
int warna_info_latar(int indeks, warna_latar *wl, const char **nama)
{
    if (indeks < 0 || indeks >= WARNAL_JUMLAH) {
        return -1;
    }
    if (wl) *wl = (warna_latar)indeks;
    if (nama) *nama = NAMA_WARNAL[indeks];
    return 0;
}

/* ============================================================
 * FUNGSI KONVERSI UNTUK EKSPOR (PDF, DLL)
 * ============================================================ */

/* Konversi warna depan ke RGB (0.0 - 1.0) */
int warna_depan_ke_rgb(warna_depan wd, double *r, double *g, double *b)
{
    if (!r || !g || !b) return -1;
    
    switch (wd) {
        case WARNAD_DEFAULT:
            *r = 0.9; *g = 0.9; *b = 0.9;
            break;
        case WARNAD_GELAP:
            *r = 0.12; *g = 0.16; *b = 0.2;
            break;
        case WARNAD_ABU:
            *r = 0.31; *g = 0.35; *b = 0.39;
            break;
        case WARNAD_PUTIH:
            *r = 1.0; *g = 1.0; *b = 1.0;
            break;
        case WARNAD_HIJAU:
            *r = 0.39; *g = 0.78; *b = 0.39;
            break;
        case WARNAD_KUNING:
            *r = 1.0; *g = 0.73; *b = 0.29;
            break;
        case WARNAD_BIRU:
            *r = 0.2; *g = 0.73; *b = 0.73;
            break;
        case WARNAD_MERAH:
            *r = 1.0; *g = 0.49; *b = 0.49;
            break;
        default:
            *r = 0.9; *g = 0.9; *b = 0.9;
            break;
    }
    return 0;
}

/* Konversi warna latar ke RGB (0.0 - 1.0) */
int warna_latar_ke_rgb(warna_latar wl, double *r, double *g, double *b)
{
    if (!r || !g || !b) return -1;
    
    switch (wl) {
        case WARNAL_DEFAULT:
            *r = 0.02; *g = 0.04; *b = 0.06;
            break;
        case WARNAL_HITAM:
            *r = 0.02; *g = 0.04; *b = 0.06;
            break;
        case WARNAL_GELAP:
            *r = 0.12; *g = 0.16; *b = 0.2;
            break;
        case WARNAL_ABU:
            *r = 0.31; *g = 0.35; *b = 0.39;
            break;
        case WARNAL_HIJAU:
            *r = 0.39; *g = 0.78; *b = 0.39;
            break;
        case WARNAL_HIJAU_GELAP:
            *r = 0.16; *g = 0.31; *b = 0.16;
            break;
        case WARNAL_HIJAU_TUA:
            *r = 0.08; *g = 0.16; *b = 0.16;
            break;
        case WARNAL_KUNING:
            *r = 1.0; *g = 0.73; *b = 0.29;
            break;
        case WARNAL_BIRU:
            *r = 0.2; *g = 0.73; *b = 0.73;
            break;
        case WARNAL_MERAH:
            *r = 1.0; *g = 0.49; *b = 0.49;
            break;
        default:
            *r = 0.02; *g = 0.04; *b = 0.06;
            break;
    }
    return 0;
}

/* ============================================================
 * FUNGSI SERIALIZASI UNTUK FORMAT BINER
 * ============================================================ */

/* Encode warna depan dan latar ke byte tunggal */
unsigned char warna_encode(warna_depan wd, warna_latar wl)
{
    /* Format: 4 bit fg | 4 bit bg */
    unsigned char fg_bits;
    unsigned char bg_bits;
    
    fg_bits = (wd >= 0 && wd < 16) ? (unsigned char)wd : 0;
    bg_bits = (wl >= 0 && wl < 16) ? (unsigned char)wl : 0;
    
    return (fg_bits << 4) | bg_bits;
}

/* Decode byte tunggal ke warna depan dan latar */
int warna_decode(unsigned char encoded, warna_depan *wd, warna_latar *wl)
{
    if (wd) *wd = (warna_depan)((encoded >> 4) & 0x0F);
    if (wl) *wl = (warna_latar)(encoded & 0x0F);
    return 0;
}

/* ============================================================
 * FUNGSI VALIDASI
 * ============================================================ */

/* Validasi warna depan */
int warna_valid_depan(warna_depan wd)
{
    return (wd >= WARNAD_DEFAULT && wd < WARNAD_JUMLAH);
}

/* Validasi warna latar */
int warna_valid_latar(warna_latar wl)
{
    return (wl >= WARNAL_DEFAULT && wl < WARNAL_JUMLAH);
}

/* ============================================================
 * FUNGSI UTILITAS
 * ============================================================ */

/* Bandingkan dua kombinasi warna */
int warna_sama(warna_depan wd1, warna_latar wl1, warna_depan wd2, warna_latar wl2)
{
    return (wd1 == wd2 && wl1 == wl2);
}

/* Copy warna dari satu lokasi ke lokasi lain */
void warna_copy(struct warna_kombinasi *dst, const struct warna_kombinasi *src)
{
    if (dst && src) {
        dst->depan = src->depan;
        dst->latar = src->latar;
    }
}

/* Inisialisasi struktur warna_kombinasi */
void warna_init(struct warna_kombinasi *wc)
{
    if (wc) {
        wc->depan = WARNAD_DEFAULT;
        wc->latar = WARNAL_DEFAULT;
    }
}

/* Set struktur warna_kombinasi */
void warna_kombinasi_set(struct warna_kombinasi *wc, warna_depan wd, warna_latar wl)
{
    if (wc) {
        wc->depan = wd;
        wc->latar = wl;
    }
}
