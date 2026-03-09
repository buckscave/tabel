/* ========================================================================== *
 * TABEL v3.0 
 * Berkas: form.c - Formula Core (Lexer, Parser, Evaluator)
 * Standar: C89 + POSIX.1-2008
 * Penulis: Chandra Lesmana
 * ========================================================================== */

#include "tabel.h"

/* ========================================================================== *
 * KONSTANTA LOKAL
 * ========================================================================== */
#define MAKS_TOKEN_LOKAL     256
#define MAKS_TEKS_TOKEN_LOK  64
#define MAKS_RPN_LOKAL       256
#define MAKS_STAK_NILAI_LOK  256
#define MAKS_KEDALAMAN_LOK   16

/* ========================================================================== *
 * TIPE NILAI INTERNAL
 * ========================================================================== */
typedef enum {
    NILAI_ANGKA = 0,
    NILAI_TEKS  = 1,
    NILAI_ERROR = 2
} jenis_nilai_int;

struct nilai_int {
    jenis_nilai_int jenis;
    double angka;
    char teks[MAKS_TEKS];
};

/* ========================================================================== *
 * TIPE TOKEN
 * ========================================================================== */
typedef enum {
    TOK_ANGKA,
    TOK_TEKS,
    TOK_SEL,
    TOK_RANGE,
    TOK_FUNGSI,
    TOK_OP,
    TOK_KURUNG_BUKA,
    TOK_KURUNG_TUTUP,
    TOK_KOMA,
    TOK_AKHIR
} jenis_token;

struct token {
    jenis_token jenis;
    char teks[MAKS_TEKS_TOKEN_LOK];
    double angka;
};

/* ========================================================================== *
 * STRUKTUR RPN
 * ========================================================================== */
struct rpn {
    struct token t[MAKS_RPN_LOKAL];
    int n;
};

/* ========================================================================== *
 * VARIABEL GLOBAL INTERNAL
 * ========================================================================== */
static char galat_terakhir[MAKS_TEKS] = "";

/* ========================================================================== *
 * FUNGSI UTILITAS STRING
 * ========================================================================== */
/* Salin string dengan aman */
static int salin_str(char *tujuan, const char *sumber, size_t ukuran) {
    size_t i = 0;
    if (!tujuan || !sumber || ukuran == 0) return -1;
    while (i + 1 < ukuran && sumber[i] != '\0') {
        tujuan[i] = sumber[i];
        i++;
    }
    tujuan[i] = '\0';
    return (int)i;
}

/* Bandingkan string tidak peka huruf besar/kecil */
static int banding_str(const char *s1, const char *s2) {
    if (!s1 || !s2) return -1;
    while (*s1 && *s2) {
        char c1 = (char)toupper((unsigned char)*s1);
        char c2 = (char)toupper((unsigned char)*s2);
        if (c1 != c2) return (c1 < c2) ? -1 : 1;
        s1++;
        s2++;
    }
    if (*s1) return 1;
    if (*s2) return -1;
    return 0;
}

/* Periksa apakah string diawali prefix */
static int awalan_str(const char *str, const char *pref) {
    if (!str || !pref) return 0;
    while (*pref) {
        char c1 = (char)toupper((unsigned char)*str);
        char c2 = (char)toupper((unsigned char)*pref);
        if (c1 != c2) return 0;
        str++;
        pref++;
    }
    return 1;
}

/* ========================================================================== *
 * PARSING REFERENSI SEL
 * ========================================================================== */
/* Parsing referensi sel (A1, AA10, Z100) */
int parse_ref_sel(const char *s, int *x, int *y) {
    int i = 0;
    int baris = -1;
    int huruf = 0;
    int kolom = 0;

    if (!s || !x || !y) return -1;

    /* Harus diawali huruf */
    if (!isalpha((unsigned char)s[i])) return -1;

    /* Baca bagian kolom (huruf) */
    while (isalpha((unsigned char)s[i])) {
        char c = (char)toupper((unsigned char)s[i]);
        kolom = kolom * 26 + (c - 'A' + 1);
        i++;
        huruf++;
        if (huruf > 3) break; /* Maksimal 3 huruf */
    }
    kolom -= 1; /* Konversi ke indeks 0-based */

    /* Harus diikuti angka */
    if (!isdigit((unsigned char)s[i])) return -1;

    /* Baca bagian baris (angka) */
    baris = 0;
    while (isdigit((unsigned char)s[i])) {
        baris = baris * 10 + (s[i] - '0');
        i++;
    }

    /* Validasi batas */
    if (kolom < 0 || kolom >= MAKS_KOLOM) return -1;
    if (baris <= 0 || baris > MAKS_BARIS) return -1;

    *x = kolom;
    *y = baris - 1; /* Konversi ke indeks 0-based */
    return i;
}

/* Konversi teks referensi ke koordinat */
static int sel_dari_teks(const char *s, int *x, int *y) {
    return (parse_ref_sel(s, x, y) > 0) ? 0 : -1;
}

/* Konversi teks range ke koordinat */
static int range_dari_teks(const char *s, int *x1, int *y1,
                           int *x2, int *y2) {
    const char *p = strchr(s, ':');
    char buf1[MAKS_TEKS_TOKEN_LOK];
    char buf2[MAKS_TEKS_TOKEN_LOK];
    size_t len1;

    if (!p) return -1;

    len1 = (size_t)(p - s);
    if (len1 >= sizeof(buf1)) return -1;

    strncpy(buf1, s, len1);
    buf1[len1] = '\0';
    salin_str(buf2, p + 1, sizeof(buf2));

    if (sel_dari_teks(buf1, x1, y1) != 0) return -1;
    if (sel_dari_teks(buf2, x2, y2) != 0) return -1;

    return 0;
}

