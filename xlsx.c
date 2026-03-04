/* ============================================================
 * TABEL v3.0
 * Berkas: xlsx.c - Format Berkas Office Open XML (OOXML) - Excel 2007+
 * Standar: C89 + POSIX.1-2008
 * Penulis: Chandra Lesmana
 * ============================================================ */

#include "tabel.h"

/* ============================================================
 * KONSTANTA XLSX
 * ============================================================ */
#define XLSX_MAX_FILES        256
#define XLSX_MAX_PATH         256
#define XLSX_MAX_CONTENT      (16 * 1024 * 1024)  /* 16MB */
#define XLSX_MAX_SHARED       65536
#define XLSX_MAX_SHEETS       256
#define XLSX_BUFFER_SIZE      (64 * 1024)

/* ZIP file signature dan struktur */
#define ZIP_LOCAL_FILE_HEADER_SIG    0x04034B50
#define ZIP_CENTRAL_DIR_HEADER_SIG   0x02014B50
#define ZIP_END_CENTRAL_DIR_SIG      0x06054B50

/* ============================================================
 * STRUKTUR ZIP
 * ============================================================ */

/* Local file header (30 bytes + variable) */
struct zip_local_header {
    unsigned int signature;           /* 0x04034B50 */
    unsigned short version_needed;
    unsigned short flags;
    unsigned short compression;       /* 0=stored, 8=deflate */
    unsigned short mod_time;
    unsigned short mod_date;
    unsigned int crc32;
    unsigned int compressed_size;
    unsigned int uncompressed_size;
    unsigned short filename_len;
    unsigned short extra_len;
};

/* Central directory header (46 bytes + variable) */
struct zip_central_header {
    unsigned int signature;           /* 0x02014B50 */
    unsigned short version_made;
    unsigned short version_needed;
    unsigned short flags;
    unsigned short compression;
    unsigned short mod_time;
    unsigned short mod_date;
    unsigned int crc32;
    unsigned int compressed_size;
    unsigned int uncompressed_size;
    unsigned short filename_len;
    unsigned short extra_len;
    unsigned short comment_len;
    unsigned short disk_start;
    unsigned short internal_attr;
    unsigned int external_attr;
    unsigned int local_header_offset;
};

/* End of central directory (22 bytes + variable) */
struct zip_end_central {
    unsigned int signature;           /* 0x06054B50 */
    unsigned short disk_num;
    unsigned short disk_start;
    unsigned short entries_on_disk;
    unsigned short total_entries;
    unsigned int central_dir_size;
    unsigned int central_dir_offset;
    unsigned short comment_len;
};

/* File entry dalam ZIP */
struct zip_entry {
    char filename[XLSX_MAX_PATH];
    unsigned int compressed_size;
    unsigned int uncompressed_size;
    unsigned int crc32;
    unsigned short compression;
    long data_offset;               /* Offset ke data dalam file */
    long local_header_offset;       /* Offset ke local header */
};

/* ZIP file structure */
struct zip_file {
    FILE *fp;
    struct zip_entry entries[XLSX_MAX_FILES];
    int num_entries;
    int is_writing;
    /* Untuk writing */
    long central_dir_offset;
    int written_entries;
};

/* ============================================================
 * STRUKTUR XML
 * ============================================================ */

/* XML node types */
typedef enum {
    XML_NODE_ELEMENT = 0,
    XML_NODE_TEXT,
    XML_NODE_END
} xml_node_type;

/* XML attribute */
struct xml_attr {
    char name[128];
    char value[MAKS_TEKS];
    struct xml_attr *next;
};

/* XML node */
struct xml_node {
    xml_node_type type;
    char name[128];
    char text[MAKS_TEKS];
    struct xml_attr *attrs;
    struct xml_node *children;
    struct xml_node *next;
};

/* XML parser state */
struct xml_parser {
    const char *input;
    int input_len;
    int pos;
    char error[256];
};

/* ============================================================
 * STRUKTUR XLSX
 * ============================================================ */

/* Shared string entry */
struct xlsx_shared_string {
    char text[MAKS_TEKS];
    int count;   /* Berapa kali digunakan */
};

/* Cell reference */
struct xlsx_cell_ref {
    int col;     /* 0-based */
    int row;     /* 0-based */
};

/* Cell data */
struct xlsx_cell {
    int col;
    int row;
    char value[MAKS_TEKS];
    int is_formula;
    int is_number;
    int shared_string_idx;  /* -1 jika bukan shared string */
};

/* Sheet data */
struct xlsx_sheet {
    char name[128];
    struct xlsx_cell *cells;
    int num_cells;
    int max_cells;
    int max_col;
    int max_row;
};

/* Workbook */
struct xlsx_workbook {
    struct xlsx_sheet *sheets;
    int num_sheets;
    struct xlsx_shared_string *shared_strings;
    int num_shared_strings;
    int max_shared_strings;
};

/* ============================================================
 * VARIABEL GLOBAL
 * ============================================================ */
static char xlsx_error_msg[512] = "";

/* ============================================================
 * FUNGSI HELPER
 * ============================================================ */

/* Set error message */
static void xlsx_set_error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(xlsx_error_msg, sizeof(xlsx_error_msg), fmt, args);
    va_end(args);
}

/* Get error message */
const char *xlsx_get_error(void) {
    return xlsx_error_msg;
}

/* Konversi kolom index ke huruf Excel (0=A, 26=AA, dst) */
void xlsx_col_to_letters(int col, char *buf, size_t buf_size) {
    int i = 0;
    int temp = col;

    buf[0] = '\0';

    /* Hitung berapa digit */
    int digits = 0;
    int t = col + 1;
    while (t > 0) {
        digits++;
        t = (t - 1) / 26;
    }

    if (digits == 0) digits = 1;

    /* Isi dari belakang */
    int pos = digits;
    buf[pos] = '\0';

    temp = col + 1;
    while (temp > 0 && pos > 0) {
        pos--;
        buf[pos] = (char)('A' + (temp - 1) % 26);
        temp = (temp - 1) / 26;
    }

    if (pos > 0) {
        memmove(buf, buf + pos, digits + 1);
    }
}

/* Konversi huruf Excel ke kolom index (A=0, AA=26, dst) */
static int xlsx_letters_to_col(const char *letters) {
    int col = 0;
    const char *p = letters;

    while (*p && isalpha((unsigned char)*p)) {
        col = col * 26 + (toupper((unsigned char)*p) - 'A' + 1);
        p++;
    }

    return col - 1;
}

/* Parse cell reference (A1, BC123, dst) */
static int xlsx_parse_cell_ref(const char *ref, int *col, int *row) {
    const char *p = ref;
    int c = 0, r = 0;

    if (!ref || !col || !row) return -1;

    /* Parse kolom (huruf) */
    while (*p && isalpha((unsigned char)*p)) {
        c = c * 26 + (toupper((unsigned char)*p) - 'A' + 1);
        p++;
    }
    *col = c - 1;

    /* Parse baris (angka) */
    while (*p && isdigit((unsigned char)*p)) {
        r = r * 10 + (*p - '0');
        p++;
    }
    *row = r - 1;

    return 0;
}

/* ============================================================
 * FUNGSI CRC32
 * ============================================================ */

/* CRC32 lookup table */
static unsigned int crc32_table[256];
static int crc32_table_init = 0;

/* Initialize CRC32 table */
static void crc32_init_table(void) {
    unsigned int i, j, crc;

    if (crc32_table_init) return;

    for (i = 0; i < 256; i++) {
        crc = i;
        for (j = 0; j < 8; j++) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320;
            else
                crc >>= 1;
        }
        crc32_table[i] = crc;
    }
    crc32_table_init = 1;
}

