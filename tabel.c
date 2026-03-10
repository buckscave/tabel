/* ============================================================
 * TABEL v3.0 
 * Berkas: tabel.c - Model Inti (Buffer, Lembar, Undo/Redo)
 * Standar: C89 + POSIX.1-2008
 * Penulis: Chandra Lesmana
 * ============================================================
 * 
 * Berkas ini berisi fungsi-fungsi inti:
 * - Data Global
 * - Manajemen Lembar (Sheet)
 * - Manajemen Buffer
 * - Undo/Redo
 * - Fungsi utama (main)
 * 
 * Untuk operasi sel, baris, kolom - lihat sel.c
 * Untuk tampilan bantuan - lihat bantuan.c
 * ============================================================ */

#include "tabel.h"

/* ============================================================
 * Data Global
 * ============================================================ */

/* Clipboard Global */
char papan_klip[MAKS_TEKS];
char ***papan_klip_area = NULL;
int klip_x1 = -1, klip_y1 = -1;
int klip_x2 = -1, klip_y2 = -1;
int klip_ada_area = 0;
int klip_mode = 0;

/* Buffer Manager */
struct buffer_tabel **buffers = NULL;
int jumlah_buffer = 0;
int buffer_aktif = -1;

/* Terminal State */
struct termios term_simpan;
int mode_raw_aktif = 0;
volatile sig_atomic_t perlu_resize = 0;

/* Seleksi State */
int sedang_memilih = 0;
int jangkar_x = -1; 
int jangkar_y = -1;
int seleksi_x1 = -1; 
int seleksi_y1 = -1; 
int seleksi_x2 = -1; 
int seleksi_y2 = -1;
int seleksi_aktif = 0;
char pesan_status[256] = "";
struct konfigurasi *konfig_global = NULL;

/* Random Selection */
int seleksi_random_x[MAKS_SELEKSI];
int seleksi_random_y[MAKS_SELEKSI];
int seleksi_random = 0;

/* ============================================================
 * Lembar (Sheet) Management
 * ============================================================ */

struct lembar_tabel *buat_lembar(int baris, int kolom, const char *nama) {
    struct lembar_tabel *lem;
    int r, c;

    if (baris < 1) baris = 1;
    if (kolom < 1) kolom = 1;
    if (baris > MAKS_BARIS) baris = MAKS_BARIS;
    if (kolom > MAKS_KOLOM) kolom = MAKS_KOLOM;

    lem = calloc(1, sizeof(*lem));
    if (!lem) return NULL;

    lem->baris = baris;
    lem->kolom = kolom;
    lem->aktif_x = lem->aktif_y = 0;
    lem->lihat_kolom = lem->lihat_baris = 0;

    if (nama && nama[0]) {
        salin_str_aman(lem->nama, nama, NAMA_LEMBAR_MAKS);
    } else {
        snprintf(lem->nama, NAMA_LEMBAR_MAKS, "Lembar");
    }

    /* Alokasi grid */
    lem->isi = calloc(baris, sizeof(char**));
    lem->perataan_sel = calloc(baris, sizeof(perataan_teks*));
    lem->tergabung = calloc(baris, sizeof(struct info_gabung*));
    for (r = 0; r < baris; r++) {
        lem->isi[r] = calloc(kolom, sizeof(char*));
        lem->perataan_sel[r] = calloc(kolom, sizeof(perataan_teks));
        lem->tergabung[r] = calloc(kolom, sizeof(struct info_gabung));
        for (c = 0; c < kolom; c++) {
            lem->isi[r][c] = calloc(MAKS_TEKS, sizeof(char));
            lem->perataan_sel[r][c] = RATA_KIRI;
            lem->tergabung[r][c].x_anchor = -1;
            lem->tergabung[r][c].y_anchor = -1;
        }
    }