/* ========================================================================== *
 * LEXER (TOKENISASI)
 * ========================================================================== */
/* Lexer: ubah string formula menjadi token */
static int lexer(const char *ekspresi, struct token *keluaran, int maks) {
    int i = 0;
    int n = 0;
    int harap_unary = 1;

    while (ekspresi[i] && n < maks) {
        unsigned char c = (unsigned char)ekspresi[i];

        /* Lewati spasi */
        if (isspace(c)) {
            i++;
            continue;
        }

        /* Kurung buka */
        if (c == '(') {
            keluaran[n].jenis = TOK_KURUNG_BUKA;
            keluaran[n].teks[0] = '(';
            keluaran[n].teks[1] = '\0';
            n++;
            i++;
            harap_unary = 1;
            continue;
        }

        /* Kurung tutup */
        if (c == ')') {
            keluaran[n].jenis = TOK_KURUNG_TUTUP;
            keluaran[n].teks[0] = ')';
            keluaran[n].teks[1] = '\0';
            n++;
            i++;
            harap_unary = 0;
            continue;
        }

        /* Koma pemisah argumen */
        if (c == ',') {
            keluaran[n].jenis = TOK_KOMA;
            keluaran[n].teks[0] = ',';
            keluaran[n].teks[1] = '\0';
            n++;
            i++;
            harap_unary = 1;
            continue;
        }

        /* Titik koma sebagai pemisah alternatif */
        if (c == ';') {
            keluaran[n].jenis = TOK_KOMA;
            keluaran[n].teks[0] = ';';
            keluaran[n].teks[1] = '\0';
            n++;
            i++;
            harap_unary = 1;
            continue;
        }

        /* String literal */
        if (c == '"') {
            int j = 0;
            i++; /* Lewati tanda kutip pembuka */
            keluaran[n].jenis = TOK_TEKS;
            while (ekspresi[i] && ekspresi[i] != '"' &&
                   j < MAKS_TEKS_TOKEN_LOK - 1) {
                keluaran[n].teks[j++] = ekspresi[i++];
            }
            keluaran[n].teks[j] = '\0';
            if (ekspresi[i] == '"') i++;
            n++;
            harap_unary = 0;
            continue;
        }

        /* Angka atau angka negatif di awal */
        if (isdigit(c) ||
            (c == '.' && isdigit((unsigned char)ekspresi[i+1]))) {
            char buf[MAKS_TEKS_TOKEN_LOK];
            int j = 0;
            int ada_titik = 0;

            while ((isdigit((unsigned char)ekspresi[i]) ||
                    ekspresi[i] == '.') && j < MAKS_TEKS_TOKEN_LOK - 1) {
                if (ekspresi[i] == '.') {
                    if (ada_titik) break;
                    ada_titik = 1;
                }
                buf[j++] = ekspresi[i++];
            }
            buf[j] = '\0';
            keluaran[n].jenis = TOK_ANGKA;
            keluaran[n].angka = atof(buf);
            keluaran[n].teks[0] = '\0';
            n++;
            harap_unary = 0;
            continue;
        }

        /* Unary minus atau plus */
        if ((c == '-' || c == '+') && harap_unary) {
            char buf[MAKS_TEKS_TOKEN_LOK];
            int j = 0;
            int ada_titik = 0;

            if (c == '-') buf[j++] = '-';
            i++;

            while ((isdigit((unsigned char)ekspresi[i]) ||
                    ekspresi[i] == '.') && j < MAKS_TEKS_TOKEN_LOK - 1) {
                if (ekspresi[i] == '.') {
                    if (ada_titik) break;
                    ada_titik = 1;
                }
                buf[j++] = ekspresi[i++];
            }
            buf[j] = '\0';

            if (j > 0) { /* Ada angka setelah tanda */
                keluaran[n].jenis = TOK_ANGKA;
                keluaran[n].angka = atof(buf);
                keluaran[n].teks[0] = '\0';
                n++;
                harap_unary = 0;
            } else { /* Tidak ada angka, ini operator */
                keluaran[n].jenis = TOK_OP;
                keluaran[n].teks[0] = (char)c;
                keluaran[n].teks[1] = '\0';
                n++;
                harap_unary = 1;
            }
            continue;
        }

        /* Huruf: bisa jadi referensi sel, range, atau fungsi */
        if (isalpha(c)) {
            int px, py;
            int k = parse_ref_sel(ekspresi + i, &px, &py);

            if (k > 0) {
                /* Cek apakah ini range (A1:B2) */
                if (ekspresi[i + k] == ':' &&
                    parse_ref_sel(ekspresi + i + k + 1, &px, &py) > 0) {
                    int k2 = parse_ref_sel(ekspresi + i + k + 1, &px, &py);
                    keluaran[n].jenis = TOK_RANGE;
                    salin_str(keluaran[n].teks, ekspresi + i,
                              MAKS_TEKS_TOKEN_LOK);
                    n++;
                    i += k + 1 + k2;
                } else {
                    /* Referensi sel tunggal */
                    keluaran[n].jenis = TOK_SEL;
                    salin_str(keluaran[n].teks, ekspresi + i,
                              MAKS_TEKS_TOKEN_LOK);
                    n++;
                    i += k;
                }
            } else {
                /* Fungsi */
                int j = 0;
                keluaran[n].jenis = TOK_FUNGSI;
                while ((isalpha((unsigned char)ekspresi[i]) ||
                        isdigit((unsigned char)ekspresi[i]) ||
                        ekspresi[i] == '_') &&
                       j < MAKS_TEKS_TOKEN_LOK - 1) {
                    keluaran[n].teks[j++] = (char)toupper(
                        (unsigned char)ekspresi[i]);
                    i++;
                }
                keluaran[n].teks[j] = '\0';
                n++;
            }
            harap_unary = 0;
            continue;
        }

        /* Operator */
        if (strchr("+-*/^><=!", (char)c)) {
            char op[3];
            int j = 0;
            op[j++] = (char)c;

            /* Cek operator gabungan */
            if ((c == '>' || c == '<' || c == '!') &&
                ekspresi[i + 1] == '=') {
                op[j++] = '=';
                i += 2;
            } else if (c == '<' && ekspresi[i + 1] == '>') {
                op[j++] = '>';
                i += 2;
            } else {
                i++;
            }
            op[j] = '\0';

            keluaran[n].jenis = TOK_OP;
            salin_str(keluaran[n].teks, op, MAKS_TEKS_TOKEN_LOK);
            n++;
            harap_unary = 1;
            continue;
        }

        /* Karakter tidak dikenal */
        return -1;
    }

    /* Tambahkan token akhir */
    if (n < maks) {
        keluaran[n].jenis = TOK_AKHIR;
        keluaran[n].teks[0] = '\0';
        n++;
    }

    return n;
}

