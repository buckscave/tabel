/* ============================================================
 * TABEL v3.0 
 * Berkas: bantuan.c - Tampilan Bantuan (Split Window)
 * Standar: C89 + POSIX.1-2008
 * Penulis: Chandra Lesmana
 * ============================================================
 * 
 * Fitur:
 * - Topbar dengan "HALAMAN BANTUAN" dan nama section
 * - Split view: daftar section (kiri) dan konten (kanan)
 * - Status bar dengan info aplikasi
 * - Navigasi dengan panah dan Enter
 * - Scroll konten dengan panah atas/bawah
 * ============================================================ */

#include "tabel.h"

/* ============================================================
 * VARIABEL EKSTERNAL (dari tabel.c dan tampilan.c)
 * ============================================================ */
extern char pesan_status[256];
extern struct buffer_tabel **buffers;
extern int buffer_aktif;
extern int jumlah_buffer;

/* Variabel dari tampilan.c */
extern int lebar_layar;
extern int tinggi_layar;

/* ============================================================
 * STRUKTUR DATA BANTUAN
 * ============================================================ */

/* Entry bantuan */
struct entri_bantuan {
    const char *judul;      /* Nama section */
    const char **isi;       /* Array string konten */
    int jumlah_baris;       /* Jumlah baris konten */
};

/* State bantuan */
struct state_bantuan {
    int section_aktif;      /* Index section yang dipilih */
    int scroll_y;           /* Scroll offset untuk konten */
    int jumlah_section;
    int lebar_section;      /* Lebar panel section */
};

/* ============================================================
 * KONTEN BANTUAN
 * ============================================================ */

/* Section: Navigasi */
static const char *bantuan_navigasi[] = {
    "NAVIGASI SEL",
    "",
    "  Panah Atas/Bawah/Kiri/Kanan  : Pindah antar sel",
    "  Tab                          : Maju ke sel berikutnya",
    "  Shift+Tab                    : Mundur ke sel sebelumnya",
    "  Home                         : Lompat ke kolom pertama",
    "  End                          : Lompat ke kolom terakhir",
    "  Ctrl+Home                    : Lompat ke baris pertama",
    "  Ctrl+End                     : Lompat ke baris terakhir",
    "  Page Up / Page Down          : Gulir satu halaman",
    "  g [kolom][baris]             : Lompat ke alamat (contoh: gA1)",
    "",
    "NAVIGASI WINDOW",
    "",
    "  Alt+Panah                    : Pindah fokus window",
    "  Alt+Shift+Panah              : Resize window aktif",
};

/* Section: Edit */
static const char *bantuan_edit[] = {
    "EDIT TEKS",
    "",
    "  Enter                        : Mode edit sel",
    "  Escape                       : Batalkan edit",
    "  Backspace                    : Hapus karakter kiri",
    "  Delete                       : Hapus karakter kanan",
    "",
    "PERATAAN",
    "",
    "  [                            : Rata kiri",
    "  ]                            : Rata kanan", 
    "  \\                            : Rata tengah",
    "",
    "KONVERSI HURUF",
    "",
    "  Alt+u                        : Ubah ke HURUF BESAR",
    "  Alt+l                        : Ubah ke huruf kecil",
    "  Alt+t                        : Ubah ke Judul Case",
};

/* Section: Seleksi */
static const char *bantuan_seleksi[] = {
    "MODE SELEKSI",
    "",
    "  v                            : Mode seleksi visual",
    "  V                            : Mode seleksi baris",
    "  Ctrl+v                       : Mode seleksi kolom",
    "  Escape                        : Keluar mode seleksi",
    "",
    "OPERASI SELEKSI",
    "",
    "  y                            : Salin (yank)",
    "  x                            : Potong (cut)",
    "  p                            : Tempel (paste)",
    "  d                            : Hapus isi seleksi",
    "  m                            : Gabung sel (merge)",
    "  M                            : Urai sel (unmerge)",
};

/* Section: Baris & Kolom */
static const char *bantuan_struktur[] = {
    "OPERASI BARIS",
    "",
    "  o                            : Sisip baris setelah",
    "  O                            : Sisip baris sebelum",
    "  b (2x)                       : Hapus baris aktif",
    "  Alt+Panah Atas/Bawah         : Geser baris",
    "",
    "OPERASI KOLOM",
    "",
    "  i                            : Sisip kolom setelah",
    "  I                            : Sisip kolom sebelum",
    "  d (2x)                       : Hapus kolom aktif",
    "  Alt+Panah Kiri/Kanan         : Geser kolom",
    "",
    "UKURAN",
    "",
    "  Alt+Panah Kiri/Kanan         : Ubah lebar kolom",
    "  Alt+Panah Atas/Bawah         : Ubah tinggi baris",
};

