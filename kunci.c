/* ============================================================
 * TABEL v3.0
 * Berkas: kunci.c - Input/Controller, Multi Buffer (Tab)
 * Standar: C89 + POSIX.1-2008
 * Penulis: Chandra Lesmana
 * ============================================================ */

#include "tabel.h"

/* ============================================================
 * Helper Parsing & Input
 * ============================================================ */
static int apakah_huruf(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

static int apakah_digit(char c) {
    return (c >= '0' && c <= '9');
}

static int ke_huruf_besar(char c) {
    if (c >= 'a' && c <= 'z') return c - 'a' + 'A';
    return c;
}

/* Mengubah string kolom (misal "AA") menjadi indeks (26) */
static int kolom_ke_indeks(const char *s) {
    int kolom = 0;
    while (*s) {
        if (!apakah_huruf(*s)) break;
        kolom = kolom * 26 + (ke_huruf_besar(*s) - 'A' + 1);
        s++;
    }
    return kolom - 1;
}

/* ============================================================
 * Helper: Render Input Line dengan scrolling (newline -> \n)
 * ============================================================
 * Parameter:
 *   src         - teks sumber
 *   len         - panjang teks
 *   cursor      - posisi kursor dalam teks
 *   pos_x_input - posisi x awal di layar
 *   pos_y       - posisi y di layar
 *   scroll_pos  - offset scroll untuk overflow (pointer, akan diupdate)
 * ============================================================ */
static void render_input_line_scroll(const char *src, int len, int cursor,
                              int pos_x_input, int pos_y, int *scroll_pos) {
    char tampil[MAKS_TEKS * 2];
    int j = 0;
    int i;
    int max_x = jendela_aktif ? (jendela_aktif->rect.x + jendela_aktif->rect.w) : ambil_lebar_terminal();
    int ruang_tersedia = max_x - pos_x_input;  /* ruang untuk teks */
    int teks_visual_len = 0;
    int cursor_visual_pos = 0;
    int start_pos;
    
    if (ruang_tersedia < 1) ruang_tersedia = 1;

    /* Hitung panjang visual teks dan posisi visual kursor */
    for (i = 0; i < len && j < (int)sizeof(tampil) - 2; i++) {
        int char_visual_len;
        
        /* Hitung panjang visual karakter */
        if (src[i] == '\n') {
            char_visual_len = 2;  /* \n ditampilkan sebagai 2 karakter */
        } else if ((unsigned char)src[i] < 0x80) {
            char_visual_len = 1;
        } else {
            /* Untuk UTF-8, gunakan wcwidth approximation */
            char_visual_len = 1;  /* sederhana, anggap 1 untuk sekarang */
        }
        
        if (i < cursor) {
            cursor_visual_pos += char_visual_len;
        }
        teks_visual_len += char_visual_len;
        
        /* Konversi ke tampilan */
        if (src[i] == '\n') {
            tampil[j++] = '\\';
            tampil[j++] = 'n';
        } else {
            tampil[j++] = src[i];
        }
    }
    tampil[j] = '\0';

    /* Hitung posisi scroll jika diperlukan */
    if (teks_visual_len > ruang_tersedia) {
        /* Teks melebihi ruang - perlu scrolling */
        
        /* Pastikan kursor terlihat */
        if (cursor_visual_pos < *scroll_pos) {
            /* Kursor di kiri area terlihat - scroll ke kiri */
            *scroll_pos = cursor_visual_pos;
        } else if (cursor_visual_pos >= *scroll_pos + ruang_tersedia) {
            /* Kursor di kanan area terlihat - scroll ke kanan */
            *scroll_pos = cursor_visual_pos - ruang_tersedia + 1;
        }
        
        /* Clamp scroll_pos */
        if (*scroll_pos < 0) *scroll_pos = 0;
        if (*scroll_pos > teks_visual_len - ruang_tersedia) {
            *scroll_pos = teks_visual_len - ruang_tersedia;
        }
        
        start_pos = *scroll_pos;
    } else {
        /* Teks muat semua - tidak perlu scroll */
        start_pos = 0;
        *scroll_pos = 0;
    }

    /* Bersihkan area input 1 baris penuh */
    atur_posisi_kursor(pos_x_input, pos_y);
    for (i = pos_x_input; i <= max_x; i++) {
        write(STDOUT_FILENO, " ", 1);
    }

    /* Tulis teks dengan offset scroll (dalam byte, bukan visual) */
    atur_posisi_kursor(pos_x_input, pos_y);
    if (j > 0) {
        /* Skip karakter sampai start_pos visual */
        int visual_count = 0;
        int byte_start = 0;
        int k;
        
        for (k = 0; k < j; ) {
            int cvl;
            if (tampil[k] == '\\' && tampil[k+1] == 'n') {
                cvl = 2;
            } else {
                cvl = 1;
            }
            
            if (visual_count >= start_pos) {
                byte_start = k;
                break;
            }
            visual_count += cvl;
            k += (tampil[k] == '\\' && tampil[k+1] == 'n') ? 2 : 1;
        }
        
        /* Tulis dari byte_start sampai ruang habis */
        int tulis_len = 0;
        visual_count = 0;
        for (k = byte_start; k < j && visual_count < ruang_tersedia; ) {
            int cvl;
            if (tampil[k] == '\\' && tampil[k+1] == 'n') {
                cvl = 2;
                if (visual_count + cvl > ruang_tersedia) break;
                write(STDOUT_FILENO, tampil + k, 2);
                k += 2;
            } else {
                cvl = 1;
                write(STDOUT_FILENO, tampil + k, 1);
                k++;
            }
            visual_count += cvl;
            tulis_len++;
        }
    }
}

/* Fungsi lama untuk kompatibilitas */
void render_input_line(const char *src, int len, int pos_x_input, int pos_y) {
    int scroll_pos = 0;
    render_input_line_scroll(src, len, 0, pos_x_input, pos_y, &scroll_pos);
}

/* ============================================================
 * Mode INPUT (Grapheme & Edit)
 * ============================================================ */
int grapheme_prev(const char *s, int cursor) {
    int i = cursor;
    if (i <= 0) return 0;
    i--;
    while (i > 0 && ((unsigned char)s[i] & 0xC0) == 0x80) {
        i--;
    }
    return i;
}

int grapheme_next(const char *s, int len, int cursor) {
    int l = panjang_cp_utf8(s + cursor);
    int n = cursor + l;
    if (n > len) n = len;
    return n;
}

void lakukan_autocomplete(char *buf, int *len, int *cursor) {
    DIR *d;
    struct dirent *dir;
    char path_dir[PANJANG_PATH_MAKS] = ".";
    char partial[PANJANG_PATH_MAKS] = "";
    char *last_slash = strrchr(buf, '/');
    int dlen;

    if (last_slash) {
        dlen = (int)(last_slash - buf);
        if (dlen == 0) strcpy(path_dir, "/");
        else {
            strncpy(path_dir, buf, (size_t)dlen);
            path_dir[dlen] = '\0';
        }
        strcpy(partial, last_slash + 1);
    } else {
        strcpy(partial, buf);
    }

    d = opendir(path_dir);
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            struct stat st;
            char tmp[PANJANG_PATH_MAKS];
            if (strcmp(dir->d_name, ".") == 0) continue;
            if (strcmp(dir->d_name, "..") == 0) continue;
            if (strncmp(dir->d_name, partial, strlen(partial)) != 0) continue;
            if (last_slash) {
                dlen = (int)(last_slash - buf) + 1;
                strncpy(tmp, buf, (size_t)dlen);
                tmp[dlen] = '\0';
                strcat(tmp, dir->d_name);
                if (stat(tmp, &st) == 0 && S_ISDIR(st.st_mode)) {
                    strcat(tmp, "/");
                }
                salin_str_aman(buf, tmp, MAKS_TEKS);
            } else {
                salin_str_aman(buf, dir->d_name, MAKS_TEKS);
                if (stat(buf, &st) == 0 && S_ISDIR(st.st_mode)) {
                    strcat(buf, "/");
                }
            }
            *len = (int)strlen(buf);
            *cursor = *len;
            break;
        }
        closedir(d);
    }
}

