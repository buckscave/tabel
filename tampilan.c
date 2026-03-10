/* ============================================================
 * TABEL v3.0
 * Berkas: tampilan.c - Tampilan UI (Smart Grid, wcwidth, Poll-Safe)
 * Standar: C89 + POSIX.1-2008
 * Penulis: Chandra Lesmana
 * ============================================================
 * 
 * Catatan Refactoring:
 * - Parameter warna telah dihapus dari semua fungsi menggambar
 * - Warna dikelola melalui state global di warna.c
 * - Gunakan warna_set_depan() dan warna_set_latar() sebelum menggambar
 * - Kompatibilitas legacy dipertahankan melalui warna_glyph
 * - Topbar per-window dengan multi-sheet tabs
 * ============================================================ */

#include "tabel.h"

/* ============================================================
 * KONSTANTA LOKAL
 * ============================================================ */
#define KOLOM_MAKS    512
#define BARIS_MAKS   4096

/* ============================================================
 * BUFFER TAMPILAN
 * ============================================================ */
static struct glyph buffer_depan[BARIS_MAKS][KOLOM_MAKS];
static struct glyph buffer_belakang[BARIS_MAKS][KOLOM_MAKS];
int lebar_layar = 0;
int tinggi_layar = 0;

/* ============================================================
 * CLIP RECT
 * ============================================================ */
static int clip_x1 = 0;
static int clip_y1 = 0;
static int clip_x2 = 9999;
static int clip_y2 = 9999;

/* ============================================================
 * VARIABEL LOKAL
 * ============================================================ */

/* ============================================================
 * HELPER RENDERING - TANPA PARAMETER WARNA
 * ============================================================
 * Semua fungsi di bawah ini menggunakan state warna global
 * yang diatur melalui fungsi warna_set_depan() dan 
 * warna_set_latar() dari modul warna.c
 * ============================================================ */

/* Taruh glyph di posisi tertentu */
static void taruh_glyph(int x, int y, const char *s)
{
    struct glyph *g;
    unsigned char n;
    warna_depan wd;
    warna_latar wl;
    font_style st;

    if (x < clip_x1 || x > clip_x2 || y < clip_y1 || y > clip_y2) {
        return;
    }
    if (x < 1 || y < 1 || x > lebar_layar || y > tinggi_layar) {
        return;
    }

    g = &buffer_belakang[y - 1][x - 1];
    n = (unsigned char)panjang_cp_utf8(s);
    if (n > MAKS_GLYPH) {
        n = MAKS_GLYPH;
    }
    memcpy(g->byte, s, n);
    g->panjang = n;

    /* Ambil warna dan style dari state global */
    wd = warna_get_depan_buffer();
    wl = warna_get_latar_buffer();
    st = warna_get_style_buffer();
    g->warna_depan = wd;
    g->warna_latar = wl;
    g->style = st;
}

/* Taruh spasi di posisi tertentu */
static void taruh_spasi(int x, int y)
{
    struct glyph *g;
    warna_depan wd;
    warna_latar wl;
    font_style st;

    if (x < 1 || y < 1 || x > lebar_layar || y > tinggi_layar) {
        return;
    }
    g = &buffer_belakang[y - 1][x - 1];
    g->byte[0] = ' ';
    g->panjang = 1;

    /* Ambil warna dan style dari state global */
    wd = warna_get_depan_buffer();
    wl = warna_get_latar_buffer();
    st = warna_get_style_buffer();
    g->warna_depan = wd;
    g->warna_latar = wl;
    g->style = st;
}

/* Taruh teks di posisi tertentu */
void taruh_teks(int x, int y, const char *s)
{
    int pos_x = x;
    const char *p = s;

    while (*p && pos_x <= lebar_layar) {
        unsigned char n = (unsigned char)panjang_cp_utf8(p);
        if (n > MAKS_GLYPH) {
            n = MAKS_GLYPH;
        }
        taruh_glyph(pos_x, y, p);
        p += n;
        pos_x++;
    }
}

/* Taruh teks dengan elipsis jika terlalu panjang */
static void taruh_teks_elipsis(int x, int y, const char *s, int max_kol)
{
    int digunakan = 0;
    const char *p = s;

    if (max_kol <= 0) {
        return;
    }

    while (*p && digunakan < max_kol) {
        unsigned char n = (unsigned char)panjang_cp_utf8(p);
        if (digunakan + 1 > max_kol) {
            break;
        }
        taruh_glyph(x + digunakan, y, p);
        digunakan++;
        p += n;
    }

    if (*p && max_kol >= 1) {
        taruh_glyph(x + max_kol - 1, y, "\xE2\x80\xA6");
    }
}

/* Isi garis horizontal dengan spasi */
void isi_garis_h(int x1, int x2, int y)
{
    int x;

    if (y < 1 || y > tinggi_layar) {
        return;
    }
    if (x1 < 1) {
        x1 = 1;
    }
    if (x2 > lebar_layar) {
        x2 = lebar_layar;
    }
    for (x = x1; x <= x2; x++) {
        taruh_spasi(x, y);
    }
}

/* ============================================================
 * CLIP RECT MANAGEMENT
 * ============================================================ */

void set_clip_rect(int x, int y, int w, int h)
{
    clip_x1 = x + 1;
    clip_y1 = y + 1;
    clip_x2 = x + w;
    clip_y2 = y + h;
}

void reset_clip_rect(void)
{
    clip_x1 = 0;
    clip_y1 = 0;
    clip_x2 = 9999;
    clip_y2 = 9999;
}

/* ============================================================
 * FUNGSI TERMINAL DASAR
 * ============================================================ */

void atur_posisi_kursor(int x, int y)
{
    char esc[32];
    int n;

    n = snprintf(esc, sizeof(esc), "\033[%d;%dH", y, x);
    if (n > 0) {
        write(STDOUT_FILENO, esc, (size_t)n);
    }
}

void flush_stdout(void)
{
    fflush(stdout);
}

void masuk_mode_alternatif(void)
{
    write(STDOUT_FILENO, "\033[?1049h\033[?25l", 12);
    write(STDOUT_FILENO, "\033[2J", 4);
}

void keluar_mode_alternatif(void)
{
    write(STDOUT_FILENO, "\033[2J\033[H", 7);
    write(STDOUT_FILENO, "\033[?25h\033[?1049l", 12);
}

void pulihkan_terminal(void)
{
    if (mode_raw_aktif) {
        tcsetattr(STDIN_FILENO, TCSANOW, &term_simpan);
        mode_raw_aktif = 0;
    }
}

void tangani_sinyal_keluar(int sig)
{
    (void)sig;
    pulihkan_terminal();
    keluar_mode_alternatif();
    _exit(0);
}

int ambil_lebar_terminal(void)
{
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1) {
        return 80;
    }
    return ws.ws_col;
}

int ambil_tinggi_terminal(void)
{
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1) {
        return 24;
    }
    return ws.ws_row;
}

void atur_mode_terminal(int blocking)
{
    struct termios t;

    if (!mode_raw_aktif) {
        return;
    }
    tcgetattr(STDIN_FILENO, &t);
    if (blocking) {
        t.c_cc[VMIN] = 1;
        t.c_cc[VTIME] = 0;
    } else {
        t.c_cc[VMIN] = 0;
        t.c_cc[VTIME] = 1;
    }
    tcsetattr(STDIN_FILENO, TCSANOW, &t);
}

void baca_satu_kunci(unsigned char *buf, int *len)
{
    ssize_t n;

    n = read(STDIN_FILENO, buf, 8);
    *len = (n > 0) ? (int)n : 0;
}

/* ============================================================
 * INISIALISASI & SINYAL
 * ============================================================ */

void tangani_sinyal_resize(int sig)
{
    (void)sig;
    perlu_resize = 1;
}

void atur_sigaction(void)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = tangani_sinyal_resize;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGWINCH, &sa, NULL);
    signal(SIGINT, tangani_sinyal_keluar);
    signal(SIGTERM, tangani_sinyal_keluar);
}