/* Section: Format */
static const char *bantuan_format[] = {
    "WARNA",
    "",
    "  :cw                          : Ubah warna teks",
    "",
    "BORDER",
    "",
    "  :ba                          : Border semua sisi",
    "  :bo                          : Border outline",
    "  :bt                          : Border atas",
    "  :bb                          : Border bawah",
    "  :bl                          : Border kiri",
    "  :br                          : Border kanan",
    "  :bw                          : Warna border",
    "  :none                        : Hapus border",
    "",
    "FONT STYLE",
    "",
    "  Ctrl+b                       : Bold (tebal)",
    "  Ctrl+i                       : Italic (miring)",
    "  Ctrl+u                       : Underline (garis bawah)",
};

/* Section: File */
static const char *bantuan_file[] = {
    "SIMPAN & BUKA",
    "",
    "  w                            : Simpan file",
    "  e                            : Buka file baru",
    "  Ctrl+w                       : Simpan sebagai...",
    "",
    "FORMAT YANG DIDUKUNG",
    "",
    "  Tabel Native   : .tbl",
    "  Excel          : .xlsx, .xls",
    "  OpenDocument   : .ods",
    "  CSV/TSV        : .csv, .tsv",
    "  Text           : .txt",
    "  JSON           : .json",
    "  HTML           : .html",
    "  PDF            : .pdf",
    "",
    "UNDO/REDO",
    "",
    "  u                            : Undo",
    "  r                            : Redo",
};

/* Section: Window */
static const char *bantuan_window[] = {
    "WINDOW MANAGER",
    "",
    "  W s                          : Split horizontal",
    "  W v                          : Split vertikal",
    "  W q                          : Tutup window",
    "  W o                          : Maximalkan window",
    "",
    "NAVIGASI WINDOW",
    "",
    "  Alt+Panah                    : Pindah fokus",
    "  Alt+Shift+Panah              : Resize window",
    "",
    "TAB BUFFER",
    "",
    "  t                            : Tab baru",
    "  T                            : Tutup tab aktif",
    "  [ / ]                        : Pindah tab",
};

/* Section: Formula */
static const char *bantuan_formula[] = {
    "SYNTAX FORMULA",
    "",
    "  Awali dengan = (sama dengan)",
    "  Contoh: =A1+B2",
    "",
    "FUNGSI AGREGAT",
    "",
    "  =SUM(A1:A10)      : Jumlah",
    "  =AVG(A1:A10)      : Rata-rata",
    "  =MIN(A1:A10)      : Nilai minimum",
    "  =MAX(A1:A10)      : Nilai maksimum",
    "  =COUNT(A1:A10)    : Hitung angka",
    "  =COUNTA(A1:A10)   : Hitung tidak kosong",
    "",
    "FUNGSI MATEMATIKA",
    "",
    "  =ABS(A1), =SQRT(A1), =POWER(A1,2)",
    "  =SIN(A1), =COS(A1), =TAN(A1)",
    "  =ROUND(A1,2), =FLOOR(A1), =CEIL(A1)",
    "",
    "FUNGSI TEKS",
    "",
    "  =UPPER(A1), =LOWER(A1), =LEN(A1)",
    "  =LEFT(A1,3), =RIGHT(A1,3), =MID(A1,2,3)",
    "",
    "FUNGSI WAKTU",
    "",
    "  =TODAY(), =NOW()",
    "  =YEAR(A1), =MONTH(A1), =DAY(A1)",
};

/* Section: Pencarian */
static const char *bantuan_pencarian[] = {
    "PENCARIAN",
    "",
    "  /                            : Cari teks",
    "  n                            : Hasil berikutnya",
    "  N                            : Hasil sebelumnya",
    "",
    "GANTI",
    "",
    "  :s/cari/ganti                : Cari dan ganti",
    "",
    "SORTIR",
    "",
    "  s                            : Sortir kolom aktif",
    "  S                            : Sortir descending",
};

/* Section: Lainnya */
static const char *bantuan_lainnya[] = {
    "INFORMASI",
    "",
    "  ?                            : Tampilkan bantuan",
    "  Ctrl+g                       : Info posisi kursor",
    "",
    "KELUAR",
    "",
    "  q                            : Keluar aplikasi",
    "  Q                            : Keluar tanpa simpan",
    "",
    "",
    "TABEL v3.0",
    "Aplikasi Spreadsheet Terminal",
    "Penulis: Chandra Lesmana",
    "",
    "Tekan Escape atau q untuk kembali",
};