    /* Ukuran kolom dan baris */
    lem->lebar_kolom = calloc(kolom, sizeof(int));
    lem->tinggi_baris = calloc(baris, sizeof(int));
    for (c = 0; c < kolom; c++) lem->lebar_kolom[c] = LEBAR_KOLOM_DEFAULT;
    for (r = 0; r < baris; r++) lem->tinggi_baris[r] = TINGGI_BARIS_DEFAULT;

    /* Undo/Redo */
    lem->tumpukan_undo = calloc(UNDO_MAKS, sizeof(struct operasi_undo));
    lem->tumpukan_redo = calloc(UNDO_MAKS, sizeof(struct operasi_undo));
    lem->undo_puncak = lem->redo_puncak = 0;

    /* Pencarian */
    lem->hasil_cari_x = calloc(MAKS_BARIS * MAKS_KOLOM, sizeof(int));
    lem->hasil_cari_y = calloc(MAKS_BARIS * MAKS_KOLOM, sizeof(int));
    lem->jumlah_hasil = 0;
    lem->indeks_hasil = 0;
    lem->query_cari[0] = '\0';

    /* Warna sel */
    lem->warna_sel = calloc(baris, sizeof(struct warna_kombinasi*));
    for (r = 0; r < baris; r++) {
        lem->warna_sel[r] = calloc(kolom, sizeof(struct warna_kombinasi));
        for (c = 0; c < kolom; c++) {
            lem->warna_sel[r][c].depan = WARNAD_DEFAULT;
            lem->warna_sel[r][c].latar = WARNAL_DEFAULT;
        }
    }

    /* Border sel */
    lem->border_sel = calloc(baris, sizeof(struct info_border*));
    for (r = 0; r < baris; r++) {
        lem->border_sel[r] = calloc(kolom, sizeof(struct info_border));
        for (c = 0; c < kolom; c++) {
            lem->border_sel[r][c].top = 0;
            lem->border_sel[r][c].bottom = 0;
            lem->border_sel[r][c].left = 0;
            lem->border_sel[r][c].right = 0;
            lem->border_sel[r][c].warna_depan = WARNAD_PUTIH;
            lem->border_sel[r][c].warna_latar = WARNAL_DEFAULT;
        }
    }

    /* Format sel */
    lem->format_sel = calloc(baris, sizeof(jenis_format_sel*));
    for (r = 0; r < baris; r++) {
        lem->format_sel[r] = calloc(kolom, sizeof(jenis_format_sel));
        for (c = 0; c < kolom; c++) {
            lem->format_sel[r][c] = FORMAT_UMUM;
        }
    }

    /* Style sel */
    lem->style_sel = calloc(baris, sizeof(font_style*));
    for (r = 0; r < baris; r++) {
        lem->style_sel[r] = calloc(kolom, sizeof(font_style));
        for (c = 0; c < kolom; c++) {
            lem->style_sel[r][c] = STYLE_NORMAL;
        }
    }

    return lem;
}

struct lembar_tabel *duplikat_lembar(const struct lembar_tabel *src) {
    struct lembar_tabel *baru;
    int r, c;

    if (!src) return NULL;
    baru = buat_lembar(src->baris, src->kolom, src->nama);
    if (!baru) return NULL;

    baru->aktif_x = src->aktif_x;
    baru->aktif_y = src->aktif_y;
    baru->lihat_kolom = src->lihat_kolom;
    baru->lihat_baris = src->lihat_baris;

    for (r = 0; r < src->baris; r++) {
        baru->tinggi_baris[r] = src->tinggi_baris[r];
        for (c = 0; c < src->kolom; c++) {
            if (r == 0) baru->lebar_kolom[c] = src->lebar_kolom[c];
            salin_str_aman(baru->isi[r][c], src->isi[r][c], MAKS_TEKS);
            baru->perataan_sel[r][c] = src->perataan_sel[r][c];
            baru->tergabung[r][c] = src->tergabung[r][c];
            if (baru->warna_sel && src->warna_sel) {
                baru->warna_sel[r][c] = src->warna_sel[r][c];
            }
            if (baru->style_sel && src->style_sel) {
                baru->style_sel[r][c] = src->style_sel[r][c];
            }
            baru->border_sel[r][c] = src->border_sel[r][c];
            baru->format_sel[r][c] = src->format_sel[r][c];
        }
    }