/* ========================================================================== *
 * PARSER (SHUNTING-YARD)
 * ========================================================================== */
/* Prioritas operator */
static int prioritas_op(const char *op) {
    if (strcmp(op, "^") == 0) return 4;
    if (strcmp(op, "*") == 0 || strcmp(op, "/") == 0) return 3;
    if (strcmp(op, "+") == 0 || strcmp(op, "-") == 0) return 2;
    if (strcmp(op, ">") == 0 || strcmp(op, "<") == 0 ||
        strcmp(op, ">=") == 0 || strcmp(op, "<=") == 0 ||
        strcmp(op, "=") == 0 || strcmp(op, "<>") == 0 ||
        strcmp(op, "!=") == 0) return 1;
    return 0;
}

/* Apakah operator asosiatif kanan */
static int asosiatif_kanan(const char *op) {
    return strcmp(op, "^") == 0;
}

/* Konversi infix ke RPN */
static int ke_rpn(struct token *masuk, int jumlah_masuk, struct rpn *keluar) {
    struct token tumpukan[MAKS_TOKEN_LOKAL];
    int puncak = 0;
    int i;

    keluar->n = 0;

    for (i = 0; i < jumlah_masuk; i++) {
        struct token t = masuk[i];

        /* Operand: langsung ke output */
        if (t.jenis == TOK_ANGKA || t.jenis == TOK_TEKS ||
            t.jenis == TOK_SEL || t.jenis == TOK_RANGE) {
            keluar->t[keluar->n++] = t;
            continue;
        }

        /* Fungsi: dorong ke tumpukan */
        if (t.jenis == TOK_FUNGSI) {
            tumpukan[puncak++] = t;
            continue;
        }

        /* Koma: pop sampai kurung buka */
        if (t.jenis == TOK_KOMA) {
            while (puncak > 0 &&
                   tumpukan[puncak - 1].jenis != TOK_KURUNG_BUKA) {
                keluar->t[keluar->n++] = tumpukan[--puncak];
            }
            if (puncak <= 0) return -1;
            continue;
        }

        /* Operator */
        if (t.jenis == TOK_OP) {
            while (puncak > 0 &&
                   tumpukan[puncak - 1].jenis == TOK_OP) {
                int p1 = prioritas_op(tumpukan[puncak - 1].teks);
                int p2 = prioritas_op(t.teks);

                if ((p1 > p2) || (p1 == p2 && !asosiatif_kanan(t.teks))) {
                    keluar->t[keluar->n++] = tumpukan[--puncak];
                } else {
                    break;
                }
            }
            tumpukan[puncak++] = t;
            continue;
        }

        /* Kurung buka */
        if (t.jenis == TOK_KURUNG_BUKA) {
            tumpukan[puncak++] = t;
            continue;
        }

        /* Kurung tutup */
        if (t.jenis == TOK_KURUNG_TUTUP) {
            while (puncak > 0 &&
                   tumpukan[puncak - 1].jenis != TOK_KURUNG_BUKA) {
                keluar->t[keluar->n++] = tumpukan[--puncak];
            }
            if (puncak <= 0) return -1;
            puncak--; /* Buang kurung buka */

            /* Jika ada fungsi, pop juga */
            if (puncak > 0 &&
                tumpukan[puncak - 1].jenis == TOK_FUNGSI) {
                keluar->t[keluar->n++] = tumpukan[--puncak];
            }
            continue;
        }

        /* Token akhir */
        if (t.jenis == TOK_AKHIR) break;

        return -1;
    }

    /* Pop sisa tumpukan */
    while (puncak > 0) {
        if (tumpukan[puncak - 1].jenis == TOK_KURUNG_BUKA) return -1;
        keluar->t[keluar->n++] = tumpukan[--puncak];
    }

    return 0;
}

/* ========================================================================== *
 * EVALUATOR RPN
 * ========================================================================== */
