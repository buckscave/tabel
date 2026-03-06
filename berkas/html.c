/* ============================================================
 * TABEL v3.0
 * Berkas: html.c - Format Berkas HTM/HTML Table (<table>, <tr>, <td>, <th>)
 * Standar: C89 + POSIX.1-2008
 * Penulis: Chandra Lesmana
 * ============================================================ */

#include "tabel.h"

/* ============================================================
 * KONSTANTA HTML
 * ============================================================ */
#define HTML_MAX_LINE        (MAKS_TEKS * MAKS_KOLOM * 2)
#define HTML_MAX_TAG         256
#define HTML_MAX_ATTR        512
#define HTML_MAX_ENTITY      32
#define HTML_BUFFER_SIZE     (64 * 1024)

/* ============================================================
 * VARIABEL GLOBAL
 * ============================================================ */
static char html_error_msg[512] = "";

/* ============================================================
 * FUNGSI HELPER
 * ============================================================ */

/* Set error message */
static void html_set_error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(html_error_msg, sizeof(html_error_msg), fmt, args);
    va_end(args);
}

/* Get error message */
const char *html_get_error(void) {
    return html_error_msg;
}

/* ============================================================
 * ENTITY DECODING
 * ============================================================ */

/* HTML entity lookup table */
static const struct {
    const char *entity;
    const char *char_;
} html_entities[] = {
    {"&amp;", "&"},
    {"&lt;", "<"},
    {"&gt;", ">"},
    {"&quot;", "\""},
    {"&apos;", "'"},
    {"&nbsp;", " "},
    {"&copy;", "(c)"},
    {"&reg;", "(R)"},
    {"&trade;", "TM"},
    {"&ndash;", "-"},
    {"&mdash;", "--"},
    {"&lsquo;", "'"},
    {"&rsquo;", "'"},
    {"&ldquo;", "\""},
    {"&rdquo;", "\""},
    {"&bull;", "*"},
    {"&hellip;", "..."},
    {"&euro;", "EUR"},
    {"&pound;", "GBP"},
    {"&yen;", "JPY"},
    {"&cent;", "c"},
    {"&deg;", "deg"},
    {"&plusmn;", "+/-"},
    {"&times;", "x"},
    {"&divide;", "/"},
    {"&frac14;", "1/4"},
    {"&frac12;", "1/2"},
    {"&frac34;", "3/4"},
    {NULL, NULL}
};

/* Decode HTML entities */
static int html_decode_entities(const char *input, char *output, size_t output_size) {
    size_t i = 0, j = 0;
    
    if (!input || !output || output_size == 0) return -1;
    
    while (input[i] && j < output_size - 1) {
        if (input[i] == '&') {
            /* Cari entity */
            int found = 0;
            size_t k;
            
            /* Cek numeric entity &#nnn; atau &#xhhh; */
            if (input[i + 1] == '#') {
                unsigned long code = 0;
                char *end;
                
                if (input[i + 2] == 'x' || input[i + 2] == 'X') {
                    /* Hexadecimal */
                    code = strtoul(input + i + 3, &end, 16);
                } else {
                    /* Decimal */
                    code = strtoul(input + i + 2, &end, 10);
                }
                
                if (end && *end == ';') {
                    /* Convert code to UTF-8 */
                    if (code < 0x80) {
                        output[j++] = (char)code;
                    } else if (code < 0x800) {
                        output[j++] = (char)(0xC0 | (code >> 6));
                        if (j < output_size - 1) output[j++] = (char)(0x80 | (code & 0x3F));
                    } else if (code < 0x10000) {
                        output[j++] = (char)(0xE0 | (code >> 12));
                        if (j < output_size - 1) output[j++] = (char)(0x80 | ((code >> 6) & 0x3F));
                        if (j < output_size - 1) output[j++] = (char)(0x80 | (code & 0x3F));
                    }
                    i = (end - input) + 1;
                    found = 1;
                }
            }
            
            /* Cek named entity */
            if (!found) {
                for (k = 0; html_entities[k].entity != NULL; k++) {
                    size_t entity_len = strlen(html_entities[k].entity);
                    if (strncmp(input + i, html_entities[k].entity, entity_len) == 0) {
                        const char *ch = html_entities[k].char_;
                        while (*ch && j < output_size - 1) {
                            output[j++] = *ch++;
                        }
                        i += entity_len;
                        found = 1;
                        break;
                    }
                }
            }
            
            if (!found) {
                /* Tidak ditemukan, copy as-is */
                output[j++] = input[i++];
            }
        } else {
            output[j++] = input[i++];
        }
    }
    
    output[j] = '\0';
    return (int)j;
}