/* ============================================================
 * Motion Parser untuk Seleksi & Goto
 * ============================================================ */
static int parse_motion_target(const char *s, int *x, int *y) {
    if (!s || !x || !y) return -1;
    return parse_ref_sel(s, x, y);
}

static void eksekusi_motion_goto(struct buffer_tabel *buf, const char *target) {
    int tx, ty;
    if (parse_motion_target(target, &tx, &ty) > 0) {
        buf->cfg.aktif_x = tx;
        buf->cfg.aktif_y = ty;
        snprintf(pesan_status, sizeof(pesan_status),
                 "Goto %c%d", 'A' + tx, ty + 1);
    } else {
        snprintf(pesan_status, sizeof(pesan_status),
                 "Target goto tidak valid");
    }
}

int baca_input_edit(char *buf, int *len, int *cursor,
                    int pos_x_input, int pos_y) {
    unsigned char ch;
    unsigned char seq[8];
    unsigned char sub[3];
    unsigned char tilde;
    int n;
    int i;
    int lebar_v;
    int next;
    int prev;
    int l;
    char v_buf[MAKS_TEKS * 2];
    int v_ptr;
    int scroll_pos = 0;  /* untuk scrolling horizontal */
    int cursor_visual_pos;

    atur_mode_terminal(1);

    /* Set warna putih cerah untuk mode edit (menggunakan konstanta WARNAD_PUTIH) */
    warna_tulis_seq(STDOUT_FILENO, WARNAD_PUTIH, WARNAL_DEFAULT);

    while (1) {
        if (read(STDIN_FILENO, &ch, 1) <= 0) continue;

        if (ch == 0x1B) {
            atur_mode_terminal(0);
            n = read(STDIN_FILENO, &seq[0], 1);
            if (n == 0) {
                /* ESC murni - batal, reset warna dan full redraw */
                warna_tulis_reset(STDOUT_FILENO);
                salin_str_aman(pesan_status, "Dibatalkan",
                               sizeof(pesan_status));
                return KEY_BATAL;
            }
            atur_mode_terminal(1);

            if (seq[0] == '[') {
                n = read(STDIN_FILENO, &seq[1], 1);
                if (seq[1] == 'D') {
                    if (*cursor > 0) {
                        *cursor = grapheme_prev(buf, *cursor);
                    }
                } else if (seq[1] == 'C') {
                    if (*cursor < *len) {
                        *cursor = grapheme_next(buf, *len, *cursor);
                    }
                } else if (seq[1] == 'Z') {
                    atur_mode_terminal(0);
                    warna_tulis_reset(STDOUT_FILENO);
                    return KEY_SHIFT_TAB;
                } else if (seq[1] == '3') {
                    read(STDIN_FILENO, &tilde, 1);
                    if (tilde == '~' && *cursor < *len) {
                        next = grapheme_next(buf, *len, *cursor);
                        memmove(buf + *cursor, buf + next,
                                (size_t)(*len - next + 1));
                        *len -= (next - *cursor);
                    }
                } else if (seq[1] == '1') {
                    if (read(STDIN_FILENO, sub, 3) == 3 &&
                        sub[0] == ';' && sub[1] == '5') {
                        if (sub[2] == 'D') {
                            *cursor = 0;
                            scroll_pos = 0;  /* reset scroll */
                        } else if (sub[2] == 'C') {
                            *cursor = *len;
                        }
                    }
                } else if (seq[1] == 'H') {
                    *cursor = 0;
                    scroll_pos = 0;
                } else if (seq[1] == 'F') {
                    *cursor = *len;
                }
            } else if (seq[0] == 'O') {
                read(STDIN_FILENO, &seq[1], 1);
                if (seq[1] == 'H') {
                    *cursor = 0;
                    scroll_pos = 0;
                }
                else if (seq[1] == 'F') *cursor = *len;
            }
        } else if (ch == 0x15) {
            *len = 0;
            *cursor = 0;
            scroll_pos = 0;
            buf[0] = '\0';
        } else if (ch == 0x0B) {
            *len = *cursor;
            buf[*len] = '\0';
        } else if (ch == 0x7F || ch == 0x08) {
            if (*cursor > 0) {
                prev = grapheme_prev(buf, *cursor);
                l = *cursor - prev;
                memmove(buf + prev, buf + *cursor,
                        (size_t)(*len - *cursor + 1));
                *cursor = prev;
                *len -= l;
            }
        } else if (ch >= 0x20) {
            if (*len < MAKS_TEKS - 1) {
                memmove(buf + *cursor + 1, buf + *cursor,
                        (size_t)(*len - *cursor + 1));
                buf[*cursor] = (char)ch;
                (*cursor)++;
                (*len)++;
            }
        } else if (ch == '\n' || ch == '\r') {
            atur_mode_terminal(0);
            warna_tulis_reset(STDOUT_FILENO);
            return '\n';
        } else if (ch == '\t') {
            atur_mode_terminal(0);
            warna_tulis_reset(STDOUT_FILENO);
            return '\t';
        }

        /* Redraw Input Line dengan scrolling */
        /* Gunakan fungsi render dengan scroll */
        render_input_line_scroll(buf, *len, *cursor, pos_x_input, pos_y, &scroll_pos);

        /* Hitung posisi visual kursor dengan memperhitungkan scroll */
        v_ptr = 0;
        for (i = 0; i < *cursor && i < *len; i++) {
            if (buf[i] == '\n') {
                v_buf[v_ptr++] = '\\';
                v_buf[v_ptr++] = 'n';
            } else {
                v_buf[v_ptr++] = buf[i];
            }
        }
        v_buf[v_ptr] = '\0';
        lebar_v = lebar_tampilan_string_utf8(v_buf);
        
        /* Posisi kursor = pos_x_input + (visual_pos - scroll_pos) */
        cursor_visual_pos = lebar_v - scroll_pos;
        if (cursor_visual_pos < 0) cursor_visual_pos = 0;
        
        atur_posisi_kursor(pos_x_input + cursor_visual_pos, pos_y);
        flush_stdout();
    }
    atur_mode_terminal(0);
    warna_tulis_reset(STDOUT_FILENO);
    return 0;
}

/* ============================================================
 * Mode Edit Berantai (Multi Buffer Support)
 * ============================================================ */
