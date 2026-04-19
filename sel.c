/* ============================================================
 * TABEL v3.0 
 * Berkas: sel.c - Operasi Sel, Baris, dan Kolom
 * Standar: C89 + POSIX.1-2008
 * Penulis: Chandra Lesmana
 * ============================================================
 * 
 * Berkas ini berisi fungsi-fungsi untuk:
 * - Manipulasi teks sel
 * - Perataan dan format sel
 * - Warna dan border sel
 * - Merge/unmerge sel
 * - Operasi seleksi
 * - Operasi clipboard
 * - Navigasi sel
 * - Operasi struktur (insert/delete baris/kolom)
 * - Sortir dan pencarian
 * ============================================================ */

#include "tabel.h"

/* ============================================================
 * VARIABEL EKSTERNAL (dari tabel.c)
 * ============================================================ */
extern char papan_klip[MAKS_TEKS];
extern char ***papan_klip_area;
extern int klip_x1, klip_y1, klip_x2, klip_y2;
extern int klip_ada_area;
extern int klip_mode;

extern int sedang_memilih;
extern int jangkar_x, jangkar_y;
extern int seleksi_x1, seleksi_y1, seleksi_x2, seleksi_y2;
extern int seleksi_aktif;
extern int seleksi_random;
extern int seleksi_random_x[MAKS_SELEKSI];
extern int seleksi_random_y[MAKS_SELEKSI];
extern char pesan_status[256];

/* ============================================================
 * MANIPULASI TEKS SEL
 * ============================================================ */

/* Atur teks sel dengan opsi undo */
void atur_teks_sel(struct buffer_tabel *buf, int x, int y,
                   const char *teks, int catat_undo) {
    char sebelum[MAKS_TEKS];
    salin_str_aman(sebelum, buf->isi[y][x], MAKS_TEKS);
    salin_str_aman(buf->isi[y][x], teks, MAKS_TEKS);
    if (catat_undo) dorong_undo_teks(buf, x, y, sebelum, buf->isi[y][x]);
    buf->cfg.kotor = 1;
}

/* Atur teks sel dengan format (untuk formula) */
void atur_teks_sel_fmt(struct buffer_tabel *buf, int x, int y,
                       const char *teks, int catat_undo) {
    atur_teks_sel(buf, x, y, teks, catat_undo);
}

/* ============================================================
 * PERATAAN TEKS
 * ============================================================ */

/* Ubah perataan sel aktif */
void ubah_perataan_sel(struct buffer_tabel *buf, perataan_teks p) {
    int x = buf->cfg.aktif_x;
    int y = buf->cfg.aktif_y;
    if (apakah_ada_gabung_buf(buf, x, y)) {
        x = buf->tergabung[y][x].x_anchor;
        y = buf->tergabung[y][x].y_anchor;
    }
    buf->perataan_sel[y][x] = p;
    dorong_undo_teks(buf, x, y, "", buf->isi[y][x]);
    buf->cfg.kotor = 1;
}

/* Ubah perataan area seleksi */
void ubah_perataan_area(struct buffer_tabel *buf, perataan_teks p) {
    int x1, y1, x2, y2;
    int r, c;
    char nama_kol1[4], nama_kol2[4];

    if (seleksi_aktif) {
        x1 = seleksi_x1 < seleksi_x2 ? seleksi_x1 : seleksi_x2;
        y1 = seleksi_y1 < seleksi_y2 ? seleksi_y1 : seleksi_y2;
        x2 = seleksi_x1 > seleksi_x2 ? seleksi_x1 : seleksi_x2;
        y2 = seleksi_y1 > seleksi_y2 ? seleksi_y1 : seleksi_y2;
    } else if (sedang_memilih && jangkar_x != -1) {
        x1 = jangkar_x < buf->cfg.aktif_x ? jangkar_x : buf->cfg.aktif_x;
        y1 = jangkar_y < buf->cfg.aktif_y ? jangkar_y : buf->cfg.aktif_y;
        x2 = jangkar_x > buf->cfg.aktif_x ? jangkar_x : buf->cfg.aktif_x;
        y2 = jangkar_y > buf->cfg.aktif_y ? jangkar_y : buf->cfg.aktif_y;
    } else {
        ubah_perataan_sel(buf, p);
        return;
    }

    for (r = y1; r <= y2; r++) {
        for (c = x1; c <= x2; c++) {
            buf->perataan_sel[r][c] = p;
        }
    }
    buf->cfg.kotor = 1;
    kolom_ke_nama(x1, nama_kol1, sizeof(nama_kol1));
    kolom_ke_nama(x2, nama_kol2, sizeof(nama_kol2));
    snprintf(pesan_status, sizeof(pesan_status),
             "Perataan diubah (%s%d:%s%d)", nama_kol1, y1 + 1, nama_kol2, y2 + 1);
}

/* ============================================================
 * KONVERSI HURUF UTF-8
 * ============================================================ */

static void ke_huruf_besar_utf8(char *dst, const char *src, size_t n) {
    size_t i = 0;
    while (src[i] && n > 1) {
        unsigned char c = (unsigned char)src[i];
        if (c < 0x80) {
            *dst++ = (char)toupper((int)c);
            n--;
            i++;
        } else {
            int l = panjang_cp_utf8(src + i);
            if ((size_t)l >= n) break;
            memcpy(dst, src + i, (size_t)l);
            dst += l;
            n -= l;
            i += l;
        }
    }
    *dst = '\0';
}

static void ke_huruf_kecil_utf8(char *dst, const char *src, size_t n) {
    size_t i = 0;
    while (src[i] && n > 1) {
        unsigned char c = (unsigned char)src[i];
        if (c < 0x80) {
            *dst++ = (char)tolower((int)c);
            n--;
            i++;
        } else {
            int l = panjang_cp_utf8(src + i);
            if ((size_t)l >= n) break;
            memcpy(dst, src + i, (size_t)l);
            dst += l;
            n -= l;
            i += l;
        }
    }
    *dst = '\0';
}

static void ke_judul_utf8(char *dst, const char *src, size_t n) {
    size_t i = 0;
    int awal_kata = 1;
    while (src[i] && n > 1) {
        unsigned char c = (unsigned char)src[i];
        if (c < 0x80) {
            char ch = src[i];
            if (isalpha((int)ch)) {
                *dst++ = awal_kata ? (char)toupper((int)ch) : 
                                      (char)tolower((int)ch);
                n--;
                i++;
                awal_kata = 0;
            } else {
                *dst++ = ch;
                n--;
                i++;
                awal_kata = (ch == ' ' || ch == '\t');
            }
        } else {
            int l = panjang_cp_utf8(src + i);
            if ((size_t)l >= n) break;
            memcpy(dst, src + i, (size_t)l);
            dst += l;
            n -= l;
            i += l;
            awal_kata = 0;
        }
    }
    *dst = '\0';
}

void ubah_teks_sel_huruf_besar(struct buffer_tabel *buf, int x, int y) {
    char sebelum[MAKS_TEKS], sesudah[MAKS_TEKS];
    salin_str_aman(sebelum, buf->isi[y][x], MAKS_TEKS);
    ke_huruf_besar_utf8(sesudah, buf->isi[y][x], MAKS_TEKS);
    salin_str_aman(buf->isi[y][x], sesudah, MAKS_TEKS);
    dorong_undo_teks(buf, x, y, sebelum, buf->isi[y][x]);
    buf->cfg.kotor = 1;
}

void ubah_teks_sel_huruf_kecil(struct buffer_tabel *buf, int x, int y) {
    char sebelum[MAKS_TEKS], sesudah[MAKS_TEKS];
    salin_str_aman(sebelum, buf->isi[y][x], MAKS_TEKS);
    ke_huruf_kecil_utf8(sesudah, buf->isi[y][x], MAKS_TEKS);
    salin_str_aman(buf->isi[y][x], sesudah, MAKS_TEKS);
    dorong_undo_teks(buf, x, y, sebelum, buf->isi[y][x]);
    buf->cfg.kotor = 1;
}

void ubah_teks_sel_judul(struct buffer_tabel *buf, int x, int y) {
    char sebelum[MAKS_TEKS], sesudah[MAKS_TEKS];
    salin_str_aman(sebelum, buf->isi[y][x], MAKS_TEKS);
    ke_judul_utf8(sesudah, buf->isi[y][x], MAKS_TEKS);
    salin_str_aman(buf->isi[y][x], sesudah, MAKS_TEKS);
    dorong_undo_teks(buf, x, y, sebelum, buf->isi[y][x]);
    buf->cfg.kotor = 1;
}