/* Encode HTML entities */
static int html_encode_entities(const char *input, char *output, size_t output_size) {
    size_t i = 0, j = 0;
    
    if (!input || !output || output_size == 0) return -1;
    
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

/* ============================================================
 * HTML TAG PARSING
 * ============================================================ */

/* Struktur untuk parsing tag */
struct html_tag {
    char name[64];
    char attrs[HTML_MAX_ATTR];
    int is_closing;
    int is_self_closing;
};

/* Skip whitespace */
static void html_skip_ws(const char *html, size_t len, size_t *pos) {
    while (*pos < len && isspace((unsigned char)html[*pos])) {
        (*pos)++;
    }
}

/* Parse tag name */
static int html_parse_tag_name(const char *html, size_t len, size_t *pos, char *name, size_t name_size) {
    size_t i = 0;
    
    while (*pos < len && i < name_size - 1) {
        char c = html[*pos];
        if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == ':') {
            name[i++] = tolower((unsigned char)c);
            (*pos)++;
        } else {
            break;
        }
    }
    
    name[i] = '\0';
    return (int)i;
}

/* Parse tag */
static int html_parse_tag(const char *html, size_t len, size_t *pos, struct html_tag *tag) {
    size_t start;
    
    memset(tag, 0, sizeof(*tag));
    
    html_skip_ws(html, len, pos);
    
    if (*pos >= len || html[*pos] != '<') {
        return -1;
    }
    
    (*pos)++;  /* Skip '<' */
    
    /* Check for closing tag */
    if (html[*pos] == '/') {
        tag->is_closing = 1;
        (*pos)++;
    }
    
    /* Check for comment or doctype */
    if (html[*pos] == '!' || html[*pos] == '?') {
        /* Skip until '>' */
        while (*pos < len && html[*pos] != '>') {
            (*pos)++;
        }
        if (*pos < len) (*pos)++;
        return -1;  /* Not a regular tag */
    }
    
    /* Parse tag name */
    if (html_parse_tag_name(html, len, pos, tag->name, sizeof(tag->name)) == 0) {
        return -1;
    }
    
    /* Parse attributes */
    start = *pos;
    while (*pos < len && html[*pos] != '>') {
        if (html[*pos] == '/' && html[*pos + 1] == '>') {
            tag->is_self_closing = 1;
            (*pos) += 2;
            return 0;
        }
        (*pos)++;
    }
    
    if (*pos < len) {
        size_t attr_len = *pos - start;
        if (attr_len >= sizeof(tag->attrs)) {
            attr_len = sizeof(tag->attrs) - 1;
        }
        memcpy(tag->attrs, html + start, attr_len);
        tag->attrs[attr_len] = '\0';
        (*pos)++;  /* Skip '>' */
    }
    
    return 0;
}

/* Parse text content until next tag */
static int html_parse_text(const char *html, size_t len, size_t *pos, char *text, size_t text_size) {
    size_t start = *pos;
    size_t end;
    size_t text_len;
    
    while (*pos < len && html[*pos] != '<') {
        (*pos)++;
    }
    
    end = *pos;
    
    /* Trim trailing whitespace */
    while (end > start && isspace((unsigned char)html[end - 1])) {
        end--;
    }
    
    /* Trim leading whitespace */
    while (start < end && isspace((unsigned char)html[start])) {
        start++;
    }
    
    text_len = end - start;
    if (text_len >= text_size) {
        text_len = text_size - 1;
    }
    
    memcpy(text, html + start, text_len);
    text[text_len] = '\0';
    
    /* Decode entities */
    {
        char decoded[MAKS_TEKS];
        html_decode_entities(text, decoded, sizeof(decoded));
        strncpy(text, decoded, text_size - 1);
        text[text_size - 1] = '\0';
    }
    
    return (int)strlen(text);
}