/* Calculate CRC32 */
static unsigned int crc32_calc(const unsigned char *buf, size_t len) {
    unsigned int crc = 0xFFFFFFFF;
    size_t i;

    if (!crc32_table_init) crc32_init_table();

    for (i = 0; i < len; i++) {
        crc = crc32_table[(crc ^ buf[i]) & 0xFF] ^ (crc >> 8);
    }

    return crc ^ 0xFFFFFFFF;
}

/* ============================================================
 * FUNGSI DEFLATE/INFLATE SEDERHANA
 * ============================================================
 * Catatan: Implementasi sederhana untuk stored (uncompressed)
 *          dan deflate sederhana. Untuk deflate penuh perlu
 *          implementasi LZ77 + Huffman yang kompleks.
 * ============================================================ */

/* Deflate - sederhana (stored mode saja untuk reliabilitas) */
static int deflate_simple(const unsigned char *input, size_t input_len,
                          unsigned char *output, size_t *output_len,
                          size_t output_max) {
    /* Gunakan stored block (no compression) untuk simplicitas */
    /* Format: 1 byte final+type, 2 byte len, 2 byte nlen, data */
    
    size_t needed;
    size_t offset = 0;
    size_t remaining = input_len;
    size_t pos = 0;
    const size_t BLOCK_SIZE = 65535;

    if (!input || !output || !output_len) return -1;

    needed = 0;
    while (remaining > 0) {
        size_t block = remaining > BLOCK_SIZE ? BLOCK_SIZE : remaining;
        /* 5 bytes header + data */
        needed += 5 + block;
        remaining -= block;
    }

    if (needed > output_max) {
        *output_len = needed;
        return -1;
    }

    remaining = input_len;
    pos = 0;
    offset = 0;

    while (remaining > 0) {
        size_t block = remaining > BLOCK_SIZE ? BLOCK_SIZE : remaining;
        unsigned short block_len = (unsigned short)block;
        unsigned short block_nlen = (unsigned short)(~block_len);

        /* Final block flag + stored type (00) */
        unsigned char header = (remaining <= BLOCK_SIZE) ? 0x01 : 0x00;

        output[offset++] = header;
        output[offset++] = block_len & 0xFF;
        output[offset++] = (block_len >> 8) & 0xFF;
        output[offset++] = block_nlen & 0xFF;
        output[offset++] = (block_nlen >> 8) & 0xFF;

        memcpy(output + offset, input + pos, block);

        offset += block;
        pos += block;
        remaining -= block;
    }

    *output_len = offset;
    return 0;
}

/* Inflate - parse deflate stream */
static int inflate_simple(const unsigned char *input, size_t input_len,
                          unsigned char *output, size_t *output_len,
                          size_t output_max) {
    size_t in_pos = 0;
    size_t out_pos = 0;
    unsigned char bfinal, btype;

    if (!input || !output || !output_len) return -1;

    while (in_pos < input_len) {
        /* Read block header */
        bfinal = input[in_pos] & 0x01;
        btype = (input[in_pos] >> 1) & 0x03;
        in_pos++;

        if (btype == 0) {
            /* Stored block */
            unsigned short len, nlen;

            if (in_pos + 4 > input_len) return -1;

            len = input[in_pos] | (input[in_pos + 1] << 8);
            nlen = input[in_pos + 2] | (input[in_pos + 3] << 8);
            in_pos += 4;

            if ((unsigned short)len != (unsigned short)(~nlen)) return -1;

            if (in_pos + len > input_len) return -1;
            if (out_pos + len > output_max) return -1;

            memcpy(output + out_pos, input + in_pos, len);
            out_pos += len;
            in_pos += len;

        } else if (btype == 1 || btype == 2) {
            /* 
             * Deflate dengan Huffman - implementasi sederhana
             * Untuk simplicitas, kita gunakan implementasi dasar
             */
            return -1;  /* Not implemented - requires full Huffman */

        } else {
            return -1;  /* Invalid block type */
        }

        if (bfinal) break;
    }

    *output_len = out_pos;
    return 0;
}

/* ============================================================
 * FUNGSI ZIP - READING
 * ============================================================ */

/* Baca little-endian 16-bit */
static unsigned short read_le16(const unsigned char *p) {
    return (unsigned short)(p[0] | (p[1] << 8));
}