/* ============================================================
 * WARNA TEKS SEL
 * ============================================================ */

void eksekusi_warna_teks(struct buffer_tabel *buf, const char *cmd) {
    int x1, y1, x2, y2;
    int r, c;
    warna_depan wd_terpilih = WARNAD_DEFAULT;
    warna_latar wl_terpilih = WARNAL_DEFAULT;
    char nama_kol1[4], nama_kol2[4];

    (void)cmd;

    if (seleksi_aktif) {
        x1 = seleksi_x1 < seleksi_x2 ? seleksi_x1 : seleksi_x2;
        y1 = seleksi_y1 < seleksi_y2 ? seleksi_y1 : seleksi_y2;
        x2 = seleksi_x1 > seleksi_x2 ? seleksi_x1 : seleksi_x2;
        y2 = seleksi_y1 > seleksi_y2 ? seleksi_y1 : seleksi_y2;
    } else if (sedang_memilih && jangkar_x != -1) {
        x1 = jangkar_x < buf->cfg.aktif_x ? jangkar_x : buf->cfg.aktif_x;
        y1 = jangkar_y < buf->cfg.aktif_y ? jangkar_y : buf->cfg.aktif_y;
        x2 = jangkar_x > buf->cfg.aktif_x ? jangkar_x : buf->cfg.aktif_x;
        y2 = jangkar_y > buf->cfg.aktif_y ? jangkar_y : buf->cfg.aktif_y;
    } else {
        x1 = x2 = buf->cfg.aktif_x;
        y1 = y2 = buf->cfg.aktif_y;
    }

    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 >= buf->cfg.kolom) x2 = buf->cfg.kolom - 1;
    if (y2 >= buf->cfg.baris) y2 = buf->cfg.baris - 1;

    prompt_warna(buf, 1);

    if (buf->warna_sel) {
        wd_terpilih = buf->warna_sel[y1][x1].depan;
        wl_terpilih = buf->warna_sel[y1][x1].latar;
    }

    for (r = y1; r <= y2; r++) {
        for (c = x1; c <= x2; c++) {
            if (buf->warna_sel) {
                buf->warna_sel[r][c].depan = wd_terpilih;
                buf->warna_sel[r][c].latar = wl_terpilih;
            }
        }
    }

    buf->cfg.kotor = 1;
    kolom_ke_nama(x1, nama_kol1, sizeof(nama_kol1));
    kolom_ke_nama(x2, nama_kol2, sizeof(nama_kol2));
    snprintf(pesan_status, sizeof(pesan_status),
             "Warna teks: %s/%s (%s%d:%s%d)",
             warna_nama_depan(wd_terpilih),
             warna_nama_latar(wl_terpilih),
             nama_kol1, y1 + 1, nama_kol2, y2 + 1);
}

/* ============================================================
 * BORDER SEL
 * ============================================================ */

void eksekusi_border(struct buffer_tabel *buf, const char *cmd) {
    int x1, y1, x2, y2;
    int r, c;
    warna_depan wd_terpilih;
    warna_latar wl_terpilih;
    char nama_kol1[4], nama_kol2[4];

    if (seleksi_aktif) {
        x1 = seleksi_x1 < seleksi_x2 ? seleksi_x1 : seleksi_x2;
        y1 = seleksi_y1 < seleksi_y2 ? seleksi_y1 : seleksi_y2;
        x2 = seleksi_x1 > seleksi_x2 ? seleksi_x1 : seleksi_x2;
        y2 = seleksi_y1 > seleksi_y2 ? seleksi_y1 : seleksi_y2;
    } else if (sedang_memilih && jangkar_x != -1) {
        x1 = jangkar_x < buf->cfg.aktif_x ? jangkar_x : buf->cfg.aktif_x;
        y1 = jangkar_y < buf->cfg.aktif_y ? jangkar_y : buf->cfg.aktif_y;
        x2 = jangkar_x > buf->cfg.aktif_x ? jangkar_x : buf->cfg.aktif_x;
        y2 = jangkar_y > buf->cfg.aktif_y ? jangkar_y : buf->cfg.aktif_y;
    } else {
        x1 = x2 = buf->cfg.aktif_x;
        y1 = y2 = buf->cfg.aktif_y;
    }

    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 >= buf->cfg.kolom) x2 = buf->cfg.kolom - 1;
    if (y2 >= buf->cfg.baris) y2 = buf->cfg.baris - 1;

    if (strcmp(cmd, "bw") == 0) {
        prompt_warna(buf, 0);
        wd_terpilih = buf->border_sel[y1][x1].warna_depan;
        wl_terpilih = buf->border_sel[y1][x1].warna_latar;

        for (r = y1; r <= y2; r++) {
            for (c = x1; c <= x2; c++) {
                buf->border_sel[r][c].warna_depan = wd_terpilih;
                buf->border_sel[r][c].warna_latar = wl_terpilih;
            }
        }
        buf->cfg.kotor = 1;
        return;
    }

    for (r = y1; r <= y2; r++) {
        for (c = x1; c <= x2; c++) {
            struct info_border *b = &buf->border_sel[r][c];

            if (strcmp(cmd, "ba") == 0) {
                b->top = 1; b->bottom = 1; b->left = 1; b->right = 1;
            } else if (strcmp(cmd, "bo") == 0) {
                if (r == y1) b->top = 1;
                if (r == y2) b->bottom = 1;
                if (c == x1) b->left = 1;
                if (c == x2) b->right = 1;
            } else if (strcmp(cmd, "bt") == 0) {
                if (r == y1) b->top = 1;
            } else if (strcmp(cmd, "bb") == 0) {
                if (r == y2) b->bottom = 1;
            } else if (strcmp(cmd, "bl") == 0) {
                if (c == x1) b->left = 1;
            } else if (strcmp(cmd, "br") == 0) {
                if (c == x2) b->right = 1;
            } else if (strcmp(cmd, "none") == 0) {
                b->top = 0; b->bottom = 0; b->left = 0; b->right = 0;
            }
        }
    }

    buf->cfg.kotor = 1;

    kolom_ke_nama(x1, nama_kol1, sizeof(nama_kol1));
    kolom_ke_nama(x2, nama_kol2, sizeof(nama_kol2));
    snprintf(pesan_status, sizeof(pesan_status), "Border: %s (%s%d:%s%d)",
             cmd, nama_kol1, y1 + 1, nama_kol2, y2 + 1);
}

/* ============================================================
 * MERGE/UNMERGE SEL
 * ============================================================ */

int apakah_ada_gabung_buf(const struct buffer_tabel *buf, int x, int y) {
    return (buf->tergabung[y][x].x_anchor >= 0 &&
            buf->tergabung[y][x].y_anchor >= 0);
}

int apakah_sel_anchor_buf(const struct buffer_tabel *buf, int x, int y) {
    return apakah_ada_gabung_buf(buf, x, y) &&
           buf->tergabung[y][x].adalah_anchor;
}

void hapus_blok_gabung_buf(struct buffer_tabel *buf, int ax, int ay) {
    int w = buf->tergabung[ay][ax].lebar;
    int h = buf->tergabung[ay][ax].tinggi;
    int yy, xx;

    for (yy = ay; yy < ay + h; yy++) {
        for (xx = ax; xx < ax + w; xx++) {
            buf->tergabung[yy][xx].x_anchor = -1;
            buf->tergabung[yy][xx].y_anchor = -1;
            buf->tergabung[yy][xx].lebar = 0;
            buf->tergabung[yy][xx].tinggi = 0;
            buf->tergabung[yy][xx].adalah_anchor = 0;
        }
    }
}

void atur_blok_gabung_buf(struct buffer_tabel *buf, int ax, int ay, 
                          int lbr, int tgi) {
    int yy, xx;

    for (yy = ay; yy < ay + tgi; yy++) {
        for (xx = ax; xx < ax + lbr; xx++) {
            buf->tergabung[yy][xx].x_anchor = ax;
            buf->tergabung[yy][xx].y_anchor = ay;
            buf->tergabung[yy][xx].lebar = lbr;
            buf->tergabung[yy][xx].tinggi = tgi;
            buf->tergabung[yy][xx].adalah_anchor = 0;
        }
    }
    buf->tergabung[ay][ax].adalah_anchor = 1;
}