    return baru;
}

void bebas_lembar(struct lembar_tabel *lem) {
    int r, c;

    if (!lem) return;

    for (r = 0; r < lem->baris; r++) {
        for (c = 0; c < lem->kolom; c++) {
            free(lem->isi[r][c]);
        }
        free(lem->isi[r]);
        free(lem->perataan_sel[r]);
        if (lem->warna_sel) free(lem->warna_sel[r]);
        if (lem->style_sel) free(lem->style_sel[r]);
        free(lem->border_sel[r]);
        free(lem->tergabung[r]);
        free(lem->format_sel[r]);
    }

    free(lem->isi);
    free(lem->perataan_sel);
    free(lem->tergabung);
    free(lem->lebar_kolom);
    free(lem->tinggi_baris);
    free(lem->tumpukan_undo);
    free(lem->tumpukan_redo);
    free(lem->warna_sel);
    free(lem->style_sel);
    free(lem->border_sel);
    free(lem->format_sel);
    free(lem->hasil_cari_x);
    free(lem->hasil_cari_y);
    free(lem);
}

int tambah_lembar_ke_buffer(struct buffer_tabel *buf, struct lembar_tabel *lem) {
    struct lembar_tabel **tmp;
    char nama_default[NAMA_LEMBAR_MAKS];

    if (!buf || !lem) return -1;

    if (buf->jumlah_lembar >= MAKS_LEMBAR) {
        snprintf(pesan_status, sizeof(pesan_status),
                 "Maksimum %d lembar", MAKS_LEMBAR);
        return -1;
    }

    tmp = realloc(buf->lembar, (buf->jumlah_lembar + 1) * sizeof(*tmp));
    if (!tmp) return -1;
    buf->lembar = tmp;

    if (lem->nama[0] == '\0' || strcmp(lem->nama, "Lembar") == 0) {
        snprintf(nama_default, NAMA_LEMBAR_MAKS, "Lembar %d", 
                 buf->jumlah_lembar + 1);
        salin_str_aman(lem->nama, nama_default, NAMA_LEMBAR_MAKS);
    }

    buf->lembar[buf->jumlah_lembar] = lem;
    buf->jumlah_lembar++;

    return buf->jumlah_lembar - 1;
}

void hapus_lembar_dari_buffer(struct buffer_tabel *buf, int idx) {
    int i;

    if (!buf || idx < 0 || idx >= buf->jumlah_lembar) return;
    if (buf->jumlah_lembar <= 1) {
        snprintf(pesan_status, sizeof(pesan_status), 
                 "Tidak bisa menghapus lembar terakhir");
        return;
    }

    bebas_lembar(buf->lembar[idx]);
    for (i = idx; i < buf->jumlah_lembar - 1; i++) {
        buf->lembar[i] = buf->lembar[i + 1];
    }
    buf->jumlah_lembar--;

    if (buf->lembar_aktif >= buf->jumlah_lembar) {
        buf->lembar_aktif = buf->jumlah_lembar - 1;
    }
    if (buf->lembar_aktif < 0) buf->lembar_aktif = 0;

    sinkronkan_pointer_lembar_aktif(buf);
    buf->cfg.kotor = 1;
    
    snprintf(pesan_status, sizeof(pesan_status), 
             "Lembar %d dihapus", idx + 1);
}

void set_lembar_aktif(struct buffer_tabel *buf, int idx) {
    if (!buf || idx < 0 || idx >= buf->jumlah_lembar) return;

    buf->lembar_aktif = idx;
    sinkronkan_pointer_lembar_aktif(buf);
}

