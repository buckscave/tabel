/* ========================================================================== *
 * TABEL v3.0                                                                 *
 * Berkas: jendela.c - Modul Pengaturan Jendela                               *
 * Standar: C89 + POSIX.1-2008
 * Penulis: Chandra Lesmana
 * ========================================================================== */

#include "tabel.h"

/* Global Root Window */
struct jendela *jendela_root = NULL;
struct jendela *jendela_aktif = NULL;
extern void taruh_teks(int x, int y, const char *s);

/* ========================================================================== *
 * Helper Alokasi & Inisialisasi                                              *
 * ========================================================================== */
struct jendela *buat_jendela_baru(int buffer_idx) {
    struct jendela *j = calloc(1, sizeof(struct jendela));
    if (!j) {
        return NULL;
    }
    
    j->jenis = JENIS_DAUN;
    j->buffer_idx = buffer_idx;
    j->parent = NULL;
    j->child1 = NULL;
    j->child2 = NULL;
    
    /* Default view state */
    j->view.aktif_x = 0;
    j->view.aktif_y = 0;
    j->view.lihat_kolom = 0;
    j->view.lihat_baris = 0;

    /* Sinkronisasi awal dengan buffer jika valid */
    if (buffer_idx >= 0 && buffer_idx < jumlah_buffer) {
        struct buffer_tabel *b = buffers[buffer_idx];
        j->view.aktif_x = b->cfg.aktif_x;
        j->view.aktif_y = b->cfg.aktif_y;
        j->view.lihat_kolom = b->cfg.lihat_kolom;
        j->view.lihat_baris = b->cfg.lihat_baris;
    }

    return j;
}

void inisialisasi_window_manager(void) {
    if (jendela_root) {
        return; /* Sudah init */
    }
    
    /* Buat jendela root menunjuk ke buffer aktif saat ini */
    jendela_root = buat_jendela_baru(buffer_aktif);
    jendela_root->rect.x = 0;
    jendela_root->rect.y = 0; /* Akan diupdate layout */
    jendela_root->rect.w = ambil_lebar_terminal();
    jendela_root->rect.h = ambil_tinggi_terminal();
    
    jendela_aktif = jendela_root;
}

/* ========================================================================== *
 * Sinkronisasi State (View <-> Buffer)                                       *
 * ========================================================================== */
void simpan_state_jendela(struct jendela *j) {
    if (!j || j->jenis != JENIS_DAUN || j->buffer_idx < 0) {
        return;
    }
    struct buffer_tabel *b = buffers[j->buffer_idx];
    
    /* Simpan posisi kursor/scroll dari buffer ke struct jendela 
       sebelum pindah ke jendela lain */
    j->view.aktif_x = b->cfg.aktif_x;
    j->view.aktif_y = b->cfg.aktif_y;
    j->view.lihat_kolom = b->cfg.lihat_kolom;
    j->view.lihat_baris = b->cfg.lihat_baris;
}

void muat_state_jendela(struct jendela *j) {
    if (!j || j->jenis != JENIS_DAUN || j->buffer_idx < 0) {
        return;
    }
    struct buffer_tabel *b = buffers[j->buffer_idx];

    /* Kembalikan konfigurasi buffer sesuai state jendela ini */
    b->cfg.aktif_x = j->view.aktif_x;
    b->cfg.aktif_y = j->view.aktif_y;
    b->cfg.lihat_kolom = j->view.lihat_kolom;
    b->cfg.lihat_baris = j->view.lihat_baris;
    
    /* Set buffer aktif global agar fungsi lain (kunci.c) 
     * bekerja pada buffer yg benar */
    set_buffer_aktif(j->buffer_idx);
}

/* ========================================================================== *
 * Layouting Rekursif                                                         *
 * ========================================================================== */