/* ============================================================
 * ATTRIBUT PARSING
 * ============================================================ */

/* Get attribute value */
static int html_get_attr(const char *attrs, const char *name, char *value, size_t value_size) {
    const char *p = attrs;
    size_t name_len = strlen(name);
    
    value[0] = '\0';
    
    while (*p) {
        /* Skip whitespace */
        while (*p && isspace((unsigned char)*p)) p++;
        
        /* Check attribute name */
        if (strncasecmp(p, name, name_len) == 0) {
            p += name_len;
            
            /* Skip whitespace */
            while (*p && isspace((unsigned char)*p)) p++;
            
            /* Check for '=' */
            if (*p == '=') {
                p++;
                
                /* Skip whitespace */
                while (*p && isspace((unsigned char)*p)) p++;
                
                /* Get value */
                if (*p == '"' || *p == '\'') {
                    char quote = *p++;
                    size_t i = 0;
                    while (*p && *p != quote && i < value_size - 1) {
                        value[i++] = *p++;
                    }
                    value[i] = '\0';
                    return (int)i;
                } else {
                    size_t i = 0;
                    while (*p && !isspace((unsigned char)*p) && *p != '>' && i < value_size - 1) {
                        value[i++] = *p++;
                    }
                    value[i] = '\0';
                    return (int)i;
                }
            } else {
                /* Boolean attribute */
                strcpy(value, "1");
                return 1;
            }
        }
        
        /* Skip to next attribute */
        while (*p && !isspace((unsigned char)*p)) p++;
        if (*p == '=') {
            p++;
            while (*p && isspace((unsigned char)*p)) p++;
            if (*p == '"' || *p == '\'') {
                char quote = *p++;
                while (*p && *p != quote) p++;
                if (*p) p++;
            } else {
                while (*p && !isspace((unsigned char)*p)) p++;
            }
        }
    }
    
    return -1;
}

/* Parse colspan and rowspan */
static void html_parse_span_attrs(const char *attrs, int *colspan, int *rowspan) {
    char value[32];
    
    *colspan = 1;
    *rowspan = 1;
    
    if (html_get_attr(attrs, "colspan", value, sizeof(value)) > 0) {
        *colspan = atoi(value);
        if (*colspan < 1) *colspan = 1;
    }
    
    if (html_get_attr(attrs, "rowspan", value, sizeof(value)) > 0) {
        *rowspan = atoi(value);
        if (*rowspan < 1) *rowspan = 1;
    }
}

/* ============================================================
 * HTML IMPORT
 * ============================================================ */

/* Struktur untuk menyimpan data tabel sementara */
struct html_cell {
    char text[MAKS_TEKS];
    int is_header;
    int colspan;
    int rowspan;
    int occupied;  /* Untuk rowspan/colspan placeholder */
};

struct html_table {
    struct html_cell **cells;
    int rows;
    int cols;
    int max_rows;
    int max_cols;
};

/* Initialize HTML table */
static int html_table_init(struct html_table *table, int init_rows, int init_cols) {
    int r;
    
    memset(table, 0, sizeof(*table));
    
    table->max_rows = init_rows;
    table->max_cols = init_cols;
    
    table->cells = calloc(init_rows, sizeof(struct html_cell *));
    if (!table->cells) return -1;
    
    for (r = 0; r < init_rows; r++) {
        table->cells[r] = calloc(init_cols, sizeof(struct html_cell));
        if (!table->cells[r]) {
            while (--r >= 0) free(table->cells[r]);
            free(table->cells);
            return -1;
        }
    }
    
    return 0;
}