/* Baca little-endian 32-bit */
static unsigned int read_le32(const unsigned char *p) {
    return (unsigned int)(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
}

/* Tulis little-endian 16-bit */
static void write_le16(unsigned char *p, unsigned short v) {
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
}

/* Tulis little-endian 32-bit */
static void write_le32(unsigned char *p, unsigned int v) {
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
    p[2] = (v >> 16) & 0xFF;
    p[3] = (v >> 24) & 0xFF;
}

/* Cari End of Central Directory */
static long zip_find_end_central(FILE *fp) {
    unsigned char buf[512];
    long file_size;
    long pos;
    int i;

    /* Dapatkan ukuran file */
    fseek(fp, 0, SEEK_END);
    file_size = ftell(fp);

    /* Cari dari belakang */
    pos = file_size - sizeof(buf);
    if (pos < 0) pos = 0;

    while (pos >= 0) {
        size_t read_len;
        size_t search_start;

        fseek(fp, pos, SEEK_SET);
        read_len = fread(buf, 1, sizeof(buf), fp);

        search_start = 0;
        if (pos > 0 && read_len == sizeof(buf)) {
            /* Hindari signature yang terpotong */
            search_start = 3;
        }

        /* Cari signature */
        for (i = (int)search_start; i < (int)read_len - 3; i++) {
            if (read_le32(buf + i) == ZIP_END_CENTRAL_DIR_SIG) {
                return pos + i;
            }
        }

        if (pos == 0) break;
        pos -= sizeof(buf) - 3;
        if (pos < 0) pos = 0;
    }

    return -1;
}

/* Buka ZIP file untuk reading */
static struct zip_file *zip_open_read(const char *filename) {
    struct zip_file *zf;
    FILE *fp;
    unsigned char buf[64];
    long eocd_offset;
    struct zip_end_central eocd;
    int i;

    fp = fopen(filename, "rb");
    if (!fp) {
        xlsx_set_error("Cannot open file: %s", filename);
        return NULL;
    }

    /* Cari End of Central Directory */
    eocd_offset = zip_find_end_central(fp);
    if (eocd_offset < 0) {
        fclose(fp);
        xlsx_set_error("Invalid ZIP file: EOCD not found");
        return NULL;
    }

    /* Baca EOCD */
    fseek(fp, eocd_offset, SEEK_SET);
    if (fread(buf, 1, 22, fp) != 22) {
        fclose(fp);
        xlsx_set_error("Cannot read EOCD");
        return NULL;
    }

    eocd.signature = read_le32(buf);
    eocd.disk_num = read_le16(buf + 4);
    eocd.disk_start = read_le16(buf + 6);
    eocd.entries_on_disk = read_le16(buf + 8);
    eocd.total_entries = read_le16(buf + 10);
    eocd.central_dir_size = read_le32(buf + 12);
    eocd.central_dir_offset = read_le32(buf + 16);
    eocd.comment_len = read_le16(buf + 20);

    /* Alokasi struktur */
    zf = calloc(1, sizeof(*zf));
    if (!zf) {
        fclose(fp);
        return NULL;
    }

    zf->fp = fp;
    zf->is_writing = 0;
    zf->num_entries = eocd.total_entries;

    /* Baca central directory entries */
    fseek(fp, eocd.central_dir_offset, SEEK_SET);

    for (i = 0; i < eocd.total_entries && i < XLSX_MAX_FILES; i++) {
        struct zip_central_header ch;
        struct zip_entry *entry = &zf->entries[zf->num_entries];

        if (fread(buf, 1, 46, fp) != 46) break;

        ch.signature = read_le32(buf);
        ch.version_made = read_le16(buf + 4);
        ch.version_needed = read_le16(buf + 6);
        ch.flags = read_le16(buf + 8);
        ch.compression = read_le16(buf + 10);
        ch.mod_time = read_le16(buf + 12);
        ch.mod_date = read_le16(buf + 14);
        ch.crc32 = read_le32(buf + 16);
        ch.compressed_size = read_le32(buf + 20);
        ch.uncompressed_size = read_le32(buf + 24);
        ch.filename_len = read_le16(buf + 28);
        ch.extra_len = read_le16(buf + 30);
        ch.comment_len = read_le16(buf + 32);
        ch.disk_start = read_le16(buf + 34);
        ch.internal_attr = read_le16(buf + 36);
        ch.external_attr = read_le32(buf + 38);
        ch.local_header_offset = read_le32(buf + 42);

        /* Baca filename */
        if (ch.filename_len >= XLSX_MAX_PATH) ch.filename_len = XLSX_MAX_PATH - 1;
        fread(entry->filename, 1, ch.filename_len, fp);
        entry->filename[ch.filename_len] = '\0';

        /* Skip extra dan comment */
        fseek(fp, ch.extra_len + ch.comment_len, SEEK_CUR);

        /* Simpan info */
        entry->compressed_size = ch.compressed_size;
        entry->uncompressed_size = ch.uncompressed_size;
        entry->crc32 = ch.crc32;
        entry->compression = ch.compression;
        entry->local_header_offset = ch.local_header_offset;

        zf->num_entries = i + 1;
    }

    return zf;
}

/* Cari entry dalam ZIP */
static struct zip_entry *zip_find_entry(struct zip_file *zf, const char *filename) {
    int i;

    if (!zf || !filename) return NULL;

    for (i = 0; i < zf->num_entries; i++) {
        if (strcmp(zf->entries[i].filename, filename) == 0) {
            return &zf->entries[i];
        }
    }

    return NULL;
}

/* Ekstrak entry dari ZIP */
static int zip_extract_entry(struct zip_file *zf, struct zip_entry *entry,
                             unsigned char *output, size_t *output_len,
                             size_t output_max) {
    unsigned char buf[64];
    struct zip_local_header lh;
    unsigned char *compressed_data;

    if (!zf || !entry || !output || !output_len) return -1;

    /* Baca local header */
    fseek(zf->fp, entry->local_header_offset, SEEK_SET);
    if (fread(buf, 1, 30, zf->fp) != 30) return -1;

    lh.signature = read_le32(buf);
    lh.version_needed = read_le16(buf + 4);
    lh.flags = read_le16(buf + 6);
    lh.compression = read_le16(buf + 8);
    lh.mod_time = read_le16(buf + 10);
    lh.mod_date = read_le16(buf + 12);
    lh.crc32 = read_le32(buf + 14);
    lh.compressed_size = read_le32(buf + 18);
    lh.uncompressed_size = read_le32(buf + 22);
    lh.filename_len = read_le16(buf + 26);
    lh.extra_len = read_le16(buf + 28);

    /* Skip filename dan extra */
    fseek(zf->fp, lh.filename_len + lh.extra_len, SEEK_CUR);

    /* Baca compressed data */
    compressed_data = malloc(entry->compressed_size);
    if (!compressed_data) return -1;

    if (fread(compressed_data, 1, entry->compressed_size, zf->fp) != 
        entry->compressed_size) {
        free(compressed_data);
        return -1;
    }

    /* Decompress */
    if (entry->compression == 0) {
        /* Stored (no compression) */
        if (entry->uncompressed_size > output_max) {
            free(compressed_data);
            return -1;
        }
        memcpy(output, compressed_data, entry->uncompressed_size);
        *output_len = entry->uncompressed_size;
    } else if (entry->compression == 8) {
        /* Deflate */
        size_t out_len = entry->uncompressed_size;
        if (out_len > output_max) {
            free(compressed_data);
            return -1;
        }

        /* Coba inflate sederhana dulu */
        if (inflate_simple(compressed_data, entry->compressed_size,
                          output, output_len, output_max) != 0) {
            /* Jika gagal, gunakan zlib jika tersedia di sistem */
            /* Fallback: error */
            free(compressed_data);
            xlsx_set_error("Deflate not fully supported");
            return -1;
        }
    } else {
        free(compressed_data);
        xlsx_set_error("Unsupported compression: %d", entry->compression);
        return -1;
    }

    free(compressed_data);
    return 0;
}

/* Tutup ZIP file */
static void zip_close(struct zip_file *zf) {
    if (!zf) return;
    if (zf->fp) fclose(zf->fp);
    free(zf);
}

/* ============================================================
 * FUNGSI ZIP - WRITING
 * ============================================================ */

/* Struktur untuk menulis ZIP */
struct zip_write_entry {
    char filename[XLSX_MAX_PATH];
    unsigned char *data;
    size_t data_len;
    unsigned int crc32;
    time_t mod_time;
};

struct zip_writer {
    FILE *fp;
    struct zip_write_entry entries[XLSX_MAX_FILES];
    int num_entries;
    long central_dir_offset;
};

/* Buat ZIP writer baru */
static struct zip_writer *zip_create(const char *filename) {
    struct zip_writer *zw;

    zw = calloc(1, sizeof(*zw));
    if (!zw) return NULL;

    zw->fp = fopen(filename, "wb");
    if (!zw->fp) {
        free(zw);
        return NULL;
    }

    return zw;
}

/* Tambah entry ke ZIP */
static int zip_add_entry(struct zip_writer *zw, const char *filename,
                         const unsigned char *data, size_t data_len) {
    struct zip_write_entry *entry;

    if (!zw || !filename || !data) return -1;
    if (zw->num_entries >= XLSX_MAX_FILES) return -1;

    entry = &zw->entries[zw->num_entries];
    strncpy(entry->filename, filename, XLSX_MAX_PATH - 1);
    entry->filename[XLSX_MAX_PATH - 1] = '\0';

    entry->data = malloc(data_len);
    if (!entry->data) return -1;

    memcpy(entry->data, data, data_len);
    entry->data_len = data_len;
    entry->crc32 = crc32_calc(data, data_len);
    entry->mod_time = time(NULL);

    zw->num_entries++;
    return 0;
}

/* Tulis local file header */
static void zip_write_local_header(FILE *fp, const char *filename,
                                    size_t uncompressed_size,
                                    size_t compressed_size,
                                    unsigned int crc32, int compression) {
    unsigned char header[30];
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    unsigned short dos_time, dos_date;
    size_t filename_len = strlen(filename);

    /* DOS time format */
    dos_time = (tm->tm_sec / 2) | (tm->tm_min << 5) | (tm->tm_hour << 11);
    dos_date = tm->tm_mday | ((tm->tm_mon + 1) << 5) | 
               ((tm->tm_year - 80) << 9);

    write_le32(header, ZIP_LOCAL_FILE_HEADER_SIG);
    write_le16(header + 4, 20);           /* Version needed */
    write_le16(header + 6, 0);            /* Flags */
    write_le16(header + 8, compression);  /* Compression (0=stored) */
    write_le16(header + 10, dos_time);
    write_le16(header + 12, dos_date);
    write_le32(header + 14, crc32);
    write_le32(header + 18, compressed_size);
    write_le32(header + 22, uncompressed_size);
    write_le16(header + 26, filename_len);
    write_le16(header + 28, 0);           /* Extra len */

    fwrite(header, 1, 30, fp);
    fwrite(filename, 1, filename_len, fp);
}

/* Tulis central directory header */
static void zip_write_central_header(FILE *fp, const char *filename,
                                      size_t uncompressed_size,
                                      size_t compressed_size,
                                      unsigned int crc32, int compression,
                                      long local_offset) {
    unsigned char header[46];
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    unsigned short dos_time, dos_date;
    size_t filename_len = strlen(filename);

    dos_time = (tm->tm_sec / 2) | (tm->tm_min << 5) | (tm->tm_hour << 11);
    dos_date = tm->tm_mday | ((tm->tm_mon + 1) << 5) | 
               ((tm->tm_year - 80) << 9);

    write_le32(header, ZIP_CENTRAL_DIR_HEADER_SIG);
    write_le16(header + 4, 20);           /* Version made by */
    write_le16(header + 6, 20);           /* Version needed */
    write_le16(header + 8, 0);            /* Flags */
    write_le16(header + 10, compression);
    write_le16(header + 12, dos_time);
    write_le16(header + 14, dos_date);
    write_le32(header + 16, crc32);
    write_le32(header + 20, compressed_size);
    write_le32(header + 24, uncompressed_size);
    write_le16(header + 28, filename_len);
    write_le16(header + 30, 0);           /* Extra len */
    write_le16(header + 32, 0);           /* Comment len */
    write_le16(header + 34, 0);           /* Disk start */
    write_le16(header + 36, 0);           /* Internal attr */
    write_le32(header + 38, 0);           /* External attr */
    write_le32(header + 42, local_offset);

    fwrite(header, 1, 46, fp);
    fwrite(filename, 1, filename_len, fp);
}

/* Finalize ZIP file */
static int zip_finalize(struct zip_writer *zw) {
    long local_offsets[XLSX_MAX_FILES];
    unsigned char *compressed;
    size_t compressed_len;
    size_t compressed_max;
    int i;
    long central_dir_start;
    long central_dir_size;

    if (!zw) return -1;

    /* Tulis semua local file headers dan data */
    for (i = 0; i < zw->num_entries; i++) {
        struct zip_write_entry *entry = &zw->entries[i];
        local_offsets[i] = ftell(zw->fp);

        /* Untuk simplicitas, gunakan stored (no compression) */
        zip_write_local_header(zw->fp, entry->filename,
                               entry->data_len, entry->data_len,
                               entry->crc32, 0);
        fwrite(entry->data, 1, entry->data_len, zw->fp);
    }

    /* Tulis central directory */
    central_dir_start = ftell(zw->fp);

    for (i = 0; i < zw->num_entries; i++) {
        struct zip_write_entry *entry = &zw->entries[i];
        zip_write_central_header(zw->fp, entry->filename,
                                  entry->data_len, entry->data_len,
                                  entry->crc32, 0, local_offsets[i]);
    }

    central_dir_size = ftell(zw->fp) - central_dir_start;

    /* Tulis End of Central Directory */
    {
        unsigned char eocd[22];
        write_le32(eocd, ZIP_END_CENTRAL_DIR_SIG);
        write_le16(eocd + 4, 0);                     /* Disk number */
        write_le16(eocd + 6, 0);                     /* Disk with central dir */
        write_le16(eocd + 8, zw->num_entries);       /* Entries on disk */
        write_le16(eocd + 10, zw->num_entries);      /* Total entries */
        write_le32(eocd + 12, central_dir_size);     /* Central dir size */
        write_le32(eocd + 16, central_dir_start);    /* Central dir offset */
        write_le16(eocd + 20, 0);                    /* Comment len */
        fwrite(eocd, 1, 22, zw->fp);
    }

    return 0;
}

/* Tutup ZIP writer */
static void zip_writer_close(struct zip_writer *zw) {
    int i;

    if (!zw) return;

    for (i = 0; i < zw->num_entries; i++) {
        free(zw->entries[i].data);
    }

    if (zw->fp) fclose(zw->fp);
    free(zw);
}

/* ============================================================
 * FUNGSI XML - PARSING
 * ============================================================ */

/* Skip whitespace */
static void xml_skip_ws(struct xml_parser *xp) {
    while (xp->pos < xp->input_len && 
           isspace((unsigned char)xp->input[xp->pos])) {
        xp->pos++;
    }
}

/* Parse XML identifier */
static int xml_parse_ident(struct xml_parser *xp, char *buf, size_t buf_size) {
    size_t i = 0;

    while (xp->pos < xp->input_len && i < buf_size - 1) {
        char c = xp->input[xp->pos];
        if (isalnum((unsigned char)c) || c == '_' || c == '-' || c == ':') {
            buf[i++] = c;
            xp->pos++;
        } else {
            break;
        }
    }
    buf[i] = '\0';
    return (int)i;
}

/* Parse quoted string */
static int xml_parse_quoted(struct xml_parser *xp, char *buf, size_t buf_size) {
    char quote;
    size_t i = 0;

    if (xp->pos >= xp->input_len) return -1;

    quote = xp->input[xp->pos];
    if (quote != '"' && quote != '\'') return -1;
    xp->pos++;

    while (xp->pos < xp->input_len && i < buf_size - 1) {
        char c = xp->input[xp->pos];
        if (c == quote) {
            xp->pos++;
            buf[i] = '\0';
            return (int)i;
        }
        buf[i++] = c;
        xp->pos++;
    }

    buf[i] = '\0';
    return -1;
}

/* Parse attribute */
static struct xml_attr *xml_parse_attr(struct xml_parser *xp) {
    struct xml_attr *attr;
    char name[128];
    char value[MAKS_TEKS];

    xml_skip_ws(xp);

    if (xp->pos >= xp->input_len) return NULL;

    /* Parse name */
    if (xml_parse_ident(xp, name, sizeof(name)) == 0) return NULL;

    xml_skip_ws(xp);
    if (xp->input[xp->pos] != '=') return NULL;
    xp->pos++;
    xml_skip_ws(xp);

    /* Parse value */
    if (xml_parse_quoted(xp, value, sizeof(value)) < 0) return NULL;

    /* Create attribute */
    attr = calloc(1, sizeof(*attr));
    if (!attr) return NULL;

    strcpy(attr->name, name);
    strcpy(attr->value, value);

    return attr;
}

/* Parse text content */
static char *xml_parse_text(struct xml_parser *xp, const char *end_tag) {
    char *text;
    size_t start = xp->pos;
    size_t len;
    size_t end_tag_len = strlen(end_tag);

    /* Find end tag */
    while (xp->pos < xp->input_len) {
        if (xp->input[xp->pos] == '<') {
            /* Check if it's the end tag */
            if (strncmp(xp->input + xp->pos, end_tag, end_tag_len) == 0) {
                break;
            }
        }
        xp->pos++;
    }

    len = xp->pos - start;
    text = malloc(len + 1);
    if (!text) return NULL;

    memcpy(text, xp->input + start, len);
    text[len] = '\0';

    return text;
}

/* Free XML node */
void xml_free_node(struct xml_node *node) {
    struct xml_attr *attr, *next_attr;
    struct xml_node *child, *next_child;

    if (!node) return;

    /* Free attributes */
    attr = node->attrs;
    while (attr) {
        next_attr = attr->next;
        free(attr);
        attr = next_attr;
    }

    /* Free children */
    child = node->children;
    while (child) {
        next_child = child->next;
        xml_free_node(child);
        child = next_child;
    }

    free(node);
}

/* Find child by name */
struct xml_node *xml_find_child(struct xml_node *parent, const char *name) {
    struct xml_node *child;

    if (!parent || !name) return NULL;

    child = parent->children;
    while (child) {
        if (strcmp(child->name, name) == 0) return child;
        child = child->next;
    }

    return NULL;
}

/* Get attribute value */
const char *xml_get_attr(struct xml_node *node, const char *name) {
    struct xml_attr *attr;

    if (!node || !name) return NULL;

    attr = node->attrs;
    while (attr) {
        if (strcmp(attr->name, name) == 0) return attr->value;
        attr = attr->next;
    }

    return NULL;
}

/* Find all children by name */
struct xml_node *xml_find_children(struct xml_node *parent, const char *name,
                                    int *count) {
    struct xml_node *result = NULL;
    struct xml_node *child;
    struct xml_node *last = NULL;
    int n = 0;

    if (!parent || !name || !count) return NULL;

    child = parent->children;
    while (child) {
        if (strcmp(child->name, name) == 0) {
            struct xml_node *copy = calloc(1, sizeof(*copy));
            if (copy) {
                memcpy(copy, child, sizeof(*copy));
                copy->next = NULL;

                if (!result) {
                    result = copy;
                } else {
                    last->next = copy;
                }
                last = copy;
                n++;
            }
        }
        child = child->next;
    }

    *count = n;
    return result;
}

/* Parse XML element */
static struct xml_node *xml_parse_element(struct xml_parser *xp) {
    struct xml_node *node;
    struct xml_attr *last_attr = NULL;
    char tag_name[128];
    int self_closing = 0;

    xml_skip_ws(xp);

    /* Expect '<' */
    if (xp->pos >= xp->input_len || xp->input[xp->pos] != '<') {
        return NULL;
    }
    xp->pos++;

    /* Check for closing tag */
    if (xp->input[xp->pos] == '/') {
        return NULL;
    }

    /* Check for comment or declaration */
    if (xp->input[xp->pos] == '!' || xp->input[xp->pos] == '?') {
        /* Skip until '>' */
        while (xp->pos < xp->input_len && xp->input[xp->pos] != '>') {
            xp->pos++;
        }
        if (xp->pos < xp->input_len) xp->pos++;
        return NULL;
    }

    /* Parse tag name */
    if (xml_parse_ident(xp, tag_name, sizeof(tag_name)) == 0) {
        return NULL;
    }

    /* Create node */
    node = calloc(1, sizeof(*node));
    if (!node) return NULL;

    node->type = XML_NODE_ELEMENT;
    strcpy(node->name, tag_name);

    /* Parse attributes */
    while (xp->pos < xp->input_len) {
        struct xml_attr *attr;

        xml_skip_ws(xp);

        if (xp->input[xp->pos] == '>') {
            xp->pos++;
            break;
        }

        if (xp->input[xp->pos] == '/') {
            xp->pos++;
            if (xp->input[xp->pos] == '>') {
                xp->pos++;
                self_closing = 1;
            }
            break;
        }

        attr = xml_parse_attr(xp);
        if (attr) {
            if (!node->attrs) {
                node->attrs = attr;
            } else {
                last_attr->next = attr;
            }
            last_attr = attr;
        } else {
            break;
        }
    }

    if (self_closing) return node;

    /* Parse content */
    {
        char end_tag[256];
        struct xml_node *last_child = NULL;

        snprintf(end_tag, sizeof(end_tag), "</%s>", tag_name);

        while (xp->pos < xp->input_len) {
            struct xml_node *child;
            size_t end_pos;

            xml_skip_ws(xp);

            /* Check for end tag */
            if (strncmp(xp->input + xp->pos, end_tag, strlen(end_tag)) == 0) {
                xp->pos += strlen(end_tag);
                break;
            }

            /* Try to parse child element */
            if (xp->input[xp->pos] == '<') {
                child = xml_parse_element(xp);
                if (child) {
                    if (!node->children) {
                        node->children = child;
                    } else {
                        last_child->next = child;
                    }
                    last_child = child;
                } else {
                    /* Skip unknown content */
                    while (xp->pos < xp->input_len && xp->input[xp->pos] != '>') {
                        xp->pos++;
                    }
                    if (xp->pos < xp->input_len) xp->pos++;
                }
            } else {
                /* Text content - skip for now or store */
                while (xp->pos < xp->input_len && xp->input[xp->pos] != '<') {
                    xp->pos++;
                }
            }
        }
    }

    return node;
}

/* Parse XML document */
static struct xml_node *xml_parse(const char *input, int input_len) {
    struct xml_parser xp;
    struct xml_node *root = NULL;
    struct xml_node *last = NULL;

    memset(&xp, 0, sizeof(xp));
    xp.input = input;
    xp.input_len = input_len;
    xp.pos = 0;

    /* Parse all root elements */
    while (xp.pos < xp.input_len) {
        struct xml_node *node = xml_parse_element(&xp);
        if (node) {
            if (!root) {
                root = node;
            } else {
                last->next = node;
            }
            last = node;
        } else {
            xp.pos++;
        }
    }

    return root;
}

/* ============================================================
 * FUNGSI XML - WRITING
 * ============================================================ */

/* XML escape string */
static int xml_escape(const char *input, char *output, size_t output_size) {
    size_t i = 0;
    size_t j = 0;

    while (input[i] && j < output_size - 6) {
        switch (input[i]) {
            case '&':
                strcpy(output + j, "&amp;");
                j += 5;
                break;
            case '<':
                strcpy(output + j, "&lt;");
                j += 4;
                break;
            case '>':
                strcpy(output + j, "&gt;");
                j += 4;
                break;
            case '"':
                strcpy(output + j, "&quot;");
                j += 6;
                break;
            case '\'':
                strcpy(output + j, "&apos;");
                j += 6;
                break;
            default:
                output[j++] = input[i];
                break;
        }
        i++;
    }
    output[j] = '\0';
    return (int)j;
}

/* Write XML to buffer */
struct xml_writer {
    char *buf;
    int size;
    int capacity;
};

static void xmlw_init(struct xml_writer *xw, char *buf, int capacity) {
    xw->buf = buf;
    xw->size = 0;
    xw->capacity = capacity;
    xw->buf[0] = '\0';
}

static void xmlw_append(struct xml_writer *xw, const char *fmt, ...) {
    va_list args;
    int written;
    int remaining;

    if (xw->size >= xw->capacity - 1) return;

    remaining = xw->capacity - xw->size - 1;
    va_start(args, fmt);
    written = vsnprintf(xw->buf + xw->size, remaining, fmt, args);
    va_end(args);

    if (written > 0) {
        xw->size += written;
        if (xw->size >= xw->capacity) {
            xw->size = xw->capacity - 1;
        }
        xw->buf[xw->size] = '\0';
    }
}

static void xmlw_indent(struct xml_writer *xw, int level) {
    int i;
    for (i = 0; i < level; i++) {
        xmlw_append(xw, "  ");
    }
}

/* ============================================================
 * FUNGSI SHARED STRINGS
 * ============================================================ */

/* Cari atau tambah shared string */
static int xlsx_find_or_add_shared_string(struct xlsx_workbook *wb,
                                          const char *text) {
    int i;

    /* Cari existing */
    for (i = 0; i < wb->num_shared_strings; i++) {
        if (strcmp(wb->shared_strings[i].text, text) == 0) {
            wb->shared_strings[i].count++;
            return i;
        }
    }

    /* Tambah baru */
    if (wb->num_shared_strings >= wb->max_shared_strings) {
        int new_max = wb->max_shared_strings * 2 + 256;
        struct xlsx_shared_string *new_ss;

        new_ss = realloc(wb->shared_strings, 
                         new_max * sizeof(*wb->shared_strings));
        if (!new_ss) return -1;

        wb->shared_strings = new_ss;
        wb->max_shared_strings = new_max;
    }

    i = wb->num_shared_strings;
    strncpy(wb->shared_strings[i].text, text, MAKS_TEKS - 1);
    wb->shared_strings[i].text[MAKS_TEKS - 1] = '\0';
    wb->shared_strings[i].count = 1;
    wb->num_shared_strings++;

    return i;
}

/* ============================================================
 * PARSING XLSX
 * ============================================================ */

/* Parse shared strings */
static int xlsx_parse_shared_strings(struct xlsx_workbook *wb,
                                     const char *content, int content_len) {
    struct xml_node *root, *sst, *si_node;
    int count = 0;

    root = xml_parse(content, content_len);
    if (!root) return -1;

    /* Find sst element */
    sst = root;
    while (sst && strcmp(sst->name, "sst") != 0) {
        sst = sst->next;
    }

    if (!sst) {
        xml_free_node(root);
        return -1;
    }

    /* Parse si (shared string item) elements */
    si_node = sst->children;
    while (si_node) {
        if (strcmp(si_node->name, "si") == 0) {
            struct xml_node *t_node = xml_find_child(si_node, "t");

            if (t_node && t_node->text[0]) {
                /* Add to shared strings */
                if (wb->num_shared_strings >= wb->max_shared_strings) {
                    int new_max = wb->max_shared_strings * 2 + 256;
                    struct xlsx_shared_string *new_ss;

                    new_ss = realloc(wb->shared_strings,
                                     new_max * sizeof(*wb->shared_strings));
                    if (!new_ss) {
                        xml_free_node(root);
                        return -1;
                    }
                    wb->shared_strings = new_ss;
                    wb->max_shared_strings = new_max;
                }

                strncpy(wb->shared_strings[wb->num_shared_strings].text,
                        t_node->text, MAKS_TEKS - 1);
                wb->shared_strings[wb->num_shared_strings].text[MAKS_TEKS - 1] = '\0';
                wb->shared_strings[wb->num_shared_strings].count = 0;
                wb->num_shared_strings++;
            }
        }
        si_node = si_node->next;
    }

    xml_free_node(root);
    return 0;
}

/* Parse worksheet */
static int xlsx_parse_worksheet(struct xlsx_sheet *sheet,
                                 const char *content, int content_len) {
    struct xml_node *root, *sheetData, *row_node, *cell_node;
    int max_col = 0, max_row = 0;

    root = xml_parse(content, content_len);
    if (!root) return -1;

    /* Find sheetData */
    sheetData = root;
    while (sheetData && strcmp(sheetData->name, "sheetData") != 0) {
        sheetData = sheetData->next;
    }
    if (!sheetData) {
        /* Try finding it as child of worksheet */
        struct xml_node *ws = root;
        while (ws && strcmp(ws->name, "worksheet") != 0) {
            ws = ws->next;
        }
        if (ws) {
            sheetData = xml_find_child(ws, "sheetData");
        }
    }

    if (!sheetData) {
        xml_free_node(root);
        return 0;  /* Empty sheet */
    }

    /* Parse rows */
    row_node = sheetData->children;
    while (row_node) {
        if (strcmp(row_node->name, "row") == 0) {
            const char *r_attr = xml_get_attr(row_node, "r");
            int row_num = r_attr ? atoi(r_attr) - 1 : max_row;

            /* Parse cells */
            cell_node = row_node->children;
            while (cell_node) {
                if (strcmp(cell_node->name, "c") == 0) {
                    struct xlsx_cell cell;
                    const char *ref = xml_get_attr(cell_node, "r");
                    const char *type = xml_get_attr(cell_node, "t");
                    struct xml_node *v_node;
                    struct xml_node *f_node;

                    memset(&cell, 0, sizeof(cell));

                    if (ref) {
                        xlsx_parse_cell_ref(ref, &cell.col, &cell.row);
                    } else {
                        cell.col = max_col;
                        cell.row = row_num;
                    }

                    /* Get value */
                    v_node = xml_find_child(cell_node, "v");
                    f_node = xml_find_child(cell_node, "f");

                    if (f_node) {
                        cell.is_formula = 1;
                        /* Formula value in v_node */
                    }

                    if (v_node) {
                        if (type && strcmp(type, "s") == 0) {
                            /* Shared string index */
                            cell.shared_string_idx = atoi(v_node->text);
                            cell.is_number = 0;
                        } else if (type && strcmp(type, "str") == 0) {
                            /* String (formula result) */
                            strncpy(cell.value, v_node->text, MAKS_TEKS - 1);
                            cell.is_number = 0;
                        } else if (type && strcmp(type, "b") == 0) {
                            /* Boolean */
                            strcpy(cell.value, 
                                   (v_node->text[0] == '1') ? "TRUE" : "FALSE");
                            cell.is_number = 0;
                        } else {
                            /* Number */
                            strncpy(cell.value, v_node->text, MAKS_TEKS - 1);
                            cell.is_number = 1;
                        }
                    }

                    /* Add cell to sheet */
                    if (sheet->num_cells >= sheet->max_cells) {
                        int new_max = sheet->max_cells * 2 + 1000;
                        struct xlsx_cell *new_cells;

                        new_cells = realloc(sheet->cells,
                                            new_max * sizeof(*sheet->cells));
                        if (!new_cells) {
                            xml_free_node(root);
                            return -1;
                        }
                        sheet->cells = new_cells;
                        sheet->max_cells = new_max;
                    }

                    sheet->cells[sheet->num_cells++] = cell;

                    if (cell.col > max_col) max_col = cell.col;
                    if (cell.row > max_row) max_row = cell.row;
                }
                cell_node = cell_node->next;
            }

            if (row_num > max_row) max_row = row_num;
        }
        row_node = row_node->next;
    }

    sheet->max_col = max_col;
    sheet->max_row = max_row;

    xml_free_node(root);
    return 0;
}

/* ============================================================
 * BUKA XLSX
 * ============================================================ */

/* Buka file XLSX */
int buka_xlsx(struct buffer_tabel **pbuf, const char *fn) {
    struct zip_file *zf;
    struct xlsx_workbook wb;
    unsigned char *content;
    size_t content_len;
    struct zip_entry *entry;
    int i;

    /* Content types */
    static const char *content_types = 
        "[Content_Types].xml";

    /* Shared strings file */
    static const char *shared_strings_path = 
        "xl/sharedStrings.xml";

    /* Sheet paths pattern */
    char sheet_path[64];

    memset(&wb, 0, sizeof(wb));

    /* Buka ZIP file */
    zf = zip_open_read(fn);
    if (!zf) {
        xlsx_set_error("Cannot open XLSX file: %s", fn);
        return -1;
    }

    /* Alokasi content buffer */
    content = malloc(XLSX_MAX_CONTENT);
    if (!content) {
        zip_close(zf);
        return -1;
    }

    /* Parse workbook.xml to get sheet names */
    entry = zip_find_entry(zf, "xl/workbook.xml");
    if (entry) {
        if (zip_extract_entry(zf, entry, content, &content_len, 
                              XLSX_MAX_CONTENT) == 0) {
            /* Parse workbook - basic sheet info */
            struct xml_node *root = xml_parse((char*)content, content_len);
            if (root) {
                struct xml_node *wb_node = root;
                while (wb_node && strcmp(wb_node->name, "workbook") != 0) {
                    wb_node = wb_node->next;
                }
                if (wb_node) {
                    struct xml_node *sheets = xml_find_child(wb_node, "sheets");
                    if (sheets) {
                        struct xml_node *sheet_node = sheets->children;
                        int sheet_count = 0;

                        /* Count sheets */
                        while (sheet_node) {
                            if (strcmp(sheet_node->name, "sheet") == 0) {
                                sheet_count++;
                            }
                            sheet_node = sheet_node->next;
                        }

                        /* Allocate sheets */
                        wb.sheets = calloc(sheet_count, sizeof(*wb.sheets));
                        wb.num_sheets = sheet_count;

                        /* Parse each sheet */
                        sheet_node = sheets->children;
                        i = 0;
                        while (sheet_node && i < sheet_count) {
                            if (strcmp(sheet_node->name, "sheet") == 0) {
                                const char *name = xml_get_attr(sheet_node, "name");
                                const char *id = xml_get_attr(sheet_node, "r:id");

                                if (name) {
                                    strncpy(wb.sheets[i].name, name, 127);
                                }
                                wb.sheets[i].cells = NULL;
                                wb.sheets[i].num_cells = 0;
                                wb.sheets[i].max_cells = 0;
                                i++;
                            }
                            sheet_node = sheet_node->next;
                        }
                    }
                }
                xml_free_node(root);
            }
        }
    }

    /* Parse shared strings */
    entry = zip_find_entry(zf, shared_strings_path);
    if (entry) {
        if (zip_extract_entry(zf, entry, content, &content_len,
                              XLSX_MAX_CONTENT) == 0) {
            xlsx_parse_shared_strings(&wb, (char*)content, content_len);
        }
    }

    /* Parse first sheet */
    snprintf(sheet_path, sizeof(sheet_path), "xl/worksheets/sheet1.xml");
    entry = zip_find_entry(zf, sheet_path);
    if (entry) {
        if (zip_extract_entry(zf, entry, content, &content_len,
                              XLSX_MAX_CONTENT) == 0) {
            if (wb.num_sheets > 0) {
                wb.sheets[0].cells = malloc(1000 * sizeof(struct xlsx_cell));
                wb.sheets[0].max_cells = 1000;
                xlsx_parse_worksheet(&wb.sheets[0], (char*)content, content_len);
            }
        }
    }

    zip_close(zf);
    free(content);

    /* Convert to buffer_tabel */
    if (wb.num_sheets > 0 && wb.sheets[0].num_cells > 0) {
        struct xlsx_sheet *sheet = &wb.sheets[0];
        int max_col = sheet->max_col + 1;
        int max_row = sheet->max_row + 1;
        int c, r;

        /* Ensure minimum size */
        if (max_col < 1) max_col = 1;
        if (max_row < 1) max_row = 1;

        /* Create buffer */
        bebas_buffer(*pbuf);
        *pbuf = buat_buffer(max_row, max_col);

        if (*pbuf) {
            /* Fill cells */
            for (i = 0; i < sheet->num_cells; i++) {
                struct xlsx_cell *cell = &sheet->cells[i];

                if (cell->col < (*pbuf)->cfg.kolom && 
                    cell->row < (*pbuf)->cfg.baris) {

                    if (cell->shared_string_idx >= 0 &&
                        cell->shared_string_idx < wb.num_shared_strings) {
                        /* Shared string */
                        strncpy((*pbuf)->isi[cell->row][cell->col],
                                wb.shared_strings[cell->shared_string_idx].text,
                                MAKS_TEKS - 1);
                    } else {
                        /* Direct value */
                        strncpy((*pbuf)->isi[cell->row][cell->col],
                                cell->value, MAKS_TEKS - 1);
                    }
                }
            }
        }
    } else {
        /* Empty sheet - create default buffer */
        bebas_buffer(*pbuf);
        *pbuf = buat_buffer(100, 26);
    }

    /* Cleanup */
    for (i = 0; i < wb.num_sheets; i++) {
        free(wb.sheets[i].cells);
    }
    free(wb.sheets);
    free(wb.shared_strings);

    return 0;
}

/* ============================================================
 * SIMPAN XLSX
 * ============================================================ */

/* Generate [Content_Types].xml */
static char *generate_content_types(int num_sheets, int *out_len) {
    static char buf[4096];
    struct xml_writer xw;
    int i;

    xmlw_init(&xw, buf, sizeof(buf));

    xmlw_append(&xw, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n");
    xmlw_append(&xw, "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">\n");
    xmlw_append(&xw, "  <Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>\n");
    xmlw_append(&xw, "  <Default Extension=\"xml\" ContentType=\"application/xml\"/>\n");
    xmlw_append(&xw, "  <Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>\n");

    for (i = 0; i < num_sheets; i++) {
        xmlw_append(&xw, "  <Override PartName=\"/xl/worksheets/sheet%d.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>\n", i + 1);
    }

    xmlw_append(&xw, "  <Override PartName=\"/xl/sharedStrings.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sharedStrings+xml\"/>\n");
    xmlw_append(&xw, "</Types>");

    *out_len = xw.size;
    return buf;
}

/* Generate _rels/.rels */
static char *generate_rels(int *out_len) {
    static char buf[1024];
    struct xml_writer xw;

    xmlw_init(&xw, buf, sizeof(buf));

    xmlw_append(&xw, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n");
    xmlw_append(&xw, "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n");
    xmlw_append(&xw, "  <Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/>\n");
    xmlw_append(&xw, "</Relationships>");

    *out_len = xw.size;
    return buf;
}

/* Generate xl/_rels/workbook.xml.rels */
static char *generate_workbook_rels(int num_sheets, int *out_len) {
    static char buf[4096];
    struct xml_writer xw;
    int i;

    xmlw_init(&xw, buf, sizeof(buf));

    xmlw_append(&xw, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n");
    xmlw_append(&xw, "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n");

    for (i = 0; i < num_sheets; i++) {
        xmlw_append(&xw, "  <Relationship Id=\"rId%d\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet%d.xml\"/>\n", i + 1, i + 1);
    }

    xmlw_append(&xw, "  <Relationship Id=\"rId%d\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/sharedStrings\" Target=\"sharedStrings.xml\"/>\n", num_sheets + 1);
    xmlw_append(&xw, "</Relationships>");

    *out_len = xw.size;
    return buf;
}

/* Generate xl/workbook.xml */
static char *generate_workbook(int num_sheets, const char **sheet_names,
                                int *out_len) {
    static char buf[4096];
    struct xml_writer xw;
    int i;

    xmlw_init(&xw, buf, sizeof(buf));

    xmlw_append(&xw, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n");
    xmlw_append(&xw, "<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">\n");
    xmlw_append(&xw, "  <sheets>\n");

    for (i = 0; i < num_sheets; i++) {
        const char *name = (sheet_names && sheet_names[i]) ? sheet_names[i] : "Sheet";
        xmlw_append(&xw, "    <sheet name=\"%s\" sheetId=\"%d\" r:id=\"rId%d\"/>\n", 
                    name, i + 1, i + 1);
    }

    xmlw_append(&xw, "  </sheets>\n");
    xmlw_append(&xw, "</workbook>");

    *out_len = xw.size;
    return buf;
}

/* Generate xl/sharedStrings.xml */
static char *generate_shared_strings(struct xlsx_workbook *wb, int *out_len) {
    static char *buf = NULL;
    struct xml_writer xw;
    int i;
    char escaped[MAKS_TEKS * 2];

    if (!buf) {
        buf = malloc(1024 * 1024);  /* 1MB buffer */
    }
    if (!buf) return NULL;

    xmlw_init(&xw, buf, 1024 * 1024);

    xmlw_append(&xw, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n");
    xmlw_append(&xw, "<sst xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" count=\"%d\" uniqueCount=\"%d\">\n",
                wb->num_shared_strings, wb->num_shared_strings);

    for (i = 0; i < wb->num_shared_strings; i++) {
        xml_escape(wb->shared_strings[i].text, escaped, sizeof(escaped));
        xmlw_append(&xw, "  <si><t>%s</t></si>\n", escaped);
    }

    xmlw_append(&xw, "</sst>");

    *out_len = xw.size;
    return buf;
}

/* Generate xl/worksheets/sheetN.xml */
static char *generate_worksheet(const struct buffer_tabel *buf,
                                 struct xlsx_workbook *wb,
                                 int *out_len) {
    static char *ws_buf = NULL;
    struct xml_writer xw;
    int r, c;
    char col_letters[8];
    char cell_ref[16];
    char escaped[MAKS_TEKS * 2];
    int shared_idx;
    int has_content;
    int first_row, last_row, first_col, last_col;

    if (!ws_buf) {
        ws_buf = malloc(4 * 1024 * 1024);  /* 4MB buffer */
    }
    if (!ws_buf) return NULL;

    xmlw_init(&xw, ws_buf, 4 * 1024 * 1024);

    xmlw_append(&xw, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n");
    xmlw_append(&xw, "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">\n");
    xmlw_append(&xw, "  <sheetData>\n");

    /* Find content bounds */
    first_row = -1;
    last_row = -1;
    first_col = buf->cfg.kolom;
    last_col = 0;

    for (r = 0; r < buf->cfg.baris; r++) {
        has_content = 0;
        for (c = 0; c < buf->cfg.kolom; c++) {
            if (buf->isi[r][c][0]) {
                has_content = 1;
                if (first_row < 0) first_row = r;
                last_row = r;
                if (c < first_col) first_col = c;
                if (c > last_col) last_col = c;
            }
        }
    }

    /* Generate rows with content */
    for (r = 0; r < buf->cfg.baris; r++) {
        has_content = 0;
        for (c = 0; c < buf->cfg.kolom; c++) {
            if (buf->isi[r][c][0]) {
                has_content = 1;
                break;
            }
        }

        if (!has_content) continue;

        xmlw_append(&xw, "    <row r=\"%d\">\n", r + 1);

        for (c = 0; c < buf->cfg.kolom; c++) {
            const char *cell_val = buf->isi[r][c];
            if (!cell_val[0]) continue;

            xlsx_col_to_letters(c, col_letters, sizeof(col_letters));
            snprintf(cell_ref, sizeof(cell_ref), "%s%d", col_letters, r + 1);

            /* Check if number */
            char *endp;
            double num = strtod(cell_val, &endp);
            int is_number = (*endp == '\0' && cell_val[0] != '\0');

            if (is_number) {
                /* Write as number */
                xmlw_append(&xw, "      <c r=\"%s\"><v>%s</v></c>\n", 
                            cell_ref, cell_val);
            } else {
                /* Write as shared string */
                shared_idx = xlsx_find_or_add_shared_string(wb, cell_val);
                xmlw_append(&xw, "      <c r=\"%s\" t=\"s\"><v>%d</v></c>\n",
                            cell_ref, shared_idx);
            }
        }

        xmlw_append(&xw, "    </row>\n");
    }

    xmlw_append(&xw, "  </sheetData>\n");
    xmlw_append(&xw, "</worksheet>");

    *out_len = xw.size;
    return ws_buf;
}

/* Simpan buffer ke XLSX */
int simpan_xlsx(const struct buffer_tabel *buf, const char *fn) {
    struct zip_writer *zw;
    struct xlsx_workbook wb;
    char *content;
    int content_len;
    const char *sheet_names[1] = { "Sheet1" };
    int result = 0;

    if (!buf || !fn) return -1;

    /* Initialize workbook */
    memset(&wb, 0, sizeof(wb));
    wb.num_sheets = 1;
    wb.sheets = calloc(1, sizeof(*wb.sheets));
    if (!wb.sheets) return -1;

    /* Create ZIP writer */
    zw = zip_create(fn);
    if (!zw) {
        free(wb.sheets);
        return -1;
    }

    /* Generate and add [Content_Types].xml */
    content = generate_content_types(1, &content_len);
    zip_add_entry(zw, "[Content_Types].xml", (unsigned char*)content, content_len);

    /* Generate and add _rels/.rels */
    content = generate_rels(&content_len);
    zip_add_entry(zw, "_rels/.rels", (unsigned char*)content, content_len);

    /* Generate and add xl/workbook.xml */
    content = generate_workbook(1, sheet_names, &content_len);
    zip_add_entry(zw, "xl/workbook.xml", (unsigned char*)content, content_len);

    /* Generate and add xl/_rels/workbook.xml.rels */
    content = generate_workbook_rels(1, &content_len);
    zip_add_entry(zw, "xl/_rels/workbook.xml.rels", (unsigned char*)content, content_len);

    /* Generate worksheet */
    content = generate_worksheet(buf, &wb, &content_len);
    if (content) {
        zip_add_entry(zw, "xl/worksheets/sheet1.xml", 
                      (unsigned char*)content, content_len);
    }

    /* Generate shared strings */
    if (wb.num_shared_strings > 0) {
        content = generate_shared_strings(&wb, &content_len);
        if (content) {
            zip_add_entry(zw, "xl/sharedStrings.xml",
                          (unsigned char*)content, content_len);
        }
    }

    /* Finalize ZIP */
    if (zip_finalize(zw) != 0) {
        result = -1;
    }

    /* Cleanup */
    zip_writer_close(zw);
    free(wb.sheets);
    free(wb.shared_strings);

    return result;
}

/* ============================================================
 * FUNGSI INFORMASI
 * ============================================================ */

/* Cek apakah file adalah XLSX valid */
int xlsx_apakah_valid(const char *fn) {
    FILE *fp;
    unsigned char sig[4];

    fp = fopen(fn, "rb");
    if (!fp) return 0;

    if (fread(sig, 1, 4, fp) != 4) {
        fclose(fp);
        return 0;
    }
    fclose(fp);

    /* Check ZIP signature */
    return (sig[0] == 0x50 && sig[1] == 0x4B &&
            sig[2] == 0x03 && sig[3] == 0x04);
}

/* Dapatkan info sheet */
int xlsx_info_sheet(const char *fn, int *num_sheets, int *max_row, int *max_col) {
    struct zip_file *zf;
    struct zip_entry *entry;
    unsigned char *content;
    size_t content_len;
    char sheet_path[64];

    zf = zip_open_read(fn);
    if (!zf) return -1;

    content = malloc(XLSX_MAX_CONTENT);
    if (!content) {
        zip_close(zf);
        return -1;
    }

    /* Default values */
    if (num_sheets) *num_sheets = 1;
    if (max_row) *max_row = 0;
    if (max_col) *max_col = 0;

    /* Parse first sheet */
    snprintf(sheet_path, sizeof(sheet_path), "xl/worksheets/sheet1.xml");
    entry = zip_find_entry(zf, sheet_path);
    if (entry) {
        if (zip_extract_entry(zf, entry, content, &content_len,
                              XLSX_MAX_CONTENT) == 0) {
            struct xml_node *root = xml_parse((char*)content, content_len);
            if (root) {
                /* Find dimension */
                struct xml_node *ws = root;
                while (ws && strcmp(ws->name, "worksheet") != 0) {
                    ws = ws->next;
                }
                if (ws) {
                    struct xml_node *dim = xml_find_child(ws, "dimension");
                    if (dim) {
                        const char *ref = xml_get_attr(dim, "ref");
                        if (ref) {
                            /* Parse ref like "A1:Z100" */
                            char *colon = strchr(ref, ':');
                            if (colon) {
                                int col, row;
                                xlsx_parse_cell_ref(colon + 1, &col, &row);
                                if (max_row) *max_row = row + 1;
                                if (max_col) *max_col = col + 1;
                            }
                        }
                    }
                }
                xml_free_node(root);
            }
        }
    }

    free(content);
    zip_close(zf);

    return 0;
}
