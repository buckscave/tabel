/* ============================================================
 * TABEL v3.0 
 * Berkas: tampilan.c - Tampilan UI (Smart Grid, wcwidth, Poll-Safe)
 * Standar: C89 + POSIX.1-2008
 * Penulis: Chandra Lesmana
 * ============================================================ */

#include "tabel.h"

#define KOLOM_MAKS    512
#define BARIS_MAKS   4096

static const char *peta_warna[] = {
    ESC_NORMAL,
    FG_GELAP,
    FG_ABU,
    FG_PUTIH,
    FG_HIJAU,
    FG_KUNING,
    FG_BIRU,
    FG_MERAH,
    BG_NORMAL,
    BG_HITAM,
    BG_GELAP,
    BG_ABU,
    BG_HIJAU,
    BG_HIJAU_GELAP,
    BG_HIJAU_TUA,
    BG_KUNING,
    BG_BIRU,
    BG_MERAH
};

static struct glyph buffer_depan[BARIS_MAKS][KOLOM_MAKS];
static struct glyph buffer_belakang[BARIS_MAKS][KOLOM_MAKS];
static int lebar_layar = 0;
static int tinggi_layar = 0;

static int clip_x1 = 0; 
static int clip_y1 = 0;
static int clip_x2 = 9999; 
static int clip_y2 = 9999;

static int atas_grid = 4;
static int bar_status = 1;

static const char *nama_aplikasi = NAMA_APLIKASI;

/* ============================================================
 * Helper Rendering
 * ============================================================ */
static void taruh_glyph(int x, int y, const char *s, warna_glyph w) {
    struct glyph *g;
    unsigned char n;
    
    if (x < clip_x1 || x > clip_x2 || y < clip_y1 || y > clip_y2) return;
    if (x < 1 || y < 1 || x > lebar_layar || y > tinggi_layar) return;
    
    g = &buffer_belakang[y - 1][x - 1];
    n = (unsigned char)panjang_cp_utf8(s);
    if (n > MAKS_GLYPH) n = MAKS_GLYPH;
    memcpy(g->byte, s, n);
    g->panjang = n;
    g->warna = w;
}

static void taruh_spasi(int x, int y, warna_glyph w) {
    struct glyph *g;
    if (x < 1 || y < 1 || x > lebar_layar || y > tinggi_layar) return;
    g = &buffer_belakang[y - 1][x - 1];
    g->byte[0] = ' '; 
    g->panjang = 1; 
    g->warna = w;
}

void taruh_teks(int x, int y, const char *s, warna_glyph w) {
    int pos_x = x;
    const char *p = s;
    while (*p && pos_x <= lebar_layar) {
        unsigned char n = (unsigned char)panjang_cp_utf8(p);
        if (n > MAKS_GLYPH) n = MAKS_GLYPH;
        taruh_glyph(pos_x, y, p, w);
        p += n;
        pos_x++;
    }
}

static void taruh_teks_elipsis(int x, int y, const char *s,
    int max_kol, warna_glyph w)
{
    int digunakan = 0;
    const char *p = s;
    if (max_kol <= 0) return;
    while (*p && digunakan < max_kol) {
        unsigned char n = (unsigned char)panjang_cp_utf8(p);
        if (digunakan + 1 > max_kol) break;
        taruh_glyph(x + digunakan, y, p, w);
        digunakan++;
        p += n;
    }
    if (*p && max_kol >= 1) {
        taruh_glyph(x + max_kol - 1, y, "…", w);
    }
}

static void isi_garis_h(int x1, int x2, int y, warna_glyph w) {
    int x;
    if (y < 1 || y > tinggi_layar) return;
    if (x1 < 1) x1 = 1;
    if (x2 > lebar_layar) x2 = lebar_layar;
    for (x = x1; x <= x2; x++) taruh_spasi(x, y, w);
}

void set_clip_rect(int x, int y, int w, int h) {
    clip_x1 = x + 1; 
    clip_y1 = y + 1;
    clip_x2 = x + w;
    clip_y2 = y + h;
}

void reset_clip_rect(void) {
    clip_x1 = 0; clip_y1 = 0; clip_x2 = 9999; clip_y2 = 9999;
}

/* ============================================================
 * Fungsi Terminal Dasar
 * ============================================================ */
void atur_posisi_kursor(int x, int y) {
    char esc[32];
    int n = snprintf(esc, sizeof(esc), "\033[%d;%dH", y, x);
    if (n > 0) write(STDOUT_FILENO, esc, (size_t)n);
}

void flush_stdout(void) { fflush(stdout); }

void masuk_mode_alternatif(void) {
    write(STDOUT_FILENO, "\033[?1049h\033[?25l", 12);
    write(STDOUT_FILENO, "\033[2J", 4);
}

void keluar_mode_alternatif(void) {
    write(STDOUT_FILENO, "\033[2J\033[H", 7);
    write(STDOUT_FILENO, "\033[?25h\033[?1049l", 12);
}

void pulihkan_terminal(void) {
    if (mode_raw_aktif) {
        tcsetattr(STDIN_FILENO, TCSANOW, &term_simpan);
        mode_raw_aktif = 0;
    }
}

void tangani_sinyal_keluar(int sig) {
    (void)sig;
    pulihkan_terminal();
    keluar_mode_alternatif();
    _exit(0);
}

int ambil_lebar_terminal(void) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1) return 80;
    return ws.ws_col;
}

int ambil_tinggi_terminal(void) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1) return 24;
    return ws.ws_row;
}

void atur_mode_terminal(int blocking) {
    struct termios t;
    if (!mode_raw_aktif) return;
    tcgetattr(STDIN_FILENO, &t);
    if (blocking) { t.c_cc[VMIN] = 1; t.c_cc[VTIME] = 0; }
    else { t.c_cc[VMIN] = 0; t.c_cc[VTIME] = 1; }
    tcsetattr(STDIN_FILENO, TCSANOW, &t);
}

void baca_satu_kunci(unsigned char *buf, int *len) {
    ssize_t n = read(STDIN_FILENO, buf, 8);
    *len = (n > 0) ? (int)n : 0;
}

/* ============================================================
 * Inisialisasi & Sinyal
 * ============================================================ */
void tangani_sinyal_resize(int sig) { (void)sig; perlu_resize = 1; }