void eksekusi_gabung(struct buffer_tabel *buf) {
    int x1, y1, x2, y2;
    int ax, ay, lbr, tgi;
    char gabungan[MAKS_TEKS];
    char nama_kol1[4], nama_kol2[4];
    int r, c, yy, xx;

    if (seleksi_aktif) {
        x1 = seleksi_x1; y1 = seleksi_y1;
        x2 = seleksi_x2; y2 = seleksi_y2;
    } else if (sedang_memilih) {
        x1 = jangkar_x; y1 = jangkar_y;
        x2 = buf->cfg.aktif_x; y2 = buf->cfg.aktif_y;
    } else {
        salin_str_aman(pesan_status, "Tidak ada area seleksi", 
                       sizeof(pesan_status));
        return;
    }

    ax = (x1 < x2) ? x1 : x2;
    ay = (y1 < y2) ? y1 : y2;
    lbr = (x1 > x2 ? x1 - x2 : x2 - x1) + 1;
    tgi = (y1 > y2 ? y1 - y2 : y2 - y1) + 1;

    for (yy = ay; yy < ay + tgi; yy++) {
        for (xx = ax; xx < ax + lbr; xx++) {
            if (apakah_ada_gabung_buf(buf, xx, yy)) {
                salin_str_aman(pesan_status, "Area mengandung blok gabung",
                               sizeof(pesan_status));
                return;
            }
        }
    }

    gabungan[0] = '\0';
    for (r = ay; r < ay + tgi; r++) {
        for (c = ax; c < ax + lbr; c++) {
            const char *isi = buf->isi[r][c];
            if (strlen(isi) > 0) {
                if (strlen(gabungan) > 0) {
                    if (tgi > 1 && lbr == 1) {
                        strcat(gabungan, "\n");
                    } else {
                        strcat(gabungan, " ");
                    }
                }
                strcat(gabungan, isi);
            }
        }
    }

    if (strlen(gabungan) > 0) {
        char sebelum[MAKS_TEKS];
        salin_str_aman(sebelum, buf->isi[ay][ax], MAKS_TEKS);
        salin_str_aman(buf->isi[ay][ax], gabungan, MAKS_TEKS);
        dorong_undo_teks(buf, ax, ay, sebelum, buf->isi[ay][ax]);
    }

    for (r = ay; r < ay + tgi; r++) {
        for (c = ax; c < ax + lbr; c++) {
            if (r == ay && c == ax) continue;
            buf->isi[r][c][0] = '\0';
        }
    }

    atur_blok_gabung_buf(buf, ax, ay, lbr, tgi);

    sedang_memilih = 0;
    jangkar_x = jangkar_y = -1;
    seleksi_aktif = 0;
    seleksi_x1 = seleksi_y1 = seleksi_x2 = seleksi_y2 = -1;
    buf->cfg.aktif_x = ax;
    buf->cfg.aktif_y = ay;

    kolom_ke_nama(ax, nama_kol1, sizeof(nama_kol1));
    kolom_ke_nama(ax + lbr - 1, nama_kol2, sizeof(nama_kol2));
    snprintf(pesan_status, sizeof(pesan_status),
             "Gabung %s%d-%s%d", nama_kol1, ay + 1,
             nama_kol2, ay + tgi);

    buf->cfg.kotor = 1;
}

void eksekusi_urai_gabung(struct buffer_tabel *buf) {
    int ax = buf->cfg.aktif_x;
    int ay = buf->cfg.aktif_y;
    char nama_kol[4];

    if (!apakah_ada_gabung_buf(buf, ax, ay)) {
        salin_str_aman(pesan_status, "Sel tidak digabung", sizeof(pesan_status));
        return;
    }

    ax = buf->tergabung[ay][ax].x_anchor;
    ay = buf->tergabung[ay][ax].y_anchor;

    hapus_blok_gabung_buf(buf, ax, ay);
    dorong_undo_teks(buf, ax, ay, "", buf->isi[ay][ax]);

    kolom_ke_nama(ax, nama_kol, sizeof(nama_kol));
    snprintf(pesan_status, sizeof(pesan_status),
             "Urai gabung %s%d", nama_kol, ay + 1);

    buf->cfg.kotor = 1;
}

/* ============================================================
 * SELEKSI
 * ============================================================ */

void seleksi_baris_penuh_aktif(struct buffer_tabel *buf) {
    seleksi_x1 = 0;
    seleksi_y1 = buf->cfg.aktif_y;
    seleksi_x2 = buf->cfg.kolom - 1;
    seleksi_y2 = buf->cfg.aktif_y;
    seleksi_aktif = 1;
    sedang_memilih = 0;
    snprintf(pesan_status, sizeof(pesan_status), 
             "Seleksi Baris %d", buf->cfg.aktif_y + 1);
}

void seleksi_kolom_penuh_aktif(struct buffer_tabel *buf) {
    char nama_kol[4];
    seleksi_x1 = buf->cfg.aktif_x;
    seleksi_y1 = 0;
    seleksi_x2 = buf->cfg.aktif_x;
    seleksi_y2 = buf->cfg.baris - 1;
    seleksi_aktif = 1;
    sedang_memilih = 0;
    kolom_ke_nama(buf->cfg.aktif_x, nama_kol, sizeof(nama_kol));
    snprintf(pesan_status, sizeof(pesan_status), 
             "Seleksi Kolom %s", nama_kol);
}

void toggle_seleksi_random(struct buffer_tabel *buf, int x, int y) {
    int i, j;
    char nama_kol[4];

    (void)buf;

    for (i = 0; i < seleksi_random; i++) {
        if (seleksi_random_x[i] == x && seleksi_random_y[i] == y) {
            for (j = i; j < seleksi_random - 1; j++) {
                seleksi_random_x[j] = seleksi_random_x[j + 1];
                seleksi_random_y[j] = seleksi_random_y[j + 1];
            }
            seleksi_random--;
            kolom_ke_nama(x, nama_kol, sizeof(nama_kol));
            snprintf(pesan_status, sizeof(pesan_status),
                     "Unselect %s%d", nama_kol, y + 1);
            return;
        }
    }
    if (seleksi_random < MAKS_SELEKSI) {
        seleksi_random_x[seleksi_random] = x;
        seleksi_random_y[seleksi_random] = y;
        seleksi_random++;
        kolom_ke_nama(x, nama_kol, sizeof(nama_kol));
        snprintf(pesan_status, sizeof(pesan_status),
                 "Select %s%d", nama_kol, y + 1);
    }
}

/* ============================================================
 * CLIPBOARD
 * ============================================================ */

void salin_sel(struct buffer_tabel *buf) {
    salin_str_aman(papan_klip, buf->isi[buf->cfg.aktif_y][buf->cfg.aktif_x], 
                   MAKS_TEKS);
}

void salin_area(struct buffer_tabel *buf) {
    int x1, y1, x2, y2;
    int r, c;
    char nama_kol1[4], nama_kol2[4];

    if (seleksi_aktif) {
        x1 = seleksi_x1; y1 = seleksi_y1;
        x2 = seleksi_x2; y2 = seleksi_y2;
    } else if (sedang_memilih && jangkar_x >= 0 && jangkar_y >= 0) {
        x1 = (jangkar_x < buf->cfg.aktif_x) ? jangkar_x : buf->cfg.aktif_x;
        y1 = (jangkar_y < buf->cfg.aktif_y) ? jangkar_y : buf->cfg.aktif_y;
        x2 = (jangkar_x > buf->cfg.aktif_x) ? jangkar_x : buf->cfg.aktif_x;
        y2 = (jangkar_y > buf->cfg.aktif_y) ? jangkar_y : buf->cfg.aktif_y;
    } else {
        int x = buf->cfg.aktif_x, y = buf->cfg.aktif_y;
        salin_str_aman(papan_klip, buf->isi[y][x], MAKS_TEKS);
        klip_x1 = klip_x2 = x;
        klip_y1 = klip_y2 = y;
        klip_ada_area = 1;
        salin_str_aman(papan_klip_area[y][x], papan_klip, MAKS_TEKS);
        return;
    }

    klip_x1 = x1; klip_x2 = x2;
    klip_y1 = y1; klip_y2 = y2;
    klip_ada_area = 1;
    seleksi_aktif = 0;

    for (r = y1; r <= y2; r++) {
        for (c = x1; c <= x2; c++) {
            salin_str_aman(papan_klip_area[r][c], buf->isi[r][c], MAKS_TEKS);
        }
    }

    kolom_ke_nama(x1, nama_kol1, sizeof(nama_kol1));
    kolom_ke_nama(x2, nama_kol2, sizeof(nama_kol2));
    snprintf(pesan_status, sizeof(pesan_status),
             "Menyalin %s%d-%s%d", nama_kol1, y1 + 1, nama_kol2, y2 + 1);
}

