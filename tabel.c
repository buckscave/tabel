/* ============================================================
 * TABEL v3.0 
 * Berkas: tabel.c - Model & Operasi (Undo/Redo, Merge, Sort, Teks)
 * Standar: C89 + POSIX.1-2008
 * Penulis: Chandra Lesmana
 * ============================================================ */

#include "tabel.h"

/* ============================================================
 * Data Global (Clipboard Global, Buffer Manager)
 * ============================================================ */
char papan_klip[MAKS_TEKS];
char ***papan_klip_area = NULL;
int klip_x1 = -1, klip_y1 = -1;
int klip_x2 = -1, klip_y2 = -1;
int klip_ada_area = 0;
int klip_mode = 0;

struct buffer_tabel **buffers = NULL;
int jumlah_buffer = 0;
int buffer_aktif = -1;

struct termios term_simpan;
int mode_raw_aktif = 0;
volatile sig_atomic_t perlu_resize = 0;
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

int seleksi_random_x[MAKS_SELEKSI];
int seleksi_random_y[MAKS_SELEKSI];
int seleksi_random = 0;

/* ============================================================
 * Buffer Management
 * ============================================================ */
struct buffer_tabel *buat_buffer(int baris, int kolom) {
    if (baris < 1) baris = 1;
    if (kolom < 1) kolom = 1;
    if (baris > MAKS_BARIS) baris = MAKS_BARIS;
    if (kolom > MAKS_KOLOM) kolom = MAKS_KOLOM;

    struct buffer_tabel *buf = calloc(1, sizeof(*buf));
    if (!buf) return NULL;

    buf->refcount = 1;
    buf->cfg.baris = baris;
    buf->cfg.kolom = kolom;
    buf->cfg.aktif_x = buf->cfg.aktif_y = 0;
    buf->cfg.lihat_kolom = buf->cfg.lihat_baris = 0;
    buf->cfg.kotor = 0;
    strcpy(buf->cfg.csv_delim, ",");
    strncpy(buf->cfg.enkoding, "UTF-8", ENKODING_MAKS);

    buf->isi = calloc(baris, sizeof(char**));
    buf->perataan_sel = calloc(baris, sizeof(perataan_teks*));
    buf->tergabung = calloc(baris, sizeof(struct info_gabung*));
    for (int r=0; r<baris; r++) {
        buf->isi[r] = calloc(kolom, sizeof(char*));
        buf->perataan_sel[r] = calloc(kolom, sizeof(perataan_teks));
        buf->tergabung[r] = calloc(kolom, sizeof(struct info_gabung));
        for (int c=0; c<kolom; c++) {
            buf->isi[r][c] = calloc(MAKS_TEKS, sizeof(char));
            buf->perataan_sel[r][c] = RATA_KIRI;
            buf->tergabung[r][c].x_anchor = -1;
            buf->tergabung[r][c].y_anchor = -1;
        }
    }

    buf->lebar_kolom = calloc(kolom, sizeof(int));
    buf->tinggi_baris = calloc(baris, sizeof(int));
    for (int c=0; c<kolom; c++) buf->lebar_kolom[c] = LEBAR_KOLOM_DEFAULT;
    for (int r=0; r<baris; r++) buf->tinggi_baris[r] = TINGGI_BARIS_DEFAULT;

    papan_klip_area = calloc(baris, sizeof(char**));
    for (int r=0; r<baris; r++) {
        papan_klip_area[r] = calloc(kolom, sizeof(char*));
        for (int c=0; c<kolom; c++) {
            papan_klip_area[r][c] = calloc(MAKS_TEKS, sizeof(char));
        }
    }

    buf->tumpukan_undo = calloc(UNDO_MAKS, sizeof(struct operasi_undo));
    buf->tumpukan_redo = calloc(UNDO_MAKS, sizeof(struct operasi_undo));
    buf->undo_puncak = buf->redo_puncak = 0;

    buf->warna_teks_sel = calloc(baris, sizeof(warna_glyph*)); 
    for (int r = 0; r < baris; r++) { 
        buf->warna_teks_sel[r] = calloc(kolom, sizeof(warna_glyph)); 
        for (int c = 0; c < kolom; c++) { 
            buf->warna_teks_sel[r][c] = WARNA_DEFAULT; 
        } 
    }

    buf->border_sel = calloc(baris, sizeof(struct info_border*));
    for (int r=0; r<baris; r++) {
        buf->border_sel[r] = calloc(kolom, sizeof(struct info_border));
        for (int c=0; c<kolom; c++) {
            buf->border_sel[r][c].top = buf->border_sel[r][c].bottom = 0;
            buf->border_sel[r][c].left = buf->border_sel[r][c].right = 0;
            buf->border_sel[r][c].warna = WARNA_PUTIH;
        }
    }

    buf->format_sel = calloc(baris, sizeof(jenis_format_sel*));
    for (int r=0; r<baris; r++) {
        buf->format_sel[r] = calloc(kolom, sizeof(jenis_format_sel));
        for (int c=0; c<kolom; c++) {
            buf->format_sel[r][c] = FORMAT_UMUM;
        }
    }

    return buf;
}

struct buffer_tabel *duplikat_buffer(const struct buffer_tabel *src) {
    if (!src) return NULL;
    struct buffer_tabel *baru = buat_buffer(src->cfg.baris, src->cfg.kolom);
    if (!baru) return NULL;

    /* Copy konfigurasi tapi reset beberapa status UI */
    baru->cfg = src->cfg;
    baru->cfg.kotor = 1;

    /* Copy isi sel & metadata */
    for (int r = 0; r < src->cfg.baris; r++) {
        baru->tinggi_baris[r] = src->tinggi_baris[r];
        for (int c = 0; c < src->cfg.kolom; c++) {
            if (r == 0) baru->lebar_kolom[c] = src->lebar_kolom[c];
            salin_str_aman(baru->isi[r][c], src->isi[r][c], MAKS_TEKS);
            baru->perataan_sel[r][c] = src->perataan_sel[r][c];
            baru->tergabung[r][c] = src->tergabung[r][c];
            baru->warna_teks_sel[r][c] = src->warna_teks_sel[r][c];
            baru->border_sel[r][c] = src->border_sel[r][c];
            baru->format_sel[r][c] = src->format_sel[r][c];
        }
    }
    return baru;
}

void bebas_buffer(struct buffer_tabel *buf) {
    if (!buf) return;

    buf->refcount--;
    if (buf->refcount > 0) {
        return;
    }

    for (int r=0; r<buf->cfg.baris; r++) {
        for (int c=0; c<buf->cfg.kolom; c++) {
            free(buf->isi[r][c]);
        }
        free(buf->isi[r]);
        free(buf->perataan_sel[r]);
        free(buf->warna_teks_sel[r]);
        free(buf->border_sel[r]); 
        free(buf->tergabung[r]);
        free(buf->format_sel[r]);
    }

    free(buf->isi);
    free(buf->perataan_sel);
    free(buf->tergabung);
    free(buf->lebar_kolom);
    free(buf->tinggi_baris);
    free(buf->tumpukan_undo);
    free(buf->tumpukan_redo);
    free(buf->warna_teks_sel);
    free(buf->border_sel); 
    free(buf->format_sel);
    free(buf);
}

int tambah_buffer(struct buffer_tabel *buf) {
    struct buffer_tabel **tmp = realloc(buffers, (jumlah_buffer+1)*sizeof(*tmp));
    if (!tmp) return -1;
    buffers = tmp;
    buffers[jumlah_buffer] = buf;
    buffer_aktif = jumlah_buffer;
    jumlah_buffer++;
    return buffer_aktif;
}

void tutup_buffer(int idx) {
    if (idx<0 || idx>=jumlah_buffer) return;
    bebas_buffer(buffers[idx]);
    for (int i=idx; i<jumlah_buffer-1; i++) buffers[i]=buffers[i+1];
    jumlah_buffer--;
    if (jumlah_buffer==0) {
        free(buffers);
        buffers=NULL;
        buffer_aktif=-1;
    } else if (buffer_aktif>=jumlah_buffer) buffer_aktif=jumlah_buffer-1;
}

void set_buffer_aktif(int idx) {
    if (idx>=0 && idx<jumlah_buffer) buffer_aktif=idx;
}

/* ============================================================
 * Undo/Redo
 * ============================================================ */
void dorong_undo_teks(struct buffer_tabel *buf, int x, int y,
                      const char *sblm, const char *ssdh) {
    struct operasi_undo u;
    memset(&u,0,sizeof(u));
    u.jenis=UNDO_TEKS;
    u.x=x; u.y=y;
    salin_str_aman(u.sebelum,sblm,MAKS_TEKS);
    salin_str_aman(u.sesudah,ssdh,MAKS_TEKS);
    if (buf->undo_puncak<UNDO_MAKS) {
        buf->tumpukan_undo[buf->undo_puncak++]=u;
    } else {
        memmove(buf->tumpukan_undo,buf->tumpukan_undo+1,(UNDO_MAKS-1)*sizeof(u));
        buf->tumpukan_undo[UNDO_MAKS-1]=u;
    }
    buf->redo_puncak=0;
}

