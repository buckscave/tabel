/* ============================================================
 * TABEL v3.0 
 * Berkas: bantuan.c - Tampilan Bantuan (Integrasi Window Manager)
 * Standar: C89 + POSIX.1-2008
 * Penulis: Chandra Lesmana
 * ============================================================
 * 
 * Fitur:
 * - Integrasi penuh dengan window manager
 * - Dapat dinavigasi antar window dengan Ctrl+Alt+Arrow
 * - Top bar dengan format "BANTUAN: | TAB1 | TAB2 | ..."
 * - Status bar dengan path file dan info gulir
 * - Konten dimuat dari file teks (bukan hardcoded)
 * - Navigasi: Arrow (gulir/tab), PageUp/PageDown, Home/End
 * - Keluar dengan Alt+Q atau tutup window dengan Wq
 * ============================================================ */

#include "tabel.h"

/* ============================================================
 * KONSTANTA LOKAL
 * ============================================================ */
#define MAKS_TAB_BANTUAN    16
#define PANJANG_PATH_BANTUAN 512
#define MAKS_BARIS_BANTUAN  4096
#define PANJANG_BARIS_BANTUAN 2048

/* ============================================================
 * VARIABEL EKSTERNAL
 * ============================================================ */
extern char pesan_status[256];
extern struct buffer_tabel **buffers;
extern int buffer_aktif;
extern int jumlah_buffer;
extern int lebar_layar;
extern int tinggi_layar;
extern struct jendela *jendela_root;
extern struct jendela *jendela_aktif;

/* ============================================================
 * STRUKTUR DATA BANTUAN
 * ============================================================ */

/* Tab bantuan */
struct tab_bantuan {
    char nama[64];                      /* Nama tab */
    char path_file[PANJANG_PATH_BANTUAN]; /* Path lengkap file */
    char **isi;                         /* Array baris konten */
    int jumlah_baris;                   /* Total baris */
    int jumlah_kata;                    /* Total kata */
    int kolom_terpanjang;               /* Kolom terpanjang */
};

/* State viewer bantuan */
struct state_bantuan_viewer {
    struct tab_bantuan tabs[MAKS_TAB_BANTUAN];
    int jumlah_tab;
    int tab_aktif;
    int scroll_y;                       /* Offset gulir vertikal */
    int window_tinggi;                  /* Tinggi area konten */
    struct jendela *jendela_bantuan;    /* Pointer ke jendela bantuan */
    int initialized;                    /* Flag: sudah inisialisasi */
};

/* State global */
static struct state_bantuan_viewer g_bantuan = {0};

/* ============================================================
 * DEKLARASI FUNGSI EKSTERNAL
 * ============================================================ */
extern void warna_set_buffer(warna_depan wd, warna_latar wl);
extern void taruh_teks(int x, int y, const char *s);
extern void isi_garis_h(int x1, int x2, int y);
extern void terapkan_delta(void);
extern void atur_mode_terminal(int blocking);
extern void flush_stdout(void);
extern void bersihkan_layar_buffer(void);

/* ============================================================
 * FUNGSI HELPER
 * ============================================================ */

/* Hitung jumlah kata dalam string */
static int hitung_kata(const char *s) {
    int count = 0;
    int in_word = 0;
    
    while (*s) {
        if (*s == ' ' || *s == '\t' || *s == '\n') {
            in_word = 0;
        } else if (!in_word) {
            in_word = 1;
            count++;
        }
        s++;
    }
    return count;
}

/* Hitung lebar tampilan string (approximasi untuk UTF-8) */
static int lebar_visual_string(const char *s) {
    int len = 0;
    while (*s) {
        unsigned char c = (unsigned char)*s;
        if (c < 0x80) {
            len++;
            s++;
        } else if ((c & 0xE0) == 0xC0) {
            len += 2; /* Assume wide for CJK */
            s += 2;
        } else if ((c & 0xF0) == 0xE0) {
            len += 2;
            s += 3;
        } else if ((c & 0xF8) == 0xF0) {
            len += 2;
            s += 4;
        } else {
            s++;
        }
    }
    return len;
}

