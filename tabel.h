/* ============================================================
 * TABEL v3.0
 * Berkas: tabel.h - Header Tunggal Aplikasi Tabel
 * Standar: C89 + POSIX.1-2008
 * Penulis: Chandra Lesmana
 * ============================================================ */

#define _XOPEN_SOURCE 600

#ifndef TABEL_H
#define TABEL_H

/* ============================================================
 * Pustaka C89
 * ============================================================ */
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <locale.h>
#include <math.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ============================================================
 * Pustaka POSIX
 * ============================================================ */
#include <dirent.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <strings.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
#include <wchar.h>

/* ============================================================
 * Konstanta Aplikasi
 * ============================================================ */
#define NAMA_APLIKASI        "TABEL 3.0"
#define MAKS_GLYPH           8
#define MAKS_KOLOM           52
#define MAKS_BARIS           10000
#define MAKS_TEKS            1024
#define MAKS_SELEKSI         1024
#define UNDO_MAKS            4096
#define PANJANG_PATH_MAKS    512
#define ENKODING_MAKS        64
#define SUFFIK_META          ".meta"
#define LEBAR_KOLOM_DEFAULT  12
#define TINGGI_BARIS_DEFAULT 1
#define PANJANG_DELIM_MAKS   4
#define ARGUMEN_FUNGSI_MAKS  16
#define MAKS_LEMBAR          256
#define MAKS_LEMBAR_VIS      3
#define NAMA_LEMBAR_MAKS     32

/* Glyph Unicode untuk tabel */
#define TL  "\xE2\x94\x8C"
#define TR  "\xE2\x94\x90"
#define BL  "\xE2\x94\x94"
#define BR  "\xE2\x94\x98"
#define TC  "\xE2\x94\xAC"
#define BC  "\xE2\x94\xB4"
#define LC  "\xE2\x94\x9C"
#define RC  "\xE2\x94\xA4"
#define CR  "\xE2\x94\xBC"
#define H   "\xE2\x94\x80"
#define V   "\xE2\x94\x82"

/* Warna ANSI */
#define ESC_NORMAL       "\033[0m"
#define FG_GELAP         "\033[38;2;30;40;50m"
#define FG_ABU           "\033[38;2;80;90;100m"
#define FG_PUTIH         "\033[38;2;255;255;255m"
#define FG_HIJAU         "\033[38;2;100;200;100m"
#define FG_KUNING        "\033[38;2;255;185;75m"
#define FG_BIRU          "\033[38;2;50;185;185m"
#define FG_MERAH         "\033[38;2;255;125;125m"

/* Warna latar ANSI */
#define BG_NORMAL        "\033[49m\033[37m"
#define BG_HITAM         "\033[48;2;5;10;15m\033[37m"
#define BG_GELAP         "\033[48;2;30;40;50m\033[38;2;80;90;100m"
#define BG_ABU           "\033[48;2;80;90;100m\033[38;2;255;255;255m"
#define BG_HIJAU         "\033[48;2;100;200;100m\033[30m"
#define BG_HIJAU_GELAP   "\033[48;2;40;80;40m\033[30m"
#define BG_HIJAU_TUA     "\033[48;2;20;40;40m\033[38;2;255;255;255m"
#define BG_KUNING        "\033[48;2;255;185;75m\033[30m"
#define BG_BIRU          "\033[48;2;50;185;185m\033[30m"
#define BG_MERAH         "\033[48;2;255;125;125m\033[30m"

/* Kunci khusus */
#define KEY_BATAL        1000
#define KEY_SHIFT_TAB    1001
#define KEY_BACK_NAV     1002
#define KEY_CTRL_UP      1003 
#define KEY_CTRL_DOWN    1004

/* ============================================================
 * Konstanta FORMULA
 * ============================================================ */
#define MAKS_TOKEN         256
#define MAKS_TEKS_TOKEN    64
#define MAKS_RPN           256
#define MAKS_STAK_NILAI    256
#define MAKS_KEDALAMAN     16
#define PANJANG_FORMAT     32
#define DETIK_HARI         86400
#define DETIK_JAM          3600
#define DETIK_MENIT        60

/* ============================================================
 * Konstanta INPUT
 * ============================================================ */
#define EVT_KEY     1
#define EVT_MOUSE   2
#define EVT_RESIZE  3
#define EVT_PASTE   4

#define MOD_SHIFT   0x01
#define MOD_ALT     0x02
#define MOD_CTRL    0x04
#define MOD_SUPER   0x08

#define MOUSE_BTN_NONE     0
#define MOUSE_BTN_LEFT     1
#define MOUSE_BTN_MIDDLE   2
#define MOUSE_BTN_RIGHT    3
#define MOUSE_WHEEL_UP     4
#define MOUSE_WHEEL_DOWN   5

#define STATE_INIT         0x0001
#define STATE_RAW          0x0002
#define STATE_SIGNALS      0x0004
#define STATE_ALT_SCREEN   0x0008
#define STATE_MOUSE        0x0010
#define STATE_BRACKETED    0x0020