void update_layout_rekursif(struct jendela *j, int x, int y, int w, int h) {
    if (!j) {
        return;
    }
    
    j->rect.x = x; 
    j->rect.y = y; 
    j->rect.w = w; 
    j->rect.h = h;

    if (j->jenis == JENIS_DAUN) {
        return;
    }

    int size1, size2;
    if (j->jenis == JENIS_SPLIT_H) {
        if (j->ratio > 0 && j->ratio < h) {
            size1 = j->ratio;
        } else {
            size1 = h / 2;
        }
        size2 = h - size1;
        update_layout_rekursif(j->child1, x, y, w, size1);
        update_layout_rekursif(j->child2, x, y + size1, w, size2);
    } else if (j->jenis == JENIS_SPLIT_V) {
        if (j->ratio > 0 && j->ratio < w) {
            size1 = j->ratio;
        } else {
            size1 = w / 2;
        }
        size2 = w - size1;
        update_layout_rekursif(j->child1, x, y, size1, h);
        update_layout_rekursif(j->child2, x + size1, y, size2, h);
    }
}

void paksa_recalc_layout(void) {
    if (!jendela_root) {
        return;
    }

    int w = ambil_lebar_terminal();
    int h = ambil_tinggi_terminal();
    
    /* Topbar sekarang per-window (di dalam rect masing-masing).
     * Layout mulai dari y=0.
     * Tinggi dikurangi 1 untuk Message Bar di bawah.
     */
    update_layout_rekursif(jendela_root, 0, 0, w, h - 1);
}

/* ========================================================================== *
 * Operasi Split                                                              *
 * ========================================================================== */
void split_window_aktif(int arah, int ratio) {
    if (!jendela_aktif) return;
    simpan_state_jendela(jendela_aktif);

    struct jendela *parent_baru = calloc(1, sizeof(struct jendela));
    struct jendela *daun_baru = buat_jendela_baru(jendela_aktif->buffer_idx);
    struct jendela *daun_lama = jendela_aktif;
    daun_baru->view = daun_lama->view;

    parent_baru->jenis = (arah == 0) ? JENIS_SPLIT_H : JENIS_SPLIT_V;
    parent_baru->ratio = ratio; /* simpan rasio */
    parent_baru->parent = daun_lama->parent;
    parent_baru->child1 = daun_lama;
    parent_baru->child2 = daun_baru;

    if (daun_lama->parent) {
        if (daun_lama->parent->child1 == daun_lama) {
            daun_lama->parent->child1 = parent_baru;
        } else { 
            daun_lama->parent->child2 = parent_baru;
        }
    } else {
        jendela_root = parent_baru;
    }

    daun_lama->parent = parent_baru;
    daun_baru->parent = parent_baru;

    paksa_recalc_layout();
    jendela_aktif = daun_baru;
    muat_state_jendela(jendela_aktif);
    snprintf(pesan_status, sizeof(pesan_status), 
            "Split %s", arah ? "Vertikal" : "Horizontal");
}

void resize_window_aktif(int delta) {
    if (!jendela_aktif || !jendela_aktif->parent) {
        return;
    }

    struct jendela *p = jendela_aktif->parent;
    if (p->jenis == JENIS_SPLIT_H) {
        p->ratio += delta;
        if (p->ratio < 1) {
            p->ratio = 1;
        }
        if (p->ratio > p->rect.h - 1) {
            p->ratio = p->rect.h - 1;
        }
    } else if (p->jenis == JENIS_SPLIT_V) {
        p->ratio += delta;
        if (p->ratio < 1) {
            p->ratio = 1;
        }
        if (p->ratio > p->rect.w - 1) {
            p->ratio = p->rect.w - 1;
        }
    }
    paksa_recalc_layout();
}

static int hitung_penggunaan_idx_rekursif(struct jendela *node, int idx) {
    if (!node) return 0;
    if (node->jenis == JENIS_DAUN) return (node->buffer_idx == idx) ? 1 : 0;
    return hitung_penggunaan_idx_rekursif(node->child1, idx) +
           hitung_penggunaan_idx_rekursif(node->child2, idx);
}

int hitung_penggunaan_idx(int idx) {
    return hitung_penggunaan_idx_rekursif(jendela_root, idx);
}

/* ========================================================================== *
 * Operasi Tutup (Close)                                                      *
 * ========================================================================== */