void mode_edit_berantai(struct buffer_tabel *buf, int x_mulai,
                        int y_mulai, char inisial) {
    int x = x_mulai;
    int y = y_mulai;
    int sedang_edit = 1;
    char buffer_edit[MAKS_TEKS];
    int panjang;
    int kursor;
    char label[64];
    int pos_x_input;
    char v_buf[MAKS_TEKS * 2];
    int v_ptr;
    int i;
    int visual_offset;
    int tombol;

    /* Dapatkan batas jendela aktif untuk posisi input bar */
    int win_x = jendela_aktif ? jendela_aktif->rect.x : 0;
    int win_y = jendela_aktif ? jendela_aktif->rect.y : 1;
    int win_w = jendela_aktif ? jendela_aktif->rect.w : ambil_lebar_terminal();
    int input_bar_y = win_y + 2;  /* Input bar di baris ke-2 (setelah topbar) */

    while (sedang_edit) {
        if (x >= buf->cfg.kolom) x = buf->cfg.kolom - 1;
        if (y >= buf->cfg.baris) y = buf->cfg.baris - 1;

        if (inisial == '=') {
            if (seleksi_aktif) {
                snprintf(buffer_edit, sizeof(buffer_edit),
                         "=(%c%d:%c%d)",
                         'A' + seleksi_x1, seleksi_y1 + 1,
                         'A' + seleksi_x2, seleksi_y2 + 1);
                panjang = (int)strlen(buffer_edit);
                kursor = panjang;
                inisial = 0;
                seleksi_aktif = 0;
                seleksi_x1 = seleksi_y1 = seleksi_x2 = seleksi_y2 = -1;
            } else {
                salin_str_aman(buffer_edit, "=", MAKS_TEKS);
                panjang = 1;
                kursor = 1;
                inisial = 0;
            }
        } else if (inisial == 0) {
            salin_str_aman(buffer_edit, buf->isi[y][x], MAKS_TEKS);
            panjang = (int)strlen(buffer_edit);
            kursor = panjang;
        } else {
            buffer_edit[0] = inisial;
            buffer_edit[1] = '\0';
            panjang = 1;
            kursor = 1;
            inisial = 0;
        }

        snprintf(label, sizeof(label), "Kolom %c%d: ", 'A' + x, y + 1);
        pos_x_input = win_x + 2 + (int)strlen(label);

        /* Full redraw input bar dengan warna putih untuk mode edit */
        atur_posisi_kursor(win_x + 1, input_bar_y);
        for (i = 0; i < win_w; i++) {
            write(STDOUT_FILENO, " ", 1);
        }
        
        /* Tulis label dengan warna putih cerah 
         * (menggunakan konstanta WARNAD_PUTIH) */
        atur_posisi_kursor(win_x + 2, input_bar_y);
        warna_tulis_seq(STDOUT_FILENO, WARNAD_PUTIH, WARNAL_DEFAULT);
        write(STDOUT_FILENO, label, strlen(label));
        
        render_input_line(buffer_edit, panjang, pos_x_input, input_bar_y);

        v_ptr = 0;
        for (i = 0; i < kursor && i < panjang &&
             v_ptr < (int)sizeof(v_buf) - 1; i++) {
            if (buffer_edit[i] == '\n') {
                if (v_ptr + 2 < (int)sizeof(v_buf)) {
                    v_buf[v_ptr++] = '\\';
                    v_buf[v_ptr++] = 'n';
                } else {
                    break;
                }
            } else {
                v_buf[v_ptr++] = buffer_edit[i];
            }
        }
        v_buf[v_ptr] = '\0';
        visual_offset = lebar_tampilan_string_utf8(v_buf);
        atur_posisi_kursor(pos_x_input + visual_offset, input_bar_y);

        write(STDOUT_FILENO, "\033[?25h", 6);
        flush_stdout();

        tombol = baca_input_edit(buffer_edit, &panjang, &kursor,
                                 pos_x_input, input_bar_y);

        if (tombol == KEY_BATAL) {
            sedang_edit = 0;
        } else {
            atur_teks_sel(buf, x, y, buffer_edit, 1);
            buf->cfg.kotor = 1;

            if (tombol == '\t') {
                if (x < buf->cfg.kolom - 1) {
                    x++;
                    inisial = 0;
                    sedang_edit = 1;
                }
            } else if (tombol == KEY_SHIFT_TAB) {
                if (x > 0) {
                    x--;
                    inisial = 0;
                    sedang_edit = 1;
                }
            } else if (tombol == KEY_BACK_NAV) {
                if (y > 0) {
                    y--;
                    inisial = 0;
                    sedang_edit = 0;
                }
            } else if (tombol == '\n' || tombol == '\r') {
                if (y < buf->cfg.baris - 1) {
                    y++;
                    inisial = 0;
                    sedang_edit = 1;
                }
            } else {
                sedang_edit = 0;
            }
        }

        buf->cfg.aktif_x = x;
        buf->cfg.aktif_y = y;
        write(STDOUT_FILENO, "\033[?25l", 6);
        
        /* Full redraw input bar setelah commit/ESC dengan warna abu-abu (mode normal) */
        {
            char teks_redraw[MAKS_TEKS * 2];
            char teks_norm[MAKS_TEKS * 2];
            const char *teks_sel;
            int j = 0;
            int k;
            
            teks_sel = buf->isi[y][x];
            
            /* Normalisasi teks (newline -> \n) */
            for (k = 0; teks_sel[k] && j < (int)sizeof(teks_norm) - 2; k++) {
                if (teks_sel[k] == '\n') {
                    teks_norm[j++] = '\\';
                    teks_norm[j++] = 'n';
                } else {
                    teks_norm[j++] = teks_sel[k];
                }
            }
            teks_norm[j] = '\0';
            
            /* Format: "Kolom Xn: isi" atau "Kolom Xn: [KOSONG]" */
            if (teks_sel[0] == '\0') {
                snprintf(teks_redraw, sizeof(teks_redraw), 
                        "Kolom %c%d: [KOSONG]", 'A' + x, y + 1);
            } else {
                snprintf(teks_redraw, sizeof(teks_redraw), 
                        "Kolom %c%d: %s", 'A' + x, y + 1, teks_norm);
            }
            
            /* Clear baris dan tulis dengan warna abu-abu */
            warna_tulis_seq(STDOUT_FILENO, WARNAD_ABU, WARNAL_DEFAULT);
            atur_posisi_kursor(win_x + 1, input_bar_y);
            for (i = 0; i < win_w; i++) {
                write(STDOUT_FILENO, " ", 1);
            }
            atur_posisi_kursor(win_x + 2, input_bar_y);
            write(STDOUT_FILENO, teks_redraw, strlen(teks_redraw));
            warna_tulis_reset(STDOUT_FILENO);
            flush_stdout();
        }
        
        pastikan_aktif_terlihat(buf);
        render(buf);
    }
}

/* ============================================================
 * Penangan Escape Sequence
 * ============================================================
 * Menangani semua kombinasi tombol yang dimulai dengan ESC[...
 * Termasuk: panah, PageUp/Down, Shift panah, Alt panah, Ctrl panah,
 *           Alt+kombinasi huruf, dan sebagainya.
 * ============================================================ */
