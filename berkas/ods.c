/* ============================================================
 * TABEL v3.0
 * Berkas: ods.c - Format Berkas OpenDocument Spreadsheet (ODS) - ZIP + XML
 * Standar: C89 + POSIX.1-2008
 * Penulis: Chandra Lesmana
 * ============================================================ */

#include "tabel.h"

/* ============================================================
 * KONSTANTA ODS
 * ============================================================ */
#define ODS_MAX_FILES           256
#define ODS_MAX_PATH            256
#define ODS_MAX_CONTENT         (16 * 1024 * 1024)
#define ODS_BUFFER_SIZE         (64 * 1024)

/* ZIP file signature */
#define ZIP_LOCAL_FILE_HEADER_SIG    0x04034B50
#define ZIP_CENTRAL_DIR_HEADER_SIG   0x02014B50
#define ZIP_END_CENTRAL_DIR_SIG      0x06054B50

/* OpenDocument namespaces */
#define ODS_NS_OFFICE    "urn:oasis:names:tc:opendocument:xmlns:office:1.0"
#define ODS_NS_SPREADSHEET "urn:oasis:names:tc:opendocument:xmlns:table:1.0"
#define ODS_NS_TEXT      "urn:oasis:names:tc:opendocument:xmlns:text:1.0"
#define ODS_NS_NUMBER    "urn:oasis:names:tc:opendocument:xmlns:datastyle:1.0"
#define ODS_NS_STYLE     "urn:oasis:names:tc:opendocument:xmlns:style:1.0"
#define ODS_NS_FO        "urn:oasis:names:tc:opendocument:xmlns:xsl-fo-compatible:1.0"
#define ODS_NS_META      "urn:oasis:names:tc:opendocument:xmlns:meta:1.0"

/* ============================================================
 * STRUKTUR ZIP (Reuse dari xlsx.txt)
 * ============================================================ */

struct zip_entry {
    char filename[ODS_MAX_PATH];
    unsigned int compressed_size;
    unsigned int uncompressed_size;
    unsigned int crc32;
    unsigned short compression;
    long data_offset;
    long local_header_offset;
};

struct zip_file {
    FILE *fp;
    struct zip_entry entries[ODS_MAX_FILES];
    int num_entries;
    int is_writing;
};

struct zip_write_entry {
    char filename[ODS_MAX_PATH];
    unsigned char *data;
    size_t data_len;
    unsigned int crc32;
    time_t mod_time;
};

struct zip_writer {
    FILE *fp;
    struct zip_write_entry entries[ODS_MAX_FILES];
    int num_entries;
};

/* ============================================================
 * VARIABEL GLOBAL
 * ============================================================ */
static char ods_error_msg[512] = "";
static unsigned int crc32_table[256];
static int crc32_table_init = 0;

/* ============================================================
 * FUNGSI HELPER
 * ============================================================ */

static void ods_set_error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(ods_error_msg, sizeof(ods_error_msg), fmt, args);
    va_end(args);
}

const char *ods_get_error(void) {
    return ods_error_msg;
}

/* CRC32 functions */
static void crc32_init_table(void) {
    unsigned int i, j, crc;
    if (crc32_table_init) return;
    for (i = 0; i < 256; i++) {
        crc = i;
        for (j = 0; j < 8; j++) {
            if (crc & 1) crc = (crc >> 1) ^ 0xEDB88320;
            else crc >>= 1;
        }
        crc32_table[i] = crc;
    }
    crc32_table_init = 1;
}

