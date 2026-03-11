/* ============================================================
 * TABEL v3.0 
 * Berkas: bantuan.c - Tampilan Bantuan (Split Window dengan File)
 * Standar: C89 + POSIX.1-2008
 * Penulis: Chandra Lesmana
 * ============================================================
 * 
 * Fitur:
 * - Split window horizontal dengan buffer sendiri
 * - Top bar dengan format "BANTUAN: | TAB1 | TAB2 | ..."
 * - Status bar dengan path file dan info gulir
 * - Konten dimuat dari file teks (bukan hardcoded)
 * - Navigasi: Arrow (gulir per baris), Shift+Arrow (setengah halaman)
 * - Keluar dengan Alt+Q
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
    int cursor_y;                       /* Posisi kursor (untuk persentase) */
    int window_tinggi;                  /* Tinggi area konten */
    int aktif;                          /* Flag mode bantuan aktif */
    struct jendela *jendela_bantuan;    /* Pointer ke jendela bantuan */
    int jendela_sebelumnya_idx;         /* Index buffer sebelum bantuan */
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
    
    /* Cari direktori bantuan */
    /* 1. ./bantuan/ (relative to current dir) */
    /* 2. /usr/share/tabel/bantuan/ (system) */
    /* 3. ~/tabel/bantuan/ (home) */
    
    /* Gunakan path relatif dulu */
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
    return 0;
}

/* ============================================================
 * RENDERING BANTUAN
 * ============================================================ */

/* Gambar top bar bantuan dengan tabs */
static void gambar_topbar_bantuan(struct rect area) {
    int y = area.y + 1;
    int x = area.x + 1;
    int i;
    int pos_x;
    char buf[128];
    int sisa_ruang;
    
    /* Background topbar - hijau tua */
    warna_set_buffer(WARNAD_DEFAULT, WARNAL_HIJAU_TUA);
    isi_garis_h(area.x + 1, area.x + area.w, y);
    
    /* Tulis "BANTUAN:" dengan warna putih */
    warna_set_buffer(WARNAD_PUTIH, WARNAL_HIJAU_TUA);
    taruh_teks(x, y, " BANTUAN:");
    x += 10; /* " BANTUAN:" = 9 chars + 1 space */
    
    /* Gambar tabs */
    for (i = 0; i < g_bantuan.jumlah_tab; i++) {
        int tab_len;
        
        /* Cek apakah masih muat */
        sisa_ruang = (area.x + area.w) - x;
        if (sisa_ruang < 5) break; /* Minimal 5 karakter untuk tab */
        
        /* Pembatas "|" */
        warna_set_buffer(WARNAD_GELAP, WARNAL_HIJAU_TUA);
        taruh_teks(x, y, " |");
        x += 2;
        
        /* Tab */
        tab_len = strlen(g_bantuan.tabs[i].nama) + 2; /* +2 untuk spasi */
        if (x + tab_len > area.x + area.w) {
            /* Tab tidak muat, tulis "..." */
            warna_set_buffer(WARNAD_GELAP, WARNAL_HIJAU_TUA);
            taruh_teks(x, y, " ...");
            break;
        }
        
        if (i == g_bantuan.tab_aktif) {
            /* Tab aktif: latar hijau, teks gelap */
            warna_set_buffer(WARNAD_GELAP, WARNAL_HIJAU);
            snprintf(buf, sizeof(buf), " %s ", g_bantuan.tabs[i].nama);
        } else {
            /* Tab non-aktif: latar hijau tua, teks abu */
            warna_set_buffer(WARNAD_ABU, WARNAL_HIJAU_TUA);
            snprintf(buf, sizeof(buf), " %s ", g_bantuan.tabs[i].nama);
        }
        taruh_teks(x, y, buf);
        x += tab_len;
    }
    
    /* Tutup dengan "|" di akhir jika ada tab */
    if (g_bantuan.jumlah_tab > 0 && x < area.x + area.w) {
        warna_set_buffer(WARNAD_GELAP, WARNAL_HIJAU_TUA);
        taruh_teks(x, y, " |");
    }
}