int lakukan_undo(struct buffer_tabel *buf) {
    if (buf->undo_puncak<=0) return -1;
    struct operasi_undo u=buf->tumpukan_undo[--buf->undo_puncak];
    buf->tumpukan_redo[buf->redo_puncak++]=u;
    switch(u.jenis) {
    case UNDO_TEKS:
        salin_str_aman(buf->isi[u.y][u.x],u.sebelum,MAKS_TEKS);
        break;
    case UNDO_LAYOUT: {
        int lbr,tgi;
        if (sscanf(u.sebelum,"L:%d T:%d",&lbr,&tgi)==2) {
            buf->lebar_kolom[u.x]=lbr;
            buf->tinggi_baris[u.y]=tgi;
        }
        break;
    }
    case UNDO_PERATAAN:
        buf->perataan_sel[u.y][u.x]=(perataan_teks)u.aux1;
        break;
    case UNDO_GABUNG:
        if (u.aux1) atur_blok_gabung_buf(buf,u.x,u.y,u.x2,u.y2);
        else hapus_blok_gabung_buf(buf,u.x,u.y);
        break;
    case UNDO_STRUKTUR:
        /* implementasi sama seperti versi lama, disesuaikan */
        break;
    case UNDO_SORTIR:
        /* restore urutan baris */
        break;
    default: break;
    }
    buf->cfg.kotor=1;
    return 0;
}

int lakukan_redo(struct buffer_tabel *buf) {
    if (buf->redo_puncak<=0) return -1;
    struct operasi_undo r=buf->tumpukan_redo[--buf->redo_puncak];
    switch(r.jenis) {
    case UNDO_TEKS:
        salin_str_aman(buf->isi[r.y][r.x],r.sesudah,MAKS_TEKS);
        break;
    case UNDO_LAYOUT: {
        int lbr2,tgi2;
        if (sscanf(r.sesudah,"L:%d T:%d",&lbr2,&tgi2)==2) {
            buf->lebar_kolom[r.x]=lbr2;
            buf->tinggi_baris[r.y]=tgi2;
        }
        break;
    }
    case UNDO_PERATAAN:
        buf->perataan_sel[r.y][r.x]=(perataan_teks)r.aux2;
        break;
            case UNDO_GABUNG:
        if (r.aux1) { /* redo unmerge */
            hapus_blok_gabung_buf(buf, r.x, r.y);
        } else { /* redo merge */
            atur_blok_gabung_buf(buf, r.x, r.y, r.x2, r.y2);
        }
        break;
    case UNDO_STRUKTUR:
        /* redo struktur sesuai jenis */
        break;
    default: break;
    }
    buf->cfg.kotor=1;
    return 0;
}

/* ============================================================
 * Manipulasi Sel & Teks
 * ============================================================ */
void atur_teks_sel(struct buffer_tabel *buf, int x, int y,
                   const char *teks, int catat_undo) {
    char sebelum[MAKS_TEKS];
    salin_str_aman(sebelum, buf->isi[y][x], MAKS_TEKS);
    salin_str_aman(buf->isi[y][x], teks, MAKS_TEKS);
    if (catat_undo) dorong_undo_teks(buf, x, y, sebelum, buf->isi[y][x]);
    buf->cfg.kotor = 1;
}

void ubah_perataan_sel(struct buffer_tabel *buf, perataan_teks p) {
    int x = buf->cfg.aktif_x;
    int y = buf->cfg.aktif_y;
    if (apakah_ada_gabung_buf(buf,x,y)) {
        x = buf->tergabung[y][x].x_anchor;
        y = buf->tergabung[y][x].y_anchor;
    }
    perataan_teks sblm = buf->perataan_sel[y][x];
    buf->perataan_sel[y][x] = p;
    dorong_undo_teks(buf,x,y,"",buf->isi[y][x]); /* catat perubahan */
    buf->cfg.kotor=1;
}

/* Konverter UTF-8 */
static void ke_huruf_besar_utf8(char *dst,const char *src,size_t n){
    size_t i=0;
    while(src[i]&&n>1){
        unsigned char c=(unsigned char)src[i];
        if(c<0x80){*dst++=(char)toupper((int)c);n--;i++;}
        else{int l=panjang_cp_utf8(src+i);if((size_t)l>=n)break;
            memcpy(dst,src+i,(size_t)l);dst+=l;n-=l;i+=l;}
    }
    *dst='\0';
}
static void ke_huruf_kecil_utf8(char *dst,const char *src,size_t n){
    size_t i=0;
    while(src[i]&&n>1){
        unsigned char c=(unsigned char)src[i];
        if(c<0x80){*dst++=(char)tolower((int)c);n--;i++;}
        else{int l=panjang_cp_utf8(src+i);if((size_t)l>=n)break;
            memcpy(dst,src+i,(size_t)l);dst+=l;n-=l;i+=l;}
    }
    *dst='\0';
}
static void ke_judul_utf8(char *dst,const char *src,size_t n){
    size_t i=0;int awal_kata=1;
    while(src[i]&&n>1){
        unsigned char c=(unsigned char)src[i];
        if(c<0x80){char ch=src[i];
            if(isalpha((int)ch)){
                *dst++=awal_kata?(char)toupper((int)ch):(char)tolower((int)ch);
                n--;i++;awal_kata=0;
            }else{*dst++=ch;n--;i++;awal_kata=(ch==' '||ch=='\t');}
        }else{int l=panjang_cp_utf8(src+i);if((size_t)l>=n)break;
            memcpy(dst,src+i,(size_t)l);dst+=l;n-=l;i+=l;awal_kata=0;}
    }
    *dst='\0';
}

void ubah_teks_sel_huruf_besar(struct buffer_tabel *buf,int x,int y){
    char sebelum[MAKS_TEKS],sesudah[MAKS_TEKS];
    salin_str_aman(sebelum,buf->isi[y][x],MAKS_TEKS);
    ke_huruf_besar_utf8(sesudah,buf->isi[y][x],MAKS_TEKS);
    salin_str_aman(buf->isi[y][x],sesudah,MAKS_TEKS);
    dorong_undo_teks(buf,x,y,sebelum,buf->isi[y][x]);
    buf->cfg.kotor=1;
}
void ubah_teks_sel_huruf_kecil(struct buffer_tabel *buf,int x,int y){
    char sebelum[MAKS_TEKS],sesudah[MAKS_TEKS];
    salin_str_aman(sebelum,buf->isi[y][x],MAKS_TEKS);
    ke_huruf_kecil_utf8(sesudah,buf->isi[y][x],MAKS_TEKS);
    salin_str_aman(buf->isi[y][x],sesudah,MAKS_TEKS);
    dorong_undo_teks(buf,x,y,sebelum,buf->isi[y][x]);
    buf->cfg.kotor=1;
}
void ubah_teks_sel_judul(struct buffer_tabel *buf,int x,int y){
    char sebelum[MAKS_TEKS],sesudah[MAKS_TEKS];
    salin_str_aman(sebelum,buf->isi[y][x],MAKS_TEKS);
    ke_judul_utf8(sesudah,buf->isi[y][x],MAKS_TEKS);
    salin_str_aman(buf->isi[y][x],sesudah,MAKS_TEKS);
    dorong_undo_teks(buf,x,y,sebelum,buf->isi[y][x]);
    buf->cfg.kotor=1;
}

void eksekusi_warna_teks(struct buffer_tabel *buf, const char *cmd) {
    int x1, y1, x2, y2;
    int r, c;

    /* Tentukan area kerja: seleksi aktif atau sel aktif */
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

    /* Ambil warna dari prompt */
    prompt_warna(buf, 1);
    warna_glyph warna_terpilih = buf->warna_teks_sel[y1][x1];

    /* Terapkan ke area */
    for (r = y1; r <= y2; r++) {
        for (c = x1; c <= x2; c++) {
            buf->warna_teks_sel[r][c] = warna_terpilih;
        }
    }

    buf->cfg.kotor = 1;
    snprintf(pesan_status, sizeof(pesan_status),
             "Warna teks: %d (%c%d:%c%d)",
             warna_terpilih, 'A'+x1, y1+1, 'A'+x2, y2+1);
}