void atur_sigaction(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = tangani_sinyal_resize;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGWINCH, &sa, NULL);
    signal(SIGINT, tangani_sinyal_keluar);
    signal(SIGTERM, tangani_sinyal_keluar);
}

int inisialisasi_terminal(void) {
    struct termios t;
    int r, c;
    if (tcgetattr(STDIN_FILENO, &term_simpan) == -1) return -1;
    t = term_simpan;
    t.c_lflag &= ~(ECHO | ICANON | ISIG);
    t.c_cc[VMIN] = 0; t.c_cc[VTIME] = 1;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &t) == -1) return -1;
    mode_raw_aktif = 1;
    lebar_layar = ambil_lebar_terminal();
    tinggi_layar = ambil_tinggi_terminal();
    if (lebar_layar > KOLOM_MAKS) lebar_layar = KOLOM_MAKS;
    if (tinggi_layar > BARIS_MAKS) tinggi_layar = BARIS_MAKS;
    
    for (r = 0; r < BARIS_MAKS; r++) {
        for (c = 0; c < KOLOM_MAKS; c++) {
            buffer_depan[r][c].byte[0] = ' ';
            buffer_depan[r][c].panjang = 1;
            buffer_depan[r][c].warna = WARNA_DEFAULT;
            buffer_belakang[r][c].byte[0] = ' ';
            buffer_belakang[r][c].panjang = 1;
            buffer_belakang[r][c].warna = WARNA_DEFAULT;
        }
    }
    return 0;
}

void set_konfig_aktif(struct konfigurasi *cfg) { konfig_global = cfg; }

/* ============================================================
 * Fungsi I/O Blocking Khusus
 * ============================================================ */
void tunggu_tombol_lanjut(void) {
    unsigned char ch;
    atur_mode_terminal(1);
    read(STDIN_FILENO, &ch, 1);
    atur_mode_terminal(0);
}

/* ============================================================
 * Proses Resize Terpusat
 * ============================================================ */
void proses_resize_terpusat(void) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1) {
        ws.ws_col = 80; ws.ws_row = 24;
    }
    lebar_layar = ws.ws_col;
    tinggi_layar = ws.ws_row;
    if (lebar_layar > KOLOM_MAKS) lebar_layar = KOLOM_MAKS;
    if (tinggi_layar > BARIS_MAKS) tinggi_layar = BARIS_MAKS;

    /* reset buffer tampilan seperti biasa */

    /* recalculasi layout split window */
    paksa_recalc_layout();

    /* render ulang semua jendela */
    if (jendela_root) {
        render_tree_jendela(jendela_root);
        terapkan_delta();
    } else if (buffer_aktif >= 0 && buffers) {
        render(buffers[buffer_aktif]);
        flush_stdout();
    }
}

/* ============================================================
 * Delta Renderer
 * ============================================================ */
void bersihkan_layar_buffer(void) {
    int r, c;
    for (r = 0; r < tinggi_layar; r++) {
        for (c = 0; c < lebar_layar; c++) {
            buffer_belakang[r][c].byte[0] = ' ';
            buffer_belakang[r][c].panjang = 1;
            buffer_belakang[r][c].warna = WARNA_DEFAULT;
        }
    }
}

void terapkan_delta(void) {
    warna_glyph warna_saat_ini = WARNA_DEFAULT;
    int y, x, i;
    for (y = 0; y < tinggi_layar; y++) {
        int awal_run = -1;
        warna_glyph warna_run = WARNA_DEFAULT;
        for (x = 0; x < lebar_layar; x++) {
            struct glyph *b = &buffer_belakang[y][x];
            struct glyph *f = &buffer_depan[y][x];
            int beda = (b->panjang != f->panjang) ||
                       (memcmp(b->byte, f->byte, b->panjang) != 0) ||
                       (b->warna != f->warna);
            if (beda) {
                *f = *b;
                if (awal_run < 0) { 
                    awal_run = x; 
                    warna_run = b->warna; 
                } else if (b->warna != warna_run) {
                    atur_posisi_kursor(awal_run + 1, y + 1);
                    if (warna_saat_ini != warna_run) {
                        write(STDOUT_FILENO, peta_warna[warna_run],
                              strlen(peta_warna[warna_run]));
                        warna_saat_ini = warna_run;
                    }
                    for (i = awal_run; i < x; i++)
                        write(STDOUT_FILENO, buffer_depan[y][i].byte,
                              buffer_depan[y][i].panjang);
                    awal_run = x; 
                    warna_run = b->warna;
                }
            } else {
                if (awal_run >= 0) {
                    atur_posisi_kursor(awal_run + 1, y + 1);
                    if (warna_saat_ini != warna_run) {
                        write(STDOUT_FILENO, peta_warna[warna_run],
                              strlen(peta_warna[warna_run]));
                        warna_saat_ini = warna_run;
                    }
                    for (i = awal_run; i < x; i++)
                        write(STDOUT_FILENO, buffer_depan[y][i].byte,
                              buffer_depan[y][i].panjang);
                    awal_run = -1;
                }
            }
        }
        if (awal_run >= 0) {
            atur_posisi_kursor(awal_run + 1, y + 1);
            if (warna_saat_ini != warna_run) {
                write(STDOUT_FILENO, peta_warna[warna_run],
                      strlen(peta_warna[warna_run]));
                warna_saat_ini = warna_run;
            }
            for (i = awal_run; i < lebar_layar; i++)
                write(STDOUT_FILENO, buffer_depan[y][i].byte,
                      buffer_depan[y][i].panjang);
        }
    }
    write(STDOUT_FILENO, ESC_NORMAL, strlen(ESC_NORMAL));
    write(STDOUT_FILENO, "\033[?25l", 6);
    flush_stdout();
}

static int apakah_tergabung_sama(const struct buffer_tabel *buf,
                                 int c1, int r1, int c2, int r2) {
    struct info_gabung m1, m2;
    if (!apakah_ada_gabung_buf(buf, c1, r1) || 
        !apakah_ada_gabung_buf(buf, c2, r2)) return 0;
    
    m1 = buf->tergabung[r1][c1];
    m2 = buf->tergabung[r2][c2];
    return (m1.x_anchor == m2.x_anchor && m1.y_anchor == m2.y_anchor);
}