void salin_baris_penuh(struct buffer_tabel *buf, int row) {
    int c;
    if (!buf || row < 0 || row >= buf->cfg.baris) return;

    klip_y1 = klip_y2 = row;
    klip_x1 = 0;
    klip_x2 = buf->cfg.kolom - 1;
    klip_ada_area = 1;
    klip_mode = 1;

    for (c = 0; c < buf->cfg.kolom; c++) {
        salin_str_aman(papan_klip_area[row][c], buf->isi[row][c], MAKS_TEKS);
    }
    snprintf(pesan_status, sizeof(pesan_status),
             "Menyalin baris %d", row + 1);
}

void salin_kolom_penuh(struct buffer_tabel *buf, int col) {
    int r;
    char nama_kol[4];
    if (!buf || col < 0 || col >= buf->cfg.kolom) return;

    klip_x1 = klip_x2 = col;
    klip_y1 = 0;
    klip_y2 = buf->cfg.baris - 1;
    klip_ada_area = 1;
    klip_mode = 2;

    for (r = 0; r < buf->cfg.baris; r++) {
        salin_str_aman(papan_klip_area[r][col], buf->isi[r][col], MAKS_TEKS);
    }
    kolom_ke_nama(col, nama_kol, sizeof(nama_kol));
    snprintf(pesan_status, sizeof(pesan_status),
             "Menyalin kolom %s", nama_kol);
}

void potong_area(struct buffer_tabel *buf) {
    int r, c;
    salin_area(buf);

    for (r = klip_y1; r <= klip_y2; r++) {
        for (c = klip_x1; c <= klip_x2; c++) {
            buf->isi[r][c][0] = '\0';
        }
    }

    buf->cfg.kotor = 1;
    snprintf(pesan_status, sizeof(pesan_status), "Memotong");
}

void potong_kolom_penuh(struct buffer_tabel *buf) {
    char nama_kol[4];
    salin_kolom_penuh(buf, buf->cfg.aktif_x);
    bersihkan_kolom_aktif(buf);
    kolom_ke_nama(buf->cfg.aktif_x, nama_kol, sizeof(nama_kol));
    snprintf(pesan_status, sizeof(pesan_status), 
             "Cut Kolom %s", nama_kol);
}

void potong_baris_penuh(struct buffer_tabel *buf) {
    salin_baris_penuh(buf, buf->cfg.aktif_y);
    bersihkan_baris_aktif(buf);
    snprintf(pesan_status, sizeof(pesan_status), 
             "Cut Baris %d", buf->cfg.aktif_y + 1);
}

void lakukan_tempel(struct buffer_tabel *buf, int offset_x, int offset_y) {
    int dx, dy, r, c, tx, ty;
    int ada_isi = 0;
    char jawab[8];

    if (!buf || !klip_ada_area) return;

    if (klip_mode == 1) {
        tempel_baris_penuh(buf, buf->cfg.aktif_y, offset_y);
        return;
    } else if (klip_mode == 2) {
        tempel_kolom_penuh(buf, buf->cfg.aktif_x, offset_x);
        return;
    }

    dx = buf->cfg.aktif_x - klip_x1 + offset_x;
    dy = buf->cfg.aktif_y - klip_y1 + offset_y;

    for (r = klip_y1; r <= klip_y2; r++) {
        for (c = klip_x1; c <= klip_x2; c++) {
            tx = c + dx;
            ty = r + dy;
            if (tx >= 0 && tx < buf->cfg.kolom &&
                ty >= 0 && ty < buf->cfg.baris) {
                if (buf->isi[ty][tx][0] != '\0') ada_isi = 1;
            }
        }
    }

    if (ada_isi) {
        minta_nama_berkas(jawab, sizeof(jawab), "Timpa? (y/n): ");
        if (jawab[0] != 'y' && jawab[0] != 'Y') {
            snprintf(pesan_status, sizeof(pesan_status), 
                     "Penempelan dibatalkan");
            return;
        }
    }

    for (r = klip_y1; r <= klip_y2; r++) {
        for (c = klip_x1; c <= klip_x2; c++) {
            tx = c + dx;
            ty = r + dy;
            if (tx >= 0 && tx < buf->cfg.kolom &&
                ty >= 0 && ty < buf->cfg.baris) {
                atur_teks_sel(buf, tx, ty, papan_klip_area[r][c], 1);
            }
        }
    }

    klip_ada_area = 0;
    sedang_memilih = 0;
    jangkar_x = jangkar_y = -1;
    seleksi_aktif = 0;
    seleksi_x1 = seleksi_y1 = seleksi_x2 = seleksi_y2 = -1;

    snprintf(pesan_status, sizeof(pesan_status), "Penempelan selesai");
}

void tempel_baris_penuh(struct buffer_tabel *buf, int row, int arah) {
    int c, target;
    char jawab[8];

    if (!buf || !klip_ada_area) return;
    target = row + arah;
    if (target < 0 || target >= buf->cfg.baris) return;

    for (c = 0; c < buf->cfg.kolom; c++) {
        if (buf->isi[target][c][0] != '\0') {
            minta_nama_berkas(jawab, sizeof(jawab), "Timpa? (y/n): ");
            if (jawab[0] != 'y' && jawab[0] != 'Y') return;
            break;
        }
    }

    for (c = 0; c < buf->cfg.kolom; c++) {
        atur_teks_sel(buf, c, target, papan_klip_area[klip_y1][c], 1);
    }

    buf->cfg.kotor = 1;
    klip_ada_area = 0;
    snprintf(pesan_status, sizeof(pesan_status), 
             "Menempelkan baris penuh");
}

void tempel_kolom_penuh(struct buffer_tabel *buf, int col, int arah) {
    int r, target;
    char jawab[8];

    if (!buf || !klip_ada_area) return;
    target = col + arah;
    if (target < 0 || target >= buf->cfg.kolom) return;

    for (r = 0; r < buf->cfg.baris; r++) {
        if (buf->isi[r][target][0] != '\0') {
            minta_nama_berkas(jawab, sizeof(jawab), "Timpa? (y/n): ");
            if (jawab[0] != 'y' && jawab[0] != 'Y') return;
            break;
        }
    }

    for (r = 0; r < buf->cfg.baris; r++) {
        atur_teks_sel(buf, target, r, papan_klip_area[r][klip_x1], 1);
    }

    buf->cfg.kotor = 1;
    klip_ada_area = 0;
    snprintf(pesan_status, sizeof(pesan_status), 
             "Menempelkan kolom penuh");
}

void tempel_isi_ke_seleksi(struct buffer_tabel *buf) {
    int x_start, x_end, y_start, y_end;
    int counter, r, c;

    if (!seleksi_aktif) {
        snprintf(pesan_status, sizeof(pesan_status), 
                 "Tidak ada area seleksi untuk diisi");
        return;
    }

    if (strlen(papan_klip) == 0) {
        snprintf(pesan_status, sizeof(pesan_status), "Clipboard kosong");
        return;
    }

    x_start = (seleksi_x1 < seleksi_x2) ? seleksi_x1 : seleksi_x2;
    x_end = (seleksi_x1 > seleksi_x2) ? seleksi_x1 : seleksi_x2;
    y_start = (seleksi_y1 < seleksi_y2) ? seleksi_y1 : seleksi_y2;
    y_end = (seleksi_y1 > seleksi_y2) ? seleksi_y1 : seleksi_y2;

    counter = 0;
    for (r = y_start; r <= y_end; r++) {
        for (c = x_start; c <= x_end; c++) {
            if (apakah_ada_gabung_buf(buf, c, r) && 
                !apakah_sel_anchor_buf(buf, c, r)) {
                continue;
            }
            atur_teks_sel(buf, c, r, papan_klip, 1);
            counter++;
        }
    }

    buf->cfg.kotor = 1;
    seleksi_aktif = 0;
    seleksi_x1 = seleksi_y1 = seleksi_x2 = seleksi_y2 = -1;

    snprintf(pesan_status, sizeof(pesan_status), 
             "Mengisi %d sel", counter);
}

/* ============================================================
 * SWAP BARIS/KOLOM
 * ============================================================ */