/* Log Levels */
#define LOG_ERROR 1
#define LOG_WARN  2
#define LOG_DEBUG 3
#define LOG_INFO  4

/* Data Input Global */
#define QUEUE_SIZE   2048
#define READBUF      4096
#define DBUF_CAP     (128 * 1024)

/* ============================================================
 * Enumerasi Input
 * ============================================================ */
enum {
    K_UNKNOWN = 0x1000, 
    K_UP, 
    K_DOWN, 
    K_RIGHT, 
    K_LEFT,
    K_HOME, 
    K_END, 
    K_INSERT, 
    K_DELETE, 
    K_PAGEUP, 
    K_PAGEDOWN,
    K_F1, 
    K_F2, 
    K_F3, 
    K_F4, 
    K_F5, 
    K_F6, 
    K_F7, 
    K_F8, 
    K_F9, 
    K_F10,
    K_F11, 
    K_F12,
    K_KP_0, 
    K_KP_1, 
    K_KP_2, 
    K_KP_3, 
    K_KP_4, 
    K_KP_5, 
    K_KP_6,
    K_KP_7, 
    K_KP_8, 
    K_KP_9, 
    K_KP_DOT, 
    K_KP_ENTER, 
    K_KP_DIV,
    K_KP_MUL, 
    K_KP_ADD, 
    K_KP_SUB
};

/* Enumerasi Window */
enum { 
    JENIS_DAUN=0, 
    JENIS_SPLIT_H, 
    JENIS_SPLIT_V 
};

/* ============================================================
 * Enumerasi Warna Depan (Foreground)
 * ============================================================ */
typedef enum {
    WARNAD_DEFAULT = 0,
    WARNAD_GELAP,
    WARNAD_ABU,
    WARNAD_PUTIH,
    WARNAD_HIJAU,
    WARNAD_KUNING,
    WARNAD_BIRU,
    WARNAD_MERAH,
    WARNAD_JUMLAH
} warna_depan;

/* ============================================================
 * Enumerasi Warna Latar (Background)
 * ============================================================ */
typedef enum {
    WARNAL_DEFAULT = 0,
    WARNAL_HITAM,
    WARNAL_GELAP,
    WARNAL_ABU,
    WARNAL_HIJAU,
    WARNAL_HIJAU_GELAP,
    WARNAL_HIJAU_TUA,
    WARNAL_KUNING,
    WARNAL_BIRU,
    WARNAL_MERAH,
    WARNAL_JUMLAH
} warna_latar;

/* ============================================================
 * Font Style (bitmask untuk kombinasi)
 * ============================================================ */
typedef enum {
    STYLE_NORMAL       = 0,
    STYLE_BOLD         = 1 << 0,  /* 1 */
    STYLE_ITALIC       = 1 << 1,  /* 2 */
    STYLE_UNDERLINE    = 1 << 2,  /* 4 */
    STYLE_STRIKETHROUGH= 1 << 3   /* 8 */
} font_style;

/* ============================================================
 * Struktur Kombinasi Warna
 * ============================================================ */
struct warna_kombinasi {
    warna_depan depan;
    warna_latar latar;
};

typedef enum { 
    RATA_KIRI, 
    RATA_TENGAH, 
    RATA_KANAN 
} perataan_teks;

typedef enum { 
    URUT_NAIK=0, 
    URUT_TURUN=1 
} urutan_sortir;

typedef enum {
    UNDO_TEKS=0, 
    UNDO_LAYOUT=1, 
    UNDO_GABUNG=2,
    UNDO_STRUKTUR=3, 
    UNDO_PERATAAN=4, 
    UNDO_SORTIR=5
} jenis_undo;

/* Format Sel untuk tampilan otomatis */
typedef enum {
    FORMAT_UMUM=0,       /* Default: tampilkan apa adanya */
    FORMAT_ANGKA,        /* Angka biasa dengan desimal */
    FORMAT_TANGGAL,      /* YYYY-MM-DD */
    FORMAT_WAKTU,        /* HH:MM:SS */
    FORMAT_TIMESTAMP,    /* YYYY-MM-DD HH:MM:SS */
    FORMAT_MATA_UANG,    /* Rp 1.000.000,00 */
    FORMAT_PERSUAN,      /* 0.00% */
    FORMAT_TEKS          /* Paksa sebagai teks */
} jenis_format_sel;

/* ============================================================
 * Struktur Data Inti
 * ============================================================ */
struct konfigurasi {
    int kolom, baris;
    int aktif_x, aktif_y;
    int lihat_kolom, lihat_baris;
    int offset_x, offset_y;
    char path_file[PANJANG_PATH_MAKS];
    char enkoding[ENKODING_MAKS];
    int kotor;
    char csv_delim[PANJANG_DELIM_MAKS];
};