/* Expand table if needed */
static int html_table_expand(struct html_table *table, int need_rows, int need_cols) {
    int r;
    
    if (need_rows > table->max_rows) {
        int new_max = need_rows * 2;
        struct html_cell **new_cells;
        
        new_cells = realloc(table->cells, new_max * sizeof(struct html_cell *));
        if (!new_cells) return -1;
        
        table->cells = new_cells;
        
        for (r = table->max_rows; r < new_max; r++) {
            table->cells[r] = calloc(table->max_cols, sizeof(struct html_cell));
            if (!table->cells[r]) return -1;
        }
        
        table->max_rows = new_max;
    }
    
    if (need_cols > table->max_cols) {
        int new_max = need_cols * 2;
        
        for (r = 0; r < table->max_rows; r++) {
            struct html_cell *new_row;
            
            new_row = realloc(table->cells[r], new_max * sizeof(struct html_cell));
            if (!new_row) return -1;
            
            memset(new_row + table->max_cols, 0, 
                   (new_max - table->max_cols) * sizeof(struct html_cell));
            table->cells[r] = new_row;
        }
        
        table->max_cols = new_max;
    }
    
    return 0;
}

/* Free HTML table */
static void html_table_free(struct html_table *table) {
    int r;
    
    if (!table->cells) return;
    
    for (r = 0; r < table->max_rows; r++) {
        free(table->cells[r]);
    }
    
    free(table->cells);
    table->cells = NULL;
}

/* Find next available cell (skip occupied) */
static void html_find_next_cell(struct html_table *table, int *row, int *col) {
    while (*row < table->max_rows && table->cells[*row][*col].occupied) {
        (*col)++;
        if (*col >= table->max_cols) {
            *col = 0;
            (*row)++;
        }
    }
}

/* Mark cells as occupied for colspan/rowspan */
static void html_mark_span(struct html_table *table, int row, int col, int colspan, int rowspan) {
    int r, c;
    
    for (r = row; r < row + rowspan && r < table->max_rows; r++) {
        for (c = col; c < col + colspan && c < table->max_cols; c++) {
            if (r == row && c == col) {
                /* Actual cell */
                table->cells[r][c].colspan = colspan;
                table->cells[r][c].rowspan = rowspan;
            } else {
                /* Placeholder */
                table->cells[r][c].occupied = 1;
            }
        }
    }
}

/* Parse HTML table content */
static int html_parse_table_content(const char *html, size_t len, size_t *pos,
                                    struct html_table *table) {
    struct html_tag tag;
    int current_row = 0;
    int current_col = 0;
    int in_row = 0;
    int in_cell = 0;
    char cell_text[MAKS_TEKS];
    
    while (*pos < len) {
        if (html_parse_tag(html, len, pos, &tag) == 0) {
            if (strcmp(tag.name, "tr") == 0) {
                if (!tag.is_closing) {
                    /* Start new row */
                    in_row = 1;
                    current_col = 0;
                    
                    /* Find next available row (skip occupied by rowspan) */
                    while (current_row < table->max_rows) {
                        int all_occupied = 1;
                        int c;
                        for (c = 0; c < table->max_cols; c++) {
                            if (!table->cells[current_row][c].occupied) {
                                all_occupied = 0;
                                break;
                            }
                        }
                        if (!all_occupied) break;
                        current_row++;
                    }
                    
                    html_table_expand(table, current_row + 10, current_col + 10);
                } else {
                    /* End row */
                    in_row = 0;
                    current_row++;
                }
            } else if ((strcmp(tag.name, "td") == 0 || strcmp(tag.name, "th") == 0) && !tag.is_closing) {
                /* Start cell */
                int colspan, rowspan;
                
                in_cell = 1;
                cell_text[0] = '\0';
                
                html_parse_span_attrs(tag.attrs, &colspan, &rowspan);
                
                /* Find next available cell */
                html_find_next_cell(table, &current_row, &current_col);
                html_table_expand(table, current_row + rowspan, current_col + colspan);
                
                /* Mark span */
                html_mark_span(table, current_row, current_col, colspan, rowspan);
                
                /* Store cell info */
                table->cells[current_row][current_col].is_header = (strcmp(tag.name, "th") == 0);
                
            } else if ((strcmp(tag.name, "td") == 0 || strcmp(tag.name, "th") == 0) && tag.is_closing) {
                /* End cell */
                in_cell = 0;
                
                /* Store text */
                strncpy(table->cells[current_row][current_col].text, cell_text, MAKS_TEKS - 1);
                table->cells[current_row][current_col].text[MAKS_TEKS - 1] = '\0';
                
                /* Track actual dimensions */
                if (current_row + 1 > table->rows) {
                    table->rows = current_row + 1;
                }
                if (current_col + table->cells[current_row][current_col].colspan > table->cols) {
                    table->cols = current_col + table->cells[current_row][current_col].colspan;
                }
                
                current_col++;
            } else if (strcmp(tag.name, "table") == 0 && tag.is_closing) {
                /* End table */
                break;
            }
        } else if (in_cell) {
            /* Parse text content */
            html_parse_text(html, len, pos, cell_text, sizeof(cell_text));
        } else {
            /* Skip */
            (*pos)++;
        }
    }
    
    return 0;
}