void swap_kolom(struct buffer_tabel *buf, int col, int arah) {
    int target, r, tmpw;
    char tmp[MAKS_TEKS];
    char nama_kol1[4], nama_kol2[4];

    target = col + arah;
    if (!buf || col < 0 || target < 0 || 
        col >= buf->cfg.kolom || target >= buf->cfg.kolom) return;

    for (r = 0; r < buf->cfg.baris; r++) {
        salin_str_aman(tmp, buf->isi[r][col], MAKS_TEKS);
        salin_str_aman(buf->isi[r][col], buf->isi[r][target], MAKS_TEKS);
        salin_str_aman(buf->isi[r][target], tmp, MAKS_TEKS);
    }

    tmpw = buf->lebar_kolom[col];
    buf->lebar_kolom[col] = buf->lebar_kolom[target];
    buf->lebar_kolom[target] = tmpw;

    buf->cfg.aktif_x = target;
    buf->cfg.kotor = 1;
    kolom_ke_nama(col, nama_kol1, sizeof(nama_kol1));
    kolom_ke_nama(target, nama_kol2, sizeof(nama_kol2));
    snprintf(pesan_status, sizeof(pesan_status),
             "Swap kolom %s <-> %s", nama_kol1, nama_kol2);
}

void swap_baris(struct buffer_tabel *buf, int row, int arah) {
    int target, c, tmph;
    char tmp[MAKS_TEKS];

    target = row + arah;
    if (!buf || row < 0 || target < 0 || 
        row >= buf->cfg.baris || target >= buf->cfg.baris) return;

    for (c = 0; c < buf->cfg.kolom; c++) {
        salin_str_aman(tmp, buf->isi[row][c], MAKS_TEKS);
        salin_str_aman(buf->isi[row][c], buf->isi[target][c], MAKS_TEKS);
        salin_str_aman(buf->isi[target][c], tmp, MAKS_TEKS);
    }

    tmph = buf->tinggi_baris[row];
    buf->tinggi_baris[row] = buf->tinggi_baris[target];
    buf->tinggi_baris[target] = tmph;

    buf->cfg.aktif_y = target;
    buf->cfg.kotor = 1;
    snprintf(pesan_status, sizeof(pesan_status),
             "Swap baris %d <-> %d", row + 1, target + 1);
}

/* ============================================================
 * NAVIGASI SEL
 * ============================================================ */

void tarik_ke_anchor(struct buffer_tabel *buf) {
    int x = buf->cfg.aktif_x, y = buf->cfg.aktif_y;
    if (apakah_ada_gabung_buf(buf, x, y)) {
        buf->cfg.aktif_x = buf->tergabung[y][x].x_anchor;
        buf->cfg.aktif_y = buf->tergabung[y][x].y_anchor;
    }
}

void geser_kanan(struct buffer_tabel *buf) {
    int x = buf->cfg.aktif_x, y = buf->cfg.aktif_y;
    if (apakah_ada_gabung_buf(buf, x, y)) {
        int ax = buf->tergabung[y][x].x_anchor;
        int ay = buf->tergabung[y][x].y_anchor;
        x = ax + buf->tergabung[ay][ax].lebar - 1;
    }
    if (x < buf->cfg.kolom - 1) x++;
    buf->cfg.aktif_x = x;
    buf->cfg.aktif_y = y;
    tarik_ke_anchor(buf);
    pastikan_aktif_terlihat(buf);
}

void geser_kiri(struct buffer_tabel *buf) {
    int x = buf->cfg.aktif_x, y = buf->cfg.aktif_y;
    if (apakah_ada_gabung_buf(buf, x, y)) 
        x = buf->tergabung[y][x].x_anchor;
    if (x > 0) x--;
    buf->cfg.aktif_x = x;
    buf->cfg.aktif_y = y;
    tarik_ke_anchor(buf);
    pastikan_aktif_terlihat(buf);
}

void geser_bawah(struct buffer_tabel *buf) {
    int x = buf->cfg.aktif_x, y = buf->cfg.aktif_y;
    if (apakah_ada_gabung_buf(buf, x, y)) {
        int ax = buf->tergabung[y][x].x_anchor;
        int ay = buf->tergabung[y][x].y_anchor;
        y = ay + buf->tergabung[ay][ax].tinggi - 1;
    }
    if (y < buf->cfg.baris - 1) y++;
    buf->cfg.aktif_x = x;
    buf->cfg.aktif_y = y;
    tarik_ke_anchor(buf);
    pastikan_aktif_terlihat(buf);
}

void geser_atas(struct buffer_tabel *buf) {
    int x = buf->cfg.aktif_x, y = buf->cfg.aktif_y;
    if (apakah_ada_gabung_buf(buf, x, y)) 
        y = buf->tergabung[y][x].y_anchor;
    if (y > 0) y--;
    buf->cfg.aktif_x = x;
    buf->cfg.aktif_y = y;
    tarik_ke_anchor(buf);
    pastikan_aktif_terlihat(buf);
}

void pastikan_aktif_terlihat(struct buffer_tabel *buf) {
    int x_awal, y_awal, lv, tv, ks, ke, bs, be;
    int w_total, h_total, c, r;

    hitung_viewport(buf, &x_awal, &y_awal, &lv, &tv, &ks, &ke, &bs, &be);

    if (buf->cfg.aktif_x < ks) {
        buf->cfg.lihat_kolom = buf->cfg.aktif_x;
    } else if (buf->cfg.aktif_x > ke) {
        w_total = 0;
        c = buf->cfg.aktif_x;
        while (c >= 0) {
            int w = buf->lebar_kolom[c] + 1;
            if (w_total + w > lv && c != buf->cfg.aktif_x) {
                c++;
                break;
            }
            w_total += w;
            c--;
        }
        if (c < 0) c = 0;
        buf->cfg.lihat_kolom = c;
    }

    if (buf->cfg.aktif_y < bs) {
        buf->cfg.lihat_baris = buf->cfg.aktif_y;
    } else if (buf->cfg.aktif_y > be) {
        h_total = 0;
        r = buf->cfg.aktif_y;
        while (r >= 0) {
            int h = buf->tinggi_baris[r] + 1;
            if (h_total + h > tv && r != buf->cfg.aktif_y) {
                r++;
                break;
            }
            h_total += h;
            r--;
        }
        if (r < 0) r = 0;
        buf->cfg.lihat_baris = r;
    }

    if (buf->cfg.lihat_kolom < 0) buf->cfg.lihat_kolom = 0;
    if (buf->cfg.lihat_baris < 0) buf->cfg.lihat_baris = 0;
    if (buf->cfg.lihat_kolom > buf->cfg.kolom - 1)
        buf->cfg.lihat_kolom = buf->cfg.kolom - 1;
    if (buf->cfg.lihat_baris > buf->cfg.baris - 1)
        buf->cfg.lihat_baris = buf->cfg.baris - 1;
}

/* ============================================================
 * UBAH UKURAN SEL
 * ============================================================ */

void ubah_ukuran_sel(struct buffer_tabel *buf, int dl, int dt) {
    int lbr = buf->lebar_kolom[buf->cfg.aktif_x];
    int tgi = buf->tinggi_baris[buf->cfg.aktif_y];
    int lbr_baru = lbr + dl;
    int tgi_baru = tgi + dt;

    if (lbr_baru < 1) lbr_baru = 1;
    if (lbr_baru > 50) lbr_baru = 50;
    if (tgi_baru < 1) tgi_baru = 1;
    if (tgi_baru > 20) tgi_baru = 20;

    buf->lebar_kolom[buf->cfg.aktif_x] = lbr_baru;
    buf->tinggi_baris[buf->cfg.aktif_y] = tgi_baru;
    buf->cfg.kotor = 1;
}

void sesuaikan_lebar_kolom(struct buffer_tabel *buf, int col, int mode) {
    int maxw, minw, r, len;

    if (!buf || col < 0 || col >= buf->cfg.kolom) return;

    maxw = 1;
    minw = 9999;

    for (r = 0; r < buf->cfg.baris; r++) {
        len = lebar_tampilan_string_utf8(buf->isi[r][col]);
        if (len > maxw) maxw = len;
        if (len > 0 && len < minw) minw = len;
    }

    if (mode > 0) {
        buf->lebar_kolom[col] = maxw + 1;
    } else if (mode < 0) {
        buf->lebar_kolom[col] = minw + 1;
    }
    buf->cfg.kotor = 1;
}

void sesuaikan_semua_lebar(struct buffer_tabel *buf, int col_ref) {
    int c;

    (void)col_ref;  /* Parameter tidak digunakan, untuk kompatibilitas */

    if (!buf) return;

    for (c = 0; c < buf->cfg.kolom; c++) {
        sesuaikan_lebar_kolom(buf, c, 1);
    }
    buf->cfg.kotor = 1;
}

/* ============================================================
 * HAPUS ISI SEL
 * ============================================================ */