/* ============================================================
 * DAFTAR SECTION
 * ============================================================ */

static const char *daftar_section[] = {
    "NAVIGASI",
    "EDIT",
    "SELEKSI",
    "STRUKTUR",
    "FORMAT",
    "FILE",
    "WINDOW",
    "FORMULA",
    "PENCARIAN",
    "LAINNYA"
};

#define JUMLAH_SECTION (sizeof(daftar_section) / sizeof(daftar_section[0]))

/* Array pointer ke konten */
static const char **konten_section[] = {
    bantuan_navigasi,
    bantuan_edit,
    bantuan_seleksi,
    bantuan_struktur,
    bantuan_format,
    bantuan_file,
    bantuan_window,
    bantuan_formula,
    bantuan_pencarian,
    bantuan_lainnya
};

/* Jumlah baris per section */
static int jumlah_baris_section[] = {
    sizeof(bantuan_navigasi) / sizeof(bantuan_navigasi[0]),
    sizeof(bantuan_edit) / sizeof(bantuan_edit[0]),
    sizeof(bantuan_seleksi) / sizeof(bantuan_seleksi[0]),
    sizeof(bantuan_struktur) / sizeof(bantuan_struktur[0]),
    sizeof(bantuan_format) / sizeof(bantuan_format[0]),
    sizeof(bantuan_file) / sizeof(bantuan_file[0]),
    sizeof(bantuan_window) / sizeof(bantuan_window[0]),
    sizeof(bantuan_formula) / sizeof(bantuan_formula[0]),
    sizeof(bantuan_pencarian) / sizeof(bantuan_pencarian[0]),
    sizeof(bantuan_lainnya) / sizeof(bantuan_lainnya[0])
};

/* ============================================================
 * FUNGSI RENDER BANTUAN
 * ============================================================ */

/* Deklarasi fungsi dari tampilan.c */
extern void warna_set_buffer(warna_depan wd, warna_latar wl);
extern void taruh_teks(int x, int y, const char *s);
extern void isi_garis_h(int x1, int x2, int y);
extern void terapkan_delta(void);
extern void atur_mode_terminal(int blocking);
extern void flush_stdout(void);

/* Fungsi helper untuk tampilan bantuan */
static int hitung_lebar_max_section(void) {
    int max = 0;
    size_t i;
    for (i = 0; i < JUMLAH_SECTION; i++) {
        int len = strlen(daftar_section[i]);
        if (len > max) max = len;
    }
    return max + 4; /* Padding */
}

/* Gambar topbar bantuan */
static void gambar_topbar_bantuan(int lebar, const char *section_nama) {
    char topbar[512];
    int pos;
    int sisa;
    
    warna_set_buffer(WARNAD_DEFAULT, WARNAL_HIJAU_TUA);
    isi_garis_h(1, lebar, 1);
    
    /* Kiri: HALAMAN BANTUAN */
    snprintf(topbar, sizeof(topbar), " HALAMAN BANTUAN ");
    warna_set_buffer(WARNAD_HIJAU, WARNAL_HITAM);
    taruh_teks(1, 1, topbar);
    
    /* Kanan: Section aktif */
    if (section_nama) {
        pos = lebar - strlen(section_nama) - 3;
        if (pos > 20) {
            sisa = lebar - pos;
            warna_set_buffer(WARNAD_KUNING, WARNAL_HIJAU_TUA);
            taruh_teks(pos, 1, section_nama);
        }
    }
}

/* Gambar panel section (kiri) */
static void gambar_panel_section(struct state_bantuan *state) {
    int i;
    int y = 3;
    int x = 2;
    char buf[64];
    
    /* Header panel */
    warna_set_buffer(WARNAD_ABU, WARNAL_GELAP);
    taruh_teks(x, y, "SECTION");
    y += 2;
    
    /* Daftar section */
    for (i = 0; i < (int)JUMLAH_SECTION && y < tinggi_layar - 3; i++) {
        if (i == state->section_aktif) {
            /* Section aktif */
            warna_set_buffer(WARNAD_HIJAU, WARNAL_HITAM);
            snprintf(buf, sizeof(buf), " > %s ", daftar_section[i]);
            taruh_teks(x, y, buf);
        } else {
            /* Section tidak aktif */
            warna_set_buffer(WARNAD_ABU, WARNAL_DEFAULT);
            snprintf(buf, sizeof(buf), "   %s ", daftar_section[i]);
            taruh_teks(x, y, buf);
        }
        y++;
    }
    
    /* Garis pemisah vertikal */
    warna_set_buffer(WARNAD_GELAP, WARNAL_DEFAULT);
    for (y = 3; y < tinggi_layar - 2; y++) {
        taruh_teks(state->lebar_section, y, V);
    }
}