/* Find first table in HTML */
static int html_find_table(const char *html, size_t len, size_t *pos) {
    struct html_tag tag;
    
    *pos = 0;
    
    while (*pos < len) {
        if (html_parse_tag(html, len, pos, &tag) == 0) {
            if (strcmp(tag.name, "table") == 0 && !tag.is_closing) {
                return 0;  /* Found */
            }
        } else {
            (*pos)++;
        }
    }
    
    return -1;  /* Not found */
}

/* Main HTML open function */
int buka_html(struct buffer_tabel **pbuf, const char *fn) {
    FILE *fp;
    char *html_content;
    size_t html_len;
    size_t pos;
    struct html_table table;
    struct buffer_tabel *buf;
    int r, c;
    int ret = -1;
    
    if (!pbuf || !fn) return -1;
    
    /* Baca file */
    fp = fopen(fn, "r");
    if (!fp) {
        html_set_error("Cannot open file: %s", fn);
        return -1;
    }
    
    /* Get file size */
    fseek(fp, 0, SEEK_END);
    html_len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    /* Allocate buffer */
    html_content = malloc(html_len + 1);
    if (!html_content) {
        fclose(fp);
        html_set_error("Memory allocation failed");
        return -1;
    }
    
    /* Read content */
    html_len = fread(html_content, 1, html_len, fp);
    fclose(fp);
    html_content[html_len] = '\0';
    
    /* Initialize table */
    if (html_table_init(&table, 100, 26) != 0) {
        free(html_content);
        html_set_error("Memory allocation failed");
        return -1;
    }
    
    /* Find first table */
    if (html_find_table(html_content, html_len, &pos) != 0) {
        html_set_error("No table found in HTML file");
        html_table_free(&table);
        free(html_content);
        return -1;
    }
    
    /* Parse table */
    if (html_parse_table_content(html_content, html_len, &pos, &table) != 0) {
        html_set_error("Failed to parse HTML table");
        html_table_free(&table);
        free(html_content);
        return -1;
    }
    
    free(html_content);
    
    /* Validate dimensions */
    if (table.rows == 0 || table.cols == 0) {
        html_set_error("Empty table");
        html_table_free(&table);
        return -1;
    }
    
    /* Create buffer */
    bebas_buffer(*pbuf);
    buf = buat_buffer(table.rows, table.cols);
    if (!buf) {
        html_table_free(&table);
        return -1;
    }
    
    /* Copy data */
    for (r = 0; r < table.rows && r < buf->cfg.baris; r++) {
        for (c = 0; c < table.cols && c < buf->cfg.kolom; c++) {
            if (!table.cells[r][c].occupied) {
                strncpy(buf->isi[r][c], table.cells[r][c].text, MAKS_TEKS - 1);
                buf->isi[r][c][MAKS_TEKS - 1] = '\0';
            }
        }
    }
    
    /* Handle merged cells */
    for (r = 0; r < table.rows && r < buf->cfg.baris; r++) {
        for (c = 0; c < table.cols && c < buf->cfg.kolom; c++) {
            if (!table.cells[r][c].occupied && 
                (table.cells[r][c].colspan > 1 || table.cells[r][c].rowspan > 1)) {
                atur_blok_gabung_buf(buf, c, r, 
                                     table.cells[r][c].colspan,
                                     table.cells[r][c].rowspan);
            }
        }
    }
    
    *pbuf = buf;
    ret = 0;
    
    html_table_free(&table);
    return ret;
}