void hapus_sel_atau_area(struct buffer_tabel *buf) {
    int x1, y1, x2, y2;
    int r, c;

    if (seleksi_aktif) {
        x1 = seleksi_x1 < seleksi_x2 ? seleksi_x1 : seleksi_x2;
        y1 = seleksi_y1 < seleksi_y2 ? seleksi_y1 : seleksi_y2;
        x2 = seleksi_x1 > seleksi_x2 ? seleksi_x1 : seleksi_x2;
        y2 = seleksi_y1 > seleksi_y2 ? seleksi_y1 : seleksi_y2;

        for (r = y1; r <= y2; r++) {
            for (c = x1; c <= x2; c++) {
                buf->isi[r][c][0] = '\0';
            }
        }
        seleksi_aktif = 0;
        seleksi_x1 = seleksi_y1 = seleksi_x2 = seleksi_y2 = -1;
    } else {
        buf->isi[buf->cfg.aktif_y][buf->cfg.aktif_x][0] = '\0';
    }

    buf->cfg.kotor = 1;
    snprintf(pesan_status, sizeof(pesan_status), "Dihapus");
}

void bersihkan_kolom_aktif(struct buffer_tabel *buf) {
    int r;
    char nama_kol[4];
    for (r = 0; r < buf->cfg.baris; r++) {
        buf->isi[r][buf->cfg.aktif_x][0] = '\0';
    }
    buf->cfg.kotor = 1;
    kolom_ke_nama(buf->cfg.aktif_x, nama_kol, sizeof(nama_kol));
    snprintf(pesan_status, sizeof(pesan_status), 
             "Kolom %s dibersihkan", nama_kol);
}

void bersihkan_baris_aktif(struct buffer_tabel *buf) {
    int c;
    for (c = 0; c < buf->cfg.kolom; c++) {
        buf->isi[buf->cfg.aktif_y][c][0] = '\0';
    }
    buf->cfg.kotor = 1;
    snprintf(pesan_status, sizeof(pesan_status), 
             "Baris %d dibersihkan", buf->cfg.aktif_y + 1);
}

/* ============================================================
 * OPERASI STRUKTUR (INSERT/DELETE BARIS/KOLOM)
 * ============================================================ */

void sisip_kolom_setelah(struct buffer_tabel *buf) {
    int c, r;
    char nama_kol[4];

    if (buf->cfg.kolom >= MAKS_KOLOM) return;

    for (r = 0; r < buf->cfg.baris; r++) {
        for (c = buf->cfg.kolom; c > buf->cfg.aktif_x + 1; c--) {
            salin_str_aman(buf->isi[r][c], buf->isi[r][c-1], MAKS_TEKS);
            buf->perataan_sel[r][c] = buf->perataan_sel[r][c-1];
        }
        buf->isi[r][buf->cfg.aktif_x + 1][0] = '\0';
        buf->perataan_sel[r][buf->cfg.aktif_x + 1] = RATA_KIRI;
    }

    buf->cfg.kolom++;
    buf->cfg.kotor = 1;
    kolom_ke_nama(buf->cfg.aktif_x, nama_kol, sizeof(nama_kol));
    snprintf(pesan_status, sizeof(pesan_status), 
             "Sisip kolom setelah %s", nama_kol);
}

void sisip_kolom_sebelum(struct buffer_tabel *buf) {
    int c, r;
    char nama_kol[4];

    if (buf->cfg.kolom >= MAKS_KOLOM) return;

    for (r = 0; r < buf->cfg.baris; r++) {
        for (c = buf->cfg.kolom; c > buf->cfg.aktif_x; c--) {
            salin_str_aman(buf->isi[r][c], buf->isi[r][c-1], MAKS_TEKS);
            buf->perataan_sel[r][c] = buf->perataan_sel[r][c-1];
        }
        buf->isi[r][buf->cfg.aktif_x][0] = '\0';
        buf->perataan_sel[r][buf->cfg.aktif_x] = RATA_KIRI;
    }

    buf->cfg.kolom++;
    buf->cfg.kotor = 1;
    kolom_ke_nama(buf->cfg.aktif_x, nama_kol, sizeof(nama_kol));
    snprintf(pesan_status, sizeof(pesan_status), 
             "Sisip kolom sebelum %s", nama_kol);
}

void sisip_baris_setelah(struct buffer_tabel *buf) {
    int c, r;

    if (buf->cfg.baris >= MAKS_BARIS) return;

    for (r = buf->cfg.baris; r > buf->cfg.aktif_y + 1; r--) {
        for (c = 0; c < buf->cfg.kolom; c++) {
            salin_str_aman(buf->isi[r][c], buf->isi[r-1][c], MAKS_TEKS);
            buf->perataan_sel[r][c] = buf->perataan_sel[r-1][c];
        }
    }
    for (c = 0; c < buf->cfg.kolom; c++) {
        buf->isi[buf->cfg.aktif_y + 1][c][0] = '\0';
        buf->perataan_sel[buf->cfg.aktif_y + 1][c] = RATA_KIRI;
    }

    buf->cfg.baris++;
    buf->cfg.kotor = 1;
    snprintf(pesan_status, sizeof(pesan_status), 
             "Sisip baris setelah %d", buf->cfg.aktif_y + 1);
}

void sisip_baris_sebelum(struct buffer_tabel *buf) {
    int c, r;

    if (buf->cfg.baris >= MAKS_BARIS) return;

    for (r = buf->cfg.baris; r > buf->cfg.aktif_y; r--) {
        for (c = 0; c < buf->cfg.kolom; c++) {
            salin_str_aman(buf->isi[r][c], buf->isi[r-1][c], MAKS_TEKS);
            buf->perataan_sel[r][c] = buf->perataan_sel[r-1][c];
        }
    }
    for (c = 0; c < buf->cfg.kolom; c++) {
        buf->isi[buf->cfg.aktif_y][c][0] = '\0';
        buf->perataan_sel[buf->cfg.aktif_y][c] = RATA_KIRI;
    }

    buf->cfg.baris++;
    buf->cfg.kotor = 1;
    snprintf(pesan_status, sizeof(pesan_status), 
             "Sisip baris sebelum %d", buf->cfg.aktif_y + 1);
}

void hapus_kolom_aktif(struct buffer_tabel *buf) {
    int c, r;
    char nama_kol[4];

    if (buf->cfg.kolom <= 1) return;

    for (r = 0; r < buf->cfg.baris; r++) {
        for (c = buf->cfg.aktif_x; c < buf->cfg.kolom - 1; c++) {
            salin_str_aman(buf->isi[r][c], buf->isi[r][c+1], MAKS_TEKS);
            buf->perataan_sel[r][c] = buf->perataan_sel[r][c+1];
        }
    }

    buf->cfg.kolom--;
    if (buf->cfg.aktif_x >= buf->cfg.kolom) 
        buf->cfg.aktif_x = buf->cfg.kolom - 1;
    buf->cfg.kotor = 1;
    kolom_ke_nama(buf->cfg.aktif_x, nama_kol, sizeof(nama_kol));
    snprintf(pesan_status, sizeof(pesan_status), 
             "Hapus kolom %s", nama_kol);
}

void hapus_baris_aktif(struct buffer_tabel *buf) {
    int c, r;

    if (buf->cfg.baris <= 1) return;

    for (r = buf->cfg.aktif_y; r < buf->cfg.baris - 1; r++) {
        for (c = 0; c < buf->cfg.kolom; c++) {
            salin_str_aman(buf->isi[r][c], buf->isi[r+1][c], MAKS_TEKS);
            buf->perataan_sel[r][c] = buf->perataan_sel[r+1][c];
        }
    }

    buf->cfg.baris--;
    if (buf->cfg.aktif_y >= buf->cfg.baris) 
        buf->cfg.aktif_y = buf->cfg.baris - 1;
    buf->cfg.kotor = 1;
    snprintf(pesan_status, sizeof(pesan_status), 
             "Hapus baris %d", buf->cfg.aktif_y + 1);
}

/* ============================================================
 * STICKY (Freeze Pane)
 * ============================================================ */

void toggle_sticky_baris(struct buffer_tabel *buf) {
    int aktif_y = buf->cfg.aktif_y;

    if (aktif_y == 0) {
        /* Tidak bisa sticky di baris 0 (header) - matikan sticky */
        buf->cfg.sticky_baris = 0;
        buf->lembar[buf->lembar_aktif]->sticky_baris = 0;
        snprintf(pesan_status, sizeof(pesan_status),
                 "Sticky baris: OFF");
        return;
    }

    if (buf->cfg.sticky_baris == aktif_y) {
        /* Sudah sticky sampai posisi ini - matikan */
        buf->cfg.sticky_baris = 0;
        buf->lembar[buf->lembar_aktif]->sticky_baris = 0;
        snprintf(pesan_status, sizeof(pesan_status),
                 "Sticky baris: OFF");
    } else {
        /* Set sticky sampai baris aktif (baris 0 sampai aktif_y-1 tetap) */
        buf->cfg.sticky_baris = aktif_y;
        buf->lembar[buf->lembar_aktif]->sticky_baris = aktif_y;
        snprintf(pesan_status, sizeof(pesan_status),
                 "Sticky baris: %d baris", aktif_y);
    }
    buf->cfg.kotor = 1;
}