void sinkronkan_pointer_lembar_aktif(struct buffer_tabel *buf) {
    struct lembar_tabel *lem;

    if (!buf || !buf->lembar || buf->jumlah_lembar == 0) return;
    if (buf->lembar_aktif < 0 || buf->lembar_aktif >= buf->jumlah_lembar) {
        buf->lembar_aktif = 0;
    }

    lem = buf->lembar[buf->lembar_aktif];
    buf->isi = lem->isi;
    buf->perataan_sel = lem->perataan_sel;
    buf->lebar_kolom = lem->lebar_kolom;
    buf->tinggi_baris = lem->tinggi_baris;
    buf->tergabung = lem->tergabung;
    buf->border_sel = lem->border_sel;
    buf->warna_sel = lem->warna_sel;
    buf->style_sel = lem->style_sel;
    buf->format_sel = lem->format_sel;
    buf->tumpukan_undo = lem->tumpukan_undo;
    buf->tumpukan_redo = lem->tumpukan_redo;
    buf->undo_puncak = lem->undo_puncak;
    buf->redo_puncak = lem->redo_puncak;

    buf->cfg.kolom = lem->kolom;
    buf->cfg.baris = lem->baris;
    buf->cfg.aktif_x = lem->aktif_x;
    buf->cfg.aktif_y = lem->aktif_y;
    buf->cfg.lihat_kolom = lem->lihat_kolom;
    buf->cfg.lihat_baris = lem->lihat_baris;
}

void rename_lembar(struct buffer_tabel *buf, int idx, const char *nama) {
    if (!buf || idx < 0 || idx >= buf->jumlah_lembar || !nama) return;
    salin_str_aman(buf->lembar[idx]->nama, nama, NAMA_LEMBAR_MAKS);
    buf->cfg.kotor = 1;
}

/* ============================================================
 * Buffer Management
 * ============================================================ */

struct buffer_tabel *buat_buffer(int baris, int kolom) {
    struct buffer_tabel *buf;
    struct lembar_tabel *lem1, *lem2, *lem3;

    if (baris < 1) baris = 1;
    if (kolom < 1) kolom = 1;
    if (baris > MAKS_BARIS) baris = MAKS_BARIS;
    if (kolom > MAKS_KOLOM) kolom = MAKS_KOLOM;

    buf = calloc(1, sizeof(*buf));
    if (!buf) return NULL;

    buf->refcount = 1;
    buf->cfg.baris = baris;
    buf->cfg.kolom = kolom;
    buf->cfg.aktif_x = buf->cfg.aktif_y = 0;
    buf->cfg.lihat_kolom = buf->cfg.lihat_baris = 0;
    buf->cfg.kotor = 0;
    strcpy(buf->cfg.csv_delim, ",");
    strncpy(buf->cfg.enkoding, "UTF-8", ENKODING_MAKS);

    buf->lembar = NULL;
    buf->jumlah_lembar = 0;
    buf->lembar_aktif = 0;
    buf->lembar_scroll = 0;

    /* Buat 3 lembar default */
    lem1 = buat_lembar(baris, kolom, "Lembar 1");
    lem2 = buat_lembar(baris, kolom, "Lembar 2");
    lem3 = buat_lembar(baris, kolom, "Lembar 3");

    if (!lem1 || !lem2 || !lem3) {
        if (lem1) bebas_lembar(lem1);
        if (lem2) bebas_lembar(lem2);
        if (lem3) bebas_lembar(lem3);
        free(buf);
        return NULL;
    }

    tambah_lembar_ke_buffer(buf, lem1);
    tambah_lembar_ke_buffer(buf, lem2);
    tambah_lembar_ke_buffer(buf, lem3);

    sinkronkan_pointer_lembar_aktif(buf);