/* Ambil nilai dari sebuah sel */
static int ambil_nilai_sel(const struct buffer_tabel *buf, int x, int y,
                           struct nilai_int *keluaran) {
    const char *t;
    char *endp;
    double v;

    if (!buf || !keluaran) return -1;

    /* Di luar batas */
    if (x < 0 || x >= buf->cfg.kolom || y < 0 || y >= buf->cfg.baris) {
        keluaran->jenis = NILAI_TEKS;
        keluaran->teks[0] = '\0';
        keluaran->angka = 0.0;
        return 0;
    }

    t = buf->isi[y][x];

    /* Sel kosong */
    if (t[0] == '\0') {
        keluaran->jenis = NILAI_ANGKA;
        keluaran->angka = 0.0;
        keluaran->teks[0] = '\0';
        return 0;
    }

    /* Formula: evaluasi rekursif */
    if (t[0] == '=') {
        int adalah_num;
        char err[64];
        if (evaluasi_string(buf, x, y, t, &v, &adalah_num, err,
                            sizeof(err)) == 0 && adalah_num) {
            keluaran->jenis = NILAI_ANGKA;
            keluaran->angka = v;
            keluaran->teks[0] = '\0';
        } else {
            keluaran->jenis = NILAI_TEKS;
            keluaran->teks[0] = '\0';
            keluaran->angka = 0.0;
        }
        return 0;
    }

    /* Coba parse sebagai angka */
    endp = NULL;
    v = strtod(t, &endp);
    if (endp && endp != t && *endp == '\0') {
        keluaran->jenis = NILAI_ANGKA;
        keluaran->angka = v;
        keluaran->teks[0] = '\0';
    } else {
        keluaran->jenis = NILAI_TEKS;
        salin_str(keluaran->teks, t, MAKS_TEKS);
        keluaran->angka = 0.0;
    }

    return 0;
}

/* Evaluasi fungsi range */
static int eval_fungsi_range(const struct buffer_tabel *buf,
                             const char *nama, const char *range,
                             double *keluaran) {
    int x1, y1, x2, y2;

    if (!buf || !nama || !range || !keluaran) return -1;

    if (range_dari_teks(range, &x1, &y1, &x2, &y2) != 0) return -1;

    /* Dispatch ke hitung.c */
    return hitung_agregat(buf, nama, range, keluaran);
}