/* Gambar panel konten (kanan) */
static void gambar_panel_konten(struct state_bantuan *state) {
    const char **konten = konten_section[state->section_aktif];
    int jumlah = jumlah_baris_section[state->section_aktif];
    int x = state->lebar_section + 3;
    int y = 3;
    int i;
    int max_y = tinggi_layar - 3;
    int max_scroll = jumlah - (max_y - y + 1);
    
    /* Batasi scroll */
    if (state->scroll_y < 0) state->scroll_y = 0;
    if (state->scroll_y > max_scroll && max_scroll > 0) state->scroll_y = max_scroll;
    
    /* Render konten */
    for (i = state->scroll_y; i < jumlah && y < max_y; i++) {
        const char *baris = konten[i];
        
        /* Cek apakah header section */
        if (baris[0] != '\0' && baris[0] != ' ' && strstr(baris, ":") == NULL) {
            /* Header - warna kuning */
            warna_set_buffer(WARNAD_KUNING, WARNAL_DEFAULT);
        } else if (baris[0] == ' ') {
            /* Konten - warna abu */
            warna_set_buffer(WARNAD_ABU, WARNAL_DEFAULT);
        } else {
            /* Baris kosong atau lainnya */
            warna_set_buffer(WARNAD_DEFAULT, WARNAL_DEFAULT);
        }
        
        taruh_teks(x, y, baris);
        y++;
    }
    
    /* Indikator scroll jika perlu */
    if (jumlah > (max_y - 3)) {
        char indikator[32];
        snprintf(indikator, sizeof(indikator), "[%d/%d]", state->scroll_y + 1, jumlah);
        warna_set_buffer(WARNAD_GELAP, WARNAL_DEFAULT);
        taruh_teks(lebar_layar - 10, tinggi_layar - 2, indikator);
    }
}

/* Gambar status bar bantuan */
static void gambar_statusbar_bantuan(const struct buffer_tabel *buf) {
    int y = tinggi_layar;
    char kiri[256], kanan[256];
    
    warna_set_buffer(WARNAD_DEFAULT, WARNAL_HIJAU_TUA);
    isi_garis_h(1, lebar_layar, y);
    
    /* Kiri: Info file */
    if (buf) {
        snprintf(kiri, sizeof(kiri), " %s%s ",
                 buf->cfg.path_file[0] ? buf->cfg.path_file : "[Tanpa Nama]",
                 buf->cfg.kotor ? "+" : "");
    } else {
        strcpy(kiri, " [Bantuan Mode]");
    }
    warna_set_buffer(WARNAD_DEFAULT, WARNAL_HIJAU_TUA);
    taruh_teks(2, y, kiri);
    
    /* Kanan: Info navigasi */
    snprintf(kanan, sizeof(kanan), " Panah: pilih | Enter: buka | q: keluar ");
    warna_set_buffer(WARNAD_GELAP, WARNAL_HIJAU);
    taruh_teks(lebar_layar - strlen(kanan) - 1, y, kanan);
}

/* Gambar message bar bantuan */
static void gambar_messagebar_bantuan(void) {
    int y = tinggi_layar - 1;
    
    warna_set_buffer(WARNAD_DEFAULT, WARNAL_DEFAULT);
    isi_garis_h(1, lebar_layar, y);
    
    if (pesan_status[0] != '\0') {
        taruh_teks(2, y, pesan_status);
    } else {
        taruh_teks(2, y, "Tekan ? untuk bantuan, q untuk keluar");
    }
}

/* Fungsi helper untuk snprintf */
static int snprintf_helper(char *str, size_t size, const char *fmt, ...) {
    va_list args;
    int ret;
    va_start(args, fmt);
    ret = vsnprintf(str, size, fmt, args);
    va_end(args);
    return ret;
}

/* ============================================================
 * FUNGSI UTAMA BANTUAN
 * ============================================================ */