int inisialisasi_terminal(void)
{
    struct termios t;
    int r, c;

    if (tcgetattr(STDIN_FILENO, &term_simpan) == -1) {
        return -1;
    }
    t = term_simpan;
    t.c_lflag &= ~(ECHO | ICANON | ISIG);
    t.c_cc[VMIN] = 0;
    t.c_cc[VTIME] = 1;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &t) == -1) {
        return -1;
    }
    mode_raw_aktif = 1;

    lebar_layar = ambil_lebar_terminal();
    tinggi_layar = ambil_tinggi_terminal();

    if (lebar_layar > KOLOM_MAKS) {
        lebar_layar = KOLOM_MAKS;
    }
    if (tinggi_layar > BARIS_MAKS) {
        tinggi_layar = BARIS_MAKS;
    }

    /* Inisialisasi buffer dengan warna default */
    warna_reset_buffer();
    for (r = 0; r < BARIS_MAKS; r++) {
        for (c = 0; c < KOLOM_MAKS; c++) {
            buffer_depan[r][c].byte[0] = ' ';
            buffer_depan[r][c].panjang = 1;
            buffer_depan[r][c].warna_depan = WARNAD_DEFAULT;
            buffer_depan[r][c].warna_latar = WARNAL_DEFAULT;
            buffer_depan[r][c].style = STYLE_NORMAL;

            buffer_belakang[r][c].byte[0] = ' ';
            buffer_belakang[r][c].panjang = 1;
            buffer_belakang[r][c].warna_depan = WARNAD_DEFAULT;
            buffer_belakang[r][c].warna_latar = WARNAL_DEFAULT;
            buffer_belakang[r][c].style = STYLE_NORMAL;
        }
    }

    return 0;
}

void set_konfig_aktif(struct konfigurasi *cfg)
{
    konfig_global = cfg;
}

/* ============================================================
 * FUNGSI I/O BLOCKING KHUSUS
 * ============================================================ */

void tunggu_tombol_lanjut(void)
{
    unsigned char ch;

    atur_mode_terminal(1);
    read(STDIN_FILENO, &ch, 1);
    atur_mode_terminal(0);
}

/* ============================================================
 * PROSES RESIZE TERPUSAT
 * ============================================================ */

/* Reset buffer depan - dipanggil saat resize untuk memaksa full redraw */
static void reset_buffer_depan(void)
{
    int r, c;

    for (r = 0; r < BARIS_MAKS; r++) {
        for (c = 0; c < KOLOM_MAKS; c++) {
            buffer_depan[r][c].byte[0] = '\0'; /* Berbeda dari spasi untuk memaksa redraw */
            buffer_depan[r][c].panjang = 0;
            buffer_depan[r][c].warna_depan = WARNAD_DEFAULT;
            buffer_depan[r][c].warna_latar = WARNAL_DEFAULT;
            buffer_depan[r][c].style = STYLE_NORMAL;
        }
    }
}

void proses_resize_terpusat(void)
{
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1) {
        ws.ws_col = 80;
        ws.ws_row = 24;
    }
    lebar_layar = ws.ws_col;
    tinggi_layar = ws.ws_row;

    if (lebar_layar > KOLOM_MAKS) {
        lebar_layar = KOLOM_MAKS;
    }
    if (tinggi_layar > BARIS_MAKS) {
        tinggi_layar = BARIS_MAKS;
    }

    /* Reset buffer depan untuk memaksa full redraw setelah resize */
    reset_buffer_depan();

    /* Reset terminal ANSI state (warna, style, dll) */
    write(STDOUT_FILENO, "\033[0m", 4);

    paksa_recalc_layout();

    if (jendela_root) {
        bersihkan_layar_buffer();  /* Reset back buffer dan style state */
        render_tree_jendela(jendela_root);
        terapkan_delta();
    } else if (buffer_aktif >= 0 && buffers) {
        render(buffers[buffer_aktif]);
        flush_stdout();
    }
}

/* ============================================================
 * DELTA RENDERER - DENGAN WARNA TERPISAH
 * ============================================================ */

void bersihkan_layar_buffer(void)
{
    int r, c;

    warna_reset_buffer();
    warna_reset_style_buffer();
    for (r = 0; r < tinggi_layar; r++) {
        for (c = 0; c < lebar_layar; c++) {
            buffer_belakang[r][c].byte[0] = ' ';
            buffer_belakang[r][c].panjang = 1;
            buffer_belakang[r][c].warna_depan = WARNAD_DEFAULT;
            buffer_belakang[r][c].warna_latar = WARNAL_DEFAULT;
            buffer_belakang[r][c].style = STYLE_NORMAL;
        }
    }
}

void terapkan_delta(void)
{
    warna_depan wd_saat_ini = WARNAD_DEFAULT;
    warna_latar wl_saat_ini = WARNAL_DEFAULT;
    font_style style_saat_ini = STYLE_NORMAL;
    char seq_buf[80];
    int seq_len;
    int y, x, i;

    for (y = 0; y < tinggi_layar; y++) {
        int awal_run = -1;
        warna_depan wd_run = WARNAD_DEFAULT;
        warna_latar wl_run = WARNAL_DEFAULT;
        font_style style_run = STYLE_NORMAL;

        for (x = 0; x < lebar_layar; x++) {
            struct glyph *b = &buffer_belakang[y][x];
            struct glyph *f = &buffer_depan[y][x];
            int beda;

            beda = (b->panjang != f->panjang) ||
                   (memcmp(b->byte, f->byte, b->panjang) != 0) ||
                   (b->warna_depan != f->warna_depan) ||
                   (b->warna_latar != f->warna_latar) ||
                   (b->style != f->style);

            if (beda) {
                *f = *b;
                if (awal_run < 0) {
                    awal_run = x;
                    wd_run = b->warna_depan;
                    wl_run = b->warna_latar;
                    style_run = b->style;
                } else if (b->warna_depan != wd_run || 
                           b->warna_latar != wl_run ||
                           b->style != style_run) {
                    /* Warna/style berubah, tulis run sebelumnya */
                    atur_posisi_kursor(awal_run + 1, y + 1);

                    if (wd_saat_ini != wd_run || wl_saat_ini != wl_run || 
                        style_saat_ini != style_run) {
                        /* Gabung warna dan style dalam satu sequence */
                        seq_len = warna_seq_gabung_style(wd_run, wl_run, style_run,
                                                         seq_buf, sizeof(seq_buf));
                        if (seq_len > 0) {
                            write(STDOUT_FILENO, seq_buf, (size_t)seq_len);
                        }
                        wd_saat_ini = wd_run;
                        wl_saat_ini = wl_run;
                        style_saat_ini = style_run;
                    }

                    for (i = awal_run; i < x; i++) {
                        write(STDOUT_FILENO, buffer_depan[y][i].byte,
                              buffer_depan[y][i].panjang);
                    }
                    awal_run = x;
                    wd_run = b->warna_depan;
                    wl_run = b->warna_latar;
                    style_run = b->style;
                }
            } else {
                if (awal_run >= 0) {
                    atur_posisi_kursor(awal_run + 1, y + 1);

                    if (wd_saat_ini != wd_run || wl_saat_ini != wl_run ||
                        style_saat_ini != style_run) {
                        /* Gabung warna dan style dalam satu sequence */
                        seq_len = warna_seq_gabung_style(wd_run, wl_run, style_run,
                                                         seq_buf, sizeof(seq_buf));
                        if (seq_len > 0) {
                            write(STDOUT_FILENO, seq_buf, (size_t)seq_len);
                        }
                        wd_saat_ini = wd_run;
                        wl_saat_ini = wl_run;
                        style_saat_ini = style_run;
                    }

                    for (i = awal_run; i < x; i++) {
                        write(STDOUT_FILENO, buffer_depan[y][i].byte,
                              buffer_depan[y][i].panjang);
                    }
                    awal_run = -1;
                }
            }
        }

        if (awal_run >= 0) {
            atur_posisi_kursor(awal_run + 1, y + 1);

            if (wd_saat_ini != wd_run || wl_saat_ini != wl_run ||
                style_saat_ini != style_run) {
                /* Gabung warna dan style dalam satu sequence */
                seq_len = warna_seq_gabung_style(wd_run, wl_run, style_run,
                                                 seq_buf, sizeof(seq_buf));
                if (seq_len > 0) {
                    write(STDOUT_FILENO, seq_buf, (size_t)seq_len);
                }
                wd_saat_ini = wd_run;
                wl_saat_ini = wl_run;
                style_saat_ini = style_run;
            }

            for (i = awal_run; i < lebar_layar; i++) {
                write(STDOUT_FILENO, buffer_depan[y][i].byte,
                      buffer_depan[y][i].panjang);
            }
        }
    }

    write(STDOUT_FILENO, ESC_NORMAL, strlen(ESC_NORMAL));
    write(STDOUT_FILENO, "\033[?25l", 6);
    flush_stdout();
}