/* Evaluasi RPN */
static int eval_rpn(const struct buffer_tabel *buf, int x, int y,
                    const struct rpn *r, double *keluaran,
                    int *adalah_angka, char *buf_galat, size_t panjang_galat) {
    struct nilai_int stak[MAKS_STAK_NILAI_LOK];
    int puncak = 0;
    int i;
    char teks_tmp[MAKS_TEKS];
    char teks_tmp2[MAKS_TEKS];

    (void)x;
    (void)y;

    for (i = 0; i < r->n; i++) {
        struct token t = r->t[i];

        /* Angka */
        if (t.jenis == TOK_ANGKA) {
            stak[puncak].jenis = NILAI_ANGKA;
            stak[puncak].angka = t.angka;
            stak[puncak].teks[0] = '\0';
            puncak++;
            continue;
        }

        /* Teks */
        if (t.jenis == TOK_TEKS) {
            stak[puncak].jenis = NILAI_TEKS;
            salin_str(stak[puncak].teks, t.teks, MAKS_TEKS);
            stak[puncak].angka = 0.0;
            puncak++;
            continue;
        }

        /* Referensi sel */
        if (t.jenis == TOK_SEL) {
            int cx, cy;
            struct nilai_int v;

            if (sel_dari_teks(t.teks, &cx, &cy) != 0) {
                if (buf_galat && panjang_galat > 0) {
                    snprintf(buf_galat, panjang_galat,
                             "Referensi tidak valid: %s", t.teks);
                }
                return -1;
            }
            ambil_nilai_sel(buf, cx, cy, &v);
            stak[puncak++] = v;
            continue;
        }

        /* Range */
        if (t.jenis == TOK_RANGE) {
            stak[puncak].jenis = NILAI_TEKS;
            salin_str(stak[puncak].teks, t.teks, MAKS_TEKS);
            stak[puncak].angka = 0.0;
            puncak++;
            continue;
        }

        /* Operator */
        if (t.jenis == TOK_OP) {
            double a, b, hasil;

            if (puncak < 2) {
                if (buf_galat && panjang_galat > 0) {
                    salin_str(buf_galat,
                              "Operand tidak cukup", panjang_galat);
                }
                return -1;
            }

            b = (stak[puncak - 1].jenis == NILAI_ANGKA) ?
                stak[puncak - 1].angka : 0.0;
            a = (stak[puncak - 2].jenis == NILAI_ANGKA) ?
                stak[puncak - 2].angka : 0.0;
            puncak -= 2;

            if (strcmp(t.teks, "+") == 0) {
                hasil = a + b;
            } else if (strcmp(t.teks, "-") == 0) {
                hasil = a - b;
            } else if (strcmp(t.teks, "*") == 0) {
                hasil = a * b;
            } else if (strcmp(t.teks, "/") == 0) {
                if (b == 0.0) {
                    if (buf_galat && panjang_galat > 0) {
                        salin_str(buf_galat, "Pembagian nol",
                                  panjang_galat);
                    }
                    return -1;
                }
                hasil = a / b;
            } else if (strcmp(t.teks, "^") == 0) {
                hasil = pow(a, b);
            } else if (strcmp(t.teks, "=") == 0) {
                hasil = (a == b) ? 1.0 : 0.0;
            } else if (strcmp(t.teks, "<>") == 0 ||
                       strcmp(t.teks, "!=") == 0) {
                hasil = (a != b) ? 1.0 : 0.0;
            } else if (strcmp(t.teks, ">") == 0) {
                hasil = (a > b) ? 1.0 : 0.0;
            } else if (strcmp(t.teks, "<") == 0) {
                hasil = (a < b) ? 1.0 : 0.0;
            } else if (strcmp(t.teks, ">=") == 0) {
                hasil = (a >= b) ? 1.0 : 0.0;
            } else if (strcmp(t.teks, "<=") == 0) {
                hasil = (a <= b) ? 1.0 : 0.0;
            } else {
                if (buf_galat && panjang_galat > 0) {
                    snprintf(buf_galat, panjang_galat,
                             "Operator tidak dikenal: %s", t.teks);
                }
                return -1;
            }

            stak[puncak].jenis = NILAI_ANGKA;
            stak[puncak].angka = hasil;
            stak[puncak].teks[0] = '\0';
            puncak++;
            continue;
        }

        /* Fungsi */
        if (t.jenis == TOK_FUNGSI) {
            double hasil;

            /* Fungsi tanpa argumen */
            if (banding_str(t.teks, "TODAY") == 0) {
                if (waktu_fn_today(&hasil) != 0) {
                    if (buf_galat && panjang_galat > 0) {
                        salin_str(buf_galat, "Gagal mendapat tanggal",
                                  panjang_galat);
                    }
                    return -1;
                }
                stak[puncak].jenis = NILAI_ANGKA;
                stak[puncak].angka = hasil;
                stak[puncak].teks[0] = '\0';
                puncak++;
                continue;
            }

            if (banding_str(t.teks, "NOW") == 0) {
                if (waktu_fn_now(&hasil) != 0) {
                    if (buf_galat && panjang_galat > 0) {
                        salin_str(buf_galat, "Gagal mendapat waktu",
                                  panjang_galat);
                    }
                    return -1;
                }
                stak[puncak].jenis = NILAI_ANGKA;
                stak[puncak].angka = hasil;
                stak[puncak].teks[0] = '\0';
                puncak++;
                continue;
            }

            if (banding_str(t.teks, "RAND") == 0 ||
                banding_str(t.teks, "RANDOM") == 0) {
                hasil = (double)rand() / (double)RAND_MAX;
                stak[puncak].jenis = NILAI_ANGKA;
                stak[puncak].angka = hasil;
                stak[puncak].teks[0] = '\0';
                puncak++;
                continue;
            }

            if (banding_str(t.teks, "PI") == 0) {
                stak[puncak].jenis = NILAI_ANGKA;
                stak[puncak].angka = 3.14159265358979323846;
                stak[puncak].teks[0] = '\0';
                puncak++;
                continue;
            }

            if (banding_str(t.teks, "TRUE") == 0) {
                stak[puncak].jenis = NILAI_ANGKA;
                stak[puncak].angka = 1.0;
                stak[puncak].teks[0] = '\0';
                puncak++;
                continue;
            }

            if (banding_str(t.teks, "FALSE") == 0) {
                stak[puncak].jenis = NILAI_ANGKA;
                stak[puncak].angka = 0.0;
                stak[puncak].teks[0] = '\0';
                puncak++;
                continue;
            }

            /* Fungsi dengan argumen */
            if (puncak < 1) {
                if (buf_galat && panjang_galat > 0) {
                    snprintf(buf_galat, panjang_galat,
                             "Argumen kurang untuk: %s", t.teks);
                }
                return -1;
            }

            /* Fungsi range */
            if (stak[puncak - 1].jenis == NILAI_TEKS) {
                if (eval_fungsi_range(buf, t.teks, stak[puncak-1].teks,
                                      &hasil) == 0) {
                    puncak--;
                    stak[puncak].jenis = NILAI_ANGKA;
                    stak[puncak].angka = hasil;
                    stak[puncak].teks[0] = '\0';
                    puncak++;
                    continue;
                }
            }

            /* Fungsi math 1 argumen */
            {
                double arg1 = (stak[puncak - 1].jenis == NILAI_ANGKA) ?
                              stak[puncak - 1].angka : 0.0;

                if (hitung_math_1_arg(t.teks, arg1, &hasil) == 0) {
                    puncak--;
                    stak[puncak].jenis = NILAI_ANGKA;
                    stak[puncak].angka = hasil;
                    stak[puncak].teks[0] = '\0';
                    puncak++;
                    continue;
                }
            }

            /* Fungsi teks 1 argumen */
            if (stak[puncak - 1].jenis == NILAI_TEKS ||
                stak[puncak - 1].jenis == NILAI_ANGKA) {
                const char *arg_str = (stak[puncak-1].jenis == NILAI_TEKS) ?
                                       stak[puncak-1].teks : "";

                if (stak[puncak - 1].jenis == NILAI_ANGKA) {
                    snprintf(teks_tmp, sizeof(teks_tmp), "%.10g",
                             stak[puncak - 1].angka);
                    arg_str = teks_tmp;
                }

                /* LEN */
                if (banding_str(t.teks, "LEN") == 0 ||
                    banding_str(t.teks, "LENGTH") == 0) {
                    puncak--;
                    teks_len(arg_str, &hasil);
                    stak[puncak].jenis = NILAI_ANGKA;
                    stak[puncak].angka = hasil;
                    stak[puncak].teks[0] = '\0';
                    puncak++;
                    continue;
                }

                /* UPPER */
                if (banding_str(t.teks, "UPPER") == 0 ||
                    banding_str(t.teks, "UCASE") == 0) {
                    puncak--;
                    teks_upper(arg_str, stak[puncak].teks, MAKS_TEKS);
                    stak[puncak].jenis = NILAI_TEKS;
                    stak[puncak].angka = 0.0;
                    puncak++;
                    continue;
                }

                /* LOWER */
                if (banding_str(t.teks, "LOWER") == 0 ||
                    banding_str(t.teks, "LCASE") == 0) {
                    puncak--;
                    teks_lower(arg_str, stak[puncak].teks, MAKS_TEKS);
                    stak[puncak].jenis = NILAI_TEKS;
                    stak[puncak].angka = 0.0;
                    puncak++;
                    continue;
                }

                /* TRIM */
                if (banding_str(t.teks, "TRIM") == 0) {
                    puncak--;
                    teks_trim(arg_str, stak[puncak].teks, MAKS_TEKS);
                    stak[puncak].jenis = NILAI_TEKS;
                    stak[puncak].angka = 0.0;
                    puncak++;
                    continue;
                }

                /* VALUE */
                if (banding_str(t.teks, "VALUE") == 0 ||
                    banding_str(t.teks, "NUMBER") == 0) {
                    puncak--;
                    if (teks_value(arg_str, &hasil) == 0) {
                        stak[puncak].jenis = NILAI_ANGKA;
                        stak[puncak].angka = hasil;
                    } else {
                        stak[puncak].jenis = NILAI_ANGKA;
                        stak[puncak].angka = 0.0;
                    }
                    stak[puncak].teks[0] = '\0';
                    puncak++;
                    continue;
                }

                /* ISNUMBER */
                if (banding_str(t.teks, "ISNUMBER") == 0 ||
                    banding_str(t.teks, "ISNUM") == 0) {
                    puncak--;
                    teks_isnumber(arg_str, &hasil);
                    stak[puncak].jenis = NILAI_ANGKA;
                    stak[puncak].angka = hasil;
                    stak[puncak].teks[0] = '\0';
                    puncak++;
                    continue;
                }

                /* ISTEXT */
                if (banding_str(t.teks, "ISTEXT") == 0 ||
                    banding_str(t.teks, "ISTEKS") == 0) {
                    puncak--;
                    teks_istext(arg_str, &hasil);
                    stak[puncak].jenis = NILAI_ANGKA;
                    stak[puncak].angka = hasil;
                    stak[puncak].teks[0] = '\0';
                    puncak++;
                    continue;
                }

                /* ISBLANK */
                if (banding_str(t.teks, "ISBLANK") == 0 ||
                    banding_str(t.teks, "ISKOSONG") == 0) {
                    puncak--;
                    teks_isblank(arg_str, &hasil);
                    stak[puncak].jenis = NILAI_ANGKA;
                    stak[puncak].angka = hasil;
                    stak[puncak].teks[0] = '\0';
                    puncak++;
                    continue;
                }
            }

            /* Fungsi dengan 2 argumen */
            if (puncak >= 2) {
                double arg1 = (stak[puncak - 1].jenis == NILAI_ANGKA) ?
                              stak[puncak - 1].angka : 0.0;
                double arg2 = (stak[puncak - 2].jenis == NILAI_ANGKA) ?
                              stak[puncak - 2].angka : 0.0;

                /* Fungsi math 2 argumen */
                if (hitung_math_2_arg(t.teks, arg2, arg1, &hasil) == 0) {
                    puncak -= 2;
                    stak[puncak].jenis = NILAI_ANGKA;
                    stak[puncak].angka = hasil;
                    stak[puncak].teks[0] = '\0';
                    puncak++;
                    continue;
                }

                /* IF */
                if (banding_str(t.teks, "IF") == 0) {
                    puncak -= 2;
                    hitung_if(arg2, arg1, 0.0, &hasil);
                    stak[puncak].jenis = NILAI_ANGKA;
                    stak[puncak].angka = hasil;
                    stak[puncak].teks[0] = '\0';
                    puncak++;
                    continue;
                }

                /* DATE */
                if (banding_str(t.teks, "DATE") == 0) {
                    if (puncak >= 3) {
                        double arg3 = (stak[puncak-3].jenis == NILAI_ANGKA) ?
                                      stak[puncak-3].angka : 0.0;
                        puncak -= 3;
                        if (waktu_fn_date(arg3, arg2, arg1, &hasil) == 0) {
                            stak[puncak].jenis = NILAI_ANGKA;
                            stak[puncak].angka = hasil;
                        } else {
                            stak[puncak].jenis = NILAI_ANGKA;
                            stak[puncak].angka = 0.0;
                        }
                        stak[puncak].teks[0] = '\0';
                        puncak++;
                        continue;
                    }
                }

                /* TIME */
                if (banding_str(t.teks, "TIME") == 0) {
                    if (puncak >= 3) {
                        double arg3 = (stak[puncak-3].jenis == NILAI_ANGKA) ?
                                      stak[puncak-3].angka : 0.0;
                        puncak -= 3;
                        waktu_fn_time(arg3, arg2, arg1, &hasil);
                        stak[puncak].jenis = NILAI_ANGKA;
                        stak[puncak].angka = hasil;
                        stak[puncak].teks[0] = '\0';
                        puncak++;
                        continue;
                    }
                }

                /* LEFT */
                if (banding_str(t.teks, "LEFT") == 0) {
                    const char *str_arg = (stak[puncak-2].jenis==NILAI_TEKS) ?
                                           stak[puncak-2].teks : "";
                    if (stak[puncak - 2].jenis == NILAI_ANGKA) {
                        snprintf(teks_tmp, sizeof(teks_tmp), "%.10g",
                                 stak[puncak - 2].angka);
                        str_arg = teks_tmp;
                    }
                    puncak -= 2;
                    teks_left(str_arg, arg1, stak[puncak].teks, MAKS_TEKS);
                    stak[puncak].jenis = NILAI_TEKS;
                    stak[puncak].angka = 0.0;
                    puncak++;
                    continue;
                }

                /* RIGHT */
                if (banding_str(t.teks, "RIGHT") == 0) {
                    const char *str_arg = (stak[puncak-2].jenis==NILAI_TEKS) ?
                                           stak[puncak-2].teks : "";
                    if (stak[puncak - 2].jenis == NILAI_ANGKA) {
                        snprintf(teks_tmp, sizeof(teks_tmp), "%.10g",
                                 stak[puncak - 2].angka);
                        str_arg = teks_tmp;
                    }
                    puncak -= 2;
                    teks_right(str_arg, arg1, stak[puncak].teks, MAKS_TEKS);
                    stak[puncak].jenis = NILAI_TEKS;
                    stak[puncak].angka = 0.0;
                    puncak++;
                    continue;
                }

                /* FIND */
                if (banding_str(t.teks, "FIND") == 0 ||
                    banding_str(t.teks, "SEARCH") == 0) {
                    const char *str_arg = (stak[puncak-2].jenis==NILAI_TEKS) ?
                                           stak[puncak-2].teks : "";
                    const char *cari_arg = (stak[puncak-1].jenis==NILAI_TEKS) ?
                                            stak[puncak-1].teks : "";
                    puncak -= 2;
                    teks_find(cari_arg, str_arg, 1.0, &hasil);
                    stak[puncak].jenis = NILAI_ANGKA;
                    stak[puncak].angka = hasil;
                    stak[puncak].teks[0] = '\0';
                    puncak++;
                    continue;
                }

                /* DATEDIF */
                if (banding_str(t.teks, "DATEDIF") == 0) {
                    if (puncak >= 3 &&
                        stak[puncak - 1].jenis == NILAI_TEKS) {
                        double arg3 = (stak[puncak-3].jenis==NILAI_ANGKA) ?
                                      stak[puncak-3].angka : 0.0;
                        const char *unit = stak[puncak-1].teks;
                        puncak -= 3;
                        waktu_fn_datedif(arg3, arg2, unit, &hasil);
                        stak[puncak].jenis = NILAI_ANGKA;
                        stak[puncak].angka = hasil;
                        stak[puncak].teks[0] = '\0';
                        puncak++;
                        continue;
                    }
                }
            }

            /* Fungsi dengan 3 argumen */
            if (puncak >= 3) {
                double arg1 = (stak[puncak - 1].jenis == NILAI_ANGKA) ?
                              stak[puncak - 1].angka : 0.0;
                double arg2 = (stak[puncak - 2].jenis == NILAI_ANGKA) ?
                              stak[puncak - 2].angka : 0.0;
                double arg3 = (stak[puncak - 3].jenis == NILAI_ANGKA) ?
                              stak[puncak - 3].angka : 0.0;

                /* IF dengan 3 argumen */
                if (banding_str(t.teks, "IF") == 0) {
                    puncak -= 3;
                    hitung_if(arg3, arg2, arg1, &hasil);
                    stak[puncak].jenis = NILAI_ANGKA;
                    stak[puncak].angka = hasil;
                    stak[puncak].teks[0] = '\0';
                    puncak++;
                    continue;
                }

                /* MID */
                if (banding_str(t.teks, "MID") == 0 ||
                    banding_str(t.teks, "SUBSTR") == 0 ||
                    banding_str(t.teks, "SUBSTRING") == 0) {
                    const char *str_arg = (stak[puncak-3].jenis==NILAI_TEKS) ?
                                           stak[puncak-3].teks : "";
                    if (stak[puncak - 3].jenis == NILAI_ANGKA) {
                        snprintf(teks_tmp, sizeof(teks_tmp), "%.10g",
                                 stak[puncak - 3].angka);
                        str_arg = teks_tmp;
                    }
                    puncak -= 3;
                    teks_mid(str_arg, arg2, arg1, stak[puncak].teks, MAKS_TEKS);
                    stak[puncak].jenis = NILAI_TEKS;
                    stak[puncak].angka = 0.0;
                    puncak++;
                    continue;
                }

                /* REPLACE */
                if (banding_str(t.teks, "REPLACE") == 0) {
                    const char *str_arg = (stak[puncak-3].jenis==NILAI_TEKS) ?
                                           stak[puncak-3].teks : "";
                    const char *new_str = (stak[puncak-1].jenis==NILAI_TEKS) ?
                                           stak[puncak-1].teks : "";
                    if (stak[puncak - 3].jenis == NILAI_ANGKA) {
                        snprintf(teks_tmp, sizeof(teks_tmp), "%.10g",
                                 stak[puncak - 3].angka);
                        str_arg = teks_tmp;
                    }
                    puncak -= 3;
                    teks_replace(str_arg, arg2, arg1, new_str,
                                 stak[puncak].teks, MAKS_TEKS);
                    stak[puncak].jenis = NILAI_TEKS;
                    stak[puncak].angka = 0.0;
                    puncak++;
                    continue;
                }

                /* TEXT */
                if (banding_str(t.teks, "TEXT") == 0) {
                    const char *fmt_str = (stak[puncak-1].jenis==NILAI_TEKS) ?
                                           stak[puncak-1].teks : "";
                    puncak -= 2;
                    teks_format(arg2, fmt_str, stak[puncak].teks, MAKS_TEKS);
                    stak[puncak].jenis = NILAI_TEKS;
                    stak[puncak].angka = 0.0;
                    puncak++;
                    continue;
                }
            }

            /* Fungsi dengan 4 argumen */
            if (puncak >= 4) {
                /* SUBSTITUTE */
                if (banding_str(t.teks, "SUBSTITUTE") == 0) {
                    const char *teks_arg = (stak[puncak-4].jenis==NILAI_TEKS) ?
                                            stak[puncak-4].teks : "";
                    const char *cari_arg = (stak[puncak-3].jenis==NILAI_TEKS) ?
                                            stak[puncak-3].teks : "";
                    const char *ganti_arg = (stak[puncak-2].jenis==NILAI_TEKS) ?
                                             stak[puncak-2].teks : "";
                    puncak -= 4;
                    teks_substitute(teks_arg, cari_arg, ganti_arg,
                                    stak[puncak].teks, MAKS_TEKS);
                    stak[puncak].jenis = NILAI_TEKS;
                    stak[puncak].angka = 0.0;
                    puncak++;
                    continue;
                }

                /* CONCATENATE */
                if (banding_str(t.teks, "CONCATENATE") == 0 ||
                    banding_str(t.teks, "CONCAT") == 0) {
                    const char *s1 = (stak[puncak-4].jenis == NILAI_TEKS) ?
                                      stak[puncak-4].teks : "";
                    const char *s2 = (stak[puncak-3].jenis == NILAI_TEKS) ?
                                      stak[puncak-3].teks : "";
                    const char *s3 = (stak[puncak-2].jenis == NILAI_TEKS) ?
                                      stak[puncak-2].teks : "";
                    const char *s4 = (stak[puncak-1].jenis == NILAI_TEKS) ?
                                      stak[puncak-1].teks : "";
                    char tmp1[MAKS_TEKS], tmp2[MAKS_TEKS];

                    if (stak[puncak - 4].jenis == NILAI_ANGKA) {
                        snprintf(tmp1, sizeof(tmp1), "%.10g",
                                 stak[puncak - 4].angka);
                        s1 = tmp1;
                    }
                    if (stak[puncak - 3].jenis == NILAI_ANGKA) {
                        snprintf(tmp2, sizeof(tmp2), "%.10g",
                                 stak[puncak - 3].angka);
                        s2 = tmp2;
                    }

                    teks_concat(s1, s2, teks_tmp, sizeof(teks_tmp));
                    teks_concat(teks_tmp, s3, teks_tmp2, sizeof(teks_tmp2));
                    puncak -= 4;
                    teks_concat(teks_tmp2, s4, stak[puncak].teks, MAKS_TEKS);
                    stak[puncak].jenis = NILAI_TEKS;
                    stak[puncak].angka = 0.0;
                    puncak++;
                    continue;
                }
            }

            /* Fungsi tidak dikenal */
            if (buf_galat && panjang_galat > 0) {
                snprintf(buf_galat, panjang_galat,
                         "Fungsi tidak dikenal: %s", t.teks);
            }
            return -1;
        }

        /* Token akhir */
        if (t.jenis == TOK_AKHIR) break;

        /* Token tidak dikenal */
        if (buf_galat && panjang_galat > 0) {
            salin_str(buf_galat, "Token tidak dikenal", panjang_galat);
        }
        return -1;
    }

    /* Hasil harus tepat 1 nilai */
    if (puncak != 1) {
        if (buf_galat && panjang_galat > 0) {
            salin_str(buf_galat, "Ekspresi tidak valid", panjang_galat);
        }
        return -1;
    }

    /* Kembalikan hasil */
    if (stak[0].jenis == NILAI_ANGKA) {
        *keluaran = stak[0].angka;
        *adalah_angka = 1;
    } else {
        *keluaran = 0.0;
        *adalah_angka = 0;
    }

    return 0;
}