/* ============================================================
 * FUNGSI MEMUAT FILE BANTUAN
 * ============================================================ */

/* Bebaskan memori konten tab */
static void bebas_konten_tab(struct tab_bantuan *tab) {
    int i;
    if (tab->isi) {
        for (i = 0; i < tab->jumlah_baris; i++) {
            free(tab->isi[i]);
        }
        free(tab->isi);
        tab->isi = NULL;
        tab->jumlah_baris = 0;
        tab->jumlah_kata = 0;
        tab->kolom_terpanjang = 0;
    }
}

/* Muat file bantuan ke dalam tab */
static int muat_file_bantuan(struct tab_bantuan *tab, const char *path, const char *nama) {
    FILE *f;
    char baris[PANJANG_BARIS_BANTUAN];
    char **baris_array = NULL;
    int kapasitas = 256;
    int jumlah = 0;
    int total_kata = 0;
    int max_kolom = 0;
    int len_visual;
    
    /* Simpan path dan nama */
    strncpy(tab->path_file, path, PANJANG_PATH_BANTUAN - 1);
    tab->path_file[PANJANG_PATH_BANTUAN - 1] = '\0';
    strncpy(tab->nama, nama, 63);
    tab->nama[63] = '\0';
    
    /* Buka file */
    f = fopen(path, "r");
    if (!f) {
        return -1;
    }
    
    /* Alokasi awal */
    baris_array = malloc(kapasitas * sizeof(char*));
    if (!baris_array) {
        fclose(f);
        return -1;
    }
    
    /* Baca baris per baris */
    while (fgets(baris, sizeof(baris), f)) {
        /* Hapus newline di akhir */
        size_t len = strlen(baris);
        if (len > 0 && baris[len - 1] == '\n') {
            baris[len - 1] = '\0';
            len--;
        }
        
        /* Hitung statistik */
        total_kata += hitung_kata(baris);
        len_visual = lebar_visual_string(baris);
        if (len_visual > max_kolom) {
            max_kolom = len_visual;
        }
        
        /* Perlu resize? */
        if (jumlah >= kapasitas) {
            kapasitas *= 2;
            char **tmp = realloc(baris_array, kapasitas * sizeof(char*));
            if (!tmp) {
                fclose(f);
                free(baris_array);
                return -1;
            }
            baris_array = tmp;
        }
        
        /* Simpan baris */
        baris_array[jumlah] = strdup(baris);
        if (!baris_array[jumlah]) {
            fclose(f);
            while (jumlah > 0) {
                free(baris_array[--jumlah]);
            }
            free(baris_array);
            return -1;
        }
        jumlah++;
    }
    
    fclose(f);
    
    /* Simpan ke tab */
    tab->isi = baris_array;
    tab->jumlah_baris = jumlah;
    tab->jumlah_kata = total_kata;
    tab->kolom_terpanjang = max_kolom;
    
    return 0;
}

/* Inisialisasi tabs bantuan */
static int inisialisasi_tabs_bantuan(void) {
    char path_base[PANJANG_PATH_BANTUAN];
    char path_file[PANJANG_PATH_BANTUAN];
    int jumlah = 0;
    
    /* Gunakan path relatif */
    snprintf(path_base, sizeof(path_base), "bantuan");
    
    /* Daftar file bantuan yang mungkin ada */
    const char *file_bantuan[] = {
        "navigasi.txt",
        "edit.txt", 
        "seleksi.txt",
        "struktur.txt",
        "format.txt",
        "file.txt",
        "window.txt",
        "formula.txt",
        "pencarian.txt",
        "lainnya.txt"
    };
    int n_file = sizeof(file_bantuan) / sizeof(file_bantuan[0]);
    int i;
    
    /* Fallback: jika tidak ada file, gunakan konten built-in */
    int ada_file = 0;
    
    for (i = 0; i < n_file && jumlah < MAKS_TAB_BANTUAN; i++) {
        snprintf(path_file, sizeof(path_file), "%s/%s", path_base, file_bantuan[i]);
        if (muat_file_bantuan(&g_bantuan.tabs[jumlah], path_file, 
                              file_bantuan[i]) == 0) {
            /* Extract nama tanpa .txt */
            char *dot = strrchr(g_bantuan.tabs[jumlah].nama, '.');
            if (dot) *dot = '\0';
            /* Kapitalisasi huruf pertama */
            if (g_bantuan.tabs[jumlah].nama[0] >= 'a' && 
                g_bantuan.tabs[jumlah].nama[0] <= 'z') {
                g_bantuan.tabs[jumlah].nama[0] -= 32;
            }
            jumlah++;
            ada_file = 1;
        }
    }
    
    /* Jika tidak ada file sama sekali, gunakan konten built-in */
    if (!ada_file) {
        return -1;
    }
    
    g_bantuan.jumlah_tab = jumlah;
    g_bantuan.initialized = 1;
    return 0;
}