void tutup_window_aktif(void) {
    if (!jendela_aktif) {
        return;
    }
    if (jendela_aktif == jendela_root) {
        snprintf(pesan_status, sizeof(pesan_status), 
                "Jendela terakhir tidak bisa ditutup");
        return; 
    }

    struct jendela *target = jendela_aktif;
    struct jendela *parent = target->parent;
    struct jendela *sibling = (parent->child1 == target) ? 
        parent->child2 : parent->child1;
    struct jendela *grandparent = parent->parent;

    /* Fokus pindah ke sibling */
    /* Kita perlu mencari leaf terdalam dari sibling jika sibling bukan leaf 
       (misal sibling adalah container split). 
       Sederhananya: ambil sibling, jika sibling leaf, aktifkan. 
       Jika tidak, cari turunan leaf pertamanya. */
    struct jendela *next_focus = sibling;
    while (next_focus->jenis != JENIS_DAUN) {
        next_focus = next_focus->child1; 
    }

    /* Bypass Parent: Hubungkan Grandparent ke Sibling */
    sibling->parent = grandparent;
    if (grandparent) {
        if (grandparent->child1 == parent) {
            grandparent->child1 = sibling;
        } else {
            grandparent->child2 = sibling;
        }
    } else {
        jendela_root = sibling;
    }

    /* Bersihkan memori */
    free(target);
    free(parent);

    jendela_aktif = next_focus;
    muat_state_jendela(jendela_aktif);
    paksa_recalc_layout();
    snprintf(pesan_status, sizeof(pesan_status), 
            "Jendela ditutup");
}

/* ========================================================================== *
 * Navigasi Fokus (HJKL / Arrows)                                             *
 * ========================================================================== */
/* Fungsi rekursif untuk mencari jendela terdekat pada arah tertentu */
static void cari_jendela_arah(struct jendela *node, 
        struct rect r_asal, int arah, struct jendela **kandidat, 
        int *jarak_min) {
    if (!node) {
        return;
    }
    
    if (node->jenis == JENIS_DAUN) {
        if (node == jendela_aktif) {
            return; /* Skip diri sendiri */
        }

        int dist = -1;
        int valid = 0;

        /* Logika sederhana berdasarkan titik pusat */
        int cx_asal = r_asal.x + r_asal.w/2;
        int cy_asal = r_asal.y + r_asal.h/2;
        int cx_target = node->rect.x + node->rect.w/2;
        int cy_target = node->rect.y + node->rect.h/2;

        switch (arah) {
            case K_LEFT: 
                if (cx_target < r_asal.x) { /* Di kiri */
                     /* Harus ada overlap Y atau minimal dekat secara Y */
                     valid = 1;
                     dist = (r_asal.x - cx_target) + 
                         abs(cy_asal - cy_target) * 2;
                }
                break;
            case K_RIGHT:
                if (cx_target > r_asal.x + r_asal.w) { /* Di kanan */
                    valid = 1;
                    dist = (cx_target - (r_asal.x + r_asal.w)) + 
                        abs(cy_asal - cy_target) * 2;
                }
                break;
            case K_UP:
                if (cy_target < r_asal.y) { /* Di atas */
                    valid = 1;
                    dist = (r_asal.y - cy_target) + 
                        abs(cx_asal - cx_target) * 2;
                }
                break;
            case K_DOWN:
                if (cy_target > r_asal.y + r_asal.h) { /* Di bawah */
                    valid = 1;
                    dist = (cy_target - (r_asal.y + r_asal.h)) + 
                        abs(cx_asal - cx_target) * 2;
                }
                break;
        }

        if (valid && (dist < *jarak_min || *jarak_min == -1)) {
            *jarak_min = dist;
            *kandidat = node;
        }
        return;
    }

    cari_jendela_arah(node->child1, r_asal, arah, kandidat, jarak_min);
    cari_jendela_arah(node->child2, r_asal, arah, kandidat, jarak_min);
}