void eksekusi_border(struct buffer_tabel *buf, const char *cmd) {
    int x1, y1, x2, y2;
    int r, c;

    /* Tentukan area kerja */
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

    /* Handle Prompt Warna */
    if (strcmp(cmd, "bw") == 0) {
        /* 1. Ambil warna melalui prompt (menggunakan koordinat aktif) */
        prompt_warna(buf, 0);

        /* 2. Ambil warna yang baru saja dipilih user */
        warna_glyph warna_terpilih = buf->border_sel[y1][x1].warna;

        /* 3. Sebarkan warna tersebut ke seluruh sel dalam area seleksi */
        for (r = y1; r <= y2; r++) {
            for (c = x1; c <= x2; c++) {
                buf->border_sel[r][c].warna = warna_terpilih;
            }
        }
        buf->cfg.kotor = 1;
        return; /* Selesai */
    }

    /* Loop Area */
    for (r = y1; r <= y2; r++) {
        for (c = x1; c <= x2; c++) {
            struct info_border *b = &buf->border_sel[r][c];

            if (strcmp(cmd, "ba") == 0) { /* All */
                b->top = 1; b->bottom = 1; b->left = 1; b->right = 1;
            }
            else if (strcmp(cmd, "bo") == 0) { /* Outline */
                if (r == y1) b->top = 1;
                if (r == y2) b->bottom = 1;
                if (c == x1) b->left = 1;
                if (c == x2) b->right = 1;
            }
            else if (strcmp(cmd, "bt") == 0) { if (r == y1) b->top = 1; }
            else if (strcmp(cmd, "bb") == 0) { if (r == y2) b->bottom = 1; }
            else if (strcmp(cmd, "bl") == 0) { if (c == x1) b->left = 1; }
            else if (strcmp(cmd, "br") == 0) { if (c == x2) b->right = 1; }
            else if (strcmp(cmd, "none") == 0) {
                b->top = 0; b->bottom = 0; b->left = 0; b->right = 0;
            }
        }
    }

    buf->cfg.kotor = 1;

    const char *label = (strcmp(cmd, "ba")==0) ? "Semua" :
                        (strcmp(cmd, "bo")==0) ? "Outline" : cmd;
    snprintf(pesan_status, sizeof(pesan_status), "Border: %s (%c%d:%c%d)",
             label, 'A'+x1, y1+1, 'A'+x2, y2+1);
}

/* ============================================================
 * Merge/Unmerge
 * ============================================================ */
int apakah_ada_gabung_buf(const struct buffer_tabel *buf, int x, int y) {
    return (buf->tergabung[y][x].x_anchor >= 0 &&
            buf->tergabung[y][x].y_anchor >= 0);
}

int apakah_sel_anchor_buf(const struct buffer_tabel *buf, int x, int y) {
    return apakah_ada_gabung_buf(buf, x, y) &&
           buf->tergabung[y][x].adalah_anchor;
}

void hapus_blok_gabung_buf(struct buffer_tabel *buf,int ax,int ay){
    int w=buf->tergabung[ay][ax].lebar,h=buf->tergabung[ay][ax].tinggi;
    for(int yy=ay;yy<ay+h;yy++)
        for(int xx=ax;xx<ax+w;xx++){
            buf->tergabung[yy][xx].x_anchor=-1;
            buf->tergabung[yy][xx].y_anchor=-1;
            buf->tergabung[yy][xx].lebar=0;
            buf->tergabung[yy][xx].tinggi=0;
            buf->tergabung[yy][xx].adalah_anchor=0;
        }
}
void atur_blok_gabung_buf(struct buffer_tabel *buf,int ax,int ay,int lbr,int tgi){
    for(int yy=ay;yy<ay+tgi;yy++)
        for(int xx=ax;xx<ax+lbr;xx++){
            buf->tergabung[yy][xx].x_anchor=ax;
            buf->tergabung[yy][xx].y_anchor=ay;
            buf->tergabung[yy][xx].lebar=lbr;
            buf->tergabung[yy][xx].tinggi=tgi;
            buf->tergabung[yy][xx].adalah_anchor=0;
        }
    buf->tergabung[ay][ax].adalah_anchor=1;
}

void eksekusi_gabung(struct buffer_tabel *buf) {
    int x1, y1, x2, y2;

    if (seleksi_aktif) {
        x1 = seleksi_x1; y1 = seleksi_y1;
        x2 = seleksi_x2; y2 = seleksi_y2;
    } else if (sedang_memilih) {
        x1 = jangkar_x; y1 = jangkar_y;
        x2 = buf->cfg.aktif_x; y2 = buf->cfg.aktif_y;
    } else {
        salin_str_aman(pesan_status, "Tidak ada area seleksi", sizeof(pesan_status));
        return;
    }

    int ax = (x1 < x2) ? x1 : x2;
    int ay = (y1 < y2) ? y1 : y2;
    int lbr = (x1 > x2 ? x1 - x2 : x2 - x1) + 1;
    int tgi = (y1 > y2 ? y1 - y2 : y2 - y1) + 1;

    /* Cek apakah area mengandung merge lain */
    for (int yy = ay; yy < ay + tgi; yy++) {
        for (int xx = ax; xx < ax + lbr; xx++) {
            if (apakah_ada_gabung_buf(buf, xx, yy)) {
                salin_str_aman(pesan_status,
                               "Area mengandung blok gabung",
                               sizeof(pesan_status));
                return;
            }
        }
    }

    /* Gabungkan isi sel */
    char gabungan[MAKS_TEKS] = "";
    for (int r = ay; r < ay + tgi; r++) {
        for (int c = ax; c < ax + lbr; c++) {
            const char *isi = buf->isi[r][c];
            if (strlen(isi) > 0) {
                if (strlen(gabungan) > 0) {
                    if (tgi > 1 && lbr == 1) {
                        strcat(gabungan, "\n");   /* vertikal → newline */
                    } else {
                        strcat(gabungan, " ");    /* horizontal/blok → spasi */
                    }
                }
                strcat(gabungan, isi);
            }
        }
    }

    /* Isi anchor dengan gabungan */
    if (strlen(gabungan) > 0) {
        char sebelum[MAKS_TEKS];
        salin_str_aman(sebelum, buf->isi[ay][ax], MAKS_TEKS);
        salin_str_aman(buf->isi[ay][ax], gabungan, MAKS_TEKS);
        dorong_undo_teks(buf, ax, ay, sebelum, buf->isi[ay][ax]);
    }

    /* Kosongkan sel lain */
    for (int r = ay; r < ay + tgi; r++) {
        for (int c = ax; c < ax + lbr; c++) {
            if (r == ay && c == ax) continue;
            buf->isi[r][c][0] = '\0';
        }
    }

    /* Atur blok gabung visual */
    atur_blok_gabung_buf(buf, ax, ay, lbr, tgi);

    sedang_memilih = 0;
    jangkar_x = jangkar_y = -1;
    seleksi_aktif = 0;
    seleksi_x1 = seleksi_y1 = seleksi_x2 = seleksi_y2 = -1;
    buf->cfg.aktif_x = ax;
    buf->cfg.aktif_y = ay;

    snprintf(pesan_status, sizeof(pesan_status),
             "Gabung %c%d-%c%d dengan isi",
             'A' + ax, ay + 1,
             'A' + (ax + lbr - 1), ay + tgi);

    buf->cfg.kotor = 1;
}

void eksekusi_urai_gabung(struct buffer_tabel *buf) {
    int ax = buf->cfg.aktif_x;
    int ay = buf->cfg.aktif_y;

    if (!apakah_ada_gabung_buf(buf, ax, ay)) {
        salin_str_aman(pesan_status, "Sel tidak digabung", sizeof(pesan_status));
        return;
    }

    ax = buf->tergabung[ay][ax].x_anchor;
    ay = buf->tergabung[ay][ax].y_anchor;
    int lbr = buf->tergabung[ay][ax].lebar;
    int tgi = buf->tergabung[ay][ax].tinggi;

    hapus_blok_gabung_buf(buf, ax, ay);
    dorong_undo_teks(buf, ax, ay, "", buf->isi[ay][ax]); /* catat unmerge */

    snprintf(pesan_status, sizeof(pesan_status),
             "Urai gabung anchor %c%d",
             'A' + ax, ay + 1);

    buf->cfg.kotor = 1;
}

/* ============================================================
 * Fitur Seleksi Cepat & Fill Paste
 * ============================================================ */

void seleksi_baris_penuh_aktif(struct buffer_tabel *buf) {
    /* Seleksi seluruh baris dari kolom 0 sampai kolom terakhir */
    seleksi_x1 = 0;
    seleksi_y1 = buf->cfg.aktif_y;
    seleksi_x2 = buf->cfg.kolom - 1;
    seleksi_y2 = buf->cfg.aktif_y;

    seleksi_aktif = 1;
    sedang_memilih = 0; /* Matikan mode visual manual jika aktif */

    snprintf(pesan_status, sizeof(pesan_status), "Seleksi Baris %d", buf->cfg.aktif_y + 1);
}