/* ============================================================
 * HTML EXPORT
 * ============================================================ */

/* HTML export options */
struct html_export_opts {
    int with_css;           /* Include inline CSS */
    int with_borders;       /* Add borders */
    int with_header_style;  /* Style header row */
    int responsive;         /* Add responsive wrapper */
    char title[128];        /* Page title */
    char table_class[64];   /* CSS class for table */
};

/* Set default options */
static void html_set_default_opts(struct html_export_opts *opts) {
    memset(opts, 0, sizeof(*opts));
    opts->with_css = 1;
    opts->with_borders = 1;
    opts->with_header_style = 1;
    opts->responsive = 0;
    strcpy(opts->title, "TABEL Export");
    strcpy(opts->table_class, "tabel-export");
}

/* Export to HTML with options */
int simpan_html_opts(const struct buffer_tabel *buf, const char *fn, 
                     const struct html_export_opts *user_opts) {
    FILE *fp;
    struct html_export_opts opts;
    char escaped[MAKS_TEKS * 2];
    int r, c;
    int has_header = 1;  /* Assume first row is header */
    
    if (!buf || !fn) return -1;
    
    /* Set options */
    if (user_opts) {
        memcpy(&opts, user_opts, sizeof(opts));
    } else {
        html_set_default_opts(&opts);
    }
    
    fp = fopen(fn, "w");
    if (!fp) {
        html_set_error("Cannot create file: %s", fn);
        return -1;
    }
    
    /* Write HTML header */
    fprintf(fp, "<!DOCTYPE html>\n");
    fprintf(fp, "<html lang=\"en\">\n");
    fprintf(fp, "<head>\n");
    fprintf(fp, "  <meta charset=\"UTF-8\">\n");
    fprintf(fp, "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n");
    fprintf(fp, "  <title>%s</title>\n", opts.title);
    
    /* CSS styles */
    if (opts.with_css) {
        fprintf(fp, "  <style>\n");
        fprintf(fp, "    body {\n");
        fprintf(fp, "      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;\n");
        fprintf(fp, "      margin: 20px;\n");
        fprintf(fp, "      background: #f5f5f5;\n");
        fprintf(fp, "    }\n");
        fprintf(fp, "    .%s {\n", opts.table_class);
        fprintf(fp, "      border-collapse: collapse;\n");
        fprintf(fp, "      width: 100%%;\n");
        fprintf(fp, "      background: white;\n");
        fprintf(fp, "      box-shadow: 0 1px 3px rgba(0,0,0,0.2);\n");
        fprintf(fp, "    }\n");
        
        if (opts.with_borders) {
            fprintf(fp, "    .%s th, .%s td {\n", opts.table_class, opts.table_class);
            fprintf(fp, "      border: 1px solid #ddd;\n");
            fprintf(fp, "      padding: 8px 12px;\n");
            fprintf(fp, "    }\n");
        } else {
            fprintf(fp, "    .%s th, .%s td {\n", opts.table_class, opts.table_class);
            fprintf(fp, "      padding: 8px 12px;\n");
            fprintf(fp, "    }\n");
        }
        
        if (opts.with_header_style) {
            fprintf(fp, "    .%s th {\n", opts.table_class);
            fprintf(fp, "      background: #4a6fa5;\n");
            fprintf(fp, "      color: white;\n");
            fprintf(fp, "      font-weight: 600;\n");
            fprintf(fp, "      text-align: left;\n");
            fprintf(fp, "    }\n");
        }
        
        fprintf(fp, "    .%s tr:nth-child(even) {\n", opts.table_class);
        fprintf(fp, "      background: #f9f9f9;\n");
        fprintf(fp, "    }\n");
        fprintf(fp, "    .%s tr:hover {\n", opts.table_class);
        fprintf(fp, "      background: #f0f7ff;\n");
        fprintf(fp, "    }\n");
        
        fprintf(fp, "  </style>\n");
    }
    
    fprintf(fp, "</head>\n");
    fprintf(fp, "<body>\n");
    
    /* Responsive wrapper */
    if (opts.responsive) {
        fprintf(fp, "<div style=\"overflow-x: auto;\">\n");
    }
    
    /* Table start */
    fprintf(fp, "<table class=\"%s\">\n", opts.table_class);
    
    /* Table content */
    for (r = 0; r < buf->cfg.baris; r++) {
        fprintf(fp, "  <tr>\n");
        
        for (c = 0; c < buf->cfg.kolom; c++) {
            const char *tag;
            int is_merged, is_anchor;
            int colspan = 1, rowspan = 1;
            
            /* Check merged cell */
            is_merged = apakah_ada_gabung_buf(buf, c, r);
            is_anchor = apakah_sel_anchor_buf(buf, c, r);
            
            if (is_merged && !is_anchor) {
                /* Skip - this is part of a merged cell */
                continue;
            }
            
            /* Get merge info */
            if (is_anchor) {
                struct info_gabung *gab = &buf->tergabung[r][c];
                colspan = gab->lebar;
                rowspan = gab->tinggi;
            }
            
            /* Determine tag */
            tag = (r == 0 && has_header) ? "th" : "td";
            
            /* Encode content */
            html_encode_entities(buf->isi[r][c], escaped, sizeof(escaped));
            
            /* Write cell */
            if (colspan > 1 || rowspan > 1) {
                fprintf(fp, "    <%s", tag);
                if (colspan > 1) fprintf(fp, " colspan=\"%d\"", colspan);
                if (rowspan > 1) fprintf(fp, " rowspan=\"%d\"", rowspan);
                fprintf(fp, ">%s</%s>\n", escaped, tag);
            } else {
                fprintf(fp, "    <%s>%s</%s>\n", tag, escaped, tag);
            }
        }
        
        fprintf(fp, "  </tr>\n");
    }
    
    fprintf(fp, "</table>\n");
    
    if (opts.responsive) {
        fprintf(fp, "</div>\n");
    }
    
    fprintf(fp, "</body>\n");
    fprintf(fp, "</html>\n");
    
    if (fsync_berkas(fp) != 0) {
        fclose(fp);
        return -1;
    }
    
    fclose(fp);
    return 0;
}