struct glyph {
    char byte[MAKS_GLYPH];
    unsigned char panjang;
    warna_depan warna_depan;   /* Warna depan (teks) */
    warna_latar warna_latar;   /* Warna latar (background) */
    font_style style;          /* Font style (bold, italic, etc) */
};

struct info_gabung {
    int x_anchor, y_anchor;
    int lebar, tinggi;
    int adalah_anchor;
};

struct operasi_undo {
    int jenis;
    int x, y, x2, y2;
    int aux1, aux2;
    char sebelum[MAKS_TEKS];
    char sesudah[MAKS_TEKS];
};

struct info_border {
    int top, bottom, left, right;   /* 0 = tidak ada, 1 = ada */
    warna_depan warna_depan;        /* Warna depan border */
    warna_latar warna_latar;        /* Warna latar border */
};

/* ============================================================
 * Struktur Lembar (Sheet)
 * ============================================================
 * Setiap lembar memiliki buffer data sendiri.
 * Lembar 1 adalah lembar utama untuk berkas.
 * ============================================================ */
struct lembar_tabel {
    char nama[NAMA_LEMBAR_MAKS];
    int aktif_x, aktif_y;
    int lihat_kolom, lihat_baris;

    /* Grid dinamis */
    char ***isi;
    perataan_teks **perataan_sel;
    int *lebar_kolom;
    int *tinggi_baris;
    struct info_gabung **tergabung;

    /* Undo/Redo per lembar */
    struct operasi_undo *tumpukan_undo;
    struct operasi_undo *tumpukan_redo;
    int undo_puncak, redo_puncak;

    /* Data pencarian per lembar */
    int *hasil_cari_x;
    int *hasil_cari_y;
    int jumlah_hasil;
    int indeks_hasil;
    char query_cari[MAKS_TEKS];

    struct info_border **border_sel;
    struct warna_kombinasi **warna_sel;
    font_style **style_sel;           /* Font style per sel */
    jenis_format_sel **format_sel;

    /* Jumlah kolom dan baris */
    int kolom, baris;
};

/* ============================================================
 * Struktur Buffer (Multi Buffer + Multi Sheet)
 * ============================================================ */
struct buffer_tabel {
    struct konfigurasi cfg;
    int refcount;

    /* Multi Sheet */
    struct lembar_tabel **lembar;
    int jumlah_lembar;
    int lembar_aktif;
    int lembar_scroll;      /* Offset scroll untuk tab lembar */

    /* Undo/Redo global (untuk operasi struktur) */
    struct operasi_undo *tumpukan_undo;
    struct operasi_undo *tumpukan_redo;
    int undo_puncak, redo_puncak;

    /* Pointer ke data lembar aktif (untuk kompatibilitas) */
    char ***isi;
    perataan_teks **perataan_sel;
    int *lebar_kolom;
    int *tinggi_baris;
    struct info_gabung **tergabung;
    struct info_border **border_sel;
    struct warna_kombinasi **warna_sel;
    font_style **style_sel;           /* Font style per sel */
    jenis_format_sel **format_sel;
};

/* Struktur Geometri */
struct rect {
    int x, y, w, h;
};

/* View State (Independen per window) */
struct view_state {
    int aktif_x, aktif_y;
    int lihat_kolom, lihat_baris;
    int lembar_aktif;        /* Sheet aktif untuk window ini */
};

/* Struktur Jendela (Node Tree) */
struct jendela {
    int jenis;          /* DAUN, SPLIT_H, SPLIT_V */
    struct rect rect;   /* Geometri absolut */

    /* Untuk Node Internal */
    struct jendela *parent;
    struct jendela *child1;
    struct jendela *child2;

    /* Untuk Node Daun */
    int buffer_idx;     /* Index ke array buffers global */
    struct view_state view;
    int ratio;
};

/* ============================================================
 * Struktur Data INPUT
 * ============================================================ */
typedef struct {
    int type;
    union {
        struct { 
            int keycode;       
            int mods;          
            int width;         
            char keyname[48];  
            char cluster_str[32]; 
            int cluster_len;
            int is_repeat;     
        } key;
        struct { 
            int x, y; 
            int button; 
            int action; 
            int mods; 
        } mouse;
        struct { 
            int cols; 
            int rows; 
        } resize;
        struct { 
            char *text; 
            int len; 
        } paste;
    } u;
} InputEvent;

typedef void (*callback_t)(const InputEvent *e, void *ctx);
typedef void (*log_callback_t)(int level, const char *msg);