void seleksi_kolom_penuh_aktif(struct buffer_tabel *buf) {
    /* Seleksi seluruh kolom dari baris 0 sampai baris terakhir */
    seleksi_x1 = buf->cfg.aktif_x;
    seleksi_y1 = 0;
    seleksi_x2 = buf->cfg.aktif_x;
    seleksi_y2 = buf->cfg.baris - 1;

    seleksi_aktif = 1;
    sedang_memilih = 0; /* Matikan mode visual manual jika aktif */

    snprintf(pesan_status, sizeof(pesan_status), "Seleksi Kolom %c", 'A' + buf->cfg.aktif_x);
}

void tempel_isi_ke_seleksi(struct buffer_tabel *buf) {
    /* Cek apakah ada seleksi aktif */
    if (!seleksi_aktif) {
        snprintf(pesan_status, sizeof(pesan_status), "Tidak ada area seleksi untuk diisi");
        return;
    }

    /* Ambil teks terakhir yang disalin ke papan klip lokal */
    /* Papan klip lokal didefinisikan di struct buffer_tabel [cite: 713] */
    const char *teks_sumber = papan_klip;

    if (strlen(teks_sumber) == 0) {
        snprintf(pesan_status, sizeof(pesan_status), "Clipboard kosong");
        return;
    }

    /* Normalisasi koordinat seleksi */
    int x_start = (seleksi_x1 < seleksi_x2) ? seleksi_x1 : seleksi_x2;
    int x_end   = (seleksi_x1 > seleksi_x2) ? seleksi_x1 : seleksi_x2;
    int y_start = (seleksi_y1 < seleksi_y2) ? seleksi_y1 : seleksi_y2;
    int y_end   = (seleksi_y1 > seleksi_y2) ? seleksi_y1 : seleksi_y2;

    int counter = 0;

    /* Loop area seleksi dan isi setiap sel dengan teks sumber */
    for (int r = y_start; r <= y_end; r++) {
        for (int c = x_start; c <= x_end; c++) {
            /* Pastikan tidak menimpa bagian tersembunyi dari merge cell, kecuali anchor */
            if (apakah_ada_gabung_buf(buf, c, r) && !apakah_sel_anchor_buf(buf, c, r)) {
                continue;
            }
            atur_teks_sel(buf, c, r, teks_sumber, 1); /* 1 = catat undo */
            counter++;
        }
    }

    buf->cfg.kotor = 1;

    /* Matikan seleksi setelah selesai (opsional, tapi biasanya UX lebih enak begini) */
    seleksi_aktif = 0;
    seleksi_x1 = seleksi_y1 = seleksi_x2 = seleksi_y2 = -1;

    snprintf(pesan_status, sizeof(pesan_status), "Mengisi %d sel dengan '%s'", counter, teks_sumber);
}

void toggle_seleksi_random(struct buffer_tabel *buf, int x, int y) {
    if (!buf) return;
    for (int i = 0; i < seleksi_random; i++) {
        if (seleksi_random_x[i] == x && seleksi_random_y[i] == y) {
            /* hapus dari seleksi */
            for (int j = i; j < seleksi_random-1; j++) {
                seleksi_random_x[j] = seleksi_random_x[j+1];
                seleksi_random_y[j] = seleksi_random_y[j+1];
            }
            seleksi_random--;
            snprintf(pesan_status, sizeof(pesan_status),
                     "Unselect %c%d", 'A'+x, y+1);
            return;
        }
    }
    if (seleksi_random < MAKS_SELEKSI) {
        seleksi_random_x[seleksi_random] = x;
        seleksi_random_y[seleksi_random] = y;
        seleksi_random++;
        snprintf(pesan_status, sizeof(pesan_status),
                 "Select %c%d", 'A'+x, y+1);
    }
}

/* ============================================================
 * Clipboard Lokal & Global
 * ============================================================ */
void salin_sel(struct buffer_tabel *buf){
    salin_str_aman(papan_klip,buf->isi[buf->cfg.aktif_y][buf->cfg.aktif_x],MAKS_TEKS);
}

void salin_area(struct buffer_tabel *buf) {
    int x1, y1, x2, y2;

    if (seleksi_aktif) {
        x1 = seleksi_x1; y1 = seleksi_y1;
        x2 = seleksi_x2; y2 = seleksi_y2;
    } else if (sedang_memilih && jangkar_x >= 0 && jangkar_y >= 0) {
        x1 = (jangkar_x < buf->cfg.aktif_x) ? jangkar_x : buf->cfg.aktif_x;
        y1 = (jangkar_y < buf->cfg.aktif_y) ? jangkar_y : buf->cfg.aktif_y;
        x2 = (jangkar_x > buf->cfg.aktif_x) ? jangkar_x : buf->cfg.aktif_x;
        y2 = (jangkar_y > buf->cfg.aktif_y) ? jangkar_y : buf->cfg.aktif_y;
    } else {
        /* fallback: hanya sel aktif */
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

    for (int r = y1; r <= y2; r++) {
        for (int c = x1; c <= x2; c++) {
            salin_str_aman(papan_klip_area[r][c], buf->isi[r][c], MAKS_TEKS);
        }
    }

    snprintf(pesan_status, sizeof(pesan_status),
             "Menyalin %c%d-%c%d",
             'A' + x1, y1 + 1,
             'A' + x2, y2 + 1);
}

void salin_baris_penuh(struct buffer_tabel *buf, int row) {
    int c;
    if (!buf || row < 0 || row >= buf->cfg.baris) return;

    klip_y1 = klip_y2 = row;
    klip_x1 = 0;
    klip_x2 = buf->cfg.kolom - 1;
    klip_ada_area = 1;
    klip_mode = 1; /* baris penuh */

    for (c = 0; c < buf->cfg.kolom; c++) {
        salin_str_aman(papan_klip_area[row][c], buf->isi[row][c], MAKS_TEKS);
    }
    snprintf(pesan_status, sizeof(pesan_status),
             "Menyalin baris %d", row + 1);
}

void salin_kolom_penuh(struct buffer_tabel *buf, int col) {
    int r;
    if (!buf || col < 0 || col >= buf->cfg.kolom) return;

    klip_x1 = klip_x2 = col;
    klip_y1 = 0;
    klip_y2 = buf->cfg.baris - 1;
    klip_ada_area = 1;
    klip_mode = 2; /* kolom penuh */

    for (r = 0; r < buf->cfg.baris; r++) {
        salin_str_aman(papan_klip_area[r][col], buf->isi[r][col], MAKS_TEKS);
    }
    snprintf(pesan_status, sizeof(pesan_status),
             "Menyalin kolom %c", 'A' + col);
}

void potong_area(struct buffer_tabel *buf) {
    salin_area(buf);

    for (int r = klip_y1; r <= klip_y2; r++) {
        for (int c = klip_x1; c <= klip_x2; c++) {
            buf->isi[r][c][0] = '\0';
        }
    }

    buf->cfg.kotor = 1;
    snprintf(pesan_status, sizeof(pesan_status), "Memotong");
}

void potong_kolom_penuh(struct buffer_tabel *buf) {
    /* 1. Salin dulu ke clipboard (Mode Kolom) */
    salin_kolom_penuh(buf, buf->cfg.aktif_x);
    
    /* 2. Bersihkan isinya */
    bersihkan_kolom_aktif(buf);
    
    snprintf(pesan_status, sizeof(pesan_status), "Cut Kolom %c", 'A' + buf->cfg.aktif_x);
}

void potong_baris_penuh(struct buffer_tabel *buf) {
    /* 1. Salin dulu ke clipboard (Mode Baris) */
    salin_baris_penuh(buf, buf->cfg.aktif_y);
    
    /* 2. Bersihkan isinya */
    bersihkan_baris_aktif(buf);
    
    snprintf(pesan_status, sizeof(pesan_status), "Cut Baris %d", buf->cfg.aktif_y + 1);
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

    /* hitung offset relatif */
    dx = buf->cfg.aktif_x - klip_x1 + offset_x;
    dy = buf->cfg.aktif_y - klip_y1 + offset_y;

    /* cek apakah ada isi di target */
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
        minta_nama_berkas(jawab, sizeof(jawab),
                          (offset_x > 0 || offset_y > 0) ?
                          "Timpa isi sesudah? (y/n): " :
                          (offset_x < 0 || offset_y < 0) ?
                          "Timpa isi sebelum? (y/n): " :
                          "Timpa isi aktif? (y/n): ");
        if (jawab[0] != 'y' && jawab[0] != 'Y') {
            snprintf(pesan_status, sizeof(pesan_status), "Penempelan dibatalkan");
            return;
        }
    }

    /* lakukan paste */
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

    /* reset outline setelah paste */
    klip_ada_area = 0;
    sedang_memilih = 0;
    jangkar_x = jangkar_y = -1;
    seleksi_aktif = 0;
    seleksi_x1 = seleksi_y1 = seleksi_x2 = seleksi_y2 = -1;

    snprintf(pesan_status, sizeof(pesan_status),
             (offset_x > 0 || offset_y > 0) ? "Menempelkan sesudah sel aktif" :
             (offset_x < 0 || offset_y < 0) ? "Menempelkan sebelum sel aktif" :
             "Penempelan selesai");
}