const char* dapatkan_karakter_grid(const struct buffer_tabel *buf,
    int c, int r, int kolom_mulai, int kolom_akhir,
    int baris_mulai, int baris_akhir)
{
    int atas=0, bawah=0, kiri=0, kanan=0;
    if (r > baris_mulai) {
        if (c == kolom_mulai) atas=1;
        else if (c > kolom_akhir) atas=1;
        else if (!apakah_tergabung_sama(buf,c-1,r-1,c,r-1)) atas=1;
    }
    if (r <= baris_akhir) {
        if (c == kolom_mulai) bawah=1;
        else if (c > kolom_akhir) bawah=1;
        else if (!apakah_tergabung_sama(buf,c-1,r,c,r)) bawah=1;
    }
    if (c > kolom_mulai) {
        if (r == baris_mulai) kiri=1;
        else if (r > baris_akhir) kiri=1;
        else if (!apakah_tergabung_sama(buf,c-1,r-1,c-1,r)) kiri=1;
    }
    if (c <= kolom_akhir) {
        if (r == baris_mulai) kanan=1;
        else if (r > baris_akhir) kanan=1;
        else if (!apakah_tergabung_sama(buf,c,r-1,c,r)) kanan=1;
    }
    if (atas && bawah && kiri && kanan) return CR;
    if (atas && bawah && kiri && !kanan) return RC;
    if (atas && bawah && !kiri && kanan) return LC;
    if (atas && !bawah && kiri && kanan) return BC;
    if (!atas && bawah && kiri && kanan) return TC;
    if (atas && !bawah && !kiri && kanan) return BL;
    if (atas && !bawah && kiri && !kanan) return BR;
    if (!atas && bawah && !kiri && kanan) return TL;
    if (!atas && bawah && kiri && !kanan) return TR;
    if (atas && bawah && !kiri && !kanan) return V;
    if (!atas && !bawah && kiri && kanan) return H;
    return " ";
}

/* ============================================================
 * Komponen UI 
 * ============================================================ */

/* Topbar Global */
static void gambar_tab_atas(void) {
    int kolom = lebar_layar;
    int y = 1;
    char buf_app[128];
    int panjang_app, awal_app, ruang_tab, pos_x, i;

    snprintf(buf_app, sizeof(buf_app), " %s ", NAMA_APLIKASI);
    panjang_app = (int)strlen(buf_app);
    awal_app = kolom - panjang_app + 1;
    if (awal_app < 1) awal_app = 1;

    isi_garis_h(1, kolom, y, WARNA_BG_HIJAU_TUA);
    isi_garis_h(awal_app, kolom, y, WARNA_BG_HIJAU_GELAP);
    taruh_teks_elipsis(awal_app, y, buf_app,
                       kolom - awal_app + 1, WARNA_BG_HIJAU_GELAP);

    ruang_tab = awal_app - 2;
    if (ruang_tab < 1) return;

    pos_x = 1;
    for (i = 0; i < jumlah_buffer && pos_x < ruang_tab; i++) {
        char buf_tab[128];
        warna_glyph w;
        int sisa;
        const char *path_file = (buffers[i]->cfg.path_file[0] ?
                                 buffers[i]->cfg.path_file : "[Tanpa Nama]");
        const char *basename = strrchr(path_file, '/'); 
        
        if (basename) basename++; 
        else basename = path_file;
        
        snprintf(buf_tab, sizeof(buf_tab), " %s%s ",
                 basename, buffers[i]->cfg.kotor ? "+" : "");
        w = (i == buffer_aktif) ? WARNA_BG_HIJAU : WARNA_BG_HIJAU_GELAP;
        
        sisa = ruang_tab - pos_x;
        if (sisa <= 0) break;
        
        taruh_teks_elipsis(pos_x, y, buf_tab, sisa, w);
        pos_x += (int)strlen(buf_tab) + 1; 
    }
}

/* Global Message Bar */
static void gambar_message_bar(void) {
    int kolom = ambil_lebar_terminal();
    int y = tinggi_layar;

    atur_posisi_kursor(1, y);
    write(STDOUT_FILENO, "\033[2K", 4);

    if (pesan_status[0] != '\0') {
        taruh_teks_elipsis(2, y, pesan_status, kolom - 2, WARNA_DEFAULT);
    }
}

/* Input Bar (Per Window) */
static void gambar_bar_input_window(const struct buffer_tabel *buf,
                                    struct rect area) {
    int y = area.y + 1;
    char kol = (char)('A' + buf->cfg.aktif_x);
    int bar = buf->cfg.aktif_y + 1;
    const char *teks = buf->isi[buf->cfg.aktif_y][buf->cfg.aktif_x];
    char teks_norm[MAKS_TEKS * 2];
    char kiri[MAKS_TEKS * 2];
    int j = 0, i;

    for (i = 0; teks[i] && j < (int)sizeof(teks_norm) - 2; i++) {
        if (teks[i] == '\n') {
            teks_norm[j++] = '\\';
            teks_norm[j++] = 'n';
        } else {
            teks_norm[j++] = teks[i];
        }
    }
    teks_norm[j] = '\0';

    if (teks[0] == '\0') {
        snprintf(kiri, sizeof(kiri), "Kolom %c%d: [KOSONG]", kol, bar);
    } else {
        snprintf(kiri, sizeof(kiri), "Kolom %c%d: %s", kol, bar, teks_norm);
    }

    isi_garis_h(area.x + 1, area.x + area.w, y, WARNA_DEFAULT);
    taruh_teks_elipsis(area.x + 2, y, kiri, area.w - 2, WARNA_DEFAULT);
}

/* Header Kolom (Per Window) */
static void gambar_label_kolom_window(const struct buffer_tabel *buf,
    struct rect area, int x_awal, int y_awal, int kolom_mulai, int kolom_akhir)
{
    int y = y_awal - 1; 
    int x = x_awal + 1;
    int c;

    isi_garis_h(area.x + 1, area.x + area.w, y, WARNA_BG_NORMAL);
    
    for (c = kolom_mulai; c <= kolom_akhir; c++) {
        char ch[4]; 
        warna_glyph w;
        ch[0] = (char)('A' + c); 
        ch[1] = '\0';
        w = (c == buf->cfg.aktif_x) ? WARNA_KUNING : WARNA_ABU;
        taruh_teks(x + buf->lebar_kolom[c] / 2, y, ch, w);
        x += buf->lebar_kolom[c] + 1;
    }
}