typedef struct {
    int flags; 
    int is_utf8;
    struct termios orig_termios;
    
    /* Grapheme State */
    char pending_cluster[32];
    int pending_len;
    struct timespec cluster_ts;
    int last_cluster_type; /* 0=None, 1=RI, 2=Picto, 3=Text */
    
    /* Paste Buffer */
    char paste_buf[8192];
    int paste_len;
    
    /* Key Repeat Logic */
    int last_cp;
    int last_mods;
    struct timeval last_key_ts;
    int is_repeat;

    /* Event Queue */
    InputEvent queue[QUEUE_SIZE];
    int q_head, q_tail;
    pthread_mutex_t q_mtx;
    pthread_cond_t q_cv;
    
    /* Signal Self-Pipe */
    int sv_pipe[2];
    
    /* Double Buffer */
    char *dbuf;
    int  dbuf_len;
    pthread_mutex_t dbuf_mtx;
    
    /* User Callbacks */
    callback_t cb_k, cb_m, cb_r, cb_p;
    void *cb_ctx;
    
    /* Logging */
    log_callback_t log_cb;
    int log_level;
} GlobalState;

/* ============================================================
 * Input Global
 * ============================================================ */
extern GlobalState G;

/* ============================================================
 * Clipboard Global
 * ============================================================ */
extern char papan_klip[MAKS_TEKS];
extern char ***papan_klip_area;
extern int klip_x1, klip_y1;
extern int klip_x2, klip_y2;
extern int klip_ada_area;
extern int klip_mode;

/* ============================================================
 * Multi Buffer Manager
 * ============================================================ */
extern struct buffer_tabel **buffers;
extern int jumlah_buffer;
extern int buffer_aktif;

extern struct jendela *jendela_root;
extern struct jendela *jendela_aktif;

/* Multi Seleksi */
extern int seleksi_random_x[MAKS_SELEKSI];
extern int seleksi_random_y[MAKS_SELEKSI];
extern int jumlah_seleksi_random;


/* ============================================================
 * Terminal & Tampilan
 * ============================================================ */
extern struct termios term_simpan;
extern struct konfigurasi *konfig_global;
extern int mode_raw_aktif;
extern volatile sig_atomic_t perlu_resize;
extern int sedang_memilih;
extern int jangkar_x;
extern int jangkar_y;
extern int seleksi_x1; 
extern int seleksi_y1; 
extern int seleksi_x2; 
extern int seleksi_y2;
extern int seleksi_aktif;
extern int seleksi_random;

extern char pesan_status[256];

/* ============================================================
 * Prototipe Fungsi
 * ============================================================ */
/* Input API */
int input_init(int use_mouse, int use_alt_screen, int use_bracketed_paste);
void input_shutdown(void);
void input_reg_cb(callback_t kc, 
        callback_t mc, 
        callback_t rc, 
        callback_t pc, 
        void *ctx);
int input_poll_event_nonblocking(InputEvent *e);
int input_poll_event_blocking(InputEvent *e, int timeout_ms);
int input_run(void);
int input_get_str_width(const char *s);
void input_push_event(const InputEvent *ev);
int input_inject_sequence(const char *seq, int len);
int setup_signals(void);

/* Buffer Management */
struct buffer_tabel *buat_buffer(int baris, int kolom);
struct buffer_tabel *duplikat_buffer(const struct buffer_tabel *src);
void bebas_buffer(struct buffer_tabel *buf);
void set_buffer_aktif(int idx);
int hitung_penggunaan_idx(int idx);
int tambah_buffer(struct buffer_tabel *buf);
void tutup_buffer(int idx);

/* Lembar (Sheet) Management */
struct lembar_tabel *buat_lembar(int baris, int kolom, const char *nama);
struct lembar_tabel *duplikat_lembar(const struct lembar_tabel *src);
void bebas_lembar(struct lembar_tabel *lembar);
int tambah_lembar_ke_buffer(struct buffer_tabel *buf, struct lembar_tabel *lembar);
void hapus_lembar_dari_buffer(struct buffer_tabel *buf, int idx);
void set_lembar_aktif(struct buffer_tabel *buf, int idx);
void sinkronkan_pointer_lembar_aktif(struct buffer_tabel *buf);
void rename_lembar(struct buffer_tabel *buf, int idx, const char *nama);

/* Jendela */
void inisialisasi_window_manager(void);
void split_window_aktif(int arah, int ratio); /* 0=H, 1=V */
void resize_window_aktif(int delta);
void tutup_window_aktif(void);
void pindah_fokus_jendela(int arah_kunci);
void render_tree_jendela(struct jendela *node);
void render_semua_jendela(void);
void simpan_state_jendela(struct jendela *j);
void paksa_recalc_layout(void);
void render_jendela_spesifik(const struct buffer_tabel *buf, struct rect area, 
    int is_active);
void set_clip_rect(int x, int y, int w, int h);
void reset_clip_rect(void);
void terapkan_delta(void);

/* Undo/Redo */
void dorong_undo_teks(struct buffer_tabel *buf, int x, int y, const char *sblm, 
        const char *ssdh);
int lakukan_undo(struct buffer_tabel *buf);
int lakukan_redo(struct buffer_tabel *buf);

/* Manipulasi Sel & Teks */
void atur_teks_sel(struct buffer_tabel *buf, int x, int y, const char *teks, 
        int catat_undo);