    /* Alokasi clipboard area */
    papan_klip_area = calloc(MAKS_BARIS, sizeof(char**));
    if (papan_klip_area) {
        int r, c;
        for (r = 0; r < MAKS_BARIS; r++) {
            papan_klip_area[r] = calloc(MAKS_KOLOM, sizeof(char*));
            if (papan_klip_area[r]) {
                for (c = 0; c < MAKS_KOLOM; c++) {
                    papan_klip_area[r][c] = calloc(MAKS_TEKS, sizeof(char));
                }
            }
        }
    }

    return buf;
}

struct buffer_tabel *duplikat_buffer(const struct buffer_tabel *src) {
    struct buffer_tabel *baru;
    int i;

    if (!src) return NULL;
    baru = calloc(1, sizeof(*baru));
    if (!baru) return NULL;

    baru->refcount = 1;
    baru->cfg = src->cfg;
    baru->cfg.kotor = 1;

    baru->lembar = NULL;
    baru->jumlah_lembar = 0;
    baru->lembar_aktif = src->lembar_aktif;
    baru->lembar_scroll = src->lembar_scroll;

    for (i = 0; i < src->jumlah_lembar; i++) {
        struct lembar_tabel *dup = duplikat_lembar(src->lembar[i]);
        if (dup) {
            tambah_lembar_ke_buffer(baru, dup);
        }
    }

    sinkronkan_pointer_lembar_aktif(baru);

    return baru;
}

void bebas_buffer(struct buffer_tabel *buf) {
    int i;

    if (!buf) return;

    buf->refcount--;
    if (buf->refcount > 0) return;

    for (i = 0; i < buf->jumlah_lembar; i++) {
        bebas_lembar(buf->lembar[i]);
    }
    free(buf->lembar);
    free(buf);
}

int tambah_buffer(struct buffer_tabel *buf) {
    struct buffer_tabel **tmp;

    tmp = realloc(buffers, (jumlah_buffer + 1) * sizeof(*tmp));
    if (!tmp) return -1;
    buffers = tmp;
    buffers[jumlah_buffer] = buf;
    buffer_aktif = jumlah_buffer;
    jumlah_buffer++;
    return buffer_aktif;
}

void tutup_buffer(int idx) {
    int i;
    if (idx < 0 || idx >= jumlah_buffer) return;
    bebas_buffer(buffers[idx]);
    for (i = idx; i < jumlah_buffer - 1; i++) {
        buffers[i] = buffers[i + 1];
    }
    jumlah_buffer--;
    if (jumlah_buffer == 0) {
        free(buffers);
        buffers = NULL;
        buffer_aktif = -1;
    } else if (buffer_aktif >= jumlah_buffer) {
        buffer_aktif = jumlah_buffer - 1;
    }
}

void set_buffer_aktif(int idx) {
    if (idx >= 0 && idx < jumlah_buffer) buffer_aktif = idx;
}

/* ============================================================
 * Undo/Redo
 * ============================================================ */

void dorong_undo_teks(struct buffer_tabel *buf, int x, int y,
                      const char *sblm, const char *ssdh) {
    struct operasi_undo u;
    struct lembar_tabel *lem;

    memset(&u, 0, sizeof(u));
    u.jenis = UNDO_TEKS;
    u.x = x; 
    u.y = y;
    salin_str_aman(u.sebelum, sblm, MAKS_TEKS);
    salin_str_aman(u.sesudah, ssdh, MAKS_TEKS);

    if (buf->lembar_aktif >= 0 && buf->lembar_aktif < buf->jumlah_lembar) {
        lem = buf->lembar[buf->lembar_aktif];
        if (lem->undo_puncak < UNDO_MAKS) {
            lem->tumpukan_undo[lem->undo_puncak++] = u;
        } else {
            memmove(lem->tumpukan_undo, lem->tumpukan_undo + 1, 
                    (UNDO_MAKS - 1) * sizeof(u));
            lem->tumpukan_undo[UNDO_MAKS - 1] = u;
        }
        lem->redo_puncak = 0;
        buf->undo_puncak = lem->undo_puncak;
        buf->redo_puncak = lem->redo_puncak;
    }
}