void pindah_fokus_jendela(int arah_kunci) {
    if (!jendela_aktif) {
        return;
    }
    
    /* Simpan state dulu */
    simpan_state_jendela(jendela_aktif);

    struct jendela *kandidat = NULL;
    int jarak = -1;

    cari_jendela_arah(jendela_root, jendela_aktif->rect, 
            arah_kunci, &kandidat, &jarak);

    if (kandidat) {
        jendela_aktif = kandidat;
        muat_state_jendela(jendela_aktif);
    }
}

/* ========================================================================== *
 * Rendering Tree (Traversal)                                                 *
 * ========================================================================== */
/* Fungsi eksternal dari tampilan.c yang akan kita modifikasi */
extern void render_jendela_spesifik(const struct buffer_tabel *buf, 
        struct rect area, int is_active);

void render_tree_jendela(struct jendela *node) {
    if (!node) {
        return;
    }

    if (node->jenis == JENIS_DAUN) {
        struct buffer_tabel *b = buffers[node->buffer_idx];
        int old_ax, old_ay, old_lx, old_ly;

        /* Backup state buffer */
        old_ax = b->cfg.aktif_x; 
        old_ay = b->cfg.aktif_y;
        old_lx = b->cfg.lihat_kolom; 
        old_ly = b->cfg.lihat_baris;

        /* Inject state window HANYA jika bukan jendela aktif.*/
        if (node != jendela_aktif) {
            b->cfg.aktif_x     = node->view.aktif_x;
            b->cfg.aktif_y     = node->view.aktif_y;
            b->cfg.lihat_kolom = node->view.lihat_kolom;
            b->cfg.lihat_baris = node->view.lihat_baris;
        }

        /* Render isi window */
        render_jendela_spesifik(b, node->rect, node == jendela_aktif);

        /* Sinkronkan kembali ke view */
        node->view.aktif_x     = b->cfg.aktif_x;
        node->view.aktif_y     = b->cfg.aktif_y;
        node->view.lihat_kolom = b->cfg.lihat_kolom;
        node->view.lihat_baris = b->cfg.lihat_baris;

        /* Restore buffer jika bukan aktif */
        if (node != jendela_aktif) {
            b->cfg.aktif_x     = old_ax;
            b->cfg.aktif_y     = old_ay;
            b->cfg.lihat_kolom = old_lx;
            b->cfg.lihat_baris = old_ly;
        }
    } else {
        int i;
        render_tree_jendela(node->child1);
        render_tree_jendela(node->child2);

        /* Garis pembatas */
        if (node->jenis == JENIS_SPLIT_V) {
            int split_x = node->child1->rect.x + 1 + node->child1->rect.w;
            for (i = node->rect.y; i < node->rect.y + node->rect.h; i++) {
                taruh_teks(split_x, i + 1, V);
            }
        }
    }
}

/* ========================================================================== *
 * Sinkronisasi Buffer Array Shift                                            *
 * ========================================================================== */
static void perbarui_index_rekursif(struct jendela *node, int idx_ditutup) {
    if (!node) {
        return;
    }

    if (node->jenis == JENIS_DAUN) {
        if (node->buffer_idx == idx_ditutup) {
            /* Jendela ini menampilkan buffer yang ditutup, 
             * arahkan ke buffer aktif yang baru */
            node->buffer_idx = buffer_aktif;
        } else if (node->buffer_idx > idx_ditutup) {
            /* Karena array buffers bergeser ke kiri, turunkan index-nya */
            node->buffer_idx--;
        }

        /* Safeguard pamungkas agar tidak out-of-bounds 
         * (Segfault prevention) */
        if (node->buffer_idx >= jumlah_buffer) {
            node->buffer_idx = jumlah_buffer > 0 ? jumlah_buffer - 1 : 0;
        }
    } else {
        perbarui_index_rekursif(node->child1, idx_ditutup);
        perbarui_index_rekursif(node->child2, idx_ditutup);
    }
}

void sinkronkan_tree_jendela_tutup(int idx_ditutup) {
    perbarui_index_rekursif(jendela_root, idx_ditutup);
}