/* ============================================================
 * RENDERING BANTUAN (untuk integrasi window manager)
 * ============================================================ */

/* Gambar top bar bantuan dengan tabs */
static void gambar_topbar_bantuan(struct rect area, int aktif) {
    int y = area.y + 1;
    int x = area.x + 1;
    int i;
    int pos_x;
    char buf[128];
    int sisa_ruang;
    
    /* Background topbar - hijau tua atau lebih gelap jika tidak aktif */
    if (aktif) {
        warna_set_buffer(WARNAD_DEFAULT, WARNAL_HIJAU_TUA);
    } else {
        warna_set_buffer(WARNAD_GELAP, WARNAL_GELAP);
    }
    isi_garis_h(area.x + 1, area.x + area.w, y);
    
    /* Tulis "BANTUAN:" dengan warna */
    if (aktif) {
        warna_set_buffer(WARNAD_PUTIH, WARNAL_HIJAU_TUA);
    } else {
        warna_set_buffer(WARNAD_ABU, WARNAL_GELAP);
    }
    taruh_teks(x, y, " BANTUAN:");
    x += 10; /* " BANTUAN:" = 9 chars + 1 space */
    
    /* Gambar tabs */
    for (i = 0; i < g_bantuan.jumlah_tab; i++) {
        int tab_len;
        
        /* Cek apakah masih muat */
        sisa_ruang = (area.x + area.w) - x;
        if (sisa_ruang < 5) break; /* Minimal 5 karakter untuk tab */
        
        /* Pembatas "|" */
        if (aktif) {
            warna_set_buffer(WARNAD_GELAP, WARNAL_HIJAU_TUA);
        } else {
            warna_set_buffer(WARNAD_ABU, WARNAL_GELAP);
        }
        taruh_teks(x, y, " |");
        x += 2;
        
        /* Tab */
        tab_len = strlen(g_bantuan.tabs[i].nama) + 2; /* +2 untuk spasi */
        if (x + tab_len > area.x + area.w) {
            /* Tab tidak muat, tulis "..." */
            warna_set_buffer(WARNAD_GELAP, aktif ? WARNAL_HIJAU_TUA : WARNAL_GELAP);
            taruh_teks(x, y, " ...");
            break;
        }
        
        if (i == g_bantuan.tab_aktif) {
            /* Tab aktif: latar hijau, teks gelap */
            warna_set_buffer(WARNAD_GELAP, WARNAL_HIJAU);
            snprintf(buf, sizeof(buf), " %s ", g_bantuan.tabs[i].nama);
        } else {
            /* Tab non-aktif */
            if (aktif) {
                warna_set_buffer(WARNAD_ABU, WARNAL_HIJAU_TUA);
            } else {
                warna_set_buffer(WARNAD_ABU, WARNAL_GELAP);
            }
            snprintf(buf, sizeof(buf), " %s ", g_bantuan.tabs[i].nama);
        }
        taruh_teks(x, y, buf);
        x += tab_len;
    }
    
    /* Tutup dengan "|" di akhir jika ada tab */
    if (g_bantuan.jumlah_tab > 0 && x < area.x + area.w) {
        if (aktif) {
            warna_set_buffer(WARNAD_GELAP, WARNAL_HIJAU_TUA);
        } else {
            warna_set_buffer(WARNAD_ABU, WARNAL_GELAP);
        }
        taruh_teks(x, y, " |");
    }
}