/* Export to HTML */
int simpan_html(const struct buffer_tabel *buf, const char *fn) {
    struct html_export_opts opts;
    return simpan_html_opts(buf, fn, &opts);
}

/* ============================================================
 * DETEKSI DAN VALIDASI
 * ============================================================ */

/* Check if file is valid HTML with table */
int html_apakah_valid(const char *fn) {
    FILE *fp;
    char buf[1024];
    size_t len;
    
    if (!fn) return 0;
    
    fp = fopen(fn, "r");
    if (!fp) return 0;
    
    len = fread(buf, 1, sizeof(buf) - 1, fp);
    fclose(fp);
    
    buf[len] = '\0';
    
    /* Check for HTML indicators */
    if (strstr(buf, "<table") || strstr(buf, "<TABLE")) {
        return 1;
    }
    
    /* Check for HTML structure */
    if (strstr(buf, "<html") || strstr(buf, "<HTML") ||
        strstr(buf, "<!DOCTYPE") || strstr(buf, "<!doctype")) {
        /* Might be HTML but need to check for table */
        return 1;
    }
    
    return 0;
}

/* Get table info from HTML file */
int html_info_table(const char *fn, int *num_tables, int *max_row, int *max_col) {
    FILE *fp;
    char *html_content;
    size_t html_len;
    size_t pos;
    struct html_table table;
    int tables_found = 0;
    
    if (!fn) return -1;
    
    /* Initialize output */
    if (num_tables) *num_tables = 0;
    if (max_row) *max_row = 0;
    if (max_col) *max_col = 0;
    
    /* Read file */
    fp = fopen(fn, "r");
    if (!fp) return -1;
    
    fseek(fp, 0, SEEK_END);
    html_len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    html_content = malloc(html_len + 1);
    if (!html_content) {
        fclose(fp);
        return -1;
    }
    
    html_len = fread(html_content, 1, html_len, fp);
    fclose(fp);
    html_content[html_len] = '\0';
    
    /* Find and count tables */
    pos = 0;
    while (pos < html_len) {
        if (html_find_table(html_content, html_len, &pos) != 0) {
            break;
        }
        
        tables_found++;
        
        /* Parse table to get dimensions */
        if (html_table_init(&table, 100, 26) == 0) {
            html_parse_table_content(html_content, html_len, &pos, &table);
            
            if (max_row && table.rows > *max_row) {
                *max_row = table.rows;
            }
            if (max_col && table.cols > *max_col) {
                *max_col = table.cols;
            }
            
            html_table_free(&table);
        }
    }
    
    free(html_content);
    
    if (num_tables) *num_tables = tables_found;
    
    return tables_found > 0 ? 0 : -1;
}