static unsigned int crc32_calc(const unsigned char *buf, size_t len) {
    unsigned int crc = 0xFFFFFFFF;
    size_t i;
    if (!crc32_table_init) crc32_init_table();
    for (i = 0; i < len; i++) {
        crc = crc32_table[(crc ^ buf[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}

/* Little-endian helpers */
static unsigned short read_le16(const unsigned char *p) {
    return (unsigned short)(p[0] | (p[1] << 8));
}

static unsigned int read_le32(const unsigned char *p) {
    return (unsigned int)(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
}

static void write_le16(unsigned char *p, unsigned short v) {
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
}

static void write_le32(unsigned char *p, unsigned int v) {
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
    p[2] = (v >> 16) & 0xFF;
    p[3] = (v >> 24) & 0xFF;
}

/* XML escape */
static int ods_xml_escape(const char *input, char *output, size_t output_size) {
    size_t i = 0, j = 0;
    while (input[i] && j < output_size - 8) {
        switch (input[i]) {
            case '&': strcpy(output + j, "&amp;"); j += 5; break;
            case '<': strcpy(output + j, "&lt;"); j += 4; break;
            case '>': strcpy(output + j, "&gt;"); j += 4; break;
            case '"': strcpy(output + j, "&quot;"); j += 6; break;
            case '\'': strcpy(output + j, "&apos;"); j += 6; break;
            case '\t': strcpy(output + j, "&#9;"); j += 4; break;
            case '\n': strcpy(output + j, "&#10;"); j += 5; break;
            case '\r': strcpy(output + j, "&#13;"); j += 5; break;
            default: output[j++] = input[i]; break;
        }
        i++;
    }
    output[j] = '\0';
    return (int)j;
}

/* XML unescape */
static int ods_xml_unescape(const char *input, char *output, size_t output_size) {
    size_t i = 0, j = 0;
    while (input[i] && j < output_size - 1) {
        if (input[i] == '&') {
            if (strncmp(input + i, "&amp;", 5) == 0) { output[j++] = '&'; i += 5; }
            else if (strncmp(input + i, "&lt;", 4) == 0) { output[j++] = '<'; i += 4; }
            else if (strncmp(input + i, "&gt;", 4) == 0) { output[j++] = '>'; i += 4; }
            else if (strncmp(input + i, "&quot;", 6) == 0) { output[j++] = '"'; i += 6; }
            else if (strncmp(input + i, "&apos;", 6) == 0) { output[j++] = '\''; i += 6; }
            else if (strncmp(input + i, "&#10;", 5) == 0) { output[j++] = '\n'; i += 5; }
            else if (strncmp(input + i, "&#13;", 5) == 0) { output[j++] = '\r'; i += 5; }
            else if (strncmp(input + i, "&#9;", 4) == 0) { output[j++] = '\t'; i += 4; }
            else if (strncmp(input + i, "&#", 2) == 0) {
                unsigned int cp = 0;
                i += 2;
                if (input[i] == 'x') {
                    i++;
                    while (isxdigit((unsigned char)input[i])) {
                        cp = cp * 16;
                        if (input[i] >= '0' && input[i] <= '9') cp += input[i] - '0';
                        else if (input[i] >= 'A' && input[i] <= 'F') cp += input[i] - 'A' + 10;
                        else cp += input[i] - 'a' + 10;
                        i++;
                    }
                } else {
                    while (isdigit((unsigned char)input[i])) {
                        cp = cp * 10 + (input[i] - '0');
                        i++;
                    }
                }
                if (input[i] == ';') i++;
                if (cp < 0x80) output[j++] = (char)cp;
                else if (cp < 0x800) {
                    output[j++] = (char)(0xC0 | (cp >> 6));
                    output[j++] = (char)(0x80 | (cp & 0x3F));
                } else {
                    output[j++] = (char)(0xE0 | (cp >> 12));
                    output[j++] = (char)(0x80 | ((cp >> 6) & 0x3F));
                    output[j++] = (char)(0x80 | (cp & 0x3F));
                }
            }
            else output[j++] = input[i++];
        } else {
            output[j++] = input[i++];
        }
    }
    output[j] = '\0';
    return (int)j;
}

/* ============================================================
 * ZIP READING FUNCTIONS
 * ============================================================ */

static long zip_find_end_central(FILE *fp) {
    unsigned char buf[512];
    long file_size, pos;
    int i;

    fseek(fp, 0, SEEK_END);
    file_size = ftell(fp);
    pos = file_size - sizeof(buf);
    if (pos < 0) pos = 0;

    while (pos >= 0) {
        size_t read_len;
        fseek(fp, pos, SEEK_SET);
        read_len = fread(buf, 1, sizeof(buf), fp);

        for (i = 0; i < (int)read_len - 3; i++) {
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

static struct zip_file *zip_open_read(const char *filename) {
    struct zip_file *zf;
    FILE *fp;
    unsigned char buf[64];
    long eocd_offset;
    unsigned int central_dir_offset;
    unsigned short total_entries;
    int i;

    fp = fopen(filename, "rb");
    if (!fp) {
        ods_set_error("Cannot open file: %s", filename);
        return NULL;
    }

    eocd_offset = zip_find_end_central(fp);
    if (eocd_offset < 0) {
        fclose(fp);
        ods_set_error("Invalid ZIP file");
        return NULL;
    }

    fseek(fp, eocd_offset, SEEK_SET);
    if (fread(buf, 1, 22, fp) != 22) {
        fclose(fp);
        return NULL;
    }

    total_entries = read_le16(buf + 10);
    central_dir_offset = read_le32(buf + 16);

    zf = calloc(1, sizeof(*zf));
    if (!zf) {
        fclose(fp);
        return NULL;
    }

    zf->fp = fp;
    zf->num_entries = total_entries;

    fseek(fp, central_dir_offset, SEEK_SET);

    for (i = 0; i < total_entries && i < ODS_MAX_FILES; i++) {
        unsigned short filename_len, extra_len, comment_len;
        struct zip_entry *entry = &zf->entries[i];

        if (fread(buf, 1, 46, fp) != 46) break;

        entry->compressed_size = read_le32(buf + 20);
        entry->uncompressed_size = read_le32(buf + 24);
        filename_len = read_le16(buf + 28);
        extra_len = read_le16(buf + 30);
        comment_len = read_le16(buf + 32);
        entry->compression = read_le16(buf + 10);
        entry->crc32 = read_le32(buf + 16);
        entry->local_header_offset = read_le32(buf + 42);

        if (filename_len >= ODS_MAX_PATH) filename_len = ODS_MAX_PATH - 1;
        fread(entry->filename, 1, filename_len, fp);
        entry->filename[filename_len] = '\0';

        fseek(fp, extra_len + comment_len, SEEK_CUR);
    }

    return zf;
}

static struct zip_entry *zip_find_entry(struct zip_file *zf, const char *filename) {
    int i;
    for (i = 0; i < zf->num_entries; i++) {
        if (strcmp(zf->entries[i].filename, filename) == 0) {
            return &zf->entries[i];
        }
    }
    return NULL;
}

static int zip_extract_entry(struct zip_file *zf, struct zip_entry *entry,
                             unsigned char *output, size_t *output_len,
                             size_t output_max) {
    unsigned char buf[64];
    unsigned char *compressed_data;

    fseek(zf->fp, entry->local_header_offset, SEEK_SET);
    if (fread(buf, 1, 30, zf->fp) != 30) return -1;

    /* Skip filename and extra */
    fseek(zf->fp, read_le16(buf + 26) + read_le16(buf + 28), SEEK_CUR);

    compressed_data = malloc(entry->compressed_size);
    if (!compressed_data) return -1;

    if (fread(compressed_data, 1, entry->compressed_size, zf->fp) != entry->compressed_size) {
        free(compressed_data);
        return -1;
    }

    if (entry->compression == 0) {
        /* Stored */
        if (entry->uncompressed_size > output_max) {
            free(compressed_data);
            return -1;
        }
        memcpy(output, compressed_data, entry->uncompressed_size);
        *output_len = entry->uncompressed_size;
    } else if (entry->compression == 8) {
        /* Deflate - for simplicity, we only support stored mode */
        /* In real implementation, you'd need inflate */
        ods_set_error("Deflate compression not supported");
        free(compressed_data);
        return -1;
    }

    free(compressed_data);
    return 0;
}

static void zip_close(struct zip_file *zf) {
    if (!zf) return;
    if (zf->fp) fclose(zf->fp);
    free(zf);
}

/* ============================================================
 * ZIP WRITING FUNCTIONS
 * ============================================================ */

static struct zip_writer *zip_create(const char *filename) {
    struct zip_writer *zw = calloc(1, sizeof(*zw));
    if (!zw) return NULL;

    zw->fp = fopen(filename, "wb");
    if (!zw->fp) {
        free(zw);
        return NULL;
    }
    return zw;
}

static int zip_add_entry(struct zip_writer *zw, const char *filename,
                         const unsigned char *data, size_t data_len) {
    struct zip_write_entry *entry;

    if (!zw || !filename || !data) return -1;
    if (zw->num_entries >= ODS_MAX_FILES) return -1;

    entry = &zw->entries[zw->num_entries];
    strncpy(entry->filename, filename, ODS_MAX_PATH - 1);
    entry->filename[ODS_MAX_PATH - 1] = '\0';

    entry->data = malloc(data_len);
    if (!entry->data) return -1;

    memcpy(entry->data, data, data_len);
    entry->data_len = data_len;
    entry->crc32 = crc32_calc(data, data_len);
    entry->mod_time = time(NULL);

    zw->num_entries++;
    return 0;
}

static void zip_write_local_header(FILE *fp, const char *filename,
                                    size_t size, unsigned int crc32) {
    unsigned char header[30];
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    unsigned short dos_time, dos_date;
    size_t filename_len = strlen(filename);

    dos_time = (tm->tm_sec / 2) | (tm->tm_min << 5) | (tm->tm_hour << 11);
    dos_date = tm->tm_mday | ((tm->tm_mon + 1) << 5) | ((tm->tm_year - 80) << 9);

    write_le32(header, ZIP_LOCAL_FILE_HEADER_SIG);
    write_le16(header + 4, 20);
    write_le16(header + 6, 0);
    write_le16(header + 8, 0);  /* Stored (no compression) */
    write_le16(header + 10, dos_time);
    write_le16(header + 12, dos_date);
    write_le32(header + 14, crc32);
    write_le32(header + 18, size);
    write_le32(header + 22, size);
    write_le16(header + 26, filename_len);
    write_le16(header + 28, 0);

    fwrite(header, 1, 30, fp);
    fwrite(filename, 1, filename_len, fp);
}

static void zip_write_central_header(FILE *fp, const char *filename,
                                      size_t size, unsigned int crc32,
                                      long local_offset) {
    unsigned char header[46];
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    unsigned short dos_time, dos_date;
    size_t filename_len = strlen(filename);

    dos_time = (tm->tm_sec / 2) | (tm->tm_min << 5) | (tm->tm_hour << 11);
    dos_date = tm->tm_mday | ((tm->tm_mon + 1) << 5) | ((tm->tm_year - 80) << 9);

    write_le32(header, ZIP_CENTRAL_DIR_HEADER_SIG);
    write_le16(header + 4, 20);
    write_le16(header + 6, 20);
    write_le16(header + 8, 0);
    write_le16(header + 10, 0);
    write_le16(header + 12, dos_time);
    write_le16(header + 14, dos_date);
    write_le32(header + 16, crc32);
    write_le32(header + 20, size);
    write_le32(header + 24, size);
    write_le16(header + 28, filename_len);
    write_le16(header + 30, 0);
    write_le16(header + 32, 0);
    write_le16(header + 34, 0);
    write_le16(header + 36, 0);
    write_le32(header + 38, 0);
    write_le32(header + 42, local_offset);

    fwrite(header, 1, 46, fp);
    fwrite(filename, 1, filename_len, fp);
}

static int zip_finalize(struct zip_writer *zw) {
    long local_offsets[ODS_MAX_FILES];
    long central_dir_start, central_dir_size;
    int i;
    unsigned char eocd[22];

    /* Write local headers and data */
    for (i = 0; i < zw->num_entries; i++) {
        struct zip_write_entry *entry = &zw->entries[i];
        local_offsets[i] = ftell(zw->fp);
        zip_write_local_header(zw->fp, entry->filename, entry->data_len, entry->crc32);
        fwrite(entry->data, 1, entry->data_len, zw->fp);
    }

    /* Write central directory */
    central_dir_start = ftell(zw->fp);
    for (i = 0; i < zw->num_entries; i++) {
        struct zip_write_entry *entry = &zw->entries[i];
        zip_write_central_header(zw->fp, entry->filename, entry->data_len,
                                  entry->crc32, local_offsets[i]);
    }
    central_dir_size = ftell(zw->fp) - central_dir_start;

    /* Write EOCD */
    write_le32(eocd, ZIP_END_CENTRAL_DIR_SIG);
    write_le16(eocd + 4, 0);
    write_le16(eocd + 6, 0);
    write_le16(eocd + 8, zw->num_entries);
    write_le16(eocd + 10, zw->num_entries);
    write_le32(eocd + 12, central_dir_size);
    write_le32(eocd + 16, central_dir_start);
    write_le16(eocd + 20, 0);
    fwrite(eocd, 1, 22, zw->fp);

    return 0;
}

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
 * ODS PARSING FUNCTIONS
 * ============================================================ */

struct ods_parser {
    const char *input;
    int input_len;
    int pos;
    int current_row;
    int current_col;
};

static void ods_skip_ws(struct ods_parser *xp) {
    while (xp->pos < xp->input_len && isspace((unsigned char)xp->input[xp->pos])) {
        xp->pos++;
    }
}

static int ods_at_string(struct ods_parser *xp, const char *s) {
    size_t len = strlen(s);
    if (xp->pos + len > xp->input_len) return 0;
    return strncmp(xp->input + xp->pos, s, len) == 0;
}

static void ods_skip_until(struct ods_parser *xp, const char *s) {
    size_t len = strlen(s);
    while (xp->pos + len <= xp->input_len) {
        if (strncmp(xp->input + xp->pos, s, len) == 0) return;
        xp->pos++;
    }
}

static char *ods_extract_content(struct ods_parser *xp, const char *end_tag) {
    size_t start = xp->pos;
    size_t len;
    char *content;
    size_t end_len = strlen(end_tag);

    while (xp->pos < xp->input_len) {
        if (xp->input[xp->pos] == '<' &&
            strncmp(xp->input + xp->pos, end_tag, end_len) == 0) {
            break;
        }
        xp->pos++;
    }

    len = xp->pos - start;
    content = malloc(len + 1);
    if (!content) return NULL;

    memcpy(content, xp->input + start, len);
    content[len] = '\0';

    return content;
}

/* Parse table:table-cell */
static void ods_parse_cell(struct ods_parser *xp, struct buffer_tabel *buf) {
    char attr_name[128];
    char attr_value[MAKS_TEKS];
    int repeats = 1;
    int merge_rows = 0;
    int merge_cols = 0;
    char *content = NULL;
    char *unescaped = NULL;

    /* Skip attributes */
    while (xp->pos < xp->input_len && xp->input[xp->pos] != '>' &&
           xp->input[xp->pos] != '/') {

        ods_skip_ws(xp);
        if (xp->input[xp->pos] == '>' || xp->input[xp->pos] == '/') break;

        /* Parse attribute name */
        size_t ni = 0;
        while (xp->pos < xp->input_len && ni < sizeof(attr_name) - 1) {
            char c = xp->input[xp->pos];
            if (isalnum((unsigned char)c) || c == ':' || c == '-' || c == '_') {
                attr_name[ni++] = c;
                xp->pos++;
            } else break;
        }
        attr_name[ni] = '\0';

        ods_skip_ws(xp);
        if (xp->input[xp->pos] != '=') break;
        xp->pos++;
        ods_skip_ws(xp);

        /* Parse attribute value */
        char quote = xp->input[xp->pos];
        if (quote == '"' || quote == '\'') {
            xp->pos++;
            size_t vi = 0;
            while (xp->pos < xp->input_len && vi < sizeof(attr_value) - 1) {
                if (xp->input[xp->pos] == quote) {
                    xp->pos++;
                    break;
                }
                attr_value[vi++] = xp->input[xp->pos++];
            }
            attr_value[vi] = '\0';
        }

        /* Check relevant attributes */
        const char *local = strrchr(attr_name, ':');
        local = local ? local + 1 : attr_name;

        if (strcmp(local, "number-columns-repeated") == 0) {
            repeats = atoi(attr_value);
        } else if (strcmp(local, "number-rows-spanned") == 0) {
            merge_rows = atoi(attr_value);
        } else if (strcmp(local, "number-columns-spanned") == 0) {
            merge_cols = atoi(attr_value);
        }
    }

    /* Check for self-closing */
    int self_closing = 0;
    while (xp->pos < xp->input_len && xp->input[xp->pos] != '>') {
        if (xp->input[xp->pos] == '/') self_closing = 1;
        xp->pos++;
    }
    if (xp->input[xp->pos] == '>') xp->pos++;

    if (!self_closing) {
        /* Parse cell content (text:p or text:h) */
        while (xp->pos < xp->input_len) {
            ods_skip_ws(xp);

            if (xp->input[xp->pos] == '<') {
                if (ods_at_string(xp, "</table:table-cell") ||
                    ods_at_string(xp, "</table-cell")) {
                    break;
                } else if (ods_at_string(xp, "<text:p") ||
                           ods_at_string(xp, "<text:h") ||
                           ods_at_string(xp, "<p") ||
                           ods_at_string(xp, "<h")) {
                    /* Skip to content */
                    while (xp->pos < xp->input_len && xp->input[xp->pos] != '>') {
                        xp->pos++;
                    }
                    if (xp->input[xp->pos] == '>') xp->pos++;

                    /* Extract text content */
                    content = ods_extract_content(xp, "</text:p");
                    if (!content) content = ods_extract_content(xp, "</text:h");
                    if (!content) content = ods_extract_content(xp, "</p");
                    if (!content) content = ods_extract_content(xp, "</h");

                    if (content) {
                        unescaped = malloc(strlen(content) + 1);
                        if (unescaped) {
                            ods_xml_unescape(content, unescaped, strlen(content) + 1);
                        }
                    }

                    /* Skip end tag */
                    ods_skip_until(xp, ">");
                    if (xp->input[xp->pos] == '>') xp->pos++;
                } else {
                    /* Skip other elements */
                    while (xp->pos < xp->input_len && xp->input[xp->pos] != '>') {
                        xp->pos++;
                    }
                    if (xp->input[xp->pos] == '>') xp->pos++;
                }
            } else {
                xp->pos++;
            }
        }

        /* Skip end tag */
        ods_skip_until(xp, ">");
        if (xp->input[xp->pos] == '>') xp->pos++;
    }

    /* Store cell value */
    if (xp->current_row < buf->cfg.baris && xp->current_col < buf->cfg.kolom) {
        for (int i = 0; i < repeats && xp->current_col + i < buf->cfg.kolom; i++) {
            if (unescaped) {
                salin_str_aman(buf->isi[xp->current_row][xp->current_col + i],
                              unescaped, MAKS_TEKS);
            }
        }

        /* Handle merge */
        if (merge_rows > 1 || merge_cols > 1) {
            atur_blok_gabung_buf(buf, xp->current_col, xp->current_row,
                                 merge_cols > 0 ? merge_cols : 1,
                                 merge_rows > 0 ? merge_rows : 1);
        }
    }

    xp->current_col += repeats;

    if (content) free(content);
    if (unescaped) free(unescaped);
}

/* Parse table:table-row */
static void ods_parse_row(struct ods_parser *xp, struct buffer_tabel *buf) {
    /* Skip attributes */
    while (xp->pos < xp->input_len && xp->input[xp->pos] != '>') {
        xp->pos++;
    }
    if (xp->input[xp->pos] == '>') xp->pos++;

    xp->current_col = 0;

    /* Parse cells */
    while (xp->pos < xp->input_len) {
        ods_skip_ws(xp);

        if (xp->input[xp->pos] == '<') {
            if (ods_at_string(xp, "</table:table-row") ||
                ods_at_string(xp, "</table-row") ||
                ods_at_string(xp, "</tr")) {
                break;
            } else if (ods_at_string(xp, "<table:table-cell") ||
                       ods_at_string(xp, "<table-cell") ||
                       ods_at_string(xp, "<td")) {
                ods_parse_cell(xp, buf);
            } else if (xp->input[xp->pos + 1] == '/') {
                break;
            } else {
                while (xp->pos < xp->input_len && xp->input[xp->pos] != '>') {
                    xp->pos++;
                }
                if (xp->input[xp->pos] == '>') xp->pos++;
            }
        } else {
            xp->pos++;
        }
    }

    /* Skip end tag */
    ods_skip_until(xp, ">");
    if (xp->input[xp->pos] == '>') xp->pos++;

    xp->current_row++;
}

/* Parse table:table */
static void ods_parse_table(struct ods_parser *xp, struct buffer_tabel *buf) {
    /* Skip attributes */
    while (xp->pos < xp->input_len && xp->input[xp->pos] != '>') {
        xp->pos++;
    }
    if (xp->input[xp->pos] == '>') xp->pos++;

    xp->current_row = 0;

    /* Parse rows */
    while (xp->pos < xp->input_len) {
        ods_skip_ws(xp);

        if (xp->input[xp->pos] == '<') {
            if (ods_at_string(xp, "</table:table") ||
                ods_at_string(xp, "</table")) {
                break;
            } else if (ods_at_string(xp, "<table:table-row") ||
                       ods_at_string(xp, "<table-row") ||
                       ods_at_string(xp, "<tr")) {
                ods_parse_row(xp, buf);
            } else if (ods_at_string(xp, "<table:table-row") == 0 &&
                       ods_at_string(xp, "<table-row") == 0 &&
                       xp->input[xp->pos + 1] == '/') {
                break;
            } else {
                while (xp->pos < xp->input_len && xp->input[xp->pos] != '>') {
                    xp->pos++;
                }
                if (xp->input[xp->pos] == '>') xp->pos++;
            }
        } else {
            xp->pos++;
        }
    }
}

/* Parse office:spreadsheet */
static void ods_parse_spreadsheet(struct ods_parser *xp, struct buffer_tabel *buf) {
    /* Skip attributes */
    while (xp->pos < xp->input_len && xp->input[xp->pos] != '>') {
        xp->pos++;
    }
    if (xp->input[xp->pos] == '>') xp->pos++;

    /* Find first table */
    while (xp->pos < xp->input_len) {
        ods_skip_ws(xp);

        if (xp->input[xp->pos] == '<') {
            if (ods_at_string(xp, "</office:spreadsheet") ||
                ods_at_string(xp, "</spreadsheet")) {
                break;
            } else if (ods_at_string(xp, "<table:table") ||
                       ods_at_string(xp, "<table")) {
                ods_parse_table(xp, buf);
                break;  /* Only parse first table */
            } else if (xp->input[xp->pos + 1] == '/') {
                break;
            } else {
                while (xp->pos < xp->input_len && xp->input[xp->pos] != '>') {
                    xp->pos++;
                }
                if (xp->input[xp->pos] == '>') xp->pos++;
            }
        } else {
            xp->pos++;
        }
    }
}

/* Parse content.xml */
static int ods_parse_content(const char *content, size_t len, struct buffer_tabel *buf) {
    struct ods_parser xp;

    memset(&xp, 0, sizeof(xp));
    xp.input = content;
    xp.input_len = len;
    xp.pos = 0;

    /* Find office:spreadsheet element */
    while (xp.pos < xp.input_len) {
        ods_skip_ws(&xp);

        if (xp.input[xp.pos] == '<') {
            if (ods_at_string(&xp, "<office:spreadsheet") ||
                ods_at_string(&xp, "<spreadsheet")) {
                ods_parse_spreadsheet(&xp, buf);
                break;
            } else if (ods_at_string(&xp, "<table:table")) {
                ods_parse_table(&xp, buf);
                break;
            } else {
                while (xp.pos < xp.input_len && xp.input[xp.pos] != '>') {
                    xp.pos++;
                }
                if (xp.input[xp.pos] == '>') xp.pos++;
            }
        } else {
            xp.pos++;
        }
    }

    return 0;
}

/* ============================================================
 * ODS WRITING FUNCTIONS
 * ============================================================ */

/* Generate content.xml */
static char *ods_generate_content(const struct buffer_tabel *buf, size_t *size) {
    char *content;
    size_t capacity = buf->cfg.baris * buf->cfg.kolom * 64 + 4096;
    size_t pos = 0;
    int r, c;
    char escaped[MAKS_TEKS * 2];

    content = malloc(capacity);
    if (!content) return NULL;

    /* XML declaration */
    pos += sprintf(content + pos, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");

    /* Root element */
    pos += sprintf(content + pos,
        "<office:document-content\n"
        " xmlns:office=\"%s\"\n"
        " xmlns:style=\"%s\"\n"
        " xmlns:text=\"%s\"\n"
        " xmlns:table=\"%s\"\n"
        " xmlns:number=\"%s\"\n"
        " office:version=\"1.2\">\n",
        ODS_NS_OFFICE, ODS_NS_STYLE, ODS_NS_TEXT, ODS_NS_SPREADSHEET, ODS_NS_NUMBER);

    /* Automatic styles */
    pos += sprintf(content + pos, " <office:automatic-styles>\n");
    pos += sprintf(content + pos, "  <style:style style:name=\"Default\" style:family=\"table-cell\"/>\n");
    pos += sprintf(content + pos, " </office:automatic-styles>\n");

    /* Body */
    pos += sprintf(content + pos, " <office:body>\n");
    pos += sprintf(content + pos, "  <office:spreadsheet>\n");

    /* Table */
    pos += sprintf(content + pos, "   <table:table table:name=\"Sheet1\">\n");

    /* Columns */
    for (c = 0; c < buf->cfg.kolom; c++) {
        pos += sprintf(content + pos,
            "    <table:table-column table:style-name=\"Default\"/>\n");
    }

    /* Rows */
    for (r = 0; r < buf->cfg.baris; r++) {
        pos += sprintf(content + pos, "    <table:table-row>\n");

        for (c = 0; c < buf->cfg.kolom; c++) {
            const char *val = buf->isi[r][c];

            if (val && strlen(val) > 0) {
                /* Check if it's a number */
                char *endptr;
                double num = strtod(val, &endptr);

                ods_xml_escape(val, escaped, sizeof(escaped));

                if (*endptr == '\0' && endptr != val) {
                    /* Number */
                    pos += sprintf(content + pos,
                        "     <table:table-cell office:value-type=\"float\" office:value=\"%s\">\n",
                        escaped);
                    pos += sprintf(content + pos,
                        "      <text:p>%s</text:p>\n", escaped);
                    pos += sprintf(content + pos, "     </table:table-cell>\n");
                } else {
                    /* String */
                    pos += sprintf(content + pos,
                        "     <table:table-cell office:value-type=\"string\">\n");
                    pos += sprintf(content + pos,
                        "      <text:p>%s</text:p>\n", escaped);
                    pos += sprintf(content + pos, "     </table:table-cell>\n");
                }
            } else {
                /* Empty cell */
                pos += sprintf(content + pos, "     <table:table-cell/>\n");
            }
        }

        pos += sprintf(content + pos, "    </table:table-row>\n");
    }

    pos += sprintf(content + pos, "   </table:table>\n");
    pos += sprintf(content + pos, "  </office:spreadsheet>\n");
    pos += sprintf(content + pos, " </office:body>\n");
    pos += sprintf(content + pos, "</office:document-content>\n");

    *size = pos;
    return content;
}

/* Generate styles.xml */
static char *ods_generate_styles(size_t *size) {
    char *content = malloc(2048);
    if (!content) return NULL;

    *size = sprintf(content,
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<office:document-styles\n"
        " xmlns:office=\"%s\"\n"
        " xmlns:style=\"%s\"\n"
        " xmlns:text=\"%s\"\n"
        " office:version=\"1.2\">\n"
        " <office:styles>\n"
        "  <style:default-style style:family=\"table-cell\">\n"
        "   <style:paragraph-properties/>\n"
        "  </style:default-style>\n"
        " </office:styles>\n"
        "</office:document-styles>\n",
        ODS_NS_OFFICE, ODS_NS_STYLE, ODS_NS_TEXT);

    return content;
}

/* Generate meta.xml */
static char *ods_generate_meta(size_t *size) {
    char *content = malloc(1024);
    if (!content) return NULL;

    *size = sprintf(content,
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<office:document-meta\n"
        " xmlns:office=\"%s\"\n"
        " xmlns:meta=\"%s\"\n"
        " office:version=\"1.2\">\n"
        " <office:meta>\n"
        "  <meta:generator>TABEL 3.0</meta:generator>\n"
        " </office:meta>\n"
        "</office:document-meta>\n",
        ODS_NS_OFFICE, ODS_NS_META);

    return content;
}

/* Generate mimetype */
static char *ods_generate_mimetype(size_t *size) {
    char *content = malloc(64);
    if (!content) return NULL;

    strcpy(content, "application/vnd.oasis.opendocument.spreadsheet");
    *size = strlen(content);

    return content;
}

/* Generate manifest.xml */
static char *ods_generate_manifest(size_t *size) {
    char *content = malloc(1024);
    if (!content) return NULL;

    *size = sprintf(content,
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<manifest:manifest\n"
        " xmlns:manifest=\"urn:oasis:names:tc:opendocument:xmlns:manifest:1.0\">\n"
        " <manifest:file-entry manifest:full-path=\"/\" manifest:media-type=\"application/vnd.oasis.opendocument.spreadsheet\"/>\n"
        " <manifest:file-entry manifest:full-path=\"content.xml\" manifest:media-type=\"text/xml\"/>\n"
        " <manifest:file-entry manifest:full-path=\"styles.xml\" manifest:media-type=\"text/xml\"/>\n"
        " <manifest:file-entry manifest:full-path=\"meta.xml\" manifest:media-type=\"text/xml\"/>\n"
        "</manifest:manifest>\n");

    return content;
}

/* ============================================================
 * FUNGSI PUBLIK
 * ============================================================ */

/* Buka file ODS */
int buka_ods(struct buffer_tabel **pbuf, const char *fn) {
    struct zip_file *zf;
    struct zip_entry *entry;
    unsigned char *content;
    size_t content_size;

    if (!pbuf || !fn) return -1;

    zf = zip_open_read(fn);
    if (!zf) {
        ods_set_error("Cannot open ODS file: %s", fn);
        return -1;
    }

    /* Find content.xml */
    entry = zip_find_entry(zf, "content.xml");
    if (!entry) {
        ods_set_error("No content.xml found in ODS");
        zip_close(zf);
        return -1;
    }

    /* Extract content */
    content = malloc(entry->uncompressed_size + 1);
    if (!content) {
        zip_close(zf);
        return -1;
    }

    if (zip_extract_entry(zf, entry, content, &content_size, entry->uncompressed_size + 1) != 0) {
        free(content);
        zip_close(zf);
        return -1;
    }

    content[content_size] = '\0';
    zip_close(zf);

    /* Create buffer */
    bebas_buffer(*pbuf);
    *pbuf = buat_buffer(100, 26);

    /* Parse content */
    ods_parse_content((char *)content, content_size, *pbuf);

    free(content);
    return 0;
}

/* Simpan file ODS */
int simpan_ods(const struct buffer_tabel *buf, const char *fn) {
    struct zip_writer *zw;
    char *content, *styles, *meta, *mimetype, *manifest;
    size_t content_size, styles_size, meta_size, mimetype_size, manifest_size;

    if (!buf || !fn) return -1;

    zw = zip_create(fn);
    if (!zw) {
        ods_set_error("Cannot create ODS file: %s", fn);
        return -1;
    }

    /* Generate files */
    mimetype = ods_generate_mimetype(&mimetype_size);
    content = ods_generate_content(buf, &content_size);
    styles = ods_generate_styles(&styles_size);
    meta = ods_generate_meta(&meta_size);
    manifest = ods_generate_manifest(&manifest_size);

    /* Add to ZIP - mimetype must be first and uncompressed */
    if (mimetype) zip_add_entry(zw, "mimetype", (unsigned char *)mimetype, mimetype_size);
    if (content) zip_add_entry(zw, "content.xml", (unsigned char *)content, content_size);
    if (styles) zip_add_entry(zw, "styles.xml", (unsigned char *)styles, styles_size);
    if (meta) zip_add_entry(zw, "meta.xml", (unsigned char *)meta, meta_size);
    if (manifest) zip_add_entry(zw, "META-INF/manifest.xml", (unsigned char *)manifest, manifest_size);

    /* Finalize */
    zip_finalize(zw);
    zip_writer_close(zw);

    if (mimetype) free(mimetype);
    if (content) free(content);
    if (styles) free(styles);
    if (meta) free(meta);
    if (manifest) free(manifest);

    return 0;
}

/* Cek validitas file ODS */
int ods_apakah_valid(const char *fn) {
    struct zip_file *zf;
    struct zip_entry *entry;

    zf = zip_open_read(fn);
    if (!zf) return 0;

    entry = zip_find_entry(zf, "content.xml");
    zip_close(zf);

    return entry != NULL;
}