void toggle_sticky_kolom(struct buffer_tabel *buf) {
    int aktif_x = buf->cfg.aktif_x;
    char nama_kol[4];

    if (aktif_x == 0) {
        /* Tidak bisa sticky di kolom 0 - matikan sticky */
        buf->cfg.sticky_kolom = 0;
        buf->lembar[buf->lembar_aktif]->sticky_kolom = 0;
        snprintf(pesan_status, sizeof(pesan_status),
                 "Sticky kolom: OFF");
        return;
    }

    if (buf->cfg.sticky_kolom == aktif_x) {
        /* Sudah sticky sampai posisi ini - matikan */
        buf->cfg.sticky_kolom = 0;
        buf->lembar[buf->lembar_aktif]->sticky_kolom = 0;
        snprintf(pesan_status, sizeof(pesan_status),
                 "Sticky kolom: OFF");
    } else {
        /* Set sticky sampai kolom aktif (kolom 0 sampai aktif_x-1 tetap) */
        buf->cfg.sticky_kolom = aktif_x;
        buf->lembar[buf->lembar_aktif]->sticky_kolom = aktif_x;
        kolom_ke_nama(aktif_x, nama_kol, sizeof(nama_kol));
        snprintf(pesan_status, sizeof(pesan_status),
                 "Sticky kolom: %d kolom (sampai %s)", aktif_x, nama_kol);
    }
    buf->cfg.kotor = 1;
}

/* ============================================================
 * HAPUS BARIS/KOLOM RANGE (berdasarkan seleksi)
 * ============================================================ */

void hapus_baris_range(struct buffer_tabel *buf, int y_mulai, int y_akhir) {
    int jumlah_hapus;
    int c, r;

    if (!buf) return;
    if (y_mulai < 0 || y_akhir < 0) return;
    if (y_mulai > y_akhir) { int tmp = y_mulai; y_mulai = y_akhir; y_akhir = tmp; }
    if (y_akhir >= buf->cfg.baris) y_akhir = buf->cfg.baris - 1;

    jumlah_hapus = y_akhir - y_mulai + 1;

    /* Jika menghapus semua baris, sisakan 1 */
    if (jumlah_hapus >= buf->cfg.baris) {
        jumlah_hapus = buf->cfg.baris - 1;
        y_mulai = 0;
    }

    /* Geser baris ke atas untuk mengisi kekosongan */
    for (r = y_mulai; r < buf->cfg.baris - jumlah_hapus; r++) {
        for (c = 0; c < buf->cfg.kolom; c++) {
            salin_str_aman(buf->isi[r][c], buf->isi[r + jumlah_hapus][c],
                           MAKS_TEKS);
            buf->perataan_sel[r][c] = buf->perataan_sel[r + jumlah_hapus][c];
        }
    }

    buf->cfg.baris -= jumlah_hapus;
    if (buf->cfg.baris < 1) buf->cfg.baris = 1;
    if (buf->cfg.aktif_y >= buf->cfg.baris)
        buf->cfg.aktif_y = buf->cfg.baris - 1;
    buf->cfg.kotor = 1;

    /* Bersihkan seleksi */
    seleksi_aktif = 0;

    snprintf(pesan_status, sizeof(pesan_status),
             "Hapus %d baris (%d-%d)", jumlah_hapus, y_mulai + 1, y_akhir + 1);
}

void hapus_kolom_range(struct buffer_tabel *buf, int x_mulai, int x_akhir) {
    int jumlah_hapus;
    int c, r;

    if (!buf) return;
    if (x_mulai < 0 || x_akhir < 0) return;
    if (x_mulai > x_akhir) { int tmp = x_mulai; x_mulai = x_akhir; x_akhir = tmp; }
    if (x_akhir >= buf->cfg.kolom) x_akhir = buf->cfg.kolom - 1;

    jumlah_hapus = x_akhir - x_mulai + 1;

    /* Jika menghapus semua kolom, sisakan 1 */
    if (jumlah_hapus >= buf->cfg.kolom) {
        jumlah_hapus = buf->cfg.kolom - 1;
        x_mulai = 0;
    }

    /* Geser kolom ke kiri untuk mengisi kekosongan */
    for (r = 0; r < buf->cfg.baris; r++) {
        for (c = x_mulai; c < buf->cfg.kolom - jumlah_hapus; c++) {
            salin_str_aman(buf->isi[r][c], buf->isi[r][c + jumlah_hapus],
                           MAKS_TEKS);
            buf->perataan_sel[r][c] = buf->perataan_sel[r][c + jumlah_hapus];
        }
    }

    buf->cfg.kolom -= jumlah_hapus;
    if (buf->cfg.kolom < 1) buf->cfg.kolom = 1;
    if (buf->cfg.aktif_x >= buf->cfg.kolom)
        buf->cfg.aktif_x = buf->cfg.kolom - 1;
    buf->cfg.kotor = 1;

    /* Bersihkan seleksi */
    seleksi_aktif = 0;

    snprintf(pesan_status, sizeof(pesan_status),
             "Hapus %d kolom", jumlah_hapus);
}

/* ============================================================
 * SORTIR
 * ============================================================ */

void sortir_kolom_aktif(struct buffer_tabel *buf) {
    int *indeks;
    char **tmp;
    int r, c, n;
    int i, j, t;

    n = buf->cfg.baris;
    indeks = calloc(n, sizeof(int));
    tmp = calloc(n, sizeof(char*));

    if (!indeks || !tmp) {
        free(indeks);
        free(tmp);
        return;
    }

    for (r = 0; r < n; r++) {
        indeks[r] = r;
        tmp[r] = calloc(MAKS_TEKS, sizeof(char));
    }

    /* Bubble sort */
    for (i = 0; i < n - 1; i++) {
        for (j = 0; j < n - i - 1; j++) {
            if (strcmp(buf->isi[indeks[j]][buf->cfg.aktif_x],
                       buf->isi[indeks[j+1]][buf->cfg.aktif_x]) > 0) {
                t = indeks[j];
                indeks[j] = indeks[j+1];
                indeks[j+1] = t;
            }
        }
    }

    /* Copy sorted values back */
    for (c = 0; c < buf->cfg.kolom; c++) {
        for (r = 0; r < n; r++) {
            salin_str_aman(tmp[r], buf->isi[indeks[r]][c], MAKS_TEKS);
        }
        for (r = 0; r < n; r++) {
            salin_str_aman(buf->isi[r][c], tmp[r], MAKS_TEKS);
        }
    }

    for (r = 0; r < n; r++) free(tmp[r]);
    free(indeks);
    free(tmp);

    buf->cfg.kotor = 1;
    snprintf(pesan_status, sizeof(pesan_status), 
             "Kolom %d disortir", buf->cfg.aktif_x + 1);
}

void sortir_multi_kolom(struct buffer_tabel *buf, const int *kol,
                        const urutan_sortir *urut, int nkol) {
    /* Placeholder for multi-column sort */
    (void)buf;
    (void)kol;
    (void)urut;
    (void)nkol;
}

/* ============================================================
 * PENCARIAN
 * ============================================================ */

int lakukan_cari(struct buffer_tabel *buf, const char *query) {
    struct lembar_tabel *lem;
    int r, c;

    if (!buf || !query || !query[0]) return 0;

    lem = buf->lembar[buf->lembar_aktif];
    lem->jumlah_hasil = 0;
    lem->indeks_hasil = 0;
    salin_str_aman(lem->query_cari, query, MAKS_TEKS);

    for (r = 0; r < buf->cfg.baris && lem->jumlah_hasil < lem->baris * lem->kolom; r++) {
        for (c = 0; c < buf->cfg.kolom; c++) {
            if (strstr(buf->isi[r][c], query) != NULL) {
                lem->hasil_cari_x[lem->jumlah_hasil] = c;
                lem->hasil_cari_y[lem->jumlah_hasil] = r;
                lem->jumlah_hasil++;
            }
        }
    }

    if (lem->jumlah_hasil > 0) {
        buf->cfg.aktif_x = lem->hasil_cari_x[0];
        buf->cfg.aktif_y = lem->hasil_cari_y[0];
        pastikan_aktif_terlihat(buf);
    }

    snprintf(pesan_status, sizeof(pesan_status),
             "Ditemukan %d hasil", lem->jumlah_hasil);

    return lem->jumlah_hasil;
}