int lakukan_undo(struct buffer_tabel *buf) {
    struct operasi_undo u;
    struct lembar_tabel *lem;

    if (buf->undo_puncak <= 0) return -1;

    lem = buf->lembar[buf->lembar_aktif];
    u = lem->tumpukan_undo[--lem->undo_puncak];
    lem->tumpukan_redo[lem->redo_puncak++] = u;
    buf->undo_puncak = lem->undo_puncak;
    buf->redo_puncak = lem->redo_puncak;

    switch (u.jenis) {
    case UNDO_TEKS:
        salin_str_aman(buf->isi[u.y][u.x], u.sebelum, MAKS_TEKS);
        break;
    case UNDO_LAYOUT: {
        int lbr, tgi;
        if (sscanf(u.sebelum, "L:%d T:%d", &lbr, &tgi) == 2) {
            buf->lebar_kolom[u.x] = lbr;
            buf->tinggi_baris[u.y] = tgi;
        }
        break;
    }
    case UNDO_PERATAAN:
        buf->perataan_sel[u.y][u.x] = (perataan_teks)u.aux1;
        break;
    case UNDO_GABUNG:
        if (u.aux1) atur_blok_gabung_buf(buf, u.x, u.y, u.x2, u.y2);
        else hapus_blok_gabung_buf(buf, u.x, u.y);
        break;
    default:
        break;
    }
    buf->cfg.kotor = 1;
    return 0;
}

int lakukan_redo(struct buffer_tabel *buf) {
    struct operasi_undo r;
    struct lembar_tabel *lem;

    if (buf->redo_puncak <= 0) return -1;

    lem = buf->lembar[buf->lembar_aktif];
    r = lem->tumpukan_redo[--lem->redo_puncak];
    buf->undo_puncak = lem->undo_puncak;
    buf->redo_puncak = lem->redo_puncak;

    switch (r.jenis) {
    case UNDO_TEKS:
        salin_str_aman(buf->isi[r.y][r.x], r.sesudah, MAKS_TEKS);
        break;
    case UNDO_LAYOUT: {
        int lbr2, tgi2;
        if (sscanf(r.sesudah, "L:%d T:%d", &lbr2, &tgi2) == 2) {
            buf->lebar_kolom[r.x] = lbr2;
            buf->tinggi_baris[r.y] = tgi2;
        }
        break;
    }
    case UNDO_PERATAAN:
        buf->perataan_sel[r.y][r.x] = (perataan_teks)r.aux2;
        break;
    case UNDO_GABUNG:
        if (r.aux1) {
            hapus_blok_gabung_buf(buf, r.x, r.y);
        } else {
            atur_blok_gabung_buf(buf, r.x, r.y, r.x2, r.y2);
        }
        break;
    default:
        break;
    }
    buf->cfg.kotor = 1;
    return 0;
}

/* ============================================================
 * Fungsi Utama
 * ============================================================ */

int main(void) {
    /* Inisialisasi terminal */
    if (inisialisasi_terminal() != 0) {
        fprintf(stderr, "Gagal inisialisasi terminal\n");
        return 1;
    }
    masuk_mode_alternatif();
    atur_sigaction();

    /* Buat buffer awal (100 baris, 26 kolom) */
    struct buffer_tabel *buf = buat_buffer(100, 26);
    if (!buf) {
        fprintf(stderr, "Gagal membuat buffer\n");
        keluar_mode_alternatif();
        pulihkan_terminal();
        return 1;
    }
    tambah_buffer(buf);

    /* Inisialisasi window manager */
    inisialisasi_window_manager();
    paksa_recalc_layout();

    /* Jalankan loop utama aplikasi */
    mulai_aplikasi_poll(buf);

    /* Pulihkan terminal sebelum keluar */
    keluar_mode_alternatif();
    pulihkan_terminal();
    return 0;
}