void tempel_baris_penuh(struct buffer_tabel *buf, int row, int arah) {
    int c, target;
    if (!buf || !klip_ada_area) return;
    target = row + arah;
    if (target < 0 || target >= buf->cfg.baris) return;

    int ada_isi = 0;
    for (c = 0; c < buf->cfg.kolom; c++) {
        if (buf->isi[target][c][0] != '\0') ada_isi = 1;
    }
    if (ada_isi) {
        char jawab[8];
        minta_nama_berkas(jawab, sizeof(jawab),
                          (arah > 0) ? "Timpa baris sesudah? (y/n): "
                                     : "Timpa baris sebelum? (y/n): ");
        if (jawab[0] != 'y' && jawab[0] != 'Y') return;
    }

    for (c = 0; c < buf->cfg.kolom; c++) {
        atur_teks_sel(buf, c, target, papan_klip_area[klip_y1][c], 1);
    }

    buf->cfg.kotor = 1;
    klip_ada_area = 0;
    snprintf(pesan_status, sizeof(pesan_status),
             (arah > 0) ? "Menempelkan baris penuh sesudah baris aktif"
                        : "Menempelkan baris penuh sebelum baris aktif");
}

void tempel_kolom_penuh(struct buffer_tabel *buf, int col, int arah) {
    int r, target;
    if (!buf || !klip_ada_area) return;
    target = col + arah;
    if (target < 0 || target >= buf->cfg.kolom) return;

    int ada_isi = 0;
    for (r = 0; r < buf->cfg.baris; r++) {
        if (buf->isi[r][target][0] != '\0') ada_isi = 1;
    }
    if (ada_isi) {
        char jawab[8];
        minta_nama_berkas(jawab, sizeof(jawab),
                          (arah > 0) ? "Timpa kolom sesudah? (y/n): "
                                     : "Timpa kolom sebelum? (y/n): ");
        if (jawab[0] != 'y' && jawab[0] != 'Y') return;
    }

    for (r = 0; r < buf->cfg.baris; r++) {
        atur_teks_sel(buf, target, r, papan_klip_area[r][klip_x1], 1);
    }

    buf->cfg.kotor = 1;
    klip_ada_area = 0;
    snprintf(pesan_status, sizeof(pesan_status),
             (arah > 0) ? "Menempelkan kolom penuh sesudah kolom aktif"
                        : "Menempelkan kolom penuh sebelum kolom aktif");
}

void swap_kolom(struct buffer_tabel *buf, int col, int arah) {
    int target = col + arah;
    if (!buf || col < 0 || target < 0 || col >= buf->cfg.kolom || target >= buf->cfg.kolom) return;

    for (int r = 0; r < buf->cfg.baris; r++) {
        char tmp[MAKS_TEKS];
        salin_str_aman(tmp, buf->isi[r][col], MAKS_TEKS);
        salin_str_aman(buf->isi[r][col], buf->isi[r][target], MAKS_TEKS);
        salin_str_aman(buf->isi[r][target], tmp, MAKS_TEKS);
    }

    int tmpw = buf->lebar_kolom[col];
    buf->lebar_kolom[col] = buf->lebar_kolom[target];
    buf->lebar_kolom[target] = tmpw;

    buf->cfg.aktif_x = target;
    buf->cfg.kotor = 1;
    snprintf(pesan_status, sizeof(pesan_status),
             "Swap kolom %c <-> %c", 'A'+col, 'A'+target);
}

void swap_baris(struct buffer_tabel *buf, int row, int arah) {
    int target = row + arah;
    if (!buf || row < 0 || target < 0 || row >= buf->cfg.baris || target >= buf->cfg.baris) return;

    for (int c = 0; c < buf->cfg.kolom; c++) {
        char tmp[MAKS_TEKS];
        salin_str_aman(tmp, buf->isi[row][c], MAKS_TEKS);
        salin_str_aman(buf->isi[row][c], buf->isi[target][c], MAKS_TEKS);
        salin_str_aman(buf->isi[target][c], tmp, MAKS_TEKS);
    }

    int tmph = buf->tinggi_baris[row];
    buf->tinggi_baris[row] = buf->tinggi_baris[target];
    buf->tinggi_baris[target] = tmph;

    buf->cfg.aktif_y = target;
    buf->cfg.kotor = 1;
    snprintf(pesan_status, sizeof(pesan_status),
             "Swap baris %d <-> %d", row+1, target+1);
}

/* ============================================================
 * Navigasi
 * ============================================================ */
void tarik_ke_anchor(struct buffer_tabel *buf) {
    int x = buf->cfg.aktif_x, y = buf->cfg.aktif_y;
    if (apakah_ada_gabung_buf(buf,x,y)) {
        buf->cfg.aktif_x = buf->tergabung[y][x].x_anchor;
        buf->cfg.aktif_y = buf->tergabung[y][x].y_anchor;
    }
}

void geser_kanan(struct buffer_tabel *buf) {
    int x = buf->cfg.aktif_x, y = buf->cfg.aktif_y;
    if (apakah_ada_gabung_buf(buf,x,y)) {
        int ax = buf->tergabung[y][x].x_anchor;
        int ay = buf->tergabung[y][x].y_anchor;
        x = ax + buf->tergabung[ay][ax].lebar - 1;
    }
    if (x < buf->cfg.kolom-1) x++;
    buf->cfg.aktif_x = x;
    buf->cfg.aktif_y = y;
    tarik_ke_anchor(buf);
    pastikan_aktif_terlihat(buf);
}

void geser_kiri(struct buffer_tabel *buf) {
    int x = buf->cfg.aktif_x, y = buf->cfg.aktif_y;
    if (apakah_ada_gabung_buf(buf,x,y)) x = buf->tergabung[y][x].x_anchor;
    if (x > 0) x--;
    buf->cfg.aktif_x = x;
    buf->cfg.aktif_y = y;
    tarik_ke_anchor(buf);
    pastikan_aktif_terlihat(buf);
}

void geser_bawah(struct buffer_tabel *buf) {
    int x = buf->cfg.aktif_x, y = buf->cfg.aktif_y;
    if (apakah_ada_gabung_buf(buf,x,y)) {
        int ax = buf->tergabung[y][x].x_anchor;
        int ay = buf->tergabung[y][x].y_anchor;
        y = ay + buf->tergabung[ay][ax].tinggi - 1;
    }
    if (y < buf->cfg.baris-1) y++;
    buf->cfg.aktif_x = x;
    buf->cfg.aktif_y = y;
    tarik_ke_anchor(buf);
    pastikan_aktif_terlihat(buf);
}

void geser_atas(struct buffer_tabel *buf) {
    int x = buf->cfg.aktif_x, y = buf->cfg.aktif_y;
    if (apakah_ada_gabung_buf(buf,x,y)) y = buf->tergabung[y][x].y_anchor;
    if (y > 0) y--;
    buf->cfg.aktif_x = x;
    buf->cfg.aktif_y = y;
    tarik_ke_anchor(buf);
    pastikan_aktif_terlihat(buf);
}