/* Tampilkan bantuan dengan split window */
void tampilkan_bantuan(void) {
    struct state_bantuan state;
    struct buffer_tabel *buf = NULL;
    int ch;
    int selesai = 0;
    
    /* Inisialisasi state */
    memset(&state, 0, sizeof(state));
    state.section_aktif = 0;
    state.scroll_y = 0;
    state.lebar_section = hitung_lebar_max_section();
    state.jumlah_section = JUMLAH_SECTION;
    
    /* Ambil buffer aktif */
    if (buffer_aktif >= 0 && buffers) {
        buf = buffers[buffer_aktif];
    }
    
    /* Set mode terminal */
    atur_mode_terminal(1);
    
    /* Loop utama bantuan */
    while (!selesai) {
        /* Bersihkan buffer tampilan */
        int r, c;
        for (r = 0; r < tinggi_layar; r++) {
            for (c = 0; c < lebar_layar; c++) {
                /* Reset buffer - fungsi ini dari tampilan.c */
            }
        }
        
        /* Render komponen */
        gambar_topbar_bantuan(lebar_layar, daftar_section[state.section_aktif]);
        gambar_panel_section(&state);
        gambar_panel_konten(&state);
        gambar_statusbar_bantuan(buf);
        gambar_messagebar_bantuan();
        
        /* Flush ke layar */
        terapkan_delta();
        flush_stdout();
        
        /* Baca input */
        ch = 0;
        if (read(STDIN_FILENO, &ch, 1) > 0) {
            /* Proses input */
            if (ch == 27) {
                /* Escape - keluar */
                selesai = 1;
            } else if (ch == 'q' || ch == 'Q') {
                /* q - keluar */
                selesai = 1;
            } else if (ch == '?' || ch == 'h') {
                /* ? atau h - tetap di bantuan */
            } else if (ch == K_UP || ch == 'k') {
                /* Panah atas - geser section atau scroll */
                if (state.scroll_y > 0) {
                    state.scroll_y--;
                }
            } else if (ch == K_DOWN || ch == 'j') {
                /* Panah bawah - geser section atau scroll */
                state.scroll_y++;
            } else if (ch == K_LEFT) {
                /* Panah kiri - section sebelumnya */
                if (state.section_aktif > 0) {
                    state.section_aktif--;
                    state.scroll_y = 0;
                }
            } else if (ch == K_RIGHT || ch == '\r' || ch == '\n') {
                /* Panah kanan atau Enter - section berikutnya */
                if (state.section_aktif < (int)JUMLAH_SECTION - 1) {
                    state.section_aktif++;
                    state.scroll_y = 0;
                }
            } else if (ch == K_HOME) {
                /* Home - ke section pertama */
                state.section_aktif = 0;
                state.scroll_y = 0;
            } else if (ch == K_END) {
                /* End - ke section terakhir */
                state.section_aktif = JUMLAH_SECTION - 1;
                state.scroll_y = 0;
            } else if (ch == K_PAGEUP) {
                /* Page Up - scroll satu halaman */
                state.scroll_y -= 10;
                if (state.scroll_y < 0) state.scroll_y = 0;
            } else if (ch == K_PAGEDOWN) {
                /* Page Down - scroll satu halaman */
                state.scroll_y += 10;
            }
        }
    }
    
    /* Bersihkan buffer input */
    {
        unsigned char buang;
        while (read(STDIN_FILENO, &buang, 1) > 0);
    }
    
    /* Kembalikan tampilan buffer aktif */
    if (buffer_aktif >= 0 && buffers) {
        /* render(buffers[buffer_aktif]); */
        flush_stdout();
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
    int n = sizeof(teks_bantuan) / sizeof(teks_bantuan[0]);
    int i;
    unsigned char buang;

    warna_set_buffer(WARNAD_DEFAULT, WARNAL_DEFAULT);
    for (i = 0; i < n && i < tinggi_layar - 2; i++) {
        taruh_teks(2, 2 + i, teks_bantuan[i]);
    }

    warna_set_buffer(WARNAD_PUTIH, WARNAL_DEFAULT);
    taruh_teks(2, tinggi_layar - 1,
               "[ Tekan tombol apapun untuk kembali... ]");

    terapkan_delta();
    
    /* Tunggu input */
    atur_mode_terminal(1);
    while (read(STDIN_FILENO, &buang, 1) <= 0);

    /* Bersihkan buffer */
    while (read(STDIN_FILENO, &buang, 1) > 0);

    /* Kembali ke tampilan buffer */
    if (buffer_aktif >= 0 && buffers) {
        flush_stdout();
    }
}