/* Nomor Baris (Per Window) */
static void gambar_nomor_baris_window(const struct buffer_tabel *buf,
    struct rect area, int y_awal, int baris_mulai, int baris_akhir)
{
    int y = y_awal;
    int r;
    for (r = baris_mulai; r <= baris_akhir; r++) {
        char num[16]; 
        warna_glyph w;
        snprintf(num, sizeof(num), "%2d", r + 1);
        w = (r == buf->cfg.aktif_y) ? WARNA_KUNING : WARNA_ABU;
        taruh_teks(area.x + 2, y + 1, num, w);
        y += buf->tinggi_baris[r] + 1;
    }
}

/* Status Bar (Per Window) */
static void gambar_bar_status_window(const struct buffer_tabel *buf,
    struct rect area, int is_active) 
{
    int y = area.y + area.h;
    char kiri[256], kanan[256];
    const char *ext = "[unknown]";
    int panjang_kanan_asli, lebar_tampil_kanan, awal_kanan, lebar_maks_kiri;
    warna_glyph w_bg, w_bg_kanan;

    if (strstr(buf->cfg.path_file, ".csv")) ext = "csv";
    else if (strstr(buf->cfg.path_file, ".tbl")) ext = "tabel";
    else if (strstr(buf->cfg.path_file, ".txt")) ext = "txt";
    else if (strstr(buf->cfg.path_file, ".db")) ext = "db";
    else if (strstr(buf->cfg.path_file, ".sql")) ext = "sql";
    else if (strstr(buf->cfg.path_file, ".md")) ext = "md";
    else if (strstr(buf->cfg.path_file, ".xlsx")) ext = "xlsx";
    else if (strstr(buf->cfg.path_file, ".xls")) ext = "xls";
    else if (strstr(buf->cfg.path_file, ".xml")) ext = "xml";
    else if (strstr(buf->cfg.path_file, ".ods")) ext = "ods";
    else if (strstr(buf->cfg.path_file, ".pdf")) ext = "pdf";
    else if (strstr(buf->cfg.path_file, ".html")) ext = "html";
    else if (strstr(buf->cfg.path_file, ".htm")) ext = "html";
    else if (strstr(buf->cfg.path_file, ".json")) ext = "json";
    else if (strstr(buf->cfg.path_file, ".tsv")) ext = "tsv";

    snprintf(kanan, sizeof(kanan),
             " %s %s %s %s K:%d/%d B:%d/%d %s L:%d T:%d %s ? ",
             buf->cfg.enkoding, V, ext, V,
             buf->cfg.aktif_x + 1, buf->cfg.kolom,
             buf->cfg.aktif_y + 1, buf->cfg.baris, V,
             buf->lebar_kolom[buf->cfg.aktif_x],
             buf->tinggi_baris[buf->cfg.aktif_y], V);

    panjang_kanan_asli = lebar_tampilan_string_utf8(kanan);
    lebar_tampil_kanan = (panjang_kanan_asli > area.w) ? 
                          area.w : panjang_kanan_asli;
    awal_kanan = area.x + area.w - lebar_tampil_kanan + 1;
    if (awal_kanan < area.x + 1) awal_kanan = area.x + 1;

    snprintf(kiri, sizeof(kiri), "%s", 
            buf->cfg.path_file[0] ? buf->cfg.path_file : "[Tanpa Nama]");

    w_bg = is_active ? WARNA_BG_HIJAU_TUA : WARNA_BG_GELAP;
    w_bg_kanan = is_active ? WARNA_BG_HIJAU : WARNA_BG_ABU;

    isi_garis_h(area.x + 1, area.x + area.w, y, w_bg);
    taruh_teks_elipsis(awal_kanan, y, kanan, lebar_tampil_kanan, w_bg_kanan);

    lebar_maks_kiri = awal_kanan - (area.x + 2);
    if (lebar_maks_kiri > 2) {
        taruh_teks_elipsis(area.x + 1, y, kiri, lebar_maks_kiri, w_bg);
    }
}

/* ============================================================
 * Renderer Grid (Absolut Terhadap Clip Rect)
 * ============================================================ */
static void gambar_grid_cerdas(const struct buffer_tabel *buf,
    int x_awal, int y_awal, int kolom_mulai, int kolom_akhir,
    int baris_mulai, int baris_akhir)
{
    int py = y_awal;
    int r, c, k;

    for (r = baris_mulai; r <= baris_akhir + 1; r++) {
        int px = x_awal;
        for (c = kolom_mulai; c <= kolom_akhir + 1; c++) {
            const char *sambungan = dapatkan_karakter_grid(buf, c, r,
                    kolom_mulai, kolom_akhir, baris_mulai, baris_akhir);
            taruh_teks(px, py, sambungan, WARNA_GELAP);
            
            if (c <= kolom_akhir) {
                int gambar_h = 1;
                if (r > baris_mulai && r <= baris_akhir) {
                    if (apakah_ada_gabung_buf(buf, c, r-1) &&
                        apakah_ada_gabung_buf(buf, c, r))
                        gambar_h = 0;
                }
                if (gambar_h) {
                    for (k = 0; k < buf->lebar_kolom[c]; k++)
                        taruh_teks(px + 1 + k, py, H, WARNA_GELAP);
                }
                px += buf->lebar_kolom[c] + 1;
            }
        }
        if (r <= baris_akhir) {
            int px_v = x_awal;
            for (c = kolom_mulai; c <= kolom_akhir + 1; c++) {
                int gambar_v = 1;
                if (c > kolom_mulai && c <= kolom_akhir) {
                    if (apakah_ada_gabung_buf(buf, c-1, r) &&
                        apakah_ada_gabung_buf(buf, c, r))
                        gambar_v = 0;
                }
                if (gambar_v) {
                    for (k = 0; k < buf->tinggi_baris[r]; k++)
                        taruh_teks(px_v, py + 1 + k, V, WARNA_GELAP);
                }
                if (c <= kolom_akhir) px_v += buf->lebar_kolom[c] + 1;
            }
            py += buf->tinggi_baris[r] + 1;
        }
    }
}