/* Gambar konten bantuan */
static void gambar_konten_bantuan(struct rect area, int aktif) {
    struct tab_bantuan *tab = &g_bantuan.tabs[g_bantuan.tab_aktif];
    int y_start = area.y + 2; /* Baris pertama konten (setelah topbar) */
    int y_end = area.y + area.h - 1; /* Sebelum status bar */
    int x_start = area.x + 2;
    int y;
    int i;
    int baris_tampil;
    int max_scroll;
    char buf[PANJANG_BARIS_BANTUAN + 32];
    
    /* Hitung tinggi area konten */
    g_bantuan.window_tinggi = y_end - y_start;
    if (g_bantuan.window_tinggi < 1) g_bantuan.window_tinggi = 1;
    
    /* Hitung maksimum scroll */
    max_scroll = tab->jumlah_baris - g_bantuan.window_tinggi;
    if (max_scroll < 0) max_scroll = 0;
    
    /* Batasi scroll */
    if (g_bantuan.scroll_y < 0) g_bantuan.scroll_y = 0;
    if (g_bantuan.scroll_y > max_scroll) g_bantuan.scroll_y = max_scroll;
    
    /* Clear area konten */
    warna_set_buffer(WARNAD_DEFAULT, WARNAL_DEFAULT);
    for (y = y_start; y <= y_end; y++) {
        isi_garis_h(area.x + 1, area.x + area.w, y);
    }
    
    /* Render konten */
    baris_tampil = 0;
    for (i = g_bantuan.scroll_y; i < tab->jumlah_baris && y_start + baris_tampil <= y_end; i++) {
        const char *baris = tab->isi[i];
        y = y_start + baris_tampil;
        
        /* Deteksi header (baris yang tidak diawali spasi dan tidak kosong) */
        if (baris[0] != '\0' && baris[0] != ' ' && baris[0] != '\t') {
            /* Header - warna kuning */
            warna_set_buffer(WARNAD_KUNING, WARNAL_DEFAULT);
        } else {
            /* Konten normal - warna abu-abu */
            warna_set_buffer(WARNAD_ABU, WARNAL_DEFAULT);
        }
        
        /* Tulis baris dengan truncation jika perlu */
        snprintf(buf, sizeof(buf), "%s", baris);
        taruh_teks(x_start, y, buf);
        
        baris_tampil++;
    }
}

/* Gambar status bar bantuan */
static void gambar_statusbar_bantuan(struct rect area, int aktif) {
    struct tab_bantuan *tab = &g_bantuan.tabs[g_bantuan.tab_aktif];
    int y = area.y + area.h;
    char kiri[PANJANG_PATH_BANTUAN + 32];
    char kanan[256];
    int persen;
    int panjang_kanan;
    int awal_kanan;
    
    /* Hitung persentase gulir */
    if (tab->jumlah_baris <= g_bantuan.window_tinggi) {
        persen = 100;
    } else if (tab->jumlah_baris - g_bantuan.window_tinggi <= 0) {
        persen = 100;
    } else {
        persen = (g_bantuan.scroll_y * 100) / (tab->jumlah_baris - g_bantuan.window_tinggi);
        if (persen > 100) persen = 100;
    }
    
    /* Status kiri: path lengkap file */
    snprintf(kiri, sizeof(kiri), " %s", tab->path_file);
    
    /* Status kanan: persentase, baris, kolom, kata */
    snprintf(kanan, sizeof(kanan), 
             " Gulir:%d%% | Baris:%d | Kolom:%d | Kata:%d ",
             persen, tab->jumlah_baris, tab->kolom_terpanjang, tab->jumlah_kata);
    
    /* Background status bar */
    if (aktif) {
        warna_set_buffer(WARNAD_DEFAULT, WARNAL_HIJAU_TUA);
    } else {
        warna_set_buffer(WARNAD_GELAP, WARNAL_GELAP);
    }
    isi_garis_h(area.x + 1, area.x + area.w, y);
    
    /* Tulis kiri */
    if (aktif) {
        warna_set_buffer(WARNAD_PUTIH, WARNAL_HIJAU_TUA);
    } else {
        warna_set_buffer(WARNAD_ABU, WARNAL_GELAP);
    }
    taruh_teks(area.x + 1, y, kiri);
    
    /* Tulis kanan */
    panjang_kanan = strlen(kanan);
    awal_kanan = area.x + area.w - panjang_kanan;
    if (awal_kanan < area.x + 1) awal_kanan = area.x + 1;
    
    if (aktif) {
        warna_set_buffer(WARNAD_GELAP, WARNAL_HIJAU);
    } else {
        warna_set_buffer(WARNAD_ABU, WARNAL_GELAP);
    }
    taruh_teks(awal_kanan, y, kanan);
}