static int kunci_escape_sequence(struct buffer_tabel *cur,
                                 unsigned char *buf_kunci,
                                 int panjang_kunci,
                                 int scroll_amount,
                                 int half_scroll) {
    /* ESC tunggal: membatalkan seleksi atau mode */
    if (buf_kunci[0] == 0x1B && panjang_kunci == 1) {
        if (sedang_memilih || seleksi_aktif) {
            sedang_memilih = 0;
            seleksi_aktif = 0;
            jangkar_x = -1;
            jangkar_y = -1;
            snprintf(pesan_status, sizeof(pesan_status),
                     "Seleksi dibatalkan");
            return 1;
        }
        if (klip_ada_area) {
            klip_ada_area = 0;
            snprintf(pesan_status, sizeof(pesan_status),
                     "Clipboard dibatalkan");
            return 1;
        }
        if (seleksi_random) {
            seleksi_random = 0;
            snprintf(pesan_status, sizeof(pesan_status),
                     "Seleksi random dibatalkan");
            return 1;
        }
        snprintf(pesan_status, sizeof(pesan_status), "Normal");
        return 1;
    }

    /* Ctrl+Alt+Arrows (Super/Mod+Arrows): ESC[1;6A/B/C/D */
    if (panjang_kunci >= 6 &&
        buf_kunci[0] == 0x1B && buf_kunci[1] == '[' &&
        buf_kunci[2] == '1' && buf_kunci[3] == ';' &&
        buf_kunci[4] == '6') {
        switch (buf_kunci[5]) {
            case 'A': pindah_fokus_jendela(K_UP);    break;
            case 'B': pindah_fokus_jendela(K_DOWN);  break;
            case 'C': pindah_fokus_jendela(K_RIGHT); break;
            case 'D': pindah_fokus_jendela(K_LEFT);  break;
        }
        return 1;
    }

    /* Super/Mod+Arrows untuk resize window: ESC[1;7A/B/C/D */
    if (panjang_kunci >= 6 &&
        buf_kunci[0] == 0x1B && buf_kunci[1] == '[' &&
        buf_kunci[2] == '1' && buf_kunci[3] == ';' &&
        buf_kunci[4] == '7') {
        switch (buf_kunci[5]) {
            case 'A': resize_window_aktif(-2); break;
            case 'B': resize_window_aktif(+2); break;
            case 'C': resize_window_aktif(+2); break;
            case 'D': resize_window_aktif(-2); break;
        }
        return 1;
    }

    /* Shift+Tab: ESC[Z */
    if (panjang_kunci >= 3 &&
        buf_kunci[0] == 0x1B && buf_kunci[1] == '[' &&
        buf_kunci[2] == 'Z') {
        geser_kiri(cur);
        return 1;
    }

    /* Panah (Arrow keys): ESC[A/B/C/D atau ESCOA/B/C/D */
    if (panjang_kunci >= 3 && buf_kunci[0] == 0x1B && 
       (buf_kunci[1] == '[' || buf_kunci[1] == 'O')) {
        int is_arrow = 1;
        switch (buf_kunci[2]) {
            case 'A': geser_atas(cur); break;
            case 'B': geser_bawah(cur); break;
            case 'C': geser_kanan(cur); break;
            case 'D': geser_kiri(cur); break;
            default: is_arrow = 0; break; /* Mencegah bentrok dengan Alt+Arrow */
        }
        if (is_arrow) return 1;
    }

    /* PageUp, PageDown, Delete: ESC[5~, ESC[6~, ESC[3~ */
    if (panjang_kunci >= 4 && buf_kunci[0] == 0x1B && 
        buf_kunci[1] == '[' && buf_kunci[3] == '~') {
        int is_nav = 1;
        switch (buf_kunci[2]) {
            case '5':
                cur->cfg.aktif_y -= scroll_amount;
                if (cur->cfg.aktif_y < 0) cur->cfg.aktif_y = 0;
                break;
            case '6':
                cur->cfg.aktif_y += scroll_amount;
                if (cur->cfg.aktif_y >= cur->cfg.baris) {
                    cur->cfg.aktif_y = cur->cfg.baris - 1;
                }
                break;
            case '3':
                hapus_sel_atau_area(cur);
                break;
            default: is_nav = 0; break;
        }
        if (is_nav) return 1;
    }

    /* Shift+Arrows: ESC[1;2A/B/C/D */
    if (panjang_kunci >= 6 && buf_kunci[0] == 0x1B && 
            buf_kunci[1] == '[' && buf_kunci[2] == '1') {
        if (buf_kunci[4] == '2') {
            switch (buf_kunci[5]) {
                case 'A':
                    cur->cfg.aktif_y -= half_scroll;
                    if (cur->cfg.aktif_y < 0) {
                        cur->cfg.aktif_y = 0;
                    }
                    break;
                case 'B':
                    cur->cfg.aktif_y += half_scroll;
                    if (cur->cfg.aktif_y >= cur->cfg.baris) {
                        cur->cfg.aktif_y = cur->cfg.baris - 1;
                    }
                    break;
                case 'C':
                    cur->cfg.aktif_x += 5;
                    if (cur->cfg.aktif_x >= cur->cfg.kolom) {
                        cur->cfg.aktif_x = cur->cfg.kolom - 1;
                    }
                    break;
                case 'D':
                    cur->cfg.aktif_x -= 5;
                    if (cur->cfg.aktif_x < 0) {
                        cur->cfg.aktif_x = 0;
                    }
                    break;
            }
            /* Clamp nilai */
            if (cur->cfg.aktif_y < 0) cur->cfg.aktif_y = 0;
            if (cur->cfg.aktif_y >= cur->cfg.baris) {
                cur->cfg.aktif_y = cur->cfg.baris - 1;
            }
            if (cur->cfg.aktif_x < 0) cur->cfg.aktif_x = 0;
            if (cur->cfg.aktif_x >= cur->cfg.kolom) {
                cur->cfg.aktif_x = cur->cfg.kolom - 1;
            }
            return 1;
        }

        /* Alt+Arrows (resize sel): ESC[1;3A/B/C/D */
        if (buf_kunci[4] == '3') {
            switch (buf_kunci[5]) {
                case 'A': ubah_ukuran_sel(cur, 0, -1); break;
                case 'B': ubah_ukuran_sel(cur, 0, +1); break;
                case 'C': ubah_ukuran_sel(cur, +1, 0); break;
                case 'D': ubah_ukuran_sel(cur, -1, 0); break;
            }
            return 1;
        }

        /* Ctrl+Arrows (swap baris/kolom): ESC[1;5A/B/C/D */
        if (buf_kunci[4] == '5') {
            switch (buf_kunci[5]) {
                case 'A': swap_baris(cur, cur->cfg.aktif_y, -1); break;
                case 'B': swap_baris(cur, cur->cfg.aktif_y, +1); break;
                case 'C': swap_kolom(cur, cur->cfg.aktif_x, +1); break;
                case 'D': swap_kolom(cur, cur->cfg.aktif_x, -1); break;
            }
            return 1;
        }
    }

    /* Alt+P (0xC3 0xB0): Paste penuh sebelum */
    if (panjang_kunci == 2 &&
        (unsigned char)buf_kunci[0] == 0xC3 &&
        (unsigned char)buf_kunci[1] == 0xB0) {
        if (klip_mode == 1) {
            tempel_baris_penuh(cur, cur->cfg.aktif_y, -1);
        } else if (klip_mode == 2) {
            tempel_kolom_penuh(cur, cur->cfg.aktif_x, -1);
        }
        return 1;
    }

    /* Alt+Q (0xC3 0xB1): Tutup window aktif */
    if (panjang_kunci == 2 &&
        (unsigned char)buf_kunci[0] == 0xC3 &&
        (unsigned char)buf_kunci[1] == 0xB1) {
        tutup_window_aktif();
        return 1;
    }

    /* Alt+r (ESC r atau 0x1B 0x72): Toggle orientasi split */
    if (panjang_kunci == 2 &&
        (unsigned char)buf_kunci[0] == 0xC3 &&
        (unsigned char)buf_kunci[1] == 0x72) {
        toggle_orientasi_split();
        return 1;
    }

    /* Alt+y (0xC3 0xB9): Yank kolom penuh */
    if (panjang_kunci == 2 &&
        (unsigned char)buf_kunci[0] == 0xC3 &&
        (unsigned char)buf_kunci[1] == 0xB9) {
        salin_kolom_penuh(cur, cur->cfg.aktif_x);
        return 1;
    }

    /* Alt+x (0xC3 0xB8): Cut kolom penuh */
    if (panjang_kunci == 2 &&
        (unsigned char)buf_kunci[0] == 0xC3 &&
        (unsigned char)buf_kunci[1] == 0xB8) {
        potong_kolom_penuh(cur);
        return 1;
    }

    /* Alt+v (0xC3 0xB6): Seleksi kolom penuh */
    if (panjang_kunci == 2 &&
        (unsigned char)buf_kunci[0] == 0xC3 &&
        (unsigned char)buf_kunci[1] == 0xB6) {
        seleksi_kolom_penuh_aktif(cur);
        return 1;
    }

    /* Alt+. (0xC2 0xAE): Adjust width to longest */
    if (panjang_kunci == 2 &&
        (unsigned char)buf_kunci[0] == 0xC2 &&
        (unsigned char)buf_kunci[1] == 0xAE) {
        sesuaikan_lebar_kolom(cur, cur->cfg.aktif_x, 1);
        return 1;
    }

    /* Alt+, (0xC2 0xAC): Adjust width to shortest */
    if (panjang_kunci == 2 &&
        (unsigned char)buf_kunci[0] == 0xC2 &&
        (unsigned char)buf_kunci[1] == 0xAC) {
        sesuaikan_lebar_kolom(cur, cur->cfg.aktif_x, -1);
        return 1;
    }

    /* Alt+= (0xC2 0xBD): Adjust all columns */
    if (panjang_kunci == 2 &&
        (unsigned char)buf_kunci[0] == 0xC2 &&
        (unsigned char)buf_kunci[1] == 0xBD) {
        sesuaikan_semua_lebar(cur, cur->cfg.aktif_x);
        return 1;
    }

    return 0;
}