/* Gambar konten bantuan */
static void gambar_konten_bantuan(struct rect area) {
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
    
    /* Update posisi kursor untuk persentase */
    g_bantuan.cursor_y = g_bantuan.scroll_y;
    
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
static void gambar_statusbar_bantuan(struct rect area) {
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
    warna_set_buffer(WARNAD_DEFAULT, WARNAL_HIJAU_TUA);
    isi_garis_h(area.x + 1, area.x + area.w, y);
    
    /* Tulis kiri */
    warna_set_buffer(WARNAD_PUTIH, WARNAL_HIJAU_TUA);
    taruh_teks(area.x + 1, y, kiri);
    
    /* Tulis kanan */
    panjang_kanan = strlen(kanan);
    awal_kanan = area.x + area.w - panjang_kanan;
    if (awal_kanan < area.x + 1) awal_kanan = area.x + 1;
    
    warna_set_buffer(WARNAD_GELAP, WARNAL_HIJAU);
    taruh_teks(awal_kanan, y, kanan);
}

/* Render penuh window bantuan */
static void render_bantuan_window(struct rect area) {
    /* Clear area */
    warna_set_buffer(WARNAD_DEFAULT, WARNAL_DEFAULT);
    for (int y = area.y + 1; y <= area.y + area.h; y++) {
        isi_garis_h(area.x + 1, area.x + area.w, y);
    }
    
    /* Gambar komponen */
    gambar_topbar_bantuan(area);
    gambar_konten_bantuan(area);
    gambar_statusbar_bantuan(area);
}

/* ============================================================
 * INPUT HANDLER BANTUAN
 * ============================================================ */

/* Proses input untuk mode bantuan */
/* Return: 0 = lanjut, 1 = keluar */
static int proses_input_bantuan(int ch, unsigned char *seq, int seq_len) {
    struct tab_bantuan *tab = &g_bantuan.tabs[g_bantuan.tab_aktif];
    int max_scroll;
    int half_page;
    
    half_page = g_bantuan.window_tinggi / 2;
    if (half_page < 1) half_page = 1;
    
    max_scroll = tab->jumlah_baris - g_bantuan.window_tinggi;
    if (max_scroll < 0) max_scroll = 0;
    
    /* Alt+Q untuk keluar (ESC + q atau 0xC3 0xB1 untuk Alt+q dalam UTF-8) */
    /* Dalam terminal, Alt+q biasanya dikirim sebagai ESC q atau 0x1B 0x71 */
    
    /* Cek escape sequence */
    if (ch == 0x1B) {
        /* Escape - mungkin Alt+kombinasi atau Escape biasa */
        if (seq_len >= 1) {
            /* Ada karakter setelah ESC */
            if (seq[0] == 'q' || seq[0] == 'Q') {
                /* Alt+Q - tutup bantuan */
                return 1;
            }
        }
        /* Escape saja - juga keluar */
        return 1;
    }
    
    /* Keyboard handling */
    switch (ch) {
        case 'q':
        case 'Q':
            /* q biasa - tidak melakukan apa-apa di mode bantuan */
            /* Hanya Alt+Q yang bisa keluar */
            break;
            
        case K_UP:
        case 'k':
            /* Gulir ke atas 1 baris */
            if (g_bantuan.scroll_y > 0) {
                g_bantuan.scroll_y--;
            }
            break;
            
        case K_DOWN:
        case 'j':
            /* Gulir ke bawah 1 baris */
            if (g_bantuan.scroll_y < max_scroll) {
                g_bantuan.scroll_y++;
            }
            break;
            
        case K_LEFT:
        case 'h':
            /* Tab sebelumnya */
            if (g_bantuan.tab_aktif > 0) {
                g_bantuan.tab_aktif--;
                g_bantuan.scroll_y = 0;
            }
            break;
            
        case K_RIGHT:
        case 'l':
            /* Tab berikutnya */
            if (g_bantuan.tab_aktif < g_bantuan.jumlah_tab - 1) {
                g_bantuan.tab_aktif++;
                g_bantuan.scroll_y = 0;
            }
            break;
            
        case K_PAGEUP:
            /* Gulir satu halaman ke atas */
            g_bantuan.scroll_y -= g_bantuan.window_tinggi;
            if (g_bantuan.scroll_y < 0) g_bantuan.scroll_y = 0;
            break;
            
        case K_PAGEDOWN:
            /* Gulir satu halaman ke bawah */
            g_bantuan.scroll_y += g_bantuan.window_tinggi;
            if (g_bantuan.scroll_y > max_scroll) g_bantuan.scroll_y = max_scroll;
            break;
            
        case K_HOME:
            /* Gulir ke awal */
            g_bantuan.scroll_y = 0;
            break;
            
        case K_END:
            /* Gulir ke akhir */
            g_bantuan.scroll_y = max_scroll;
            break;
    }
    
    /* Shift+Arrow (escape sequence [1;2A/B) */
    if (ch == 0x1B && seq_len >= 4 && seq[0] == '[' && seq[1] == '1' && 
        seq[2] == ';' && seq[3] == '2') {
        if (seq_len >= 5) {
            switch (seq[4]) {
                case 'A': /* Shift+Up - gulir setengah halaman ke atas */
                    g_bantuan.scroll_y -= half_page;
                    if (g_bantuan.scroll_y < 0) g_bantuan.scroll_y = 0;
                    break;
                case 'B': /* Shift+Down - gulir setengah halaman ke bawah */
                    g_bantuan.scroll_y += half_page;
                    if (g_bantuan.scroll_y > max_scroll) g_bantuan.scroll_y = max_scroll;
                    break;
            }
        }
    }
    
    return 0;
}

/* ============================================================
 * FUNGSI UTAMA BANTUAN
 * ============================================================ */

/* Cek apakah mode bantuan aktif */
int bantuan_aktif(void) {
    return g_bantuan.aktif;
}

/* Tutup bantuan */
void tutup_bantuan(void) {
    if (!g_bantuan.aktif) return;
    
    /* Tutup jendela bantuan jika ada */
    if (g_bantuan.jendela_bantuan) {
        /* Restore jendela aktif sebelumnya */
        /* Jendela bantuan akan ditutup oleh sistem window manager */
        g_bantuan.jendela_bantuan = NULL;
    }
    
    g_bantuan.aktif = 0;
    
    /* Trigger layout recalc */
    paksa_recalc_layout();
}

/* Tampilkan bantuan dengan split window */
void tampilkan_bantuan(void) {
    unsigned char buf[16];
    int len;
    int selesai = 0;
    struct rect area_bantuan;
    
    /* Jika sudah aktif, jangan buka lagi */
    if (g_bantuan.aktif) return;
    
    /* Inisialisasi tabs jika belum */
    if (g_bantuan.jumlah_tab == 0) {
        if (inisialisasi_tabs_bantuan() != 0) {
            /* Fallback ke bantuan sederhana */
            tampilkan_bantuan_sederhana();
            return;
        }
    }
    
    /* Reset state */
    g_bantuan.scroll_y = 0;
    g_bantuan.tab_aktif = 0;
    g_bantuan.aktif = 1;
    
    /* Set area bantuan (bottom half of screen) */
    area_bantuan.x = 0;
    area_bantuan.y = tinggi_layar / 2;
    area_bantuan.w = lebar_layar;
    area_bantuan.h = tinggi_layar - area_bantuan.y;
    
    atur_mode_terminal(1);
    
    /* Loop utama bantuan */
    while (!selesai && g_bantuan.aktif) {
        /* Render bantuan */
        render_bantuan_window(area_bantuan);
        
        /* Flush ke layar */
        terapkan_delta();
        flush_stdout();
        
        /* Baca input */
        len = 0;
        if (read(STDIN_FILENO, buf, 1) > 0) {
            int ch = buf[0];
            
            /* Baca escape sequence jika ada */
            if (ch == 0x1B) {
                /* Non-blocking read untuk karakter berikutnya */
                atur_mode_terminal(0);
                if (read(STDIN_FILENO, buf + 1, 5) > 0) {
                    len = 1; /* Ada escape sequence */
                }
                atur_mode_terminal(1);
            }
            
            /* Proses input */
            selesai = proses_input_bantuan(ch, buf + 1, len);
        }
    }
    
    g_bantuan.aktif = 0;
    
    /* Bersihkan buffer input */
    atur_mode_terminal(0);
    while (read(STDIN_FILENO, buf, 1) > 0);
    atur_mode_terminal(1);
}

/* Toggle bantuan - buka/tutup */
void toggle_bantuan(void) {
    if (g_bantuan.aktif) {
        /* Tutup bantuan */
        tutup_bantuan();
    } else {
        /* Buka bantuan */
        tampilkan_bantuan();
    }
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
        "  Alt+s        : Split Horizontal",
        "  Alt+v        : Split Vertikal",
        "  Alt+q        : Tutup Window",
        "",
        "Bantuan:",
        "  ?            : Tampilkan bantuan",
        "  Alt+q        : Keluar dari bantuan",
        "",
        "Keluar:",
        "  q            : Keluar aplikasi",
    };
    int n = sizeof(teks_bantuan) / sizeof(teks_bantuan[0]);
    int i;
    unsigned char buang;
    int y_start = 2;

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
               " Tekan Alt+q atau Escape untuk keluar dari bantuan ");

    terapkan_delta();
    
    /* Tunggu input */
    atur_mode_terminal(1);
    while (read(STDIN_FILENO, &buang, 1) <= 0);

    /* Bersihkan buffer */
    while (read(STDIN_FILENO, &buang, 1) > 0);
}

/* Cleanup */
void bersihkan_bantuan(void) {
    int i;
    for (i = 0; i < g_bantuan.jumlah_tab; i++) {
        bebas_konten_tab(&g_bantuan.tabs[i]);
    }
    g_bantuan.jumlah_tab = 0;
    g_bantuan.aktif = 0;
}