static void gambar_isi_sel_lihat(const struct buffer_tabel *buf,
    int x_awal, int y_awal, int kolom_mulai, int baris_mulai,
    int c, int r)
{
    int ax = c, ay = r;
    int lebar_sel = 1, tinggi_sel = 1;
    int x0, y0, bw = 0, bh = 0;
    int i, k;
    int lebar_vis, tinggi_vis;
    const char *teks = buf->isi[ay][ax];
    warna_glyph warna_teks = buf->warna_teks_sel[ay][ax]; 
    char teks_norm[MAKS_TEKS];
    int j = 0;
    int start = 0;
    int baris = 0;

    if (warna_teks == WARNA_DEFAULT) { 
        warna_teks = WARNA_DEFAULT;
    }

    if (apakah_ada_gabung_buf(buf, c, r)) {
        if (!apakah_sel_anchor_buf(buf, c, r)) return;
        ax = buf->tergabung[r][c].x_anchor;
        ay = buf->tergabung[r][c].y_anchor;
        lebar_sel = buf->tergabung[ay][ax].lebar;
        tinggi_sel = buf->tergabung[ay][ax].tinggi;
    }

    x0 = x_awal + 1; 
    y0 = y_awal;
    for (i = kolom_mulai; i < ax; i++) x0 += buf->lebar_kolom[i] + 1;
    for (i = baris_mulai; i < ay; i++) y0 += buf->tinggi_baris[i] + 1;
    for (i = 0; i < lebar_sel; i++) bw += buf->lebar_kolom[ax + i] + 1;
    for (i = 0; i < tinggi_sel; i++) bh += buf->tinggi_baris[ay + i] + 1;
    
    lebar_vis = bw - 1; 
    tinggi_vis = bh - 1;

    for (i = 0; i < tinggi_vis; i++) {
        for (k = 0; k < lebar_vis; k++) {
            taruh_spasi(x0 + k, y0 + 1 + i, WARNA_DEFAULT);
        }
    }

    if (teks[0] == '\0') return;

    if (warna_teks == WARNA_DEFAULT) { 
        if (teks[0] == '=') { 
            double val; 
            int adalah_angka; 
            char buf_err[64]; 
            if (evaluasi_string(buf, ax, ay, teks, &val, &adalah_angka, 
                        buf_err, sizeof(buf_err)) == 0 && adalah_angka) { 
                static char buf_num[MAKS_TEKS];
                jenis_format_sel fmt = ambil_format_sel(buf, ax, ay);

                /* Jika format UMUM, deteksi otomatis */
                if (fmt == FORMAT_UMUM) {
                    fmt = (jenis_format_sel)deteksi_format_otomatis(val);
                }

                /* Format nilai sesuai format sel */
                if (fmt != FORMAT_UMUM && fmt != FORMAT_ANGKA) {
                    format_nilai_ke_teks(val, fmt, buf_num, sizeof(buf_num));
                } else {
                    snprintf(buf_num, sizeof(buf_num), "%.10g", val);
                }
                teks = buf_num; 
                warna_teks = WARNA_MERAH; 
            } else { 
                teks = "#ERR"; 
                warna_teks = WARNA_MERAH; 
            } 
        } else { 
            char *endptr; 
            strtod(teks, &endptr); 
            if (*endptr == '\0' && teks[0] != '\0') { 
                warna_teks = WARNA_MERAH; 
            } 
        } 
    }

    for (i = 0; teks[i] && j < MAKS_TEKS-1; i++) {
        if (teks[i] == '\\' && teks[i+1] == 'n') {
            teks_norm[j++] = '\n';
            i++; 
        } else {
            teks_norm[j++] = teks[i];
        }
    }
    teks_norm[j] = '\0';

    while (teks_norm[start] && baris < tinggi_vis) {
        char line[MAKS_TEKS];
        int idx = 0;
        int kol, offset = 0;

        while (teks_norm[start] && teks_norm[start] != '\n' && 
               idx < MAKS_TEKS-1) {
            line[idx++] = teks_norm[start++];
        }
        line[idx] = '\0';

        if (teks_norm[start] == '\n') start++;

        kol = lebar_tampilan_string_utf8(line);
        if (buf->perataan_sel[ay][ax] == RATA_TENGAH)
            offset = (lebar_vis - kol) / 2;
        else if (buf->perataan_sel[ay][ax] == RATA_KANAN)
            offset = (lebar_vis - kol);

        taruh_teks_elipsis(x0 + offset, y0 + 1 + baris,
                           line, lebar_vis, warna_teks);
        baris++;
    }
}

static void gambar_kotak_outline(const struct buffer_tabel *buf,
    int x_awal, int y_awal, int ks, int ke, int bs, int be,
    int x1, int y1, int x2, int y2, warna_glyph w)
{
    int minx = x1 < x2 ? x1 : x2; 
    int maxx = x1 > x2 ? x1 : x2;
    int miny = y1 < y2 ? y1 : y2; 
    int maxy = y1 > y2 ? y1 : y2;
    int cx, cy, px1, px2, py1, py2, i;

    if (maxx < ks || minx > ke || maxy < bs || miny > be) return;

    cx = x_awal;
    for (i = ks; i < minx; i++) cx += buf->lebar_kolom[i] + 1;
    px1 = cx;
    for (i = (minx < ks ? ks : minx); i <= maxx; i++)
        cx += buf->lebar_kolom[i] + 1;
    px2 = cx;

    cy = y_awal;
    for (i = bs; i < miny; i++) cy += buf->tinggi_baris[i] + 1;
    py1 = cy;
    for (i = (miny < bs ? bs : miny); i <= maxy; i++)
        cy += buf->tinggi_baris[i] + 1;
    py2 = cy;

    if (miny >= bs) {
        taruh_teks(px1, py1, TL, w);
        for (i=px1+1; i<px2; i++) taruh_teks(i, py1, H, w);
        taruh_teks(px2, py1, TR, w);
    }
    if (maxy <= be) {
        taruh_teks(px1, py2, BL, w);
        for (i=px1+1; i<px2; i++) taruh_teks(i, py2, H, w);
        taruh_teks(px2, py2, BR, w);
    }
    if (minx >= ks) {
        for (i=py1+1; i<py2; i++) taruh_teks(px1, i, V, w);
    }
    if (maxx <= ke) {
        for (i=py1+1; i<py2; i++) taruh_teks(px2, i, V, w);
    }
}

static int cek_garis_horizontal(const struct buffer_tabel *buf, int c, int r) {
    int ada = 0;
    if (r < buf->cfg.baris && c < buf->cfg.kolom && c >= 0 && r >= 0) {
        if (buf->border_sel[r][c].top) ada = 1;
    }
    if (r > 0 && c < buf->cfg.kolom && c >= 0) {
        if (buf->border_sel[r-1][c].bottom) ada = 1;
    }
    return ada;
}