void ubah_perataan_sel(struct buffer_tabel *buf, perataan_teks p);
void ubah_teks_sel_huruf_besar(struct buffer_tabel *buf, int x, int y);
void ubah_teks_sel_huruf_kecil(struct buffer_tabel *buf, int x, int y);
void ubah_teks_sel_judul(struct buffer_tabel *buf, int x, int y);
void seleksi_baris_penuh_aktif(struct buffer_tabel *buf);
void seleksi_kolom_penuh_aktif(struct buffer_tabel *buf);
void toggle_seleksi_random(struct buffer_tabel *buf, int x, int y);

/* Clipboard */
void salin_sel(struct buffer_tabel *buf);
void salin_area(struct buffer_tabel *buf);
void salin_kolom_penuh(struct buffer_tabel *buf, int col);
void salin_baris_penuh(struct buffer_tabel *buf, int row);
void potong_area(struct buffer_tabel *buf);
void potong_kolom_penuh(struct buffer_tabel *buf);
void potong_baris_penuh(struct buffer_tabel *buf);
void lakukan_tempel(struct buffer_tabel *buf, int offset_x, int offset_y);
void tempel_baris_penuh(struct buffer_tabel *buf, int row, int arah);
void tempel_kolom_penuh(struct buffer_tabel *buf, int col, int arah);
void tempel_isi_ke_seleksi(struct buffer_tabel *buf);
void swap_kolom(struct buffer_tabel *buf, int col, int arah);
void swap_baris(struct buffer_tabel *buf, int row, int arah);

/* Navigasi */
void tarik_ke_anchor(struct buffer_tabel *buf);
void geser_kanan(struct buffer_tabel *buf);
void geser_kiri(struct buffer_tabel *buf);
void geser_bawah(struct buffer_tabel *buf);
void geser_atas(struct buffer_tabel *buf);
void pastikan_aktif_terlihat(struct buffer_tabel *buf);

/* Ubah Ukuran */
void ubah_ukuran_sel(struct buffer_tabel *buf, int dl, int dt);
void sesuaikan_lebar_kolom(struct buffer_tabel *buf, int col, int mode);
void sesuaikan_semua_lebar(struct buffer_tabel *buf, int col_ref);

/* Hapus Isi Sel */
void hapus_sel_atau_area(struct buffer_tabel *buf);
void bersihkan_kolom_aktif(struct buffer_tabel *buf);
void bersihkan_baris_aktif(struct buffer_tabel *buf);

/* Operasi Gabung */
int apakah_ada_gabung_buf(const struct buffer_tabel *buf, int x, int y);
int apakah_sel_anchor_buf(const struct buffer_tabel *buf, int x, int y);
void hapus_blok_gabung_buf(struct buffer_tabel *buf, int ax, int ay);
void atur_blok_gabung_buf(struct buffer_tabel *buf, int ax, int ay,
        int lbr, int tgi);
void eksekusi_gabung(struct buffer_tabel *buf);
void eksekusi_urai_gabung(struct buffer_tabel *buf);

/* Struktur (Insert/Delete) */
void sisip_kolom_setelah(struct buffer_tabel *buf);
void sisip_kolom_sebelum(struct buffer_tabel *buf);
void sisip_baris_setelah(struct buffer_tabel *buf);
void sisip_baris_sebelum(struct buffer_tabel *buf);
void hapus_kolom_aktif(struct buffer_tabel *buf);
void hapus_baris_aktif(struct buffer_tabel *buf);

/* Sortir */
void sortir_kolom_aktif(struct buffer_tabel *buf);
void sortir_multi_kolom(struct buffer_tabel *buf, const int *kol,
                         const urutan_sortir *urut, int nkol);

/* Pencarian */
int lakukan_cari(struct buffer_tabel *buf, const char *query);
int lakukan_cari_ganti(struct buffer_tabel *buf, const char *cmd);
void cari_next(struct buffer_tabel *buf);
void cari_prev(struct buffer_tabel *buf);

void eksekusi_warna_teks(struct buffer_tabel *buf, const char *cmd);
void eksekusi_border(struct buffer_tabel *buf, const char *cmd);

/* Format Sel */
void atur_format_sel(struct buffer_tabel *buf, int x, int y,
                     jenis_format_sel fmt);
jenis_format_sel ambil_format_sel(const struct buffer_tabel *buf, int x, int y);
void atur_format_area(struct buffer_tabel *buf, jenis_format_sel fmt);
int format_nilai_ke_teks(double nilai, jenis_format_sel fmt,
                         char *keluaran, size_t ukuran);
int deteksi_format_otomatis(double nilai);

/* Font Style */
void toggle_font_style(struct buffer_tabel *buf, font_style style);
void toggle_font_style_area(struct buffer_tabel *buf, font_style style);

/* ============================================================
 * FORMULA - Prototipe Fungsi Publik
 * ============================================================ */

/* --- formula.c --- */
int apakah_string_formula(const char *s);
int apakah_sel_formula(int x, int y);
int evaluasi_string(const struct buffer_tabel *buf, int x, int y,
                    const char *ekspresi, double *keluaran,
                    int *adalah_angka, char *buf_galat, size_t panjang_galat);