/* Render konten bantuan ke area tertentu (dipanggil dari window manager) */
void render_konten_bantuan(struct rect area, int aktif) {
    /* Clear area */
    int y;
    warna_set_buffer(WARNAD_DEFAULT, WARNAL_DEFAULT);
    for (y = area.y + 1; y <= area.y + area.h; y++) {
        isi_garis_h(area.x + 1, area.x + area.w, y);
    }
    
    /* Gambar komponen */
    gambar_topbar_bantuan(area, aktif);
    gambar_konten_bantuan(area, aktif);
    gambar_statusbar_bantuan(area, aktif);
}

/* ============================================================
 * INPUT HANDLER BANTUAN (untuk integrasi window manager)
 * ============================================================ */

/* Proses input untuk jendela bantuan */
/* Return: 1 jika input ditangani, 0 jika tidak */
int proses_input_bantuan_window(int ch, unsigned char *seq, int seq_len) {
    struct tab_bantuan *tab;
    int max_scroll;
    int half_page;
    
    /* Pastikan ada tab */
    if (g_bantuan.jumlah_tab == 0) {
        return 0;
    }
    
    tab = &g_bantuan.tabs[g_bantuan.tab_aktif];
    
    half_page = g_bantuan.window_tinggi / 2;
    if (half_page < 1) half_page = 1;
    
    max_scroll = tab->jumlah_baris - g_bantuan.window_tinggi;
    if (max_scroll < 0) max_scroll = 0;
    
    /* Alt+Q untuk keluar (ESC + q/Q) */
    if (ch == 0x1B && seq_len >= 1) {
        if (seq[0] == 'q' || seq[0] == 'Q') {
            /* Alt+Q - tutup jendela bantuan */
            tutup_bantuan();
            return 1;
        }
        /* Escape sequence lain (seperti arrow keys) - jangan keluar! */
        /* Lanjutkan untuk diproses di bawah */
    }
    
    /* Escape saja - jangan keluar, biarkan window manager handle */
    if (ch == 0x1B && seq_len == 0) {
        return 0; /* Biarkan window manager yang menangani */
    }
    
    /* Arrow keys: ESC [ A/B/C/D */
    if (ch == 0x1B && seq_len >= 2 && seq[0] == '[') {
        switch (seq[1]) {
            case 'A': /* Up - gulir ke atas */
                if (g_bantuan.scroll_y > 0) {
                    g_bantuan.scroll_y--;
                }
                return 1;
                
            case 'B': /* Down - gulir ke bawah */
                if (g_bantuan.scroll_y < max_scroll) {
                    g_bantuan.scroll_y++;
                }
                return 1;
                
            case 'C': /* Right - tab berikutnya */
                if (g_bantuan.tab_aktif < g_bantuan.jumlah_tab - 1) {
                    g_bantuan.tab_aktif++;
                    g_bantuan.scroll_y = 0;
                }
                return 1;
                
            case 'D': /* Left - tab sebelumnya */
                if (g_bantuan.tab_aktif > 0) {
                    g_bantuan.tab_aktif--;
                    g_bantuan.scroll_y = 0;
                }
                return 1;
        }
    }
    
    /* vim-style navigation */
    switch (ch) {
        case 'k':
            /* Gulir ke atas 1 baris */
            if (g_bantuan.scroll_y > 0) {
                g_bantuan.scroll_y--;
            }
            return 1;
            
        case 'j':
            /* Gulir ke bawah 1 baris */
            if (g_bantuan.scroll_y < max_scroll) {
                g_bantuan.scroll_y++;
            }
            return 1;
            
        case 'h':
            /* Tab sebelumnya */
            if (g_bantuan.tab_aktif > 0) {
                g_bantuan.tab_aktif--;
                g_bantuan.scroll_y = 0;
            }
            return 1;
            
        case 'l':
            /* Tab berikutnya */
            if (g_bantuan.tab_aktif < g_bantuan.jumlah_tab - 1) {
                g_bantuan.tab_aktif++;
                g_bantuan.scroll_y = 0;
            }
            return 1;
    }
    
    /* PageUp/PageDown */
    /* ESC [ 5 ~ atau ESC [ 6 ~ */
    if (ch == 0x1B && seq_len >= 3 && seq[0] == '[' && seq[2] == '~') {
        switch (seq[1]) {
            case '5': /* PageUp */
                g_bantuan.scroll_y -= g_bantuan.window_tinggi;
                if (g_bantuan.scroll_y < 0) g_bantuan.scroll_y = 0;
                return 1;
                
            case '6': /* PageDown */
                g_bantuan.scroll_y += g_bantuan.window_tinggi;
                if (g_bantuan.scroll_y > max_scroll) g_bantuan.scroll_y = max_scroll;
                return 1;
        }
    }
    
    /* Home/End: ESC [ H atau ESC [ F */
    if (ch == 0x1B && seq_len >= 2 && seq[0] == '[') {
        switch (seq[1]) {
            case 'H': /* Home - gulir ke awal */
                g_bantuan.scroll_y = 0;
                return 1;
                
            case 'F': /* End - gulir ke akhir */
                g_bantuan.scroll_y = max_scroll;
                return 1;
        }
    }
    
    /* Shift+Arrow: ESC [ 1 ; 2 A/B */
    if (ch == 0x1B && seq_len >= 4 && seq[0] == '[' && seq[1] == '1' && 
        seq[2] == ';' && seq[3] == '2') {
        if (seq_len >= 5) {
            switch (seq[4]) {
                case 'A': /* Shift+Up - gulir setengah halaman ke atas */
                    g_bantuan.scroll_y -= half_page;
                    if (g_bantuan.scroll_y < 0) g_bantuan.scroll_y = 0;
                    return 1;
                    
                case 'B': /* Shift+Down - gulir setengah halaman ke bawah */
                    g_bantuan.scroll_y += half_page;
                    if (g_bantuan.scroll_y > max_scroll) g_bantuan.scroll_y = max_scroll;
                    return 1;
            }
        }
    }
    
    /* Input tidak ditangani - kembalikan ke window manager */
    return 0;
}