static int cek_garis_vertikal(const struct buffer_tabel *buf, int c, int r) {
    int ada = 0;
    if (r < buf->cfg.baris && c < buf->cfg.kolom && c >= 0 && r >= 0) {
        if (buf->border_sel[r][c].left) ada = 1;
    }
    if (c > 0 && r < buf->cfg.baris && r >= 0) {
        if (buf->border_sel[r][c-1].right) ada = 1;
    }
    return ada;
}

static warna_glyph ambil_warna_border(const struct buffer_tabel *buf, 
                                      int c, int r, int vertikal) 
{
    struct info_border *b;
    if (r < 0 || r >= buf->cfg.baris || c < 0 || c >= buf->cfg.kolom) 
        return WARNA_PUTIH;

    b = &buf->border_sel[r][c];
    if (vertikal) {
        if (b->left) return b->warna;
        if (c > 0 && buf->border_sel[r][c-1].right) 
            return buf->border_sel[r][c-1].warna;
    } else {
        if (b->top) return b->warna;
        if (r > 0 && buf->border_sel[r-1][c].bottom) 
            return buf->border_sel[r-1][c].warna;
    }
    return WARNA_PUTIH;
}

static const char* resolve_border_char(int up, int down, int left, int right) {
    int sum = up + down + left + right;
    
    if (sum < 2) return NULL; 
    if (sum == 4) return CR; 
    if (sum == 3) {
        if (!up) return TC;    
        if (!down) return BC;  
        if (!left) return LC;  
        if (!right) return RC; 
    }
    if (sum == 2) {
        if (left && right) return H;  
        if (up && down) return V;     
        if (down && right) return TL; 
        if (down && left) return TR;  
        if (up && right) return BL;   
        if (up && left) return BR;    
    }
    return NULL;
}

static void gambar_layer_border_smart(const struct buffer_tabel *buf,
    int x_awal, int y_awal, int ks, int ke, int bs, int be) 
{
    int py = y_awal;
    int r, c;

    for (r = bs; r <= be + 1; r++) {
        int px = x_awal;
        for (c = ks; c <= ke + 1; c++) {
            int up = 0, down = 0, left = 0, right = 0;
            warna_glyph w = WARNA_PUTIH;
            int ada_koneksi = 0;

            if (cek_garis_vertikal(buf, c, r-1)) { 
                up = 1; ada_koneksi = 1; 
                w = ambil_warna_border(buf, c, r-1, 1); 
            }
            if (cek_garis_vertikal(buf, c, r)) { 
                down = 1; ada_koneksi = 1; 
                if(!up) w = ambil_warna_border(buf, c, r, 1); 
            }
            if (cek_garis_horizontal(buf, c-1, r)) { 
                left = 1; ada_koneksi = 1; 
                if(!up && !down) w = ambil_warna_border(buf, c-1, r, 0); 
            }
            if (cek_garis_horizontal(buf, c, r)) { 
                right = 1; ada_koneksi = 1; 
                if(!up && !down && !left) 
                    w = ambil_warna_border(buf, c, r, 0); 
            }

            if (ada_koneksi) {
                const char *glyph = resolve_border_char(up, down, left, right);
                if (glyph) taruh_teks(px, py, glyph, w);
            }

            if (right && c <= ke) {
                warna_glyph wc = ambil_warna_border(buf, c, r, 0);
                int lebar = buf->lebar_kolom[c];
                int k;
                for (k = 1; k <= lebar; k++) {
                    taruh_teks(px + k, py, H, wc);
                }
            }

            if (down && r <= be) {
                warna_glyph wc = ambil_warna_border(buf, c, r, 1);
                int tinggi = buf->tinggi_baris[r];
                int k;
                for (k = 1; k <= tinggi; k++) {
                    taruh_teks(px, py + k, V, wc);
                }
            }

            if (c <= ke) px += buf->lebar_kolom[c] + 1;
        }
        if (r <= be) py += buf->tinggi_baris[r] + 1;
    }
}

/* ============================================================
 * Kalkulasi Viewport
 * ============================================================ */
static void hitung_viewport_window(const struct buffer_tabel *buf,
    struct rect area, int *x_awal, int *y_awal, int *lebar_vis, int *tinggi_vis,
    int *kolom_mulai, int *kolom_akhir, int *baris_mulai, int *baris_akhir)
{
    const struct konfigurasi *cfg = &buf->cfg;
    int padding_kiri = 5;
    int tinggi_tersedia = area.h - 3; 
    int lebar_tersedia = area.w - padding_kiri;
    int c, x, r, y;

    if (tinggi_tersedia < 1) tinggi_tersedia = 1;
    if (lebar_tersedia < 8) lebar_tersedia = 8;

    *x_awal = area.x + padding_kiri; 
    *y_awal = area.y + 3; 
    
    *lebar_vis = lebar_tersedia; 
    *tinggi_vis = tinggi_tersedia;

    c = cfg->lihat_kolom; x = 0;
    while (c < cfg->kolom) {
        int w = buf->lebar_kolom[c] + 1;
        if (x + w > lebar_tersedia) break;
        x += w; c++;
    }
    *kolom_mulai = cfg->lihat_kolom;
    *kolom_akhir = (c - 1 < *kolom_mulai) ? *kolom_mulai : c - 1;
    
    r = cfg->lihat_baris; y = 0;
    while (r < cfg->baris) {
        int h = buf->tinggi_baris[r] + 1;
        if (y + h > tinggi_tersedia) break;
        y += h; r++;
    }
    *baris_mulai = cfg->lihat_baris;
    *baris_akhir = (r - 1 < *baris_mulai) ? *baris_mulai : r - 1;
}

void hitung_viewport(const struct buffer_tabel *buf,
    int *x_awal, int *y_awal, int *lebar_vis, int *tinggi_vis,
    int *kolom_mulai, int *kolom_akhir, int *baris_mulai, int *baris_akhir)
{
    struct rect area;
    
    /* Gunakan dimensi window aktif jika tersedia, kalau tidak gunakan layar penuh */
    if (jendela_aktif && jendela_aktif->jenis == JENIS_DAUN) {
        area = jendela_aktif->rect;
    } else {
        area.x = 0;
        area.y = 1;
        area.w = lebar_layar;
        area.h = tinggi_layar - 2;
    }
    
    hitung_viewport_window(buf, area, x_awal, y_awal, lebar_vis, tinggi_vis, 
                           kolom_mulai, kolom_akhir, baris_mulai, baris_akhir);
}