/* ========================================================================== *
 * API PUBLIK
 * ========================================================================== */
/* Periksa apakah string adalah formula */
int apakah_string_formula(const char *s) {
    return (s && s[0] == '=') ? 1 : 0;
}

/* Periksa apakah sel adalah formula (placeholder) */
int apakah_sel_formula(int x, int y) {
    (void)x;
    (void)y;
    return 0;
}

/* Evaluasi string formula */
int evaluasi_string(const struct buffer_tabel *buf, int x, int y,
                    const char *ekspresi, double *keluaran,
                    int *adalah_angka, char *buf_galat, size_t panjang_galat) {
    struct token toks[MAKS_TOKEN_LOKAL];
    struct rpn r;
    int jumlah_toks;

    if (!buf || !ekspresi || !keluaran || !adalah_angka) return -1;

    /* Harus diawali '=' */
    if (ekspresi[0] != '=') {
        if (buf_galat && panjang_galat > 0) {
            salin_str(buf_galat, "Bukan formula", panjang_galat);
        }
        *adalah_angka = 0;
        return -1;
    }

    /* Tokenisasi */
    jumlah_toks = lexer(ekspresi + 1, toks, MAKS_TOKEN_LOKAL);
    if (jumlah_toks <= 0) {
        if (buf_galat && panjang_galat > 0) {
            salin_str(buf_galat, "Kesalahan tokenisasi", panjang_galat);
        }
        *adalah_angka = 0;
        return -1;
    }

    /* Konversi ke RPN */
    if (ke_rpn(toks, jumlah_toks, &r) != 0) {
        if (buf_galat && panjang_galat > 0) {
            salin_str(buf_galat, "Kesalahan parsing", panjang_galat);
        }
        *adalah_angka = 0;
        return -1;
    }

    /* Evaluasi RPN */
    if (eval_rpn(buf, x, y, &r, keluaran, adalah_angka, buf_galat,
                 panjang_galat) != 0) {
        *adalah_angka = 0;
        return -1;
    }

    return 0;
}

/* Evaluasi sel */
int evaluasi_sel(const struct buffer_tabel *buf, int x, int y,
                 double *keluaran, int *adalah_angka, int kedalaman) {
    const char *t;
    char galat[64];

    if (!buf || !keluaran || !adalah_angka) return -1;

    /* Cegah rekursi */
    if (kedalaman > MAKS_KEDALAMAN_LOK) {
        *adalah_angka = 0;
        return -1;
    }

    /* Validasi batas */
    if (x < 0 || x >= buf->cfg.kolom ||
        y < 0 || y >= buf->cfg.baris) {
        *adalah_angka = 0;
        return -1;
    }

    t = buf->isi[y][x];
    if (t[0] != '=') return -1;

    return evaluasi_string(buf, x, y, t, keluaran, adalah_angka, galat,
                           sizeof(galat));
}

/* Dapatkan galat terakhir */
const char *dapatkan_galat_terakhir(void) {
    return galat_terakhir;
}

/* Bersihkan galat */
void bersihkan_galat_terakhir(void) {
    galat_terakhir[0] = '\0';
}