/* ============================================================
 * Penangan Prefix Commands
 * ============================================================
 * Menangani kombinasi prefix + huruf:
 *   W -> s/v/q  : Window split/close
 *   d -> c/r    : Delete column/row
 *   c -> c/r    : Clean column/row
 *   b -> t/b/l/r/o/a/w/n : Border operations
 *   t -> w      : Text coloring
 * ============================================================ */
static int kunci_prefix(struct buffer_tabel *cur,
                        unsigned char c,
                        int *prefix_window,
                        int *prefix_delete,
                        int *prefix_clean,
                        int *prefix_border,
                        int *prefix_textcolor) {
    /* Prefix Window: W -> s/v/q */
    if (*prefix_window) {
        if (c == 's') {
            split_window_aktif(0, -1);
        } else if (c == 'v') {
            split_window_aktif(1, -1);
        } else if (c == 'r') {
            toggle_orientasi_split();
        } else {
            snprintf(pesan_status, sizeof(pesan_status), "Batal Window");
        }
        *prefix_window = 0;
        return 1;
    }

    /* Prefix Delete: d -> c/r */
    if (*prefix_delete) {
        if (c == 'c') {
            hapus_kolom_aktif(cur);
            snprintf(pesan_status, sizeof(pesan_status), "Hapus Kolom");
        } else if (c == 'r') {
            hapus_baris_aktif(cur);
            snprintf(pesan_status, sizeof(pesan_status), "Hapus Baris");
        } else {
            snprintf(pesan_status, sizeof(pesan_status), "Batal hapus");
        }
        *prefix_delete = 0;
        return 1;
    }

    /* Prefix Clean: c -> c/r */
    if (*prefix_clean) {
        if (c == 'c') {
            bersihkan_kolom_aktif(cur);
        } else if (c == 'r') {
            bersihkan_baris_aktif(cur);
        } else {
            snprintf(pesan_status, sizeof(pesan_status), "Batal clean");
        }
        *prefix_clean = 0;
        return 1;
    }

    /* Prefix Border: b -> t/b/l/r/o/a/w/n/x */
    if (*prefix_border) {
        if (c == 't') {
            eksekusi_border(cur, "bt");
        } else if (c == 'b') {
            eksekusi_border(cur, "bb");
        } else if (c == 'l') {
            eksekusi_border(cur, "bl");
        } else if (c == 'r') {
            eksekusi_border(cur, "br");
        } else if (c == 'o') {
            eksekusi_border(cur, "bo");
        } else if (c == 'a') {
            eksekusi_border(cur, "ba");
        } else if (c == 'w') {
            eksekusi_border(cur, "bw");
        } else if (c == 'n' || c == 'x') {
            eksekusi_border(cur, "none");
        } else {
            snprintf(pesan_status, sizeof(pesan_status), "Batal Border");
        }
        *prefix_border = 0;
        seleksi_aktif = 0;
        sedang_memilih = 0;
        return 1;
    }

    /* Prefix Text Coloring: t -> w */
    if (*prefix_textcolor) {
        if (c == 'w') {
            eksekusi_warna_teks(cur, " ");
        }
        *prefix_textcolor = 0;
        seleksi_aktif = 0;
        sedang_memilih = 0;
        return 1;
    }

    return 0;
}

/* ============================================================
 * Penangan Single Letter Shortcuts
 * ============================================================
 * Menangani semua pintasan satu tombol dan Ctrl+kombinasi:
 *   q        : Keluar aplikasi
 *   T        : Tutup tab
 *   [ / ]    : Tab sebelumnya/berikutnya
 *   Enter    : Edit atau konfirmasi seleksi
 *   =        : Input formula
 *   Tab      : Geser kanan
 *   p / P    : Paste / Fill paste
 *   y / x    : Yank / Cut
 *   v        : Mode visual
 *   V        : Seleksi ke target
 *   m / M    : Merge / Unmerge
 *   n / N    : Cari berikutnya/sebelumnya
 *   U / L / E: Ubah kas (upper/lower/title)
 *   i / I    : Sisip kolom setelah/sebelum
 *   o / O    : Sisip baris setelah/sebelum
 *   j        : Tab baru
 *   G        : Ke baris terakhir
 *   g        : Goto sel
 *   w        : Simpan berkas
 *   e        : Buka berkas
 *   /        : Cari
 *   \        : Ganti
 *   s        : Sortir kolom
 *   ?        : Bantuan
 *   u / r    : Undo / Redo
 *   { } |    : Perataan kiri/kanan/tengah
 *   Space    : Toggle seleksi random
 *   Backspace: Hapus sel/area
 *   Ctrl+P   : Paste penuh sesudah
 *   Ctrl+Y   : Yank baris penuh
 *   Ctrl+X   : Cut baris penuh
 *   Ctrl+V   : Seleksi baris penuh
 * ============================================================ */