/* ============================================================
 * FUNGSI UTAMA BANTUAN
 * ============================================================ */

/* Inisialisasi konten bantuan */
int inisialisasi_konten_bantuan(void) {
    if (g_bantuan.initialized) {
        return 0;
    }
    return inisialisasi_tabs_bantuan();
}

/* Cek apakah jendela bantuan ada */
int bantuan_aktif(void) {
    return (g_bantuan.jendela_bantuan != NULL);
}

/* Dapatkan pointer ke jendela bantuan */
struct jendela *dapatkan_jendela_bantuan(void) {
    return g_bantuan.jendela_bantuan;
}

/* Tutup bantuan */
void tutup_bantuan(void) {
    if (!g_bantuan.jendela_bantuan) return;
    
    /* Jika jendela aktif adalah bantuan, pindahkan fokus dulu */
    if (jendela_aktif == g_bantuan.jendela_bantuan) {
        /* Cari jendela lain untuk fokus */
        if (g_bantuan.jendela_bantuan->parent) {
            struct jendela *sibling = 
                (g_bantuan.jendela_bantuan->parent->child1 == g_bantuan.jendela_bantuan) ?
                g_bantuan.jendela_bantuan->parent->child2 :
                g_bantuan.jendela_bantuan->parent->child1;
            
            if (sibling) {
                /* Cari leaf terdalam */
                while (sibling->jenis != JENIS_DAUN) {
                    sibling = sibling->child1;
                }
                jendela_aktif = sibling;
            }
        }
    }
    
    /* Hapus jendela bantuan dari tree */
    if (g_bantuan.jendela_bantuan->parent) {
        struct jendela *parent = g_bantuan.jendela_bantuan->parent;
        struct jendela *sibling = (parent->child1 == g_bantuan.jendela_bantuan) ?
            parent->child2 : parent->child1;
        struct jendela *grandparent = parent->parent;
        
        /* Hubungkan sibling ke grandparent */
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
        
        free(g_bantuan.jendela_bantuan);
        free(parent);
    } else {
        /* Jendela bantuan adalah root - tidak seharusnya terjadi */
        free(g_bantuan.jendela_bantuan);
        jendela_root = NULL;
    }
    
    g_bantuan.jendela_bantuan = NULL;
    
    /* Trigger layout recalc */
    paksa_recalc_layout();
    
    snprintf(pesan_status, sizeof(pesan_status), "Bantuan ditutup");
}