/* ============================================================
 * INTERACTIVE EXPORT
 * ============================================================ */

/* Prompt user for HTML export options */
void prompt_html_opts(struct html_export_opts *opts) {
    char input[64];
    int len, cursor;
    int rows, cols;
    
    if (!opts) return;
    
    html_set_default_opts(opts);
    
    rows = ambil_tinggi_terminal();
    cols = ambil_lebar_terminal();
    
    /* Prompt title */
    atur_posisi_kursor(1, rows);
    for (int i = 0; i < cols; i++) write(STDOUT_FILENO, " ", 1);
    
    atur_posisi_kursor(2, rows);
    printf("Judul halaman [%s]: ", opts->title);
    fflush(stdout);
    
    len = 0;
    cursor = 0;
    if (baca_input_edit(input, &len, &cursor, 20 + strlen(opts->title), rows) != KEY_BATAL) {
        if (len > 0) {
            strncpy(opts->title, input, sizeof(opts->title) - 1);
        }
    }
    
    /* Prompt CSS */
    atur_posisi_kursor(1, rows);
    for (int i = 0; i < cols; i++) write(STDOUT_FILENO, " ", 1);
    
    atur_posisi_kursor(2, rows);
    printf("Sertakan CSS? [Y/n]: ");
    fflush(stdout);
    
    len = 0;
    cursor = 0;
    if (baca_input_edit(input, &len, &cursor, 21, rows) != KEY_BATAL) {
        if (len > 0 && (input[0] == 'n' || input[0] == 'N')) {
            opts->with_css = 0;
        }
    }
    
    /* Prompt borders */
    atur_posisi_kursor(1, rows);
    for (int i = 0; i < cols; i++) write(STDOUT_FILENO, " ", 1);
    
    atur_posisi_kursor(2, rows);
    printf("Sertakan border? [Y/n]: ");
    fflush(stdout);
    
    len = 0;
    cursor = 0;
    if (baca_input_edit(input, &len, &cursor, 22, rows) != KEY_BATAL) {
        if (len > 0 && (input[0] == 'n' || input[0] == 'N')) {
            opts->with_borders = 0;
        }
    }
}

/* Interactive HTML export */
int export_html_interaktif(const struct buffer_tabel *buf, const char *fn) {
    struct html_export_opts opts;
    
    prompt_html_opts(&opts);
    
    return simpan_html_opts(buf, fn, &opts);
}