/* ============================================================
 * Overlay Prompts
 * ============================================================ */
void prompt_warna(struct buffer_tabel *buf, int mode_teks) {
    struct { 
        const char *nama; 
        warna_glyph warna; 
    } opsi[] = {
        {"Default", WARNA_DEFAULT}, {"Gelap", WARNA_GELAP},   
        {"Abu", WARNA_ABU}, {"Putih", WARNA_PUTIH},     
        {"Hijau", WARNA_HIJAU}, {"Kuning", WARNA_KUNING},
        {"Biru", WARNA_BIRU}, {"Merah", WARNA_MERAH}
    };
    int jumlah = 8;
    int pilihan = 0;
    int x1, y1, x2, y2, min_x, max_x, min_y, max_y;
    int area_size;
    warna_glyph *backup = NULL;

    x1 = seleksi_aktif ? seleksi_x1 : buf->cfg.aktif_x;
    y1 = seleksi_aktif ? seleksi_y1 : buf->cfg.aktif_y;
    x2 = seleksi_aktif ? seleksi_x2 : buf->cfg.aktif_x;
    y2 = seleksi_aktif ? seleksi_y2 : buf->cfg.aktif_y;
    
    min_x = (x1 < x2) ? x1 : x2; max_x = (x1 > x2) ? x1 : x2;
    min_y = (y1 < y2) ? y1 : y2; max_y = (y1 > y2) ? y1 : y2;
    area_size = (max_y - min_y + 1) * (max_x - min_x + 1);
    
    if (mode_teks) {
        backup = malloc(area_size * sizeof(warna_glyph));
        if (backup) {
            int idx = 0, r, c;
            for (r = min_y; r <= max_y; r++)
                for (c = min_x; c <= max_x; c++)
                    backup[idx++] = buf->warna_teks_sel[r][c];
        }
    }

    atur_mode_terminal(1);

    while (1) {
        int y_ui, x_pos, i;
        char label[64];
        unsigned char ch[8]; 
        int len;

        if (mode_teks) {
            int r, c;
            for (r = min_y; r <= max_y; r++)
                for (c = min_x; c <= max_x; c++)
                    buf->warna_teks_sel[r][c] = opsi[pilihan].warna;
        }

        render(buf);

        if (!mode_teks) {
            int x_awal, y_awal, lv, tv, ks, ke, bs, be;
            hitung_viewport(buf, &x_awal, &y_awal, &lv, &tv, 
                            &ks, &ke, &bs, &be);
            gambar_kotak_outline(buf, x_awal, y_awal, ks, ke, bs, be,
                                 x1, y1, x2, y2, opsi[pilihan].warna);
        }

        y_ui = tinggi_layar; 
        isi_garis_h(1, lebar_layar, y_ui, WARNA_BG_NORMAL); 
        
        snprintf(label, sizeof(label), "Pilih Warna %s: ", 
                 mode_teks ? "Teks" : "Border");
        taruh_teks(2, y_ui, label, WARNA_PUTIH);
        
        x_pos = 2 + strlen(label);
        for (i = 0; i < jumlah; i++) {
            if (i == pilihan) {
                taruh_teks(x_pos, y_ui, "[", WARNA_KUNING);
                taruh_teks(x_pos+1, y_ui, opsi[i].nama, WARNA_PUTIH);
                taruh_teks(x_pos+1+strlen(opsi[i].nama), y_ui, "]", 
                           WARNA_KUNING);
                x_pos += strlen(opsi[i].nama) + 4;
            } else {
                taruh_teks(x_pos, y_ui, opsi[i].nama, WARNA_ABU);
                x_pos += strlen(opsi[i].nama) + 2;
            }
        }

        terapkan_delta(); 

        baca_satu_kunci(ch, &len);

        if (ch[0] == 27 && len == 1) { 
            if (mode_teks && backup) {
                int idx = 0, r, c;
                for (r = min_y; r <= max_y; r++)
                    for (c = min_x; c <= max_x; c++)
                        buf->warna_teks_sel[r][c] = backup[idx++];
            }
            break; 
        } 
        else if (ch[0] == '\r' || ch[0] == '\n') { 
            if (!mode_teks) {
                int r, c;
                for (r = min_y; r <= max_y; r++)
                    for (c = min_x; c <= max_x; c++)
                        buf->border_sel[r][c].warna = opsi[pilihan].warna;
            }
            buf->cfg.kotor = 1;
            break;
        }
        else if (ch[0] == 27 && len >= 3 && ch[1] == '[') { 
            if (ch[2] == 'C') pilihan = (pilihan + 1) % jumlah;     
            if (ch[2] == 'D') pilihan = (pilihan - 1 + jumlah) % jumlah; 
        }
    }

    if (backup) free(backup);
    atur_mode_terminal(0);
    render(buf); 
}

/* ============================================================
 * Render Utama Terpadu (Single/Multi Window)
 * ============================================================ */
void render_jendela_spesifik(const struct buffer_tabel *buf,
                             struct rect area, int is_active) 
{
    int x_awal, y_awal, lv, tv, ks, ke, bs, be;

    set_clip_rect(area.x, area.y, area.w, area.h);

    gambar_bar_input_window(buf, area);
    hitung_viewport_window(buf, area, &x_awal, &y_awal, &lv, &tv, 
                           &ks, &ke, &bs, &be);
    
    gambar_label_kolom_window(buf, area, x_awal, y_awal, ks, ke);
    gambar_nomor_baris_window(buf, area, y_awal, bs, be);
    
    gambar_grid_cerdas(buf, x_awal, y_awal, ks, ke, bs, be);
    
    {
        int r, c;
        for (r = bs; r <= be; r++) {
            for (c = ks; c <= ke; c++) {
                gambar_isi_sel_lihat(buf, x_awal, y_awal, ks, bs, c, r);
            }
        }
    }
    
    gambar_layer_border_smart(buf, x_awal, y_awal, ks, ke, bs, be);
    