/* Toggle bantuan - buka/tutup */
void toggle_bantuan(void) {
    if (g_bantuan.jendela_bantuan) {
        /* Tutup bantuan */
        tutup_bantuan();
    } else {
        /* Buka bantuan */
        tampilkan_bantuan();
    }
}

/* Tampilkan bantuan dengan split window horizontal */
void tampilkan_bantuan(void) {
    struct jendela *parent_baru;
    struct jendela *jendela_bantuan_baru;
    
    /* Jika sudah ada, jangan buka lagi */
    if (g_bantuan.jendela_bantuan) return;
    
    /* Inisialisasi tabs jika belum */
    if (!g_bantuan.initialized) {
        if (inisialisasi_tabs_bantuan() != 0) {
            /* Fallback ke bantuan sederhana */
            tampilkan_bantuan_sederhana();
            return;
        }
    }
    
    /* Pastikan ada jendela root */
    if (!jendela_root) {
        tampilkan_bantuan_sederhana();
        return;
    }
    
    /* Reset state */
    g_bantuan.scroll_y = 0;
    g_bantuan.tab_aktif = 0;
    
    /* Buat split horizontal baru */
    /* Simpan state jendela aktif */
    if (jendela_aktif) {
        simpan_state_jendela(jendela_aktif);
    }
    
    /* Alokasi parent baru */
    parent_baru = calloc(1, sizeof(struct jendela));
    if (!parent_baru) {
        tampilkan_bantuan_sederhana();
        return;
    }
    
    /* Alokasi jendela bantuan */
    jendela_bantuan_baru = calloc(1, sizeof(struct jendela));
    if (!jendela_bantuan_baru) {
        free(parent_baru);
        tampilkan_bantuan_sederhana();
        return;
    }
    
    /* Setup jendela bantuan */
    jendela_bantuan_baru->jenis = JENIS_DAUN;
    jendela_bantuan_baru->buffer_idx = -1; /* Tidak terkait buffer */
    jendela_bantuan_baru->parent = parent_baru;
    jendela_bantuan_baru->child1 = NULL;
    jendela_bantuan_baru->child2 = NULL;
    jendela_bantuan_baru->adalah_bantuan = 1;
    jendela_bantuan_baru->view.aktif_x = 0;
    jendela_bantuan_baru->view.aktif_y = 0;
    jendela_bantuan_baru->view.lihat_kolom = 0;
    jendela_bantuan_baru->view.lihat_baris = 0;
    
    /* Setup parent split */
    parent_baru->jenis = JENIS_SPLIT_H;
    parent_baru->ratio = tinggi_layar / 2; /* Split di tengah */
    parent_baru->parent = jendela_root->parent;
    parent_baru->child1 = jendela_root;  /* Window lama di atas */
    parent_baru->child2 = jendela_bantuan_baru; /* Bantuan di bawah */
    
    /* Update parent jendela root */
    jendela_root->parent = parent_baru;
    
    /* Set root ke parent baru */
    jendela_root = parent_baru;
    
    /* Simpan referensi */
    g_bantuan.jendela_bantuan = jendela_bantuan_baru;
    
    /* Set fokus ke bantuan */
    jendela_aktif = jendela_bantuan_baru;
    
    /* Trigger layout recalc */
    paksa_recalc_layout();
    
    snprintf(pesan_status, sizeof(pesan_status), 
             "Bantuan: Gunakan panah untuk navigasi, Alt+Q untuk tutup");
}