int lakukan_cari_ganti(struct buffer_tabel *buf, const char *cmd) {
    char cari[MAKS_TEKS], ganti[MAKS_TEKS];
    char *p1, *p2;
    int r, c, count = 0;

    if (!buf || !cmd) return 0;

    cari[0] = '\0';
    ganti[0] = '\0';

    p1 = strchr(cmd, '/');
    if (p1) {
        p1++;
        p2 = strchr(p1, '/');
        if (p2) {
            salin_str_aman(cari, p1, (size_t)(p2 - p1 + 1));
            cari[p2 - p1] = '\0';
            salin_str_aman(ganti, p2 + 1, MAKS_TEKS);
        }
    }

    if (cari[0] == '\0') return 0;

    for (r = 0; r < buf->cfg.baris; r++) {
        for (c = 0; c < buf->cfg.kolom; c++) {
            if (strstr(buf->isi[r][c], cari) != NULL) {
                atur_teks_sel(buf, c, r, ganti, 1);
                count++;
            }
        }
    }

    buf->cfg.kotor = 1;
    snprintf(pesan_status, sizeof(pesan_status),
             "Ganti %d kali", count);

    return count;
}

void cari_next(struct buffer_tabel *buf) {
    struct lembar_tabel *lem;

    if (!buf) return;
    lem = buf->lembar[buf->lembar_aktif];

    if (lem->jumlah_hasil == 0) {
        snprintf(pesan_status, sizeof(pesan_status), "Tidak ada pencarian");
        return;
    }

    lem->indeks_hasil++;
    if (lem->indeks_hasil >= lem->jumlah_hasil) {
        lem->indeks_hasil = 0;
    }

    buf->cfg.aktif_x = lem->hasil_cari_x[lem->indeks_hasil];
    buf->cfg.aktif_y = lem->hasil_cari_y[lem->indeks_hasil];
    pastikan_aktif_terlihat(buf);

    snprintf(pesan_status, sizeof(pesan_status),
             "Hasil %d/%d", lem->indeks_hasil + 1, lem->jumlah_hasil);
}

void cari_prev(struct buffer_tabel *buf) {
    struct lembar_tabel *lem;

    if (!buf) return;
    lem = buf->lembar[buf->lembar_aktif];

    if (lem->jumlah_hasil == 0) {
        snprintf(pesan_status, sizeof(pesan_status), "Tidak ada pencarian");
        return;
    }

    lem->indeks_hasil--;
    if (lem->indeks_hasil < 0) {
        lem->indeks_hasil = lem->jumlah_hasil - 1;
    }

    buf->cfg.aktif_x = lem->hasil_cari_x[lem->indeks_hasil];
    buf->cfg.aktif_y = lem->hasil_cari_y[lem->indeks_hasil];
    pastikan_aktif_terlihat(buf);

    snprintf(pesan_status, sizeof(pesan_status),
             "Hasil %d/%d", lem->indeks_hasil + 1, lem->jumlah_hasil);
}

/* ============================================================
 * FORMAT SEL
 * ============================================================ */

void atur_format_sel(struct buffer_tabel *buf, int x, int y,
                     jenis_format_sel fmt) {
    if (!buf || x < 0 || y < 0 || 
        x >= buf->cfg.kolom || y >= buf->cfg.baris) return;
    buf->format_sel[y][x] = fmt;
    buf->cfg.kotor = 1;
}

jenis_format_sel ambil_format_sel(const struct buffer_tabel *buf, int x, int y) {
    if (!buf || x < 0 || y < 0 || 
        x >= buf->cfg.kolom || y >= buf->cfg.baris) return FORMAT_UMUM;
    return buf->format_sel[y][x];
}

void atur_format_area(struct buffer_tabel *buf, jenis_format_sel fmt) {
    int x1, y1, x2, y2;
    int r, c;

    if (seleksi_aktif) {
        x1 = seleksi_x1 < seleksi_x2 ? seleksi_x1 : seleksi_x2;
        y1 = seleksi_y1 < seleksi_y2 ? seleksi_y1 : seleksi_y2;
        x2 = seleksi_x1 > seleksi_x2 ? seleksi_x1 : seleksi_x2;
        y2 = seleksi_y1 > seleksi_y2 ? seleksi_y1 : seleksi_y2;
    } else {
        x1 = x2 = buf->cfg.aktif_x;
        y1 = y2 = buf->cfg.aktif_y;
    }

    for (r = y1; r <= y2; r++) {
        for (c = x1; c <= x2; c++) {
            buf->format_sel[r][c] = fmt;
        }
    }

    buf->cfg.kotor = 1;
    snprintf(pesan_status, sizeof(pesan_status), "Format diterapkan");
}

/* Toggle font style untuk sel aktif */
void toggle_font_style(struct buffer_tabel *buf, font_style style) {
    int x = buf->cfg.aktif_x;
    int y = buf->cfg.aktif_y;
    
    if (!buf->style_sel) return;
    
    /* Toggle: jika sudah ada, hapus; jika belum, tambahkan */
    buf->style_sel[y][x] ^= style;
    buf->cfg.kotor = 1;
}

/* Toggle font style untuk area seleksi atau sel aktif */
void toggle_font_style_area(struct buffer_tabel *buf, font_style style) {
    int x1, y1, x2, y2;
    int r, c;
    const char *style_nama = "";

    if (!buf->style_sel) return;

    if (seleksi_aktif) {
        x1 = seleksi_x1 < seleksi_x2 ? seleksi_x1 : seleksi_x2;
        y1 = seleksi_y1 < seleksi_y2 ? seleksi_y1 : seleksi_y2;
        x2 = seleksi_x1 > seleksi_x2 ? seleksi_x1 : seleksi_x2;
        y2 = seleksi_y1 > seleksi_y2 ? seleksi_y1 : seleksi_y2;
    } else {
        x1 = x2 = buf->cfg.aktif_x;
        y1 = y2 = buf->cfg.aktif_y;
    }

    for (r = y1; r <= y2; r++) {
        for (c = x1; c <= x2; c++) {
            buf->style_sel[r][c] ^= style;
        }
    }

    buf->cfg.kotor = 1;
    
    /* Nama style untuk pesan */
    switch (style) {
        case STYLE_BOLD: style_nama = "Bold"; break;
        case STYLE_ITALIC: style_nama = "Italic"; break;
        case STYLE_UNDERLINE: style_nama = "Underline"; break;
        case STYLE_STRIKETHROUGH: style_nama = "Strikethrough"; break;
        default: style_nama = "Style"; break;
    }
    snprintf(pesan_status, sizeof(pesan_status), "%s di-toggle", style_nama);
}

int format_nilai_ke_teks(double nilai, jenis_format_sel fmt,
                         char *keluaran, size_t ukuran) {
    time_t t;
    struct tm *tm_info;

    if (!keluaran || ukuran == 0) return -1;

    switch (fmt) {
    case FORMAT_ANGKA:
        snprintf(keluaran, ukuran, "%.2f", nilai);
        break;
    case FORMAT_TANGGAL:
        t = (time_t)nilai;
        tm_info = localtime(&t);
        if (tm_info) {
            strftime(keluaran, ukuran, "%Y-%m-%d", tm_info);
        } else {
            snprintf(keluaran, ukuran, "%.0f", nilai);
        }
        break;
    case FORMAT_WAKTU:
        t = (time_t)nilai;
        tm_info = localtime(&t);
        if (tm_info) {
            strftime(keluaran, ukuran, "%H:%M:%S", tm_info);
        } else {
            snprintf(keluaran, ukuran, "%.0f", nilai);
        }
        break;
    case FORMAT_TIMESTAMP:
        t = (time_t)nilai;
        tm_info = localtime(&t);
        if (tm_info) {
            strftime(keluaran, ukuran, "%Y-%m-%d %H:%M:%S", tm_info);
        } else {
            snprintf(keluaran, ukuran, "%.0f", nilai);
        }
        break;
    case FORMAT_MATA_UANG:
        snprintf(keluaran, ukuran, "Rp %.2f", nilai);
        break;
    case FORMAT_PERSUAN:
        snprintf(keluaran, ukuran, "%.2f%%", nilai * 100);
        break;
    case FORMAT_TEKS:
    case FORMAT_UMUM:
    default:
        snprintf(keluaran, ukuran, "%g", nilai);
        break;
    }

    return 0;
}

int deteksi_format_otomatis(double nilai) {
    (void)nilai;
    return FORMAT_UMUM;
}