    if (klip_ada_area) {
        gambar_kotak_outline(buf, x_awal, y_awal, ks, ke, bs, be,
                             klip_x1, klip_y1,
                             klip_x2, klip_y2, WARNA_HIJAU);
    }
    if (sedang_memilih && jangkar_x >= 0) {
        gambar_kotak_outline(buf, x_awal, y_awal, ks, ke, bs, be,
                             jangkar_x, jangkar_y,
                             buf->cfg.aktif_x, buf->cfg.aktif_y, WARNA_BIRU);
    }
    if (seleksi_aktif) {
        gambar_kotak_outline(buf, x_awal, y_awal, ks, ke, bs, be,
                             seleksi_x1, seleksi_y1,
                             seleksi_x2, seleksi_y2, WARNA_BIRU);
    }
    
    {
        int ax = buf->cfg.aktif_x, ay = buf->cfg.aktif_y;
        int bx = ax, by = ay;
        if (apakah_ada_gabung_buf(buf, ax, ay)) {
            bx = buf->tergabung[ay][ax].x_anchor + 
                 buf->tergabung[ay][ax].lebar - 1;
            by = buf->tergabung[ay][ax].y_anchor + 
                 buf->tergabung[ay][ax].tinggi - 1;
            ax = buf->tergabung[ay][ax].x_anchor;
            ay = buf->tergabung[ay][ax].y_anchor;
        }
        gambar_kotak_outline(buf, x_awal, y_awal, ks, ke, bs, be,
                             ax, ay, bx, by, WARNA_PUTIH);
    }
    
    {
        int i;
        for (i = 0; i < seleksi_random; i++) {
            int cx = seleksi_random_x[i];
            int cy = seleksi_random_y[i];
            if (cx < 0 || cy < 0 || 
                cx >= buf->cfg.kolom || cy >= buf->cfg.baris) continue;
            gambar_kotak_outline(buf, x_awal, y_awal, ks, ke, bs, be,
                                 cx, cy, cx, cy, WARNA_BIRU);
        }
    }

    gambar_bar_status_window(buf, area, is_active);

    reset_clip_rect();
}

void render_semua_jendela(void) {
    bersihkan_layar_buffer();
    gambar_tab_atas();
    
    if (jendela_root) {
        render_tree_jendela(jendela_root);
    } else {
        if (buffer_aktif >= 0 && buffers) {
            struct rect area;
            area.x = 0; 
            area.y = 1; 
            area.w = lebar_layar; 
            area.h = tinggi_layar - 2;
            render_jendela_spesifik(buffers[buffer_aktif], area, 1);
        }
    }
    
    gambar_message_bar();
    pesan_status[0] = '\0';
    terapkan_delta();
}

void render(const struct buffer_tabel *buf) {
    (void)buf; 
    render_semua_jendela();
}

void render_aseli(const struct buffer_tabel *buf) {
    struct rect area;
    area.x = 0; 
    area.y = 1; 
    area.w = lebar_layar; 
    area.h = tinggi_layar - 2;
    
    bersihkan_layar_buffer();
    gambar_tab_atas();
    render_jendela_spesifik(buf, area, 1);
    gambar_message_bar();
    pesan_status[0] = '\0';
    terapkan_delta();
}

/* ============================================================
 * Tampilan Bantuan
 * ============================================================ */
void tampilkan_bantuan(void) {
    const char *teks_bantuan[] = {
        "=== BANTUAN TABEL v3.0 ===", "",
        "Navigasi:",
        "  Panah        : Pindah sel",
        "  Shift+Tab    : Mundur sel",
        "  Alt+Panah    : Resize sel",
        "  g[A][1]      : Lompat ke A1",
        "",
        "Edit:",
        "  Enter        : Edit sel",
        "  [ , ] , \\    : Align Kiri, Kanan, Tengah",
        "",
        "Seleksi & Blok:",
        "  v            : Seleksi Manual",
        "  V            : Seleksi (Label + Nomor Baris Sel)",
        "  Alt + v      : Seleksi Baris Penuh",
        "  Ctrl + v     : Seleksi Kolom Penuh",
        "  y            : Menyalin Isi Sel",
        "  Alt + y      : Menyalin Kolom Penuh",
        "  Ctrl + y     : Menyalin Baris Penuh",
        "  x            : Memotong Isi Sel",
        "  Alt + x      : Memotong Kolom Penuh",
        "  Ctrl + x     : Memotong Baris Penuh",
        "  p            : Menempelkan Isi Sel",
        "  Alt + p      : Menempelkan Sebelum Kolom/Baris Aktif",
        "  Ctrl + p     : Menempelkan Sesudah Kolom/Baris Aktif",
        "  P            : Menempelkan Isi Sel Kedalam Area Seleksi",
        "  m            : Menggabung Sel",
        "  M            : Mengurai Sel Yang Tergabung",
        "",
        "Window Manager:",
        "  W s / W v    : Split Window Horizontal / Vertikal",
        "  W q          : Tutup Window Saat Ini",
        "  Alt+Shift+Panah : Resize Window",
        "  Alt+Panah    : Pindah Fokus Window",
        "",
        "Baris & Kolom:",
        "  i / I        : Insert Kolom",
        "  o / O        : Insert Baris",
        "  d / b        : Hapus Kolom / Baris (tekan 2x)",
        "",
        "Data & File:",
        "  s            : Urutkan (Sort)",
        "  w / e        : Simpan / Buka (CSV/TXT)",
        "  u / r        : Undo / Redo",
        "  q            : Keluar",
        "",
        "Formula:",
        "  Awali dengan '=', Contoh: =A1+B2, =SUM(A1:A5)",
        "",
        "Tab:",
        "  t            : Buka tab baru",
        "  T            : Tutup tab aktif",
        "  [ / ]        : Pindah tab sebelumnya/berikutnya",
    };
    int n = sizeof(teks_bantuan)/sizeof(teks_bantuan[0]);
    int i;
    unsigned char buang;

    bersihkan_layar_buffer();
    for (i = 0; i < n && i < tinggi_layar-2; i++)
        taruh_teks(2, 2 + i, teks_bantuan[i], WARNA_DEFAULT);

    taruh_teks(2, tinggi_layar - 1,
               "[ Tekan tombol apapun untuk kembali... ]", WARNA_PUTIH);

    terapkan_delta();
    tunggu_tombol_lanjut();

    while (read(STDIN_FILENO, &buang, 1) > 0);

    if (buffer_aktif >= 0 && buffers) {
        render(buffers[buffer_aktif]);
        flush_stdout();
    }
}