/* Versi sederhana bantuan (fallback) */
void tampilkan_bantuan_sederhana(void) {
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
        "  V            : Seleksi ke Target",
        "  y / x        : Copy / Cut",
        "  p / P        : Paste / Fill Paste",
        "  m / M        : Merge / Unmerge",
        "",
        "Window Manager:",
        "  Ws           : Split Horizontal",
        "  Wv           : Split Vertikal",
        "  Wq           : Tutup Window",
        "  Ctrl+Alt+Panah: Pindah fokus",
        "",
        "Bantuan:",
        "  ?            : Tampilkan bantuan",
        "  Alt+Q        : Keluar dari bantuan",
        "",
        "Keluar:",
        "  q            : Keluar aplikasi",
    };
    int n = sizeof(teks_bantuan) / sizeof(teks_bantuan[0]);
    int i;
    unsigned char buang;
    int y_start = 2;
    unsigned char seq[8];
    int seq_len;
    int ch;

    /* Clear screen area */
    warna_set_buffer(WARNAD_DEFAULT, WARNAL_DEFAULT);
    for (i = 1; i <= tinggi_layar; i++) {
        isi_garis_h(1, lebar_layar, i);
    }
    
    /* Tulis konten */
    for (i = 0; i < n && y_start + i < tinggi_layar - 2; i++) {
        if (strstr(teks_bantuan[i], "===") != NULL) {
            warna_set_buffer(WARNAD_KUNING, WARNAL_DEFAULT);
        } else if (teks_bantuan[i][0] != ' ' && teks_bantuan[i][0] != '\0') {
            warna_set_buffer(WARNAD_HIJAU, WARNAL_DEFAULT);
        } else {
            warna_set_buffer(WARNAD_ABU, WARNAL_DEFAULT);
        }
        taruh_teks(2, y_start + i, teks_bantuan[i]);
    }

    /* Status bar */
    warna_set_buffer(WARNAD_PUTIH, WARNAL_HIJAU_TUA);
    isi_garis_h(1, lebar_layar, tinggi_layar);
    taruh_teks(2, tinggi_layar, 
               " Tekan Alt+Q atau Escape untuk keluar dari bantuan ");

    terapkan_delta();
    flush_stdout();
    
    /* Tunggu input */
    atur_mode_terminal(1);
    while (1) {
        if (read(STDIN_FILENO, &ch, 1) > 0) {
            seq_len = 0;
            
            /* Baca escape sequence jika ada */
            if (ch == 0x1B) {
                atur_mode_terminal(0);
                if (read(STDIN_FILENO, seq, 5) > 0) {
                    seq_len = 1;
                    /* Cek Alt+Q */
                    if (seq[0] == 'q' || seq[0] == 'Q') {
                        atur_mode_terminal(1);
                        break;
                    }
                }
                atur_mode_terminal(1);
                /* Escape saja juga keluar */
                if (seq_len == 0) {
                    break;
                }
            }
        }
    }

    /* Bersihkan buffer */
    atur_mode_terminal(0);
    while (read(STDIN_FILENO, &buang, 1) > 0);
    atur_mode_terminal(1);
}

/* Cleanup */
void bersihkan_bantuan(void) {
    int i;
    for (i = 0; i < g_bantuan.jumlah_tab; i++) {
        bebas_konten_tab(&g_bantuan.tabs[i]);
    }
    g_bantuan.jumlah_tab = 0;
    g_bantuan.initialized = 0;
    g_bantuan.jendela_bantuan = NULL;
}