static int kunci_satu_huruf(struct buffer_tabel *cur,
                            unsigned char c,
                            int *kunci_sebelumnya) {
    char target[32];
    char nama[PANJANG_PATH_MAKS];
    char q[MAKS_TEKS];
    char cwd[PANJANG_PATH_MAKS];
    struct buffer_tabel *nb;
    struct buffer_tabel *baru;
    int idx_ditutup;
    int n;
    int ok;
    int jx, jy, ax, ay;
    int tx, ty;
    int jx_tmp, jy_tmp;
    int klip_mode_save;

    /* q: Keluar aplikasi */
    if (c == 'q') {
        pulihkan_terminal();
        keluar_mode_alternatif();
        return -1; /* Sinyal keluar */
    }

    /* T: Tutup tab */
    if (c == 'T') {
        idx_ditutup = buffer_aktif;
        tutup_buffer(buffer_aktif);
        if (jumlah_buffer == 0) {
            tambah_buffer(buat_buffer(100, 26));
        }
        sinkronkan_tree_jendela_tutup(idx_ditutup);
        if (jendela_aktif) {
            jendela_aktif->buffer_idx = buffer_aktif;
        }
        return 1;
    }

    /* [: Tab sebelumnya */
    if (c == '[') {
        n = buffer_aktif - 1;
        if (n < 0) n = jumlah_buffer - 1;
        set_buffer_aktif(n);
        if (jendela_aktif) {
            jendela_aktif->buffer_idx = buffer_aktif;
        }
        return 1;
    }

    /* ]: Tab berikutnya */
    if (c == ']') {
        n = buffer_aktif + 1;
        if (n >= jumlah_buffer) n = 0;
        set_buffer_aktif(n);
        if (jendela_aktif) {
            jendela_aktif->buffer_idx = buffer_aktif;
        }
        return 1;
    }

    /* Enter: Edit atau konfirmasi seleksi */
    if (c == '\n' || c == '\r') {
        if (sedang_memilih) {
            jx = jangkar_x;
            jy = jangkar_y;
            ax = cur->cfg.aktif_x;
            ay = cur->cfg.aktif_y;
            seleksi_x1 = (jx < ax) ? jx : ax;
            seleksi_y1 = (jy < ay) ? jy : ay;
            seleksi_x2 = (jx > ax) ? jx : ax;
            seleksi_y2 = (jy > ay) ? jy : ay;
            seleksi_aktif = 1;
            sedang_memilih = 0;
            jangkar_x = jangkar_y = -1;
            snprintf(pesan_status, sizeof(pesan_status), "Seleksi aktif");
        } else {
            mode_edit_berantai(cur, cur->cfg.aktif_x, cur->cfg.aktif_y, 0);
            *kunci_sebelumnya = 0;
        }
        return 1;
    }

    /* =: Input formula */
    if (c == '=') {
        mode_edit_berantai(cur, cur->cfg.aktif_x, cur->cfg.aktif_y, '=');
        return 1;
    }

    /* Tab: Geser kanan */
    if (c == '\t') {
        geser_kanan(cur);
        return 1;
    }

    /* p: Paste ke sel/baris/kolom aktif */
    if (c == 'p') {
        klip_mode_save = klip_mode;
        lakukan_tempel(cur, 0, 0);
        if (klip_mode_save == 1) {
            tempel_baris_penuh(cur, cur->cfg.aktif_y, 0);
        } else if (klip_mode_save == 2) {
            tempel_kolom_penuh(cur, cur->cfg.aktif_x, 0);
        }
        return 1;
    }

    /* P: Fill paste (isi seleksi dengan clipboard) */
    if (c == 'P') {
        tempel_isi_ke_seleksi(cur);
        return 1;
    }

    /* y: Yank (salin area) */
    if (c == 'y') {
        salin_area(cur);
        klip_ada_area = 1;
        sedang_memilih = 0;
        snprintf(pesan_status, sizeof(pesan_status), "Yank");
        return 1;
    }

    /* x: Cut (potong area) */
    if (c == 'x') {
        potong_area(cur);
        klip_ada_area = 1;
        sedang_memilih = 0;
        snprintf(pesan_status, sizeof(pesan_status), "Cut");
        return 1;
    }

    /* v: Mode visual */
    if (c == 'v') {
        if (!sedang_memilih) {
            seleksi_aktif = 0;
            seleksi_x1 = -1;
            seleksi_x2 = -1;
            seleksi_y1 = -1;
            seleksi_y2 = -1;
            sedang_memilih = 1;
            klip_ada_area = 0;
            jangkar_x = cur->cfg.aktif_x;
            jangkar_y = cur->cfg.aktif_y;
            salin_str_aman(pesan_status, "VISUAL", sizeof(pesan_status));
        } else {
            sedang_memilih = 0;
            jangkar_x = -1;
            jangkar_y = -1;
            salin_str_aman(pesan_status, "Batal Visual", sizeof(pesan_status));
        }
        return 1;
    }

    /* V: Seleksi ke target */
    if (c == 'V') {
        minta_nama_berkas(target, sizeof(target), "Seleksi ke: ");
        if (strlen(target) > 0) {
            if (parse_ref_sel(target, &tx, &ty) > 0) {
                jx_tmp = cur->cfg.aktif_x;
                jy_tmp = cur->cfg.aktif_y;
                seleksi_x1 = (jx_tmp < tx) ? jx_tmp : tx;
                seleksi_y1 = (jy_tmp < ty) ? jy_tmp : ty;
                seleksi_x2 = (jx_tmp > tx) ? jx_tmp : tx;
                seleksi_y2 = (jy_tmp > ty) ? jy_tmp : ty;
                seleksi_aktif = 1;
                sedang_memilih = 0;
                cur->cfg.aktif_x = tx;
                cur->cfg.aktif_y = ty;
            }
        }
        return 1;
    }

    /* m: Merge sel */
    if (c == 'm') {
        eksekusi_gabung(cur);
        klip_ada_area = 0;
        sedang_memilih = 0;
        return 1;
    }

    /* M: Unmerge sel */
    if (c == 'M') {
        eksekusi_urai_gabung(cur);
        return 1;
    }

    /* n: Cari berikutnya */
    if (c == 'n') {
        cari_next(cur);
        return 1;
    }

    /* N: Cari sebelumnya */
    if (c == 'N') {
        cari_prev(cur);
        return 1;
    }

    /* U: Ubah ke huruf besar */
    if (c == 'U') {
        ubah_teks_sel_huruf_besar(cur, cur->cfg.aktif_x, cur->cfg.aktif_y);
        return 1;
    }

    /* L: Ubah ke huruf kecil */
    if (c == 'L') {
        ubah_teks_sel_huruf_kecil(cur, cur->cfg.aktif_x, cur->cfg.aktif_y);
        return 1;
    }

    /* E: Ubah ke title case */
    if (c == 'E') {
        ubah_teks_sel_judul(cur, cur->cfg.aktif_x, cur->cfg.aktif_y);
        return 1;
    }

    /* i: Sisip kolom setelah */
    if (c == 'i') {
        sisip_kolom_setelah(cur);
        return 1;
    }

    /* I: Sisip kolom sebelum */
    if (c == 'I') {
        sisip_kolom_sebelum(cur);
        return 1;
    }

    /* o: Sisip baris setelah */
    if (c == 'o') {
        sisip_baris_setelah(cur);
        return 1;
    }

    /* O: Sisip baris sebelum */
    if (c == 'O') {
        sisip_baris_sebelum(cur);
        return 1;
    }

    /* j: Tab baru */
    if (c == 'j') {
        nb = buat_buffer(100, 26);
        if (nb) {
            tambah_buffer(nb);
            if (jendela_aktif) {
                jendela_aktif->buffer_idx = buffer_aktif;
            }
            snprintf(pesan_status, sizeof(pesan_status), "Tab baru");
        }
        return 1;
    }

    /* G: Ke baris terakhir */
    if (c == 'G') {
        cur->cfg.aktif_y = cur->cfg.baris - 1;
        return 1;
    }

    /* g: Goto sel */
    if (c == 'g') {
        minta_nama_berkas(target, sizeof(target), "Goto: ");
        if (strlen(target) > 0) {
            eksekusi_motion_goto(cur, target);
        }
        return 1;
    }

    /* w: Simpan berkas (Detach jika nama berubah) */
    if (c == 'w') {
        minta_nama_berkas(nama, sizeof(nama), "Simpan: ");
        if (strlen(nama) > 0) {
            int is_new_name = (strcmp(cur->cfg.path_file, nama) != 0) || (cur->cfg.path_file[0] == '\0');
            int win_count = hitung_penggunaan_idx(buffer_aktif);
            
            /* --- LOGIKA DETACH SHARED BUFFER --- */
            if ((win_count > 1 || cur->refcount > 1) && is_new_name) {
                /* Detach: Shared buffer disimpan dengan nama baru (Save As) */
                baru = duplikat_buffer(cur);
                salin_str_aman(baru->cfg.path_file, nama, sizeof(baru->cfg.path_file));
                
                ok = simpan_txt(baru, nama);
                if (strstr(nama, ".csv")) ok = simpan_csv(baru, nama);
                else if (strstr(nama, ".tbl")) ok = simpan_tbl(baru, nama);
                else if (strstr(nama, ".sql")) ok = simpan_sql(baru, nama);
                else if (strstr(nama, ".db")) ok = simpan_db(baru, nama);
                else if (strstr(nama, ".md")) ok = simpan_md(baru, nama);
                else if (strstr(nama, ".pdf")) ok = simpan_pdf_baru(baru, nama);
                else if (strstr(nama, ".xlsx")) ok = simpan_xlsx(baru, nama);
                else if (strstr(nama, ".xls")) ok = simpan_xls(baru, nama);
                else if (strstr(nama, ".xml")) ok = simpan_xml(baru, nama);
                else if (strstr(nama, ".ods")) ok = simpan_ods(baru, nama);
                else if (strstr(nama, ".html")) ok = simpan_html(baru, nama);
                else if (strstr(nama, ".json")) ok = simpan_json(baru, nama);
                
                if (ok == 0) {
                    baru->cfg.kotor = 0;
                    
                    if (win_count > 1) {
                        /* Jendela memisahkan diri ke tab baru */
                        int idx_baru = tambah_buffer(baru);
                        if (jendela_aktif) jendela_aktif->buffer_idx = idx_baru;
                    } else {
                        /* Hanya satu jendela di tab ini, cukup ganti buffernya */
                        bebas_buffer(cur); 
                        buffers[buffer_aktif] = baru;
                    }
                    snprintf(pesan_status, sizeof(pesan_status), "Disimpan & Dilepas: %s", nama);
                } else {
                    bebas_buffer(baru);
                    snprintf(pesan_status, sizeof(pesan_status), "Gagal Simpan");
                }
            } else {
                /* Simpan normal pada buffer yang sama (misal Ctrl+S) */
                ok = simpan_txt(cur, nama);
                if (strstr(nama, ".csv")) ok = simpan_csv(cur, nama);
                else if (strstr(nama, ".tbl")) ok = simpan_tbl(cur, nama);
                else if (strstr(nama, ".sql")) ok = simpan_sql(cur, nama);
                else if (strstr(nama, ".db")) ok = simpan_db(cur, nama);
                else if (strstr(nama, ".md")) ok = simpan_md(cur, nama);
                else if (strstr(nama, ".pdf")) ok = simpan_pdf_baru(cur, nama);
                else if (strstr(nama, ".xlsx")) ok = simpan_xlsx(cur, nama);
                else if (strstr(nama, ".xls")) ok = simpan_xls(cur, nama);
                else if (strstr(nama, ".xml")) ok = simpan_xml(cur, nama);
                else if (strstr(nama, ".ods")) ok = simpan_ods(cur, nama);
                else if (strstr(nama, ".html")) ok = simpan_html(cur, nama);
                else if (strstr(nama, ".json")) ok = simpan_json(cur, nama);
                
                if (ok == 0) {
                    salin_str_aman(cur->cfg.path_file, nama, sizeof(cur->cfg.path_file));
                    cur->cfg.kotor = 0;
                    snprintf(pesan_status, sizeof(pesan_status), "Disimpan: %s", nama);
                } else {
                    snprintf(pesan_status, sizeof(pesan_status), "Gagal Simpan");
                }
            }
        }
        return 1;
    }

    /* W: Export ke PDF dengan konfigurasi interaktif */
    if (c == 'E') {
        minta_nama_berkas(nama, sizeof(nama), "Export PDF: ");
        if (strlen(nama) > 0) {
            /* Pastikan ekstensi .pdf */
            if (!strstr(nama, ".pdf")) {
                gabung_str_aman(nama, ".pdf", sizeof(nama));
            }

            ok = export_pdf_interaktif(cur, nama);

            if (ok == 0) {
                snprintf(pesan_status, sizeof(pesan_status), "PDF berhasil: %s", nama);
            } else {
                snprintf(pesan_status, sizeof(pesan_status), "Gagal export PDF");
            }
        }
        return 1;
    }

    /* e: Buka berkas (Detach jika di dalam Shared Buffer) */
    if (c == 'e') {
        minta_nama_berkas(nama, sizeof(nama), "Buka: ");
        if (strlen(nama) > 0) {
            baru = NULL;
            ok = buka_txt(&baru, nama);
            if (strstr(nama, ".csv")) ok = buka_csv(&baru, nama);
            else if (strstr(nama, ".tbl")) ok = buka_tbl(&baru, nama);
            else if (strstr(nama, ".sql")) ok = buka_sql(&baru, nama);
            else if (strstr(nama, ".db")) ok = buka_db(&baru, nama);
            else if (strstr(nama, ".md")) ok = buka_md(&baru, nama);
            else if (strstr(nama, ".xlsx")) ok = buka_xlsx(&baru, nama);
            else if (strstr(nama, ".xls")) ok = buka_xls(&baru, nama);
            else if (strstr(nama, ".xml")) ok = buka_xml(&baru, nama);
            else if (strstr(nama, ".ods")) ok = buka_ods(&baru, nama);
            else if (strstr(nama, ".html")) ok = buka_html(&baru, nama);
            else if (strstr(nama, ".htm")) ok = buka_html(&baru, nama);
            else if (strstr(nama, ".json")) ok = buka_json(&baru, nama);
            
            if (ok == 0) {
                if (getcwd(cwd, sizeof(cwd))) {
                    snprintf(baru->cfg.path_file, sizeof(baru->cfg.path_file), "%s/%s", cwd, nama);
                } else {
                    salin_str_aman(baru->cfg.path_file, nama, sizeof(baru->cfg.path_file));
                }
                
                /* --- LOGIKA DETACH SHARED BUFFER --- */
                int win_count = hitung_penggunaan_idx(buffer_aktif);
                
                if (win_count > 1 || cur->refcount > 1) {
                    /* Detach: Buka berkas di buffer baru, biarkan yang lama dipakai window/tab lain */
                    int idx_baru = tambah_buffer(baru);
                    if (jendela_aktif) jendela_aktif->buffer_idx = idx_baru;
                } else {
                    /* Normal: Timpa buffer saat ini karena tidak ada yang share */
                    bebas_buffer(buffers[buffer_aktif]);
                    buffers[buffer_aktif] = baru;
                }
                snprintf(pesan_status, sizeof(pesan_status), "Membuka: %s", nama);
            } else {
                bebas_buffer(baru);
                snprintf(pesan_status, sizeof(pesan_status), "Gagal Buka");
            }
        }
        return 1;
    }

    /* /: Cari */
    if (c == '/') {
        minta_nama_berkas(q, sizeof(q), "Cari: ");
        if (strlen(q) > 0) lakukan_cari(cur, q);
        return 1;
    }

    /* \ atau Ctrl+/: Ganti */
    if (c == '\\' || c == 0x1F) {
        minta_nama_berkas(q, sizeof(q), "Ganti: ");
        if (strlen(q) > 0) {
            n = lakukan_cari_ganti(cur, q);
            snprintf(pesan_status, sizeof(pesan_status),
                     "Mengganti %d sel", n);
        }
        return 1;
    }

    /* s: Sortir kolom aktif */
    if (c == 's') {
        sortir_kolom_aktif(cur);
        return 1;
    }

    /* ?: Bantuan */
    if (c == '?') {
        tampilkan_bantuan();
        return 1;
    }

    /* u: Undo */
    if (c == 'u') {
        lakukan_undo(cur);
        return 1;
    }

    /* r: Redo */
    if (c == 'r') {
        lakukan_redo(cur);
        return 1;
    }

    /* {: Rata kiri */
    if (c == '{') {
        ubah_perataan_sel(cur, RATA_KIRI);
        return 1;
    }

    /* }: Rata kanan */
    if (c == '}') {
        ubah_perataan_sel(cur, RATA_KANAN);
        return 1;
    }

    /* |: Rata tengah */
    if (c == '|') {
        ubah_perataan_sel(cur, RATA_TENGAH);
        return 1;
    }

    /* Space: Toggle seleksi random */
    if (c == ' ') {
        toggle_seleksi_random(cur, cur->cfg.aktif_x, cur->cfg.aktif_y);
        return 1;
    }

    /* Backspace (0x7F): Hapus sel/area */
    if (c == 0x7F) {
        hapus_sel_atau_area(cur);
        return 1;
    }

    /* Ctrl+P (0x10): Paste penuh sesudah */
    if (c == 0x10) {
        if (klip_mode == 1) {
            tempel_baris_penuh(cur, cur->cfg.aktif_y, +1);
        } else if (klip_mode == 2) {
            tempel_kolom_penuh(cur, cur->cfg.aktif_x, +1);
        }
        return 1;
    }

    /* Ctrl+Y (0x19): Yank baris penuh */
    if (c == 0x19) {
        salin_baris_penuh(cur, cur->cfg.aktif_y);
        return 1;
    }

    /* Ctrl+X (0x18): Cut baris penuh */
    if (c == 0x18) {
        potong_baris_penuh(cur);
        return 1;
    }

    /* Ctrl+V (0x16): Seleksi baris penuh */
    if (c == 0x16) {
        seleksi_baris_penuh_aktif(cur);
        return 1;
    }

    /* ============================================================
     * Font Style Shortcuts
     * ============================================================ */

    /* Ctrl+B (0x02): Bold */
    if (c == 0x02) {
        toggle_font_style_area(cur, STYLE_BOLD);
        return 1;
    }

    /* Ctrl+K (0x0B): Italic (Ctrl+I menghasilkan TAB) */
    if (c == 0x0B) {
        toggle_font_style_area(cur, STYLE_ITALIC);
        return 1;
    }

    /* Ctrl+U (0x15): Underline */
    if (c == 0x15) {
        toggle_font_style_area(cur, STYLE_UNDERLINE);
        return 1;
    }

    /* Ctrl+D (0x04): Strikethrough */
    if (c == 0x04) {
        toggle_font_style_area(cur, STYLE_STRIKETHROUGH);
        return 1;
    }

    /* ============================================================
     * Multi-Sheet Navigation Shortcuts
     * ============================================================ */

    /* (: Lembar sebelumnya */
    if (c == '(') {
        if (cur->lembar_aktif > 0) {
            set_lembar_aktif(cur, cur->lembar_aktif - 1);
            snprintf(pesan_status, sizeof(pesan_status),
                     "Lembar: %s", cur->lembar[cur->lembar_aktif]->nama);
        }
        return 1;
    }

    /* ): Lembar berikutnya */
    if (c == ')') {
        if (cur->lembar_aktif < cur->jumlah_lembar - 1) {
            set_lembar_aktif(cur, cur->lembar_aktif + 1);
            snprintf(pesan_status, sizeof(pesan_status),
                     "Lembar: %s", cur->lembar[cur->lembar_aktif]->nama);
        }
        return 1;
    }

    /* +: Tambah lembar baru */
    if (c == '+') {
        {
            struct lembar_tabel *lem_baru = buat_lembar(cur->cfg.baris, 
                                                        cur->cfg.kolom, NULL);
            if (lem_baru) {
                int idx = tambah_lembar_ke_buffer(cur, lem_baru);
                if (idx >= 0) {
                    set_lembar_aktif(cur, idx);
                    snprintf(pesan_status, sizeof(pesan_status),
                             "Lembar baru: %s", lem_baru->nama);
                }
            }
        }
        return 1;
    }

    /* _: Hapus lembar aktif */
    if (c == '_') {
        if (cur->jumlah_lembar > 1) {
            int idx_hapus = cur->lembar_aktif;
            hapus_lembar_dari_buffer(cur, idx_hapus);
        } else {
            snprintf(pesan_status, sizeof(pesan_status),
                     "Tidak bisa menghapus lembar terakhir");
        }
        return 1;
    }

    /* :: Ganti nama lembar */
    if (c == ':') {
        {
            char nama_baru[NAMA_LEMBAR_MAKS];
            minta_nama_berkas(nama_baru, sizeof(nama_baru), "Nama Lembar: ");
            if (strlen(nama_baru) > 0) {
                rename_lembar(cur, cur->lembar_aktif, nama_baru);
                snprintf(pesan_status, sizeof(pesan_status),
                         "Lembar diubah: %s", nama_baru);
            }
        }
        return 1;
    }

    return 0;
}