int evaluasi_sel(const struct buffer_tabel *buf, int x, int y,
                 double *keluaran, int *adalah_angka, int kedalaman);
int parse_ref_sel(const char *s, int *x, int *y);
const char *dapatkan_galat_terakhir(void);
void bersihkan_galat_terakhir(void);

/* --- hitung.c --- */
int hitung_agregat(const struct buffer_tabel *buf, const char *nama,
                   const char *range, double *keluaran);
int hitung_math_1_arg(const char *nama, double arg, double *keluaran);
int hitung_math_2_arg(const char *nama, double arg1, double arg2,
                      double *keluaran);
int hitung_if(double kondisi, double benar, double salah, double *keluaran);

/* --- teks.c --- */
int teks_len(const char *str, double *keluaran);
int teks_left(const char *str, double jumlah, char *keluaran, size_t ukuran);
int teks_right(const char *str, double jumlah, char *keluaran, size_t ukuran);
int teks_mid(const char *str, double mulai, double jumlah,
             char *keluaran, size_t ukuran);
int teks_upper(const char *str, char *keluaran, size_t ukuran);
int teks_lower(const char *str, char *keluaran, size_t ukuran);
int teks_trim(const char *str, char *keluaran, size_t ukuran);
int teks_concat(const char *s1, const char *s2, char *keluaran, size_t ukuran);
int teks_find(const char *cari, const char *dalam, double mulai,
              double *keluaran);
int teks_substitute(const char *teks, const char *cari, const char *ganti,
                    char *keluaran, size_t ukuran);
int teks_replace(const char *teks, double pos, double jumlah,
                 const char *baru, char *keluaran, size_t ukuran);
int teks_value(const char *teks, double *keluaran);
int teks_format(double angka, const char *format, char *keluaran, size_t ukuran);
int teks_isnumber(const char *teks, double *keluaran);
int teks_istext(const char *teks, double *keluaran);
int teks_isblank(const char *teks, double *keluaran);

/* --- waktu.c --- */
int waktu_sekarang_epoch(void);
int waktu_buat_tanggal(int tahun, int bulan, int hari, time_t *hasil);
int waktu_buat_waktu(int jam, int menit, int detik, time_t *hasil);
int waktu_ekstrak_tanggal(time_t epoch, int *tahun, int *bulan, int *hari);
int waktu_ekstrak_waktu(time_t epoch, int *jam, int *menit, int *detik);
int waktu_format_tanggal(const char *id_format, time_t epoch,
                          char *keluaran, size_t ukuran);
int waktu_format_waktu(const char *id_format, time_t epoch,
                        char *keluaran, size_t ukuran);
int waktu_parse_tanggal(const char *str, time_t *hasil);
int waktu_parse_waktu(const char *str, time_t *hasil);

int waktu_fn_date(double tahun, double bulan, double hari, double *keluaran);
int waktu_fn_time(double jam, double menit, double detik, double *keluaran);
int waktu_fn_today(double *keluaran);
int waktu_fn_now(double *keluaran);
int waktu_fn_year(double serial, double *keluaran);
int waktu_fn_month(double serial, double *keluaran);
int waktu_fn_day(double serial, double *keluaran);
int waktu_fn_hour(double serial, double *keluaran);
int waktu_fn_minute(double serial, double *keluaran);
int waktu_fn_second(double serial, double *keluaran);
int waktu_fn_weekday(double serial, double *keluaran);
int waktu_fn_datedif(double serial1, double serial2, const char *unit,
                      double *keluaran);
int waktu_fn_edate(double serial, double bulan, double *keluaran);
int waktu_fn_eomonth(double serial, double bulan, double *keluaran);

int waktu_format_tanggal_serial(double serial, const char *format,
                                 char *keluaran, size_t ukuran);
int waktu_format_waktu_serial(double serial, const char *format,
                               char *keluaran, size_t ukuran);

/* --- konversi.c (opsional, bisa dimasukkan ke hitung.c) --- */
int konversi_satuan(const char *nama, double nilai, const char *ke,
                    double *keluaran);
int konversi_mata_uang(const char *id_nama, double nilai, const char *id_ke,
                       double *keluaran);

/* ============================================================
 * I/O File (berkas.c)
 * ============================================================ */
/* UTF-8 & wcwidth */
int panjang_cp_utf8(const char *p);
int lebar_tampilan_utf8(const char *p);
int lebar_tampilan_string_utf8(const char *s);
int apakah_menggabung(unsigned int cp);
unsigned int dekode_cp_utf8(const char *p, int *len);

/* I/O TBL (Native TABEL Format) */
int buka_tbl(struct buffer_tabel **pbuf, const char *fn);
int simpan_tbl(const struct buffer_tabel *buf, const char *fn);
int tbl_apakah_valid(const char *fn);
int tbl_info(const char *fn, int *version, int *rows, int *cols, 
    unsigned short *flags);