/* ============================================================
 * HELPER UNTUK GRID
 * ============================================================ */

static int apakah_tergabung_sama(const struct buffer_tabel *buf,
                                 int c1, int r1, int c2, int r2)
{
    struct info_gabung m1, m2;

    if (!apakah_ada_gabung_buf(buf, c1, r1) ||
        !apakah_ada_gabung_buf(buf, c2, r2)) {
        return 0;
    }

    m1 = buf->tergabung[r1][c1];
    m2 = buf->tergabung[r2][c2];
    return (m1.x_anchor == m2.x_anchor && m1.y_anchor == m2.y_anchor);
}

const char *dapatkan_karakter_grid(const struct buffer_tabel *buf,
    int c, int r, int kolom_mulai, int kolom_akhir,
    int baris_mulai, int baris_akhir)
{
    int atas = 0, bawah = 0, kiri = 0, kanan = 0;

    if (r > baris_mulai) {
        if (c == kolom_mulai) {
            atas = 1;
        } else if (c > kolom_akhir) {
            atas = 1;
        } else if (!apakah_tergabung_sama(buf, c - 1, r - 1, c, r - 1)) {
            atas = 1;
        }
    }
    if (r <= baris_akhir) {
        if (c == kolom_mulai) {
            bawah = 1;
        } else if (c > kolom_akhir) {
            bawah = 1;
        } else if (!apakah_tergabung_sama(buf, c - 1, r, c, r)) {
            bawah = 1;
        }
    }
    if (c > kolom_mulai) {
        if (r == baris_mulai) {
            kiri = 1;
        } else if (r > baris_akhir) {
            kiri = 1;
        } else if (!apakah_tergabung_sama(buf, c - 1, r - 1, c - 1, r)) {
            kiri = 1;
        }
    }
    if (c <= kolom_akhir) {
        if (r == baris_mulai) {
            kanan = 1;
        } else if (r > baris_akhir) {
            kanan = 1;
        } else if (!apakah_tergabung_sama(buf, c, r - 1, c, r)) {
            kanan = 1;
        }
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
 * KOMPONEN UI - MENGGUNAKAN STATE WARNA GLOBAL
 * ============================================================ */

/* Per-Window Topbar dengan File Tabs (kiri) dan Sheet Tabs (kanan) */
static void gambar_tab_atas_window(const struct buffer_tabel *buf,
                                   struct rect area, int is_active)
{
    int y = area.y + 1;
    int kolom = area.x + area.w;
    int i;
    int pos_x;
    int ruang_tab;
    int awal_lembar = 0;
    warna_latar wl_bg;

    /* Background topbar */
    wl_bg = is_active ? WARNAL_HIJAU_TUA : WARNAL_GELAP;
    warna_set_buffer(WARNAD_DEFAULT, wl_bg);
    isi_garis_h(area.x + 1, kolom, y);

    /* ===== BAGIAN KANAN: Sheet Tabs (Lembar) ===== */
    if (buf && buf->lembar && buf->jumlah_lembar > 0) {
        int scroll = buf->lembar_scroll;
        int aktif = buf->lembar_aktif;
        int jumlah = buf->jumlah_lembar;
        int vis_awal, vis_akhir;
        int pos_tab;
        int ada_lebih_kanan = 0;
        int ada_lebih_kiri = 0;
        int lebar_ideal_total;
        int lebar_avail;
        int jumlah_vis;
        int lebar_ideal[MAKS_LEMBAR]; /* Lebar ideal untuk setiap tab */
        int lebar_aktual[MAKS_LEMBAR]; /* Lebar aktual setelah shrink */
        int idx;
        int lebar_min_tab = 5; /* Minimum: " X.. " (spasi, 1 char, elipsis, spasi) */
        int lebar_maks_tab = 20; /* Maksimum lebar tab */
        int perlu_shrink;

        /* Hitung range visible tabs berdasarkan jumlah lembar */
        if (jumlah <= MAKS_LEMBAR_VIS) {
            /* 3 atau kurang: tampilkan semua, tanpa indikator */
            vis_awal = 0;
            vis_akhir = jumlah - 1;
        } else {
            /* Lebih dari 3: perlu scroll dan indikator */
            if (scroll < 0) {
                scroll = 0;
            }
            if (aktif < scroll) {
                scroll = aktif;
            }
            if (aktif >= scroll + MAKS_LEMBAR_VIS) {
                scroll = aktif - MAKS_LEMBAR_VIS + 1;
            }
            vis_awal = scroll;
            vis_akhir = scroll + MAKS_LEMBAR_VIS - 1;
            if (vis_akhir >= jumlah) {
                vis_akhir = jumlah - 1;
                vis_awal = vis_akhir - MAKS_LEMBAR_VIS + 1;
                if (vis_awal < 0) vis_awal = 0;
            }

            /* Cek apakah perlu indikator */
            if (vis_awal > 0) {
                ada_lebih_kiri = 1;
            }
            if (vis_akhir < jumlah - 1) {
                ada_lebih_kanan = 1;
            }
        }

        /* Hitung jumlah tab visible */
        jumlah_vis = vis_akhir - vis_awal + 1;

        /* Hitung lebar ideal untuk setiap tab (nama + 2 spasi) */
        lebar_ideal_total = 0;
        for (idx = 0; idx < jumlah_vis; idx++) {
            int nama_len;
            const char *nama_tab = buf->lembar[vis_awal + idx]->nama;
            nama_len = lebar_tampilan_string_utf8(nama_tab);
            /* Lebar ideal: spasi + nama + spasi */
            lebar_ideal[idx] = nama_len + 2;
            /* Batasi maksimum */
            if (lebar_ideal[idx] > lebar_maks_tab) {
                lebar_ideal[idx] = lebar_maks_tab;
            }
            lebar_ideal_total += lebar_ideal[idx];
        }

        /* Ruang untuk indikator */
        lebar_avail = 0;
        if (ada_lebih_kiri) {
            lebar_avail += 3; /* " < " */
        }
        if (ada_lebih_kanan) {
            lebar_avail += 3; /* " > " */
        }

        /* Posisi mulai sheet tabs dari kanan */
        awal_lembar = kolom - (lebar_ideal_total + lebar_avail) + 1;
        if (awal_lembar < area.x + 1) {
            awal_lembar = area.x + 1;
        }

        /* Hitung lebar yang tersedia untuk tabs */
        lebar_avail = kolom - awal_lembar + 1;
        
        /* Kurangi ruang untuk indikator */
        if (ada_lebih_kiri) lebar_avail -= 3;
        if (ada_lebih_kanan) lebar_avail -= 3;

        /* Cek apakah perlu shrink (terminal dipersempit) */
        perlu_shrink = (lebar_ideal_total > lebar_avail);

        /* Hitung lebar aktual untuk setiap tab */
        if (perlu_shrink) {
            /* Terminal dipersempit, bagi rata ruang yang tersedia */
            int lebar_per_tab = lebar_avail / jumlah_vis;
            if (lebar_per_tab < lebar_min_tab) {
                lebar_per_tab = lebar_min_tab;
            }
            for (idx = 0; idx < jumlah_vis; idx++) {
                lebar_aktual[idx] = lebar_per_tab;
            }
        } else {
            /* Ada cukup ruang, gunakan lebar ideal */
            for (idx = 0; idx < jumlah_vis; idx++) {
                lebar_aktual[idx] = lebar_ideal[idx];
            }
        }

        pos_tab = awal_lembar;

        /* Gambar indikator " < " jika ada tab lebih di kiri */
        if (ada_lebih_kiri) {
            warna_set_buffer(WARNAD_KUNING, wl_bg);
            taruh_glyph(pos_tab, y, " ");
            taruh_glyph(pos_tab + 1, y, "\u276E");
            taruh_glyph(pos_tab + 2, y, " ");
            pos_tab += 3;
        }

        /* Gambar sheet tabs dari kiri ke kanan */
        idx = 0;
        for (i = vis_awal; i <= vis_akhir && pos_tab <= kolom; i++) {
            const char *nama = buf->lembar[i]->nama;
            int nama_len_visual;
            int tab_width;
            int is_last = (i == vis_akhir && !ada_lebih_kanan);
            
            /* Hitung lebar visual nama */
            nama_len_visual = lebar_tampilan_string_utf8(nama);
            
            /* Set warna berdasarkan status aktif */
            if (i == aktif) {
                /* Sheet aktif: hitam dengan hijau */
                warna_set_buffer(WARNAD_HIJAU, WARNAL_HITAM);
            } else {
                /* Sheet non-aktif: hijau tua dengan abu */
                warna_set_buffer(WARNAD_ABU, WARNAL_HIJAU_TUA);
            }

            /* Gunakan lebar aktual yang sudah dihitung */
            tab_width = lebar_aktual[idx];
            idx++;

            /* Jika tab terakhir dan tidak ada indikator kanan, isi sampai ujung */
            if (is_last && !perlu_shrink) {
                int sisa = kolom - pos_tab + 1;
                if (sisa > tab_width) {
                    tab_width = sisa;
                }
            }
            
            /* Gambar tab dengan format " nama " (spasi di kiri dan kanan) */
            /* Spasi di kiri */
            if (pos_tab <= kolom) {
                taruh_glyph(pos_tab, y, " ");
                pos_tab++;
                tab_width--;
            }
            
            /* Tulis nama tab dengan elipsis jika melebihi lebar tab */
            /* Ruang untuk nama: tab_width - 1 (spasi kanan) */
            if (nama_len_visual <= tab_width - 1) {
                /* Nama muat, tulis langsung */
                const char *p = nama;
                while (*p && tab_width > 1) {
                    char glyph[8];
                    int glen = panjang_cp_utf8(p);
                    memcpy(glyph, p, glen);
                    glyph[glen] = '\0';
                    taruh_glyph(pos_tab, y, glyph);
                    pos_tab++;
                    tab_width--;
                    p += glen;
                }
                /* Spasi di kanan */
                if (tab_width > 0 && pos_tab <= kolom) {
                    taruh_glyph(pos_tab, y, " ");
                    pos_tab++;
                    tab_width--;
                }
            } else {
                /* Nama tidak muat, perlu elipsis */
                int max_nama = tab_width - 1;
                int written = 0;
                const char *p = nama;
                
                if (max_nama >= 3) {
                    /* Tulis nama dengan elipsis di akhir */
                    while (*p && written < max_nama - 1) {
                        char glyph[8];
                        int glen = panjang_cp_utf8(p);
                        if (written + 1 > max_nama - 1) break;
                        memcpy(glyph, p, glen);
                        glyph[glen] = '\0';
                        taruh_glyph(pos_tab, y, glyph);
                        pos_tab++;
                        written++;
                        tab_width--;
                        p += glen;
                    }
                    /* Elipsis */
                    taruh_glyph(pos_tab, y, "\xE2\x80\xA6");
                    pos_tab++;
                    tab_width--;
                }
                /* Spasi di kanan */
                if (tab_width > 0 && pos_tab <= kolom) {
                    taruh_glyph(pos_tab, y, " ");
                    pos_tab++;
                }
            }
        }

        /* Gambar indikator " > " jika ada tab lebih di kanan */
        if (ada_lebih_kanan && pos_tab + 2 <= kolom) {
            warna_set_buffer(WARNAD_KUNING, wl_bg);
            taruh_glyph(pos_tab, y, " ");
            taruh_glyph(pos_tab + 1, y, "\u276F");
            taruh_glyph(pos_tab + 2, y, " ");
        }
    }

    /* ===== BAGIAN KIRI: File Tabs (Buffer) ===== */
    ruang_tab = (buf && buf->lembar && buf->jumlah_lembar > 0) ?
                awal_lembar - 3 : kolom - 2;
    if (ruang_tab < 1) {
        ruang_tab = 1;
    }

    pos_x = area.x + 1;
    for (i = 0; i < jumlah_buffer && pos_x < ruang_tab; i++) {
        char buf_tab[128];
        int sisa;
        int tab_len;
        const char *path_file;
        const char *basename;
        warna_latar wl;
        warna_depan wd;

        path_file = (buffers[i]->cfg.path_file[0] ?
                     buffers[i]->cfg.path_file : "[Tanpa Nama]");
        basename = strrchr(path_file, '/');
        if (basename) {
            basename++;
        } else {
            basename = path_file;
        }

        snprintf(buf_tab, sizeof(buf_tab), " %s%s ",
                 basename, buffers[i]->cfg.kotor ? "+" : "");

        wd = (i == buffer_aktif) ? WARNAD_GELAP : WARNAD_ABU;
        wl = (i == buffer_aktif) ? WARNAL_HIJAU : WARNAL_HIJAU_TUA;
        warna_set_buffer(wd, wl);

        sisa = ruang_tab - pos_x;
        if (sisa <= 0) {
            break;
        }

        /* Jika tab melebihi sisa, gunakan elipsis */
        tab_len = (int)strlen(buf_tab);
        if (tab_len > sisa) {
            /* Tulis dengan elipsis */
            taruh_teks_elipsis(pos_x, y, buf_tab, sisa);
            pos_x += sisa;
        } else {
            taruh_teks_elipsis(pos_x, y, buf_tab, sisa);
            pos_x += tab_len;
        }
    }
}

/* Global Message Bar */
static void gambar_message_bar(void)
{
    int kolom = ambil_lebar_terminal();
    int y = tinggi_layar;

    /* Isi seluruh baris dengan spasi untuk menghapus residu pesan sebelumnya */
    warna_set_buffer(WARNAD_DEFAULT, WARNAL_DEFAULT);
    isi_garis_h(1, kolom, y);

    /* Tulis pesan jika ada */
    if (pesan_status[0] != '\0') {
        taruh_teks_elipsis(2, y, pesan_status, kolom - 2);
    }
}

/* Input Bar (Per Window) - Normal Mode (warna abu-abu) */
static void gambar_bar_input_window(const struct buffer_tabel *buf,
                                    struct rect area)
{
    int y = area.y + 2;
    char kol;
    int bar;
    const char *teks;
    char teks_norm[MAKS_TEKS * 2];
    char kiri[MAKS_TEKS * 2];
    int j = 0, i;

    kol = (char)('A' + buf->cfg.aktif_x);
    bar = buf->cfg.aktif_y + 1;
    teks = buf->isi[buf->cfg.aktif_y][buf->cfg.aktif_x];

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

    /* Normal mode: teks abu-abu */
    warna_set_buffer(WARNAD_ABU, WARNAL_DEFAULT);
    isi_garis_h(area.x + 1, area.x + area.w, y);
    taruh_teks_elipsis(area.x + 2, y, kiri, area.w - 2);
}

/* Header Kolom (Per Window) */
static void gambar_label_kolom_window(const struct buffer_tabel *buf,
    struct rect area, int x_awal, int y_awal, int kolom_mulai, int kolom_akhir)
{
    int y = y_awal - 1;
    int x = x_awal + 1;
    int c;

    warna_set_buffer(WARNAD_DEFAULT, WARNAL_DEFAULT);
    isi_garis_h(area.x + 1, area.x + area.w, y);

    for (c = kolom_mulai; c <= kolom_akhir; c++) {
        char ch[4];
        ch[0] = (char)('A' + c);
        ch[1] = '\0';

        if (c == buf->cfg.aktif_x) {
            warna_set_buffer(WARNAD_KUNING, WARNAL_DEFAULT);
        } else {
            warna_set_buffer(WARNAD_ABU, WARNAL_DEFAULT);
        }
        taruh_teks(x + buf->lebar_kolom[c] / 2, y, ch);
        x += buf->lebar_kolom[c] + 1;
    }
}

/* Nomor Baris (Per Window) */
static void gambar_nomor_baris_window(const struct buffer_tabel *buf,
    struct rect area, int y_awal, int baris_mulai, int baris_akhir)
{
    int y = y_awal;
    int r;
    int max_baris = buf->cfg.baris;
    int digit = 1;
    
    /* Hitung jumlah digit untuk nomor baris maksimum */
    while (max_baris >= 10) {
        digit++;
        max_baris /= 10;
    }

    for (r = baris_mulai; r <= baris_akhir; r++) {
        char num[16];
        char fmt[8];
        
        /* Format rata kanan sesuai digit */
        snprintf(fmt, sizeof(fmt), "%%%dd", digit);
        snprintf(num, sizeof(num), fmt, r + 1);

        if (r == buf->cfg.aktif_y) {
            warna_set_buffer(WARNAD_KUNING, WARNAL_DEFAULT);
        } else {
            warna_set_buffer(WARNAD_ABU, WARNAL_DEFAULT);
        }
        /* Taruh di area.x + 2: 1 border + 1 spasi kiri */
        taruh_teks(area.x + 2, y + 1, num);
        y += buf->tinggi_baris[r] + 1;
    }
}

/* Status Bar (Per Window) */
static void gambar_bar_status_window(const struct buffer_tabel *buf,
    struct rect area, int is_active)
{
    int y = area.y + area.h;
    char kiri[384], kanan[256];
    const char *ext = "[unknown]";
    const char *nama_lembar = "Lembar";
    int panjang_kanan_asli, lebar_tampil_kanan, awal_kanan, lebar_maks_kiri;
    warna_latar wl_bg, wl_bg_kanan;

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

    /* Ambil nama lembar aktif */
    if (buf->lembar && buf->lembar_aktif >= 0 && buf->lembar_aktif < buf->jumlah_lembar) {
        nama_lembar = buf->lembar[buf->lembar_aktif]->nama;
    }

    /* Status kanan: tambahkan info lembar "Lembar 1/3" */
    snprintf(kanan, sizeof(kanan),
             " %s %s %s %s Lembar:%d/%d %s Kl:%d/%d Br:%d/%d %s Lb:%d Tg:%d %s ? ",
             buf->cfg.enkoding, V, ext, V,
             buf->lembar_aktif + 1, buf->jumlah_lembar,
             V,
             buf->cfg.aktif_x + 1, buf->cfg.kolom,
             buf->cfg.aktif_y + 1, buf->cfg.baris, V,
             buf->lebar_kolom[buf->cfg.aktif_x],
             buf->tinggi_baris[buf->cfg.aktif_y], V);

    panjang_kanan_asli = lebar_tampilan_string_utf8(kanan);
    lebar_tampil_kanan = (panjang_kanan_asli > area.w) ?
                          area.w : panjang_kanan_asli;
    awal_kanan = area.x + area.w - lebar_tampil_kanan + 1;
    if (awal_kanan < area.x + 1) {
        awal_kanan = area.x + 1;
    }

    /* Status kiri: path file + nama lembar "/path/file:Lembar 1" */
    snprintf(kiri, sizeof(kiri), "%s:%s",
            buf->cfg.path_file[0] ? buf->cfg.path_file : "[Tanpa Nama]",
            nama_lembar);

    wl_bg = is_active ? WARNAL_HIJAU_TUA : WARNAL_GELAP;
    wl_bg_kanan = is_active ? WARNAL_HIJAU : WARNAL_ABU;

    warna_set_buffer(WARNAD_DEFAULT, wl_bg);
    isi_garis_h(area.x + 1, area.x + area.w, y);

    warna_set_buffer(WARNAD_GELAP, wl_bg_kanan);
    taruh_teks_elipsis(awal_kanan, y, kanan, lebar_tampil_kanan);

    lebar_maks_kiri = awal_kanan - (area.x + 2);
    if (lebar_maks_kiri > 2) {
        warna_set_buffer(WARNAD_DEFAULT, wl_bg);
        taruh_teks_elipsis(area.x + 1, y, kiri, lebar_maks_kiri);
    }
}

/* ============================================================
 * RENDERER GRID - MENGGUNAKAN STATE WARNA GLOBAL
 * ============================================================ */

static void gambar_grid_cerdas(const struct buffer_tabel *buf,
    int x_awal, int y_awal, int kolom_mulai, int kolom_akhir,
    int baris_mulai, int baris_akhir)
{
    int py = y_awal;
    int r, c, k;

    warna_set_buffer(WARNAD_GELAP, WARNAL_DEFAULT);

    for (r = baris_mulai; r <= baris_akhir + 1; r++) {
        int px = x_awal;
        for (c = kolom_mulai; c <= kolom_akhir + 1; c++) {
            const char *sambungan = dapatkan_karakter_grid(buf, c, r,
                    kolom_mulai, kolom_akhir, baris_mulai, baris_akhir);
            taruh_teks(px, py, sambungan);

            if (c <= kolom_akhir) {
                int gambar_h = 1;
                if (r > baris_mulai && r <= baris_akhir) {
                    if (apakah_ada_gabung_buf(buf, c, r - 1) &&
                        apakah_ada_gabung_buf(buf, c, r)) {
                        gambar_h = 0;
                    }
                }
                if (gambar_h) {
                    for (k = 0; k < buf->lebar_kolom[c]; k++) {
                        taruh_teks(px + 1 + k, py, H);
                    }
                }
                px += buf->lebar_kolom[c] + 1;
            }
        }
        if (r <= baris_akhir) {
            int px_v = x_awal;
            for (c = kolom_mulai; c <= kolom_akhir + 1; c++) {
                int gambar_v = 1;
                if (c > kolom_mulai && c <= kolom_akhir) {
                    if (apakah_ada_gabung_buf(buf, c - 1, r) &&
                        apakah_ada_gabung_buf(buf, c, r)) {
                        gambar_v = 0;
                    }
                }
                if (gambar_v) {
                    for (k = 0; k < buf->tinggi_baris[r]; k++) {
                        taruh_teks(px_v, py + 1 + k, V);
                    }
                }
                if (c <= kolom_akhir) {
                    px_v += buf->lebar_kolom[c] + 1;
                }
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
    const char *teks;
    warna_depan wd_teks;
    warna_latar wl_teks;
    font_style style_teks;
    char teks_norm[MAKS_TEKS];
    int j = 0;
    int start = 0;
    int baris = 0;

    /* Ambil warna dari buffer */
    if (buf->warna_sel) {
        wd_teks = buf->warna_sel[ay][ax].depan;
        wl_teks = buf->warna_sel[ay][ax].latar;
    } else {
        wd_teks = WARNAD_DEFAULT;
        wl_teks = WARNAL_DEFAULT;
    }

    /* Ambil style dari buffer */
    if (buf->style_sel) {
        style_teks = buf->style_sel[ay][ax];
    } else {
        style_teks = STYLE_NORMAL;
    }

    if (apakah_ada_gabung_buf(buf, c, r)) {
        if (!apakah_sel_anchor_buf(buf, c, r)) {
            return;
        }
        ax = buf->tergabung[r][c].x_anchor;
        ay = buf->tergabung[r][c].y_anchor;
        lebar_sel = buf->tergabung[ay][ax].lebar;
        tinggi_sel = buf->tergabung[ay][ax].tinggi;
    }

    x0 = x_awal + 1;
    y0 = y_awal;
    for (i = kolom_mulai; i < ax; i++) {
        x0 += buf->lebar_kolom[i] + 1;
    }
    for (i = baris_mulai; i < ay; i++) {
        y0 += buf->tinggi_baris[i] + 1;
    }
    for (i = 0; i < lebar_sel; i++) {
        bw += buf->lebar_kolom[ax + i] + 1;
    }
    for (i = 0; i < tinggi_sel; i++) {
        bh += buf->tinggi_baris[ay + i] + 1;
    }

    lebar_vis = bw - 1;
    tinggi_vis = bh - 1;

    /* Isi background sel (tanpa style, hanya warna) */
    warna_set_buffer(WARNAD_DEFAULT, wl_teks);
    warna_set_style_buffer(STYLE_NORMAL);
    for (i = 0; i < tinggi_vis; i++) {
        for (k = 0; k < lebar_vis; k++) {
            taruh_spasi(x0 + k, y0 + 1 + i);
        }
    }

    teks = buf->isi[ay][ax];
    if (teks[0] == '\0') {
        return;
    }

    /* Tentukan warna teks */
    if (wd_teks == WARNAD_DEFAULT) {
        if (teks[0] == '=') {
            double val;
            int adalah_angka;
            char buf_err[64];

            if (evaluasi_string(buf, ax, ay, teks, &val, &adalah_angka,
                        buf_err, sizeof(buf_err)) == 0 && adalah_angka) {
                static char buf_num[MAKS_TEKS];
                jenis_format_sel fmt = ambil_format_sel(buf, ax, ay);

                if (fmt == FORMAT_UMUM) {
                    fmt = (jenis_format_sel)deteksi_format_otomatis(val);
                }

                if (fmt != FORMAT_UMUM && fmt != FORMAT_ANGKA) {
                    format_nilai_ke_teks(val, fmt, buf_num, sizeof(buf_num));
                } else {
                    snprintf(buf_num, sizeof(buf_num), "%.10g", val);
                }
                teks = buf_num;
                wd_teks = WARNAD_MERAH;
            } else {
                teks = "#ERR";
                wd_teks = WARNAD_MERAH;
            }
        } else {
            char *endptr;
            strtod(teks, &endptr);
            if (*endptr == '\0' && teks[0] != '\0') {
                wd_teks = WARNAD_MERAH;
            }
        }
    }

    /* Normalisasi teks */
    for (i = 0; teks[i] && j < MAKS_TEKS - 1; i++) {
        if (teks[i] == '\\' && teks[i + 1] == 'n') {
            teks_norm[j++] = '\n';
            i++;
        } else {
            teks_norm[j++] = teks[i];
        }
    }
    teks_norm[j] = '\0';

    /* Render teks per baris */
    warna_set_buffer(wd_teks, wl_teks);
    warna_set_style_buffer(style_teks);
    while (teks_norm[start] && baris < tinggi_vis) {
        char line[MAKS_TEKS];
        int idx = 0;
        int kol, offset = 0;

        while (teks_norm[start] && teks_norm[start] != '\n' &&
               idx < MAKS_TEKS - 1) {
            line[idx++] = teks_norm[start++];
        }
        line[idx] = '\0';

        if (teks_norm[start] == '\n') {
            start++;
        }

        kol = lebar_tampilan_string_utf8(line);
        if (buf->perataan_sel[ay][ax] == RATA_TENGAH) {
            offset = (lebar_vis - kol) / 2;
        } else if (buf->perataan_sel[ay][ax] == RATA_KANAN) {
            offset = (lebar_vis - kol);
        }

        taruh_teks_elipsis(x0 + offset, y0 + 1 + baris, line, lebar_vis);
        baris++;
    }
}

static void gambar_kotak_outline(const struct buffer_tabel *buf,
    int x_awal, int y_awal, int ks, int ke, int bs, int be,
    int x1, int y1, int x2, int y2)
{
    int minx = x1 < x2 ? x1 : x2;
    int maxx = x1 > x2 ? x1 : x2;
    int miny = y1 < y2 ? y1 : y2;
    int maxy = y1 > y2 ? y1 : y2;
    int cx, cy, px1, px2, py1, py2, i;

    /* Warna outline diambil dari state global */
    if (maxx < ks || minx > ke || maxy < bs || miny > be) {
        return;
    }

    cx = x_awal;
    for (i = ks; i < minx; i++) {
        cx += buf->lebar_kolom[i] + 1;
    }
    px1 = cx;
    for (i = (minx < ks ? ks : minx); i <= maxx; i++) {
        cx += buf->lebar_kolom[i] + 1;
    }
    px2 = cx;

    cy = y_awal;
    for (i = bs; i < miny; i++) {
        cy += buf->tinggi_baris[i] + 1;
    }
    py1 = cy;
    for (i = (miny < bs ? bs : miny); i <= maxy; i++) {
        cy += buf->tinggi_baris[i] + 1;
    }
    py2 = cy;

    if (miny >= bs) {
        taruh_teks(px1, py1, TL);
        for (i = px1 + 1; i < px2; i++) {
            taruh_teks(i, py1, H);
        }
        taruh_teks(px2, py1, TR);
    }
    if (maxy <= be) {
        taruh_teks(px1, py2, BL);
        for (i = px1 + 1; i < px2; i++) {
            taruh_teks(i, py2, H);
        }
        taruh_teks(px2, py2, BR);
    }
    if (minx >= ks) {
        for (i = py1 + 1; i < py2; i++) {
            taruh_teks(px1, i, V);
        }
    }
    if (maxx <= ke) {
        for (i = py1 + 1; i < py2; i++) {
            taruh_teks(px2, i, V);
        }
    }
}

/* ============================================================
 * BORDER HANDLING
 * ============================================================ */

static int cek_garis_horizontal(const struct buffer_tabel *buf, int c, int r)
{
    int ada = 0;

    if (r < buf->cfg.baris && c < buf->cfg.kolom && c >= 0 && r >= 0) {
        if (buf->border_sel[r][c].top) {
            ada = 1;
        }
    }
    if (r > 0 && c < buf->cfg.kolom && c >= 0) {
        if (buf->border_sel[r - 1][c].bottom) {
            ada = 1;
        }
    }
    return ada;
}

static int cek_garis_vertikal(const struct buffer_tabel *buf, int c, int r)
{
    int ada = 0;

    if (r < buf->cfg.baris && c < buf->cfg.kolom && c >= 0 && r >= 0) {
        if (buf->border_sel[r][c].left) {
            ada = 1;
        }
    }
    if (c > 0 && r < buf->cfg.baris && r >= 0) {
        if (buf->border_sel[r][c - 1].right) {
            ada = 1;
        }
    }
    return ada;
}

static void ambil_warna_border(const struct buffer_tabel *buf,
                               int c, int r, int vertikal,
                               warna_depan *wd, warna_latar *wl)
{
    struct info_border *b;

    if (r < 0 || r >= buf->cfg.baris || c < 0 || c >= buf->cfg.kolom) {
        *wd = WARNAD_PUTIH;
        *wl = WARNAL_DEFAULT;
        return;
    }

    b = &buf->border_sel[r][c];
    if (vertikal) {
        if (b->left) {
            *wd = b->warna_depan;
            *wl = b->warna_latar;
            return;
        }
        if (c > 0 && buf->border_sel[r][c - 1].right) {
            *wd = buf->border_sel[r][c - 1].warna_depan;
            *wl = buf->border_sel[r][c - 1].warna_latar;
            return;
        }
    } else {
        if (b->top) {
            *wd = b->warna_depan;
            *wl = b->warna_latar;
            return;
        }
        if (r > 0 && buf->border_sel[r - 1][c].bottom) {
            *wd = buf->border_sel[r - 1][c].warna_depan;
            *wl = buf->border_sel[r - 1][c].warna_latar;
            return;
        }
    }
    *wd = WARNAD_PUTIH;
    *wl = WARNAL_DEFAULT;
}

static const char *resolve_border_char(int up, int down, int left, int right)
{
    int sum = up + down + left + right;

    if (sum < 2) {
        return NULL;
    }
    if (sum == 4) {
        return CR;
    }
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
            warna_depan wd = WARNAD_PUTIH;
            warna_latar wl = WARNAL_DEFAULT;
            int ada_koneksi = 0;

            if (cek_garis_vertikal(buf, c, r - 1)) {
                up = 1;
                ada_koneksi = 1;
                ambil_warna_border(buf, c, r - 1, 1, &wd, &wl);
            }
            if (cek_garis_vertikal(buf, c, r)) {
                down = 1;
                ada_koneksi = 1;
                if (!up) {
                    ambil_warna_border(buf, c, r, 1, &wd, &wl);
                }
            }
            if (cek_garis_horizontal(buf, c - 1, r)) {
                left = 1;
                ada_koneksi = 1;
                if (!up && !down) {
                    ambil_warna_border(buf, c - 1, r, 0, &wd, &wl);
                }
            }
            if (cek_garis_horizontal(buf, c, r)) {
                right = 1;
                ada_koneksi = 1;
                if (!up && !down && !left) {
                    ambil_warna_border(buf, c, r, 0, &wd, &wl);
                }
            }

            if (ada_koneksi) {
                const char *glyph = resolve_border_char(up, down, left, right);
                if (glyph) {
                    warna_set_buffer(wd, wl);
                    taruh_teks(px, py, glyph);
                }
            }

            if (right && c <= ke) {
                warna_depan wd2;
                warna_latar wl2;
                int lebar = buf->lebar_kolom[c];
                int k;

                ambil_warna_border(buf, c, r, 0, &wd2, &wl2);
                warna_set_buffer(wd2, wl2);
                for (k = 1; k <= lebar; k++) {
                    taruh_teks(px + k, py, H);
                }
            }

            if (down && r <= be) {
                warna_depan wd2;
                warna_latar wl2;
                int tinggi = buf->tinggi_baris[r];
                int k;

                ambil_warna_border(buf, c, r, 1, &wd2, &wl2);
                warna_set_buffer(wd2, wl2);
                for (k = 1; k <= tinggi; k++) {
                    taruh_teks(px, py + k, V);
                }
            }

            if (c <= ke) {
                px += buf->lebar_kolom[c] + 1;
            }
        }
        if (r <= be) {
            py += buf->tinggi_baris[r] + 1;
        }
    }
}

/* ============================================================
 * KALKULASI VIEWPORT
 * ============================================================ */

static void hitung_viewport_window(const struct buffer_tabel *buf,
    struct rect area, int *x_awal, int *y_awal, int *lebar_vis, int *tinggi_vis,
    int *kolom_mulai, int *kolom_akhir, int *baris_mulai, int *baris_akhir)
{
    const struct konfigurasi *cfg = &buf->cfg;
    int max_baris = cfg->baris;
    int digit = 1;
    int padding_kiri;
    int tinggi_tersedia;
    int lebar_tersedia;
    int c, x, r, y;
    /* Layout: border(1) + spasi(1) + nomor(digit) + spasi(1) = digit + 3 */
    
    /* Hitung jumlah digit untuk nomor baris maksimum */
    while (max_baris >= 10) {
        digit++;
        max_baris /= 10;
    }
    padding_kiri = digit + 3;
    
    /* Account for topbar (1) + input bar (1) + column header (1) = 3 */
    /* But now topbar is per-window, so we need 4 lines from area.y */
    tinggi_tersedia = area.h - 4;
    lebar_tersedia = area.w - padding_kiri;

    if (tinggi_tersedia < 1) {
        tinggi_tersedia = 1;
    }
    if (lebar_tersedia < 8) {
        lebar_tersedia = 8;
    }

    *x_awal = area.x + padding_kiri;
    *y_awal = area.y + 4;

    *lebar_vis = lebar_tersedia;
    *tinggi_vis = tinggi_tersedia;

    c = cfg->lihat_kolom;
    x = 0;
    while (c < cfg->kolom) {
        int w = buf->lebar_kolom[c] + 1;
        if (x + w > lebar_tersedia) {
            break;
        }
        x += w;
        c++;
    }
    *kolom_mulai = cfg->lihat_kolom;
    *kolom_akhir = (c - 1 < *kolom_mulai) ? *kolom_mulai : c - 1;

    r = cfg->lihat_baris;
    y = 0;
    while (r < cfg->baris) {
        int h = buf->tinggi_baris[r] + 1;
        if (y + h > tinggi_tersedia) {
            break;
        }
        y += h;
        r++;
    }
    *baris_mulai = cfg->lihat_baris;
    *baris_akhir = (r - 1 < *baris_mulai) ? *baris_mulai : r - 1;
}

void hitung_viewport(const struct buffer_tabel *buf,
    int *x_awal, int *y_awal, int *lebar_vis, int *tinggi_vis,
    int *kolom_mulai, int *kolom_akhir, int *baris_mulai, int *baris_akhir)
{
    struct rect area;

    if (jendela_aktif && jendela_aktif->jenis == JENIS_DAUN) {
        area = jendela_aktif->rect;
    } else {
        area.x = 0;
        area.y = 0;
        area.w = lebar_layar;
        area.h = tinggi_layar - 1;
    }

    hitung_viewport_window(buf, area, x_awal, y_awal, lebar_vis, tinggi_vis,
                           kolom_mulai, kolom_akhir, baris_mulai, baris_akhir);
}

/* ============================================================
 * OVERLAY PROMPTS
 * ============================================================ */

void prompt_warna(struct buffer_tabel *buf, int mode_teks)
{
    int pilihan_depan = 0;
    int pilihan_latar = 0;
    int mode_warna = 0;  /* 0 = depan, 1 = latar */
    int x1, y1, x2, y2, min_x, max_x, min_y, max_y;
    int area_size;
    struct warna_kombinasi *backup = NULL;
    int i;

    x1 = seleksi_aktif ? seleksi_x1 : buf->cfg.aktif_x;
    y1 = seleksi_aktif ? seleksi_y1 : buf->cfg.aktif_y;
    x2 = seleksi_aktif ? seleksi_x2 : buf->cfg.aktif_x;
    y2 = seleksi_aktif ? seleksi_y2 : buf->cfg.aktif_y;

    min_x = (x1 < x2) ? x1 : x2;
    max_x = (x1 > x2) ? x1 : x2;
    min_y = (y1 < y2) ? y1 : y2;
    max_y = (y1 > y2) ? y1 : y2;
    area_size = (max_y - min_y + 1) * (max_x - min_x + 1);

    if (mode_teks && buf->warna_sel) {
        backup = malloc(area_size * sizeof(struct warna_kombinasi));
        if (backup) {
            int idx = 0, r, c;
            for (r = min_y; r <= max_y; r++) {
                for (c = min_x; c <= max_x; c++) {
                    backup[idx++] = buf->warna_sel[r][c];
                }
            }
        }
    }

    atur_mode_terminal(1);

    while (1) {
        int y_ui, x_pos;
        char label[64];
        unsigned char ch[8];
        int len;
        int jumlah;
        warna_depan wd;
        warna_latar wl;

        if (mode_teks && buf->warna_sel) {
            int r, c;
            wd = (warna_depan)pilihan_depan;
            wl = (warna_latar)pilihan_latar;
            for (r = min_y; r <= max_y; r++) {
                for (c = min_x; c <= max_x; c++) {
                    buf->warna_sel[r][c].depan = wd;
                    buf->warna_sel[r][c].latar = wl;
                }
            }
        }

        render(buf);

        if (!mode_teks) {
            int x_awal, y_awal, lv, tv, ks, ke, bs, be;
            hitung_viewport(buf, &x_awal, &y_awal, &lv, &tv,
                            &ks, &ke, &bs, &be);
            warna_set_buffer((warna_depan)pilihan_depan, 
                             (warna_latar)pilihan_latar);
            gambar_kotak_outline(buf, x_awal, y_awal, ks, ke, bs, be,
                                 x1, y1, x2, y2);
        }

        y_ui = tinggi_layar;
        warna_set_buffer(WARNAD_DEFAULT, WARNAL_DEFAULT);
        isi_garis_h(1, lebar_layar, y_ui);

        snprintf(label, sizeof(label), "Warna %s (Tab: ganti mode): ",
                 mode_warna == 0 ? "Depan" : "Latar");
        warna_set_buffer(WARNAD_PUTIH, WARNAL_DEFAULT);
        taruh_teks(2, y_ui, label);

        x_pos = 2 + (int)strlen(label);

        if (mode_warna == 0) {
            jumlah = warna_jumlah_depan();
            for (i = 0; i < jumlah; i++) {
                const char *nama;
                warna_info_depan(i, &wd, &nama);
                if (i == pilihan_depan) {
                    warna_set_buffer(WARNAD_KUNING, WARNAL_DEFAULT);
                    taruh_teks(x_pos, y_ui, "[");
                    warna_set_buffer(WARNAD_PUTIH, WARNAL_DEFAULT);
                    taruh_teks(x_pos + 1, y_ui, nama);
                    warna_set_buffer(WARNAD_KUNING, WARNAL_DEFAULT);
                    taruh_teks(x_pos + 1 + (int)strlen(nama), y_ui, "]");
                    x_pos += (int)strlen(nama) + 4;
                } else {
                    warna_set_buffer(WARNAD_ABU, WARNAL_DEFAULT);
                    taruh_teks(x_pos, y_ui, nama);
                    x_pos += (int)strlen(nama) + 2;
                }
            }
        } else {
            jumlah = warna_jumlah_latar();
            for (i = 0; i < jumlah; i++) {
                const char *nama;
                warna_info_latar(i, &wl, &nama);
                if (i == pilihan_latar) {
                    warna_set_buffer(WARNAD_KUNING, WARNAL_DEFAULT);
                    taruh_teks(x_pos, y_ui, "[");
                    warna_set_buffer(WARNAD_PUTIH, wl);
                    taruh_teks(x_pos + 1, y_ui, nama);
                    warna_set_buffer(WARNAD_KUNING, WARNAL_DEFAULT);
                    taruh_teks(x_pos + 1 + (int)strlen(nama), y_ui, "]");
                    x_pos += (int)strlen(nama) + 4;
                } else {
                    warna_set_buffer(WARNAD_ABU, WARNAL_DEFAULT);
                    taruh_teks(x_pos, y_ui, nama);
                    x_pos += (int)strlen(nama) + 2;
                }
            }
        }

        terapkan_delta();

        baca_satu_kunci(ch, &len);

        if (ch[0] == 27 && len == 1) {
            if (mode_teks && backup) {
                int idx = 0, r, c;
                for (r = min_y; r <= max_y; r++) {
                    for (c = min_x; c <= max_x; c++) {
                        buf->warna_sel[r][c] = backup[idx++];
                    }
                }
            }
            break;
        } else if (ch[0] == '\t') {
            mode_warna = (mode_warna + 1) % 2;
        } else if (ch[0] == '\r' || ch[0] == '\n') {
            if (!mode_teks) {
                int r, c;
                warna_depan wd_final = (warna_depan)pilihan_depan;
                warna_latar wl_final = (warna_latar)pilihan_latar;
                for (r = min_y; r <= max_y; r++) {
                    for (c = min_x; c <= max_x; c++) {
                        buf->border_sel[r][c].warna_depan = wd_final;
                        buf->border_sel[r][c].warna_latar = wl_final;
                    }
                }
            }
            buf->cfg.kotor = 1;
            break;
        } else if (ch[0] == 27 && len >= 3 && ch[1] == '[') {
            if (mode_warna == 0) {
                jumlah = warna_jumlah_depan();
                if (ch[2] == 'C') {
                    pilihan_depan = (pilihan_depan + 1) % jumlah;
                }
                if (ch[2] == 'D') {
                    pilihan_depan = (pilihan_depan - 1 + jumlah) % jumlah;
                }
            } else {
                jumlah = warna_jumlah_latar();
                if (ch[2] == 'C') {
                    pilihan_latar = (pilihan_latar + 1) % jumlah;
                }
                if (ch[2] == 'D') {
                    pilihan_latar = (pilihan_latar - 1 + jumlah) % jumlah;
                }
            }
        }
    }

    if (backup) {
        free(backup);
    }
    atur_mode_terminal(0);
    render(buf);
}

/* ============================================================
 * RENDER UTAMA TERPADU
 * ============================================================ */

void render_jendela_spesifik(const struct buffer_tabel *buf,
                             struct rect area, int is_active)
{
    int x_awal, y_awal, lv, tv, ks, ke, bs, be;
    int r, c;
    int ax, ay, bx, by;
    int i;

    set_clip_rect(area.x, area.y, area.w, area.h);

    /* Gambar per-window topbar dengan file tabs dan sheet tabs */
    gambar_tab_atas_window(buf, area, is_active);

    /* Gambar input bar */
    gambar_bar_input_window(buf, area);

    hitung_viewport_window(buf, area, &x_awal, &y_awal, &lv, &tv,
                           &ks, &ke, &bs, &be);

    gambar_label_kolom_window(buf, area, x_awal, y_awal, ks, ke);
    gambar_nomor_baris_window(buf, area, y_awal, bs, be);

    gambar_grid_cerdas(buf, x_awal, y_awal, ks, ke, bs, be);

    for (r = bs; r <= be; r++) {
        for (c = ks; c <= ke; c++) {
            gambar_isi_sel_lihat(buf, x_awal, y_awal, ks, bs, c, r);
        }
    }

    gambar_layer_border_smart(buf, x_awal, y_awal, ks, ke, bs, be);

    /* Outline untuk clipboard area */
    if (klip_ada_area) {
        warna_set_buffer(WARNAD_HIJAU, WARNAL_DEFAULT);
        gambar_kotak_outline(buf, x_awal, y_awal, ks, ke, bs, be,
                             klip_x1, klip_y1, klip_x2, klip_y2);
    }

    /* Outline untuk seleksi */
    if (sedang_memilih && jangkar_x >= 0) {
        warna_set_buffer(WARNAD_BIRU, WARNAL_DEFAULT);
        gambar_kotak_outline(buf, x_awal, y_awal, ks, ke, bs, be,
                             jangkar_x, jangkar_y,
                             buf->cfg.aktif_x, buf->cfg.aktif_y);
    }
    if (seleksi_aktif) {
        warna_set_buffer(WARNAD_BIRU, WARNAL_DEFAULT);
        gambar_kotak_outline(buf, x_awal, y_awal, ks, ke, bs, be,
                             seleksi_x1, seleksi_y1,
                             seleksi_x2, seleksi_y2);
    }

    /* Outline sel aktif */
    ax = buf->cfg.aktif_x;
    ay = buf->cfg.aktif_y;
    bx = ax;
    by = ay;
    if (apakah_ada_gabung_buf(buf, ax, ay)) {
        bx = buf->tergabung[ay][ax].x_anchor +
             buf->tergabung[ay][ax].lebar - 1;
        by = buf->tergabung[ay][ax].y_anchor +
             buf->tergabung[ay][ax].tinggi - 1;
        ax = buf->tergabung[ay][ax].x_anchor;
        ay = buf->tergabung[ay][ax].y_anchor;
    }
    warna_set_buffer(WARNAD_PUTIH, WARNAL_DEFAULT);
    gambar_kotak_outline(buf, x_awal, y_awal, ks, ke, bs, be,
                         ax, ay, bx, by);

    /* Outline seleksi random */
    for (i = 0; i < seleksi_random; i++) {
        int cx = seleksi_random_x[i];
        int cy = seleksi_random_y[i];
        if (cx < 0 || cy < 0 || cx >= buf->cfg.kolom || cy >= buf->cfg.baris) {
            continue;
        }
        warna_set_buffer(WARNAD_BIRU, WARNAL_DEFAULT);
        gambar_kotak_outline(buf, x_awal, y_awal, ks, ke, bs, be,
                             cx, cy, cx, cy);
    }

    gambar_bar_status_window(buf, area, is_active);

    reset_clip_rect();
}

void render_semua_jendela(void)
{
    struct rect area;

    bersihkan_layar_buffer();
    /* Tidak ada global topbar lagi - topbar sekarang per-window */

    if (jendela_root) {
        render_tree_jendela(jendela_root);
    } else {
        if (buffer_aktif >= 0 && buffers) {
            area.x = 0;
            area.y = 0;
            area.w = lebar_layar;
            area.h = tinggi_layar - 1;
            render_jendela_spesifik(buffers[buffer_aktif], area, 1);
        }
    }

    gambar_message_bar();
    pesan_status[0] = '\0';
    terapkan_delta();
}

void render(const struct buffer_tabel *buf)
{
    (void)buf;
    render_semua_jendela();
}