/* ============================================================
 * Loop Utama Aplikasi (Poll Multi Buffer)
 * ============================================================
 * Fungsi utama yang mengatur input loop aplikasi.
 * Memanggil fungsi-fungsi helper berdasarkan jenis kunci.
 * ============================================================ */
void mulai_aplikasi_poll(struct buffer_tabel *buf_awal) {
    struct pollfd fd_set[1];
    int kunci_sebelumnya = 0;
    unsigned char buf_kunci[8];
    int panjang_kunci;
    int prefix_window = 0;
    int prefix_textcolor = 0;
    int prefix_delete = 0;
    int prefix_clean = 0;
    int prefix_border = 0;
    int scroll_amount;
    int half_scroll;
    int ret;
    unsigned char c;

    /* Inisialisasi buffer jika belum ada */
    if (jumlah_buffer == 0 && buf_awal) {
        tambah_buffer(buf_awal);
    } else if (jumlah_buffer == 0) {
        tambah_buffer(buat_buffer(100, 26));
    }

    while (1) {
        struct buffer_tabel *cur = buffers[buffer_aktif];
        if (!cur) break;

        set_konfig_aktif(&cur->cfg);

        /* Tangani resize terminal */
        if (perlu_resize) {
            perlu_resize = 0;
            proses_resize_terpusat();
        }

        pastikan_aktif_terlihat(cur);
        render(cur);

        /* Poll Input */
        fd_set[0].fd = STDIN_FILENO;
        fd_set[0].events = POLLIN;
        ret = poll(fd_set, 1, -1);
        if (ret == -1) {
            if (errno == EINTR) continue;
            break;
        }

        if (fd_set[0].revents & POLLIN) {
            baca_satu_kunci(buf_kunci, &panjang_kunci);
            if (panjang_kunci == 0) continue;
        } else {
            continue;
        }

        c = buf_kunci[0];

        /* Hitung scroll amount */
        scroll_amount = ambil_tinggi_terminal() - 4;
        if (scroll_amount < 1) scroll_amount = 1;
        half_scroll = scroll_amount / 2;
        if (half_scroll < 1) half_scroll = 1;

        /* --- Penangan Jendela Bantuan --- */
        /* Cek apakah jendela aktif adalah bantuan */
        if (jendela_aktif && jendela_aktif->adalah_bantuan) {
            /* Proses input untuk bantuan */
            if (proses_input_bantuan_window(c, buf_kunci + 1, panjang_kunci - 1)) {
                continue;
            }
            /* Jika tidak ditangani, biarkan jatuh ke bawah untuk navigasi window */
        }

        /* --- Penangan Prefix Commands --- */
        /* Cek apakah ada prefix aktif dan proses */
        if (prefix_window || prefix_delete || prefix_clean ||
            prefix_border || prefix_textcolor) {
            if (kunci_prefix(cur, c, &prefix_window, &prefix_delete,
                             &prefix_clean, &prefix_border,
                             &prefix_textcolor)) {
                continue;
            }
        }

        /* --- Set Prefix Flags --- */
        /* W: Prefix Window */
        if (panjang_kunci == 1 && c == 'W') {
            prefix_window = 1;
            snprintf(pesan_status, sizeof(pesan_status), "Window... (s/v/q)");
            continue;
        }

        /* d: Prefix Delete */
        if (panjang_kunci == 1 && c == 'd') {
            prefix_delete = 1;
            continue;
        }

        /* c: Prefix Clean */
        if (panjang_kunci == 1 && c == 'c') {
            prefix_clean = 1;
            continue;
        }

        /* b: Prefix Border */
        if (panjang_kunci == 1 && c == 'b') {
            prefix_border = 1;
            snprintf(pesan_status, sizeof(pesan_status),
                     "Border... (t/b/l/r/o/a/w/n)");
            continue;
        }

        /* t: Prefix Text Coloring */
        if (panjang_kunci == 1 && c == 't') {
            prefix_textcolor = 1;
            snprintf(pesan_status, sizeof(pesan_status), "Text Coloring...");
            continue;
        }

        /* --- Penangan Escape Sequence --- */
        if (kunci_escape_sequence(cur, buf_kunci, panjang_kunci,
                                  scroll_amount, half_scroll)) {
            continue;
        }

        /* --- Penangan Single Letter Shortcuts --- */
        if (panjang_kunci == 1 || panjang_kunci == 2) {
            ret = kunci_satu_huruf(cur, c, &kunci_sebelumnya);
            if (ret == -1) {
                /* Sinyal keluar dari aplikasi */
                return;
            }
            if (ret == 1) {
                continue;
            }
        }

        /* Simpan state jendela aktif */
        if (jendela_aktif) {
            simpan_state_jendela(jendela_aktif);
        }
    }
}