void pastikan_aktif_terlihat(struct buffer_tabel *buf) {
    int x_awal, y_awal, lv, tv, ks, ke, bs, be;
    
    /* Ambil viewport saat ini (sudah menyadari split window) */
    hitung_viewport(buf, &x_awal, &y_awal, &lv, &tv, &ks, &ke, &bs, &be);

    /* --- Geser viewport horizontal --- */
    if (buf->cfg.aktif_x < ks) {
        buf->cfg.lihat_kolom = buf->cfg.aktif_x;
    } else if (buf->cfg.aktif_x > ke) {
        /* Hitung mundur dari aktif_x agar pas di ujung kanan layar window */
        int w_total = 0;
        int c = buf->cfg.aktif_x;
        while (c >= 0) {
            int w = buf->lebar_kolom[c] + 1;
            if (w_total + w > lv && c != buf->cfg.aktif_x) {
                c++; /* Mundur 1 langkah karena ini kolom yang bikin overflow */
                break;
            }
            w_total += w;
            c--;
        }
        if (c < 0) c = 0;
        buf->cfg.lihat_kolom = c;
    }

    /* --- Geser viewport vertikal --- */
    if (buf->cfg.aktif_y < bs) {
        buf->cfg.lihat_baris = buf->cfg.aktif_y;
    } else if (buf->cfg.aktif_y > be) {
        /* Hitung mundur dari aktif_y agar pas di bawah layar window */
        int h_total = 0;
        int r = buf->cfg.aktif_y;
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

    /* Clamp supaya tidak keluar batas secara tidak sengaja */
    if (buf->cfg.lihat_kolom < 0) buf->cfg.lihat_kolom = 0;
    if (buf->cfg.lihat_baris < 0) buf->cfg.lihat_baris = 0;
    if (buf->cfg.lihat_kolom > buf->cfg.kolom - 1)
        buf->cfg.lihat_kolom = buf->cfg.kolom - 1;
    if (buf->cfg.lihat_baris > buf->cfg.baris - 1)
        buf->cfg.lihat_baris = buf->cfg.baris - 1;
}

/* ============================================================
 * Ubah Ukuran Sel
 * ============================================================ */
void ubah_ukuran_sel(struct buffer_tabel *buf,int dl,int dt){
    int lbr=buf->lebar_kolom[buf->cfg.aktif_x];
    int tgi=buf->tinggi_baris[buf->cfg.aktif_y];
    int lbr_baru=lbr+dl;
    int tgi_baru=tgi+dt;
    if(lbr_baru<1)lbr_baru=1;
    if(lbr_baru>50)lbr_baru=50;
    if(tgi_baru<1)tgi_baru=1;
    if(tgi_baru>20)tgi_baru=20;
    buf->lebar_kolom[buf->cfg.aktif_x]=lbr_baru;
    buf->tinggi_baris[buf->cfg.aktif_y]=tgi_baru;
    buf->cfg.kotor=1;
}

void sesuaikan_lebar_kolom(struct buffer_tabel *buf, int col, int mode) {
    if (!buf || col < 0 || col >= buf->cfg.kolom) return;
    int maxw = 1;
    int minw = MAKS_TEKS;
    int ada_nonempty = 0;

    for (int r = 0; r < buf->cfg.baris; r++) {
        const char *isi = buf->isi[r][col];
        if (isi[0] == '\0') continue; // abaikan sel kosong
        int w = lebar_tampilan_string_utf8(isi);
        if (w > maxw) maxw = w;
        if (w < minw) minw = w;
        ada_nonempty = 1;
    }

    if (mode > 0) {
        buf->lebar_kolom[col] = maxw;
        snprintf(pesan_status, sizeof(pesan_status),
                 "Kolom %c disesuaikan ke isi terpanjang (%d)", 'A'+col, maxw);
    } else {
        buf->lebar_kolom[col] = ada_nonempty ? minw : 1;
        snprintf(pesan_status, sizeof(pesan_status),
                 "Kolom %c disesuaikan ke isi terpendek (%d)", 'A'+col,
                 buf->lebar_kolom[col]);
    }

    buf->cfg.kotor = 1;
}

void sesuaikan_semua_lebar(struct buffer_tabel *buf, int col_ref) {
    if (!buf || col_ref < 0 || col_ref >= buf->cfg.kolom) return;
    int lebar_ref = buf->lebar_kolom[col_ref];
    for (int c = 0; c < buf->cfg.kolom; c++) {
        buf->lebar_kolom[c] = lebar_ref;
    }
    buf->cfg.kotor = 1;
    snprintf(pesan_status, sizeof(pesan_status),
             "Semua kolom disesuaikan ke lebar kolom %c", 'A'+col_ref);
}

/* ============================================================
 * Hapus Isi Sel
 * ============================================================ */
void hapus_sel_atau_area(struct buffer_tabel *buf){
    int x1, y1, x2, y2, r, c;

    if (sedang_memilih && jangkar_x != -1) {
        x1 = (jangkar_x < buf->cfg.aktif_x) ? jangkar_x : buf->cfg.aktif_x;
        x2 = (jangkar_x > buf->cfg.aktif_x) ? jangkar_x : buf->cfg.aktif_x;
        y1 = (jangkar_y < buf->cfg.aktif_y) ? jangkar_y : buf->cfg.aktif_y;
        y2 = (jangkar_y > buf->cfg.aktif_y) ? jangkar_y : buf->cfg.aktif_y;
    } else {
        x1 = x2 = buf->cfg.aktif_x;
        y1 = y2 = buf->cfg.aktif_y;
    }

    for (r = y1; r <= y2; r++) {
        for (c = x1; c <= x2; c++) {
            if (apakah_ada_gabung_buf(buf, c, r) && !apakah_sel_anchor_buf(buf, c, r)) {
                /* Jangan hapus sel tertutup merge, kecuali anchor */
                continue; 
            }
            atur_teks_sel(buf, c, r, "", 1);
        }
    }
    
    /* Reset status seleksi & clipboard setelah delete */
    sedang_memilih = 0;
    jangkar_x = -1; jangkar_y = -1;
    klip_ada_area = 0; 
}

void bersihkan_kolom_aktif(struct buffer_tabel *buf) {
    int col = buf->cfg.aktif_x;
    for (int r = 0; r < buf->cfg.baris; r++) {
        /* Gunakan atur_teks_sel agar tercatat di Undo stack per sel */
        atur_teks_sel(buf, col, r, "", 1);
    }
    buf->cfg.kotor = 1;
    snprintf(pesan_status, sizeof(pesan_status), "Isi Kolom %c Dibersihkan", 'A' + col);
}

void bersihkan_baris_aktif(struct buffer_tabel *buf) {
    int row = buf->cfg.aktif_y;
    for (int c = 0; c < buf->cfg.kolom; c++) {
        atur_teks_sel(buf, c, row, "", 1);
    }
    buf->cfg.kotor = 1;
    snprintf(pesan_status, sizeof(pesan_status), "Isi Baris %d Dibersihkan", row + 1);
}

/* ============================================================
 * Struktur Insert/Delete
 * ============================================================ */
void sisip_kolom_setelah(struct buffer_tabel *buf) {
    if (buf->cfg.kolom >= MAKS_KOLOM) return;
    int pos = buf->cfg.aktif_x + 1;
    for (int i = buf->cfg.kolom; i > pos; i--) buf->lebar_kolom[i] = buf->lebar_kolom[i-1];
    for (int r=0; r<buf->cfg.baris; r++) {
        for (int i=buf->cfg.kolom; i>pos; i--) {
            salin_str_aman(buf->isi[r][i], buf->isi[r][i-1], MAKS_TEKS);
            buf->perataan_sel[r][i] = buf->perataan_sel[r][i-1];
            buf->tergabung[r][i] = buf->tergabung[r][i-1];
        }
        buf->isi[r][pos][0] = '\0';
        buf->perataan_sel[r][pos] = RATA_KIRI;
        buf->tergabung[r][pos].x_anchor = -1;
    }
    buf->lebar_kolom[pos] = LEBAR_KOLOM_DEFAULT;
    buf->cfg.kolom++;
    buf->cfg.kotor=1;
}

void sisip_kolom_sebelum(struct buffer_tabel *buf) {
    if (buf->cfg.kolom >= MAKS_KOLOM) return;
    int pos = buf->cfg.aktif_x;
    for (int i = buf->cfg.kolom; i > pos; i--) buf->lebar_kolom[i] = buf->lebar_kolom[i-1];
    for (int r=0; r<buf->cfg.baris; r++) {
        for (int i=buf->cfg.kolom; i>pos; i--) {
            salin_str_aman(buf->isi[r][i], buf->isi[r][i-1], MAKS_TEKS);
            buf->perataan_sel[r][i] = buf->perataan_sel[r][i-1];
            buf->tergabung[r][i] = buf->tergabung[r][i-1];
        }
        buf->isi[r][pos][0] = '\0';
        buf->perataan_sel[r][pos] = RATA_KIRI;
        buf->tergabung[r][pos].x_anchor = -1;
    }
    buf->lebar_kolom[pos] = LEBAR_KOLOM_DEFAULT;
    buf->cfg.kolom++;
    buf->cfg.kotor=1;
}

void sisip_baris_setelah(struct buffer_tabel *buf) {
    if (buf->cfg.baris >= MAKS_BARIS) return;
    int pos = buf->cfg.aktif_y + 1;
    for (int r=buf->cfg.baris; r>pos; r--) {
        for (int c=0; c<buf->cfg.kolom; c++) {
            salin_str_aman(buf->isi[r][c], buf->isi[r-1][c], MAKS_TEKS);
            buf->perataan_sel[r][c] = buf->perataan_sel[r-1][c];
            buf->tergabung[r][c] = buf->tergabung[r-1][c];
        }
        buf->tinggi_baris[r] = buf->tinggi_baris[r-1];
    }
    for (int c=0; c<buf->cfg.kolom; c++) {
        buf->isi[pos][c][0] = '\0';
        buf->perataan_sel[pos][c] = RATA_KIRI;
        buf->tergabung[pos][c].y_anchor = -1;
    }
    buf->tinggi_baris[pos] = TINGGI_BARIS_DEFAULT;
    buf->cfg.baris++;
    buf->cfg.kotor=1;
}

void sisip_baris_sebelum(struct buffer_tabel *buf) {
    if (buf->cfg.baris >= MAKS_BARIS) return;
    int pos = buf->cfg.aktif_y;
    for (int r=buf->cfg.baris; r>pos; r--) {
        for (int c=0; c<buf->cfg.kolom; c++) {
            salin_str_aman(buf->isi[r][c], buf->isi[r-1][c], MAKS_TEKS);
            buf->perataan_sel[r][c] = buf->perataan_sel[r-1][c];
            buf->tergabung[r][c] = buf->tergabung[r-1][c];
        }
        buf->tinggi_baris[r] = buf->tinggi_baris[r-1];
    }
    for (int c=0; c<buf->cfg.kolom; c++) {
        buf->isi[pos][c][0] = '\0';
        buf->perataan_sel[pos][c] = RATA_KIRI;
        buf->tergabung[pos][c].y_anchor = -1;
    }
    buf->tinggi_baris[pos] = TINGGI_BARIS_DEFAULT;
    buf->cfg.baris++;
    buf->cfg.kotor=1;
}

void hapus_kolom_aktif(struct buffer_tabel *buf) {
    if (buf->cfg.kolom <= 1) return;
    int pos = buf->cfg.aktif_x;
    for (int r=0; r<buf->cfg.baris; r++) {
        for (int c=pos; c<buf->cfg.kolom-1; c++) {
            salin_str_aman(buf->isi[r][c], buf->isi[r][c+1], MAKS_TEKS);
            buf->perataan_sel[r][c] = buf->perataan_sel[r][c+1];
            buf->tergabung[r][c] = buf->tergabung[r][c+1];
        }
    }
    buf->cfg.kolom--;
    if (buf->cfg.aktif_x >= buf->cfg.kolom) buf->cfg.aktif_x = buf->cfg.kolom-1;
    buf->cfg.kotor=1;
}

void hapus_baris_aktif(struct buffer_tabel *buf) {
    if (buf->cfg.baris <= 1) return;
    int pos = buf->cfg.aktif_y;
    for (int r=pos; r<buf->cfg.baris-1; r++) {
        for (int c=0; c<buf->cfg.kolom; c++) {
            salin_str_aman(buf->isi[r][c], buf->isi[r+1][c], MAKS_TEKS);
            buf->perataan_sel[r][c] = buf->perataan_sel[r+1][c];
            buf->tergabung[r][c] = buf->tergabung[r+1][c];
        }
        buf->tinggi_baris[r] = buf->tinggi_baris[r+1];
    }
    buf->cfg.baris--;
    if (buf->cfg.aktif_y >= buf->cfg.baris) buf->cfg.aktif_y = buf->cfg.baris-1;
    buf->cfg.kotor=1;
}

/* ============================================================
 * Sortir
 * ============================================================ */
static int bandingkan_sel(const char *a,const char *b,urutan_sortir urut){
    double va=strtod(a,NULL),vb=strtod(b,NULL);
    if((va!=0.0||a[0]=='0')&&(vb!=0.0||b[0]=='0')){
        if(urut==URUT_NAIK) return (va<vb)?-1:(va>vb)?1:0;
        else return (va>vb)?-1:(va<vb)?1:0;
    }
    int cmp=strcmp(a,b);
    return (urut==URUT_NAIK)?cmp:-cmp;
}

void sortir_kolom_aktif(struct buffer_tabel *buf){
    int col=buf->cfg.aktif_x;
    for(int i=0;i<buf->cfg.baris-1;i++){
        for(int j=i+1;j<buf->cfg.baris;j++){
            if(bandingkan_sel(buf->isi[i][col],buf->isi[j][col],URUT_NAIK)>0){
                for(int c=0;c<buf->cfg.kolom;c++){
                    char tmp[MAKS_TEKS];
                    salin_str_aman(tmp,buf->isi[i][c],MAKS_TEKS);
                    salin_str_aman(buf->isi[i][c],buf->isi[j][c],MAKS_TEKS);
                    salin_str_aman(buf->isi[j][c],tmp,MAKS_TEKS);
                }
            }
        }
    }
    buf->cfg.kotor=1;
}

void sortir_multi_kolom(struct buffer_tabel *buf,const int *kol,
                        const urutan_sortir *urut,int nkol){
    /* Bubble sort sederhana dengan beberapa kolom sebagai kunci */
    for(int i=0;i<buf->cfg.baris-1;i++){
        for(int j=i+1;j<buf->cfg.baris;j++){
            int cmp=0;
            for(int k=0;k<nkol;k++){
                int col=kol[k];
                cmp=bandingkan_sel(buf->isi[i][col],buf->isi[j][col],urut[k]);
                if(cmp!=0) break;
            }
            if(cmp>0){
                /* Tukar seluruh isi baris i dan j */
                for(int c=0;c<buf->cfg.kolom;c++){
                    char tmp[MAKS_TEKS];
                    salin_str_aman(tmp,buf->isi[i][c],MAKS_TEKS);
                    salin_str_aman(buf->isi[i][c],buf->isi[j][c],MAKS_TEKS);
                    salin_str_aman(buf->isi[j][c],tmp,MAKS_TEKS);
                    /* Perataan dan merge ikut ditukar */
                    perataan_teks tmp_align=buf->perataan_sel[i][c];
                    buf->perataan_sel[i][c]=buf->perataan_sel[j][c];
                    buf->perataan_sel[j][c]=tmp_align;
                    struct info_gabung tmp_merge=buf->tergabung[i][c];
                    buf->tergabung[i][c]=buf->tergabung[j][c];
                    buf->tergabung[j][c]=tmp_merge;
                }
                int tmp_tinggi=buf->tinggi_baris[i];
                buf->tinggi_baris[i]=buf->tinggi_baris[j];
                buf->tinggi_baris[j]=tmp_tinggi;
            }
        }
    }
    buf->cfg.kotor=1;
}

/* ============================================================
 * Format Sel
 * ============================================================ */

/* Atur format sel tunggal */
void atur_format_sel(struct buffer_tabel *buf, int x, int y,
                     jenis_format_sel fmt) {
    if (!buf || x < 0 || x >= buf->cfg.kolom ||
        y < 0 || y >= buf->cfg.baris) return;

    /* Jika sel adalah bagian dari merge, atur di anchor */
    if (apakah_ada_gabung_buf(buf, x, y)) {
        x = buf->tergabung[y][x].x_anchor;
        y = buf->tergabung[y][x].y_anchor;
    }

    buf->format_sel[y][x] = fmt;
    buf->cfg.kotor = 1;
}

/* Ambil format sel */
jenis_format_sel ambil_format_sel(const struct buffer_tabel *buf, int x, int y) {
    if (!buf || x < 0 || x >= buf->cfg.kolom ||
        y < 0 || y >= buf->cfg.baris) return FORMAT_UMUM;
    return buf->format_sel[y][x];
}

/* Atur format untuk area seleksi */
void atur_format_area(struct buffer_tabel *buf, jenis_format_sel fmt) {
    int x1, y1, x2, y2;
    int r, c;

    if (!buf) return;

    /* Tentukan area */
    if (seleksi_aktif) {
        x1 = seleksi_x1 < seleksi_x2 ? seleksi_x1 : seleksi_x2;
        y1 = seleksi_y1 < seleksi_y2 ? seleksi_y1 : seleksi_y2;
        x2 = seleksi_x1 > seleksi_x2 ? seleksi_x1 : seleksi_x2;
        y2 = seleksi_y1 > seleksi_y2 ? seleksi_y1 : seleksi_y2;
    } else if (sedang_memilih && jangkar_x >= 0) {
        x1 = jangkar_x < buf->cfg.aktif_x ? jangkar_x : buf->cfg.aktif_x;
        y1 = jangkar_y < buf->cfg.aktif_y ? jangkar_y : buf->cfg.aktif_y;
        x2 = jangkar_x > buf->cfg.aktif_x ? jangkar_x : buf->cfg.aktif_x;
        y2 = jangkar_y > buf->cfg.aktif_y ? jangkar_y : buf->cfg.aktif_y;
    } else {
        x1 = x2 = buf->cfg.aktif_x;
        y1 = y2 = buf->cfg.aktif_y;
    }

    /* Terapkan format */
    for (r = y1; r <= y2; r++) {
        for (c = x1; c <= x2; c++) {
            buf->format_sel[r][c] = fmt;
        }
    }

    buf->cfg.kotor = 1;

    static const char *nama_format[] = {
        "Umum", "Angka", "Tanggal", "Waktu", "Timestamp",
        "Mata Uang", "Persen", "Teks"
    };

    snprintf(pesan_status, sizeof(pesan_status),
             "Format: %s (%c%d:%c%d)",
             nama_format[fmt], 'A'+x1, y1+1, 'A'+x2, y2+1);
}

/* Deteksi format otomatis berdasarkan nilai */
int deteksi_format_otomatis(double nilai) {
    double bagian_int;
    double bagian_frac;
    
    /* Jangan deteksi nilai negatif atau sangat besar sebagai tanggal */
    if (nilai < 0 || nilai > 60000) {
        return FORMAT_UMUM;
    }
    
    bagian_int = floor(nilai);
    bagian_frac = nilai - bagian_int;
    
    /* Timestamp: nilai dalam rentang tanggal DAN memiliki bagian desimal
     * Contoh: NOW() menghasilkan 20000.5 (tanggal + waktu)
     * Ini menandakan kombinasi tanggal dan waktu */
    if (nilai >= 1000 && nilai <= 50000 && bagian_frac > 0.0001) {
        return FORMAT_TIMESTAMP;
    }
    
    /* Fraksi hari murni: kemungkinan waktu saja (TIME() function)
     * Contoh: TIME(12,30,0) = 0.520833 */
    if (nilai >= 0 && nilai < 1 && bagian_frac > 0.0001) {
        return FORMAT_WAKTU;
    }
    
    /* Integer murni dalam rentang tanggal: JANGAN auto-detect sebagai tanggal
     * karena bisa jadi angka biasa seperti hasil SUM(), COUNT(), dll.
     * User harus manual set format tanggal jika diinginkan. */
    
    return FORMAT_UMUM;
}

/* Format nilai ke teks sesuai format sel */
int format_nilai_ke_teks(double nilai, jenis_format_sel fmt,
                         char *keluaran, size_t ukuran) {
    time_t epoch;
    int jam, menit, detik;
    double fraksi_hari, bagian_desimal;
    long int_part;
    struct tm tm_val;

    if (!keluaran || ukuran == 0) return -1;

    switch (fmt) {
    case FORMAT_TANGGAL:
        epoch = (time_t)(nilai * 86400);
        return waktu_format_tanggal("YYYY-MM-DD", epoch, keluaran, ukuran);

    case FORMAT_WAKTU:
        /* Fraksi hari ke HH:MM:SS */
        fraksi_hari = nilai - floor(nilai);
        if (fraksi_hari < 0) fraksi_hari = 0;
        bagian_desimal = fraksi_hari * 86400.0;
        jam = (int)(bagian_desimal / 3600) % 24;
        menit = (int)((bagian_desimal - jam * 3600) / 60) % 60;
        detik = (int)(bagian_desimal) % 60;
        snprintf(keluaran, ukuran, "%02d:%02d:%02d", jam, menit, detik);
        return 0;

    case FORMAT_TIMESTAMP:
        epoch = (time_t)(nilai * 86400);
        if (localtime_r(&epoch, &tm_val) == NULL) {
            keluaran[0] = '\0';
            return -1;
        }
        snprintf(keluaran, ukuran, "%04d-%02d-%02d %02d:%02d:%02d",
                 tm_val.tm_year + 1900, tm_val.tm_mon + 1, tm_val.tm_mday,
                 tm_val.tm_hour, tm_val.tm_min, tm_val.tm_sec);
        return 0;

    case FORMAT_MATA_UANG:
        /* Format Indonesia: Rp 1.000.000,00 */
        int_part = (long)nilai;
        if (nilai < 0) {
            snprintf(keluaran, ukuran, "Rp -%ld.%03ld,%02d",
                     (-int_part) / 1000, (-int_part) % 1000,
                     (int)((-nilai - (-int_part)) * 100 + 0.5) % 100);
        } else {
            snprintf(keluaran, ukuran, "Rp %ld.%03ld,%02d",
                     int_part / 1000, int_part % 1000,
                     (int)((nilai - int_part) * 100 + 0.5) % 100);
        }
        return 0;

    case FORMAT_PERSUAN:
        snprintf(keluaran, ukuran, "%.2f%%", nilai * 100);
        return 0;

    case FORMAT_ANGKA:
        snprintf(keluaran, ukuran, "%.2f", nilai);
        return 0;

    case FORMAT_TEKS:
    case FORMAT_UMUM:
    default:
        snprintf(keluaran, ukuran, "%.10g", nilai);
        return 0;
    }
}

/* ============================================================
 * Pencarian
 * ============================================================ */
int lakukan_cari(struct buffer_tabel *buf, const char *query) {
    if (!buf || !query) return -1;
    buf->jumlah_hasil = 0;
    buf->indeks_hasil = -1;
    salin_str_aman(buf->query_cari, query, MAKS_TEKS);

    /* Alokasi array hasil jika belum ada */
    if (!buf->hasil_cari_x) {
        buf->hasil_cari_x = calloc(buf->cfg.baris * buf->cfg.kolom, sizeof(int));
        buf->hasil_cari_y = calloc(buf->cfg.baris * buf->cfg.kolom, sizeof(int));
    }

    for (int r=0; r<buf->cfg.baris; r++) {
        for (int c=0; c<buf->cfg.kolom; c++) {
            if (strstr(buf->isi[r][c], query)) {
                buf->hasil_cari_x[buf->jumlah_hasil] = c;
                buf->hasil_cari_y[buf->jumlah_hasil] = r;
                buf->jumlah_hasil++;
            }
        }
    }

    if (buf->jumlah_hasil > 0) {
        buf->indeks_hasil = 0;
        buf->cfg.aktif_x = buf->hasil_cari_x[0];
        buf->cfg.aktif_y = buf->hasil_cari_y[0];
        snprintf(pesan_status, sizeof(pesan_status),
                 "Ditemukan %d hasil, lompat ke pertama", buf->jumlah_hasil);
        return buf->jumlah_hasil;
    } else {
        snprintf(pesan_status, sizeof(pesan_status), "Tidak ada hasil");
        return 0;
    }
}

int lakukan_cari_ganti(struct buffer_tabel *buf, const char *cmd) {
    if (!buf || !cmd) return -1;
    /* Format cmd: "cari:ganti" */
    char cari[MAKS_TEKS], ganti[MAKS_TEKS];
    const char *p = strchr(cmd, ':');
    if (!p) return -1;
    size_t lenCari = p - cmd;
    if (lenCari >= MAKS_TEKS) return -1;
    strncpy(cari, cmd, lenCari);
    cari[lenCari] = '\0';
    salin_str_aman(ganti, p+1, MAKS_TEKS);

    int count = 0;
    for (int r=0; r<buf->cfg.baris; r++) {
        for (int c=0; c<buf->cfg.kolom; c++) {
            char *pos = strstr(buf->isi[r][c], cari);
            if (pos) {
                char baru[MAKS_TEKS];
                size_t prefix = pos - buf->isi[r][c];
                snprintf(baru, sizeof(baru), "%.*s%s%s",
                         (int)prefix, buf->isi[r][c], ganti, pos+strlen(cari));
                salin_str_aman(buf->isi[r][c], baru, MAKS_TEKS);
                count++;
            }
        }
    }
    snprintf(pesan_status, sizeof(pesan_status),
             "Cari & ganti: %d perubahan", count);
    buf->cfg.kotor=1;
    return count;
}

void cari_next(struct buffer_tabel *buf) {
    if (!buf || buf->jumlah_hasil<=0) return;
    buf->indeks_hasil++;
    if (buf->indeks_hasil >= buf->jumlah_hasil) buf->indeks_hasil = 0;
    buf->cfg.aktif_x = buf->hasil_cari_x[buf->indeks_hasil];
    buf->cfg.aktif_y = buf->hasil_cari_y[buf->indeks_hasil];
    snprintf(pesan_status, sizeof(pesan_status),
             "Hasil %d/%d", buf->indeks_hasil+1, buf->jumlah_hasil);
}

void cari_prev(struct buffer_tabel *buf) {
    if (!buf || buf->jumlah_hasil<=0) return;
    buf->indeks_hasil--;
    if (buf->indeks_hasil < 0) buf->indeks_hasil = buf->jumlah_hasil-1;
    buf->cfg.aktif_x = buf->hasil_cari_x[buf->indeks_hasil];
    buf->cfg.aktif_y = buf->hasil_cari_y[buf->indeks_hasil];
    snprintf(pesan_status, sizeof(pesan_status),
             "Hasil %d/%d", buf->indeks_hasil+1, buf->jumlah_hasil);
}

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

    /* INIT WINDOW MANAGER SETELAH BUFFER PERTAMA DIBUAT */
    inisialisasi_window_manager();
    paksa_recalc_layout();

    /* Jalankan loop utama aplikasi */
    mulai_aplikasi_poll(buf);

    /* Pulihkan terminal sebelum keluar */
    keluar_mode_alternatif();
    pulihkan_terminal();
    return 0;
}