const char *tbl_get_error(void);

/* I/O TXT */
int simpan_txt(const struct buffer_tabel *buf, const char *fn);
int buka_txt(struct buffer_tabel **pbuf, const char *fn);

/* I/O CSV */
int simpan_csv(const struct buffer_tabel *buf, const char *fn);
int buka_csv(struct buffer_tabel **pbuf, const char *fn);

/* I/O SQL */
int simpan_sql(const struct buffer_tabel *buf, const char *fn);
int buka_sql(struct buffer_tabel **pbuf, const char *fn);

/* I/O DB (Biner) */
int simpan_db(const struct buffer_tabel *buf, const char *fn);
int buka_db(struct buffer_tabel **pbuf, const char *fn);

/* I/O TSV */
int simpan_tsv(const struct buffer_tabel *buf, const char *fn);
int buka_tsv(struct buffer_tabel **pbuf, const char *fn);

/* I/O Markdown */
int simpan_md(const struct buffer_tabel *buf, const char *fn);
int buka_md(struct buffer_tabel **pbuf, const char *fn);

/* I/O PDF (Native PDF Generator) */
int simpan_pdf(const struct buffer_tabel *buf, const char *fn);
struct pdf_config;
int simpan_pdf_v2(const struct buffer_tabel *buf, const char *fn,
                   struct pdf_config *cfg);
int simpan_pdf_baru(const struct buffer_tabel *buf, const char *fn);
int export_pdf_interaktif(const struct buffer_tabel *buf, const char *fn);
void prompt_pdf_config(struct pdf_config *cfg);
int hitung_jumlah_halaman_pdf(const struct buffer_tabel *buf,
                               struct pdf_config *cfg);
void info_export_pdf(const struct buffer_tabel *buf,
                     struct pdf_config *cfg, char *info, size_t info_size);
void set_pdf_config_default(struct pdf_config *cfg);

/* I/O XLSX - buka dan simpan format Excel */
int buka_xlsx(struct buffer_tabel **pbuf, const char *fn);
int simpan_xlsx(const struct buffer_tabel *buf, const char *fn);
int xlsx_apakah_valid(const char *fn);
int xlsx_info_sheet(const char *fn, int *num_sheets, int *max_row, int *max_col);
const char *xlsx_get_error(void);
void xlsx_col_to_letters(int col, char *buf, size_t buf_size);

 /* I/O XLS (Excel 97-2003 Format - Native BIFF/OLE2) */
int buka_xls(struct buffer_tabel **pbuf, const char *fn);
int simpan_xls(const struct buffer_tabel *buf, const char *fn);
int xls_apakah_valid(const char *fn);
const char *xls_get_error(void);

/* I/O XML Spreadsheet (Excel 2002-2003 XML Format) */
int buka_xml(struct buffer_tabel **pbuf, const char *fn);
int simpan_xml(const struct buffer_tabel *buf, const char *fn);
int buka_xml_spreadsheet(struct buffer_tabel **pbuf, const char *fn);
int simpan_xml_spreadsheet(const struct buffer_tabel *buf, const char *fn);
int xml_spreadsheet_apakah_valid(const char *fn);
const char *xmlss_get_error(void);

/* I/O ODS (OpenDocument Spreadsheet - Native ZIP+XML) */
int buka_ods(struct buffer_tabel **pbuf, const char *fn);
int simpan_ods(const struct buffer_tabel *buf, const char *fn);
int ods_apakah_valid(const char *fn);
const char *ods_get_error(void);

/* I/O HTML (HTML Table Format) */
int buka_html(struct buffer_tabel **pbuf, const char *fn);
int simpan_html(const struct buffer_tabel *buf, const char *fn);
struct html_export_opts;
int simpan_html_opts(const struct buffer_tabel *buf, const char *fn,
                     const struct html_export_opts *opts);
void prompt_html_opts(struct html_export_opts *opts);
int export_html_interaktif(const struct buffer_tabel *buf, const char *fn);
int html_apakah_valid(const char *fn);
int html_info_table(const char *fn, int *num_tables, int *max_row, int *max_col);
const char *html_get_error(void);

/* I/O JSON (JavaScript Object Notation) */
int buka_json(struct buffer_tabel **pbuf, const char *fn);
int simpan_json(const struct buffer_tabel *buf, const char *fn);
struct json_export_opts;
int simpan_json_opts(const struct buffer_tabel *buf, const char *fn,
                     const struct json_export_opts *opts);
void prompt_json_opts(struct json_export_opts *opts);
int export_json_interaktif(const struct buffer_tabel *buf, const char *fn);
int json_apakah_valid(const char *fn);
int json_info(const char *fn, int *is_array, int *row_estimate, int *col_estimate);
const char *json_get_error(void);

/* Prompt Nama Berkas */
void minta_nama_berkas(char *buf, size_t n, const char *label);

/* Helper */
int salin_str_aman(char *dst, const char *src, size_t n);
int gabung_str_aman(char *dst, const char *src, size_t n);
int fsync_berkas(FILE *f);

/* ============================================================
 * Input/Controller (kunci.c)
 * ============================================================ */
int grapheme_prev(const char *s, int cursor);
int grapheme_next(const char *s, int len, int cursor);
void lakukan_autocomplete(char *buf, int *len, int *cursor);
void mode_edit_berantai(struct buffer_tabel *buf, 
        int x_mulai, int y_mulai, char inisial);
void mulai_aplikasi_poll(struct buffer_tabel *buf);

/* ============================================================
 * Tampilan (tampilan.c)
 * ============================================================ */
/* Utilitas Terminal */
void atur_posisi_kursor(int x, int y);
void masuk_mode_alternatif(void);
void keluar_mode_alternatif(void);
void bersihkan_layar_buffer(void);
void flush_stdout(void);
void pulihkan_terminal(void);
void tangani_sinyal_keluar(int sig);
void tangani_sinyal_resize(int sig);
void atur_sigaction(void);
void proses_resize_terpusat(void);
int inisialisasi_terminal(void);
int ambil_lebar_terminal(void);
int ambil_tinggi_terminal(void);

/* I/O Terminal */
void atur_mode_terminal(int blocking);
void baca_satu_kunci(unsigned char *buf, int *len);
int baca_input_edit(char *buf, int *len, int *cursor, 
        int pos_x_input, int pos_y);
void tunggu_tombol_lanjut(void);

/* Render & UI */
void prompt_warna(struct buffer_tabel *buf, int mode_teks);
void render(const struct buffer_tabel *buf);
void tampilkan_bantuan(void);
void hitung_viewport(const struct buffer_tabel *buf,
    int *x_awal, int *y_awal, int *lebar_vis, int *tinggi_vis,
    int *kolom_mulai, int *kolom_akhir, int *baris_mulai, int *baris_akhir);
void set_konfig_aktif(struct konfigurasi *cfg);

void sinkronkan_tree_jendela_tutup(int idx_ditutup);

/* ============================================================
 * Warna (warna.c) - Pengelolaan Warna Terpisah
 * ============================================================ */
/* Set/Get warna depan */
void warna_set_depan(warna_depan wd);
warna_depan warna_get_depan(void);
void warna_set_depan_buffer(warna_depan wd);
warna_depan warna_get_depan_buffer(void);

/* Set/Get warna latar */
void warna_set_latar(warna_latar wl);
warna_latar warna_get_latar(void);
void warna_set_latar_buffer(warna_latar wl);
warna_latar warna_get_latar_buffer(void);

/* Set/Get kombinasi */
void warna_set(warna_depan wd, warna_latar wl);
void warna_set_buffer(warna_depan wd, warna_latar wl);
void warna_reset(void);
void warna_reset_buffer(void);

/* Font Style */
void warna_set_style_buffer(font_style style);
font_style warna_get_style_buffer(void);
void warna_reset_style_buffer(void);
int warna_seq_style(font_style style, char *buf, size_t ukuran);

/* Output ANSI */
const char *warna_seq_depan(warna_depan wd);
const char *warna_seq_latar(warna_latar wl);
int warna_seq_gabung(warna_depan wd, warna_latar wl, char *buf, 
    size_t ukuran);
int warna_seq_gabung_style(warna_depan wd, warna_latar wl, font_style style,
    char *buf, size_t ukuran);
int warna_tulis_seq(int fd, warna_depan wd, warna_latar wl);
int warna_tulis_seq_saat_ini(int fd);
int warna_tulis_reset(int fd);

/* Nama warna */
const char *warna_nama_depan(warna_depan wd);
const char *warna_nama_latar(warna_latar wl);

/* Info untuk prompt */
int warna_jumlah_depan(void);
int warna_jumlah_latar(void);
int warna_info_depan(int indeks, warna_depan *wd, const char **nama);
int warna_info_latar(int indeks, warna_latar *wl, const char **nama);

/* Konversi RGB untuk ekspor */
int warna_depan_ke_rgb(warna_depan wd, double *r, double *g, double *b);
int warna_latar_ke_rgb(warna_latar wl, double *r, double *g, double *b);

/* Serialisasi */
unsigned char warna_encode(warna_depan wd, warna_latar wl);
int warna_decode(unsigned char encoded, warna_depan *wd, warna_latar *wl);

/* Validasi */
int warna_valid_depan(warna_depan wd);
int warna_valid_latar(warna_latar wl);

/* Utilitas */
int warna_sama(warna_depan wd1, warna_latar wl1, warna_depan wd2, 
    warna_latar wl2);
void warna_copy(struct warna_kombinasi *dst, 
    const struct warna_kombinasi *src);
void warna_init(struct warna_kombinasi *wc);
void warna_kombinasi_set(struct warna_kombinasi *wc, warna_depan wd, 
    warna_latar wl);

#endif /* TABEL_H */
