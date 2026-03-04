/* ============================================================
 * TABEL v3.0
 * Berkas: json.c - Format Berkas JSON (JavaScript Object Notation)
 * Standar: C89 + POSIX.1-2008
 * Penulis: Chandra Lesmana
 * ============================================================ */

#include "tabel.h"

/* ============================================================
 * KONSTANTA JSON
 * ============================================================ */
#define JSON_MAX_LINE        (MAKS_TEKS * MAKS_KOLOM * 2)
#define JSON_MAX_KEY         256
#define JSON_MAX_NESTING     16
#define JSON_BUFFER_SIZE     (64 * 1024)

/* JSON token types */
typedef enum {
    JSON_TOKEN_EOF = 0,
    JSON_TOKEN_LBRACE,      /* { */
    JSON_TOKEN_RBRACE,      /* } */
    JSON_TOKEN_LBRACKET,    /* [ */
    JSON_TOKEN_RBRACKET,    /* ] */
    JSON_TOKEN_COLON,       /* : */
    JSON_TOKEN_COMMA,       /* , */
    JSON_TOKEN_STRING,
    JSON_TOKEN_NUMBER,
    JSON_TOKEN_TRUE,
    JSON_TOKEN_FALSE,
    JSON_TOKEN_NULL,
    JSON_TOKEN_ERROR
} json_token_type;

/* ============================================================
 * VARIABEL GLOBAL
 * ============================================================ */
static char json_error_msg[512] = "";

/* ============================================================
 * FUNGSI HELPER
 * ============================================================ */

/* Set error message */
static void json_set_error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(json_error_msg, sizeof(json_error_msg), fmt, args);
    va_end(args);
}

/* Get error message */
const char *json_get_error(void) {
    return json_error_msg;
}

/* ============================================================
 * JSON LEXER
 * ============================================================ */

/* JSON lexer state */
struct json_lexer {
    const char *input;
    int input_len;
    int pos;
    int line;
    int col;
};

/* JSON token */
struct json_token {
    json_token_type type;
    char value[MAKS_TEKS];
    double num_value;
};

/* Initialize lexer */
static void json_lexer_init(struct json_lexer *lex, const char *input, int input_len) {
    memset(lex, 0, sizeof(*lex));
    lex->input = input;
    lex->input_len = input_len;
    lex->line = 1;
    lex->col = 1;
}

/* Skip whitespace */
static void json_skip_ws(struct json_lexer *lex) {
    while (lex->pos < lex->input_len) {
        char c = lex->input[lex->pos];
        if (c == ' ' || c == '\t' || c == '\r') {
            lex->pos++;
            lex->col++;
        } else if (c == '\n') {
            lex->pos++;
            lex->line++;
            lex->col = 1;
        } else {
            break;
        }
    }
}

/* Parse Unicode escape sequence */
static int json_parse_unicode_escape(struct json_lexer *lex, unsigned int *codepoint) {
    int i;
    unsigned int cp = 0;
    
    for (i = 0; i < 4 && lex->pos < lex->input_len; i++) {
        char c = lex->input[lex->pos];
        int digit;
        
        if (c >= '0' && c <= '9') {
            digit = c - '0';
        } else if (c >= 'a' && c <= 'f') {
            digit = c - 'a' + 10;
        } else if (c >= 'A' && c <= 'F') {
            digit = c - 'A' + 10;
        } else {
            return -1;
        }
        
        cp = (cp << 4) | digit;
        lex->pos++;
        lex->col++;
    }
    
    *codepoint = cp;
    return 0;
}

/* Parse string */
static int json_parse_string(struct json_lexer *lex, char *output, size_t output_size) {
    size_t out_pos = 0;
    
    if (lex->input[lex->pos] != '"') {
        return -1;
    }
    
    lex->pos++;  /* Skip opening quote */
    lex->col++;
    
    while (lex->pos < lex->input_len && out_pos < output_size - 1) {
        char c = lex->input[lex->pos];
        
        if (c == '"') {
            lex->pos++;
            lex->col++;
            output[out_pos] = '\0';
            return 0;
        }
        
        if (c == '\\') {
            lex->pos++;
            lex->col++;
            
            if (lex->pos >= lex->input_len) {
                return -1;
            }
            
            c = lex->input[lex->pos];
            
            switch (c) {
                case '"':  output[out_pos++] = '"'; break;
                case '\\': output[out_pos++] = '\\'; break;
                case '/':  output[out_pos++] = '/'; break;
                case 'b':  output[out_pos++] = '\b'; break;
                case 'f':  output[out_pos++] = '\f'; break;
                case 'n':  output[out_pos++] = '\n'; break;
                case 'r':  output[out_pos++] = '\r'; break;
                case 't':  output[out_pos++] = '\t'; break;
                case 'u': {
                    unsigned int codepoint;
                    lex->pos++;
                    lex->col++;
                    if (json_parse_unicode_escape(lex, &codepoint) != 0) {
                        return -1;
                    }
                    /* Convert to UTF-8 */
                    if (codepoint < 0x80) {
                        output[out_pos++] = (char)codepoint;
                    } else if (codepoint < 0x800) {
                        if (out_pos + 2 >= output_size) break;
                        output[out_pos++] = (char)(0xC0 | (codepoint >> 6));
                        output[out_pos++] = (char)(0x80 | (codepoint & 0x3F));
                    } else if (codepoint < 0x10000) {
                        if (out_pos + 3 >= output_size) break;
                        output[out_pos++] = (char)(0xE0 | (codepoint >> 12));
                        output[out_pos++] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
                        output[out_pos++] = (char)(0x80 | (codepoint & 0x3F));
                    } else if (codepoint < 0x110000) {
                        if (out_pos + 4 >= output_size) break;
                        output[out_pos++] = (char)(0xF0 | (codepoint >> 18));
                        output[out_pos++] = (char)(0x80 | ((codepoint >> 12) & 0x3F));
                        output[out_pos++] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
                        output[out_pos++] = (char)(0x80 | (codepoint & 0x3F));
                    }
                    lex->pos--;  /* Will be incremented below */
                    break;
                }
                default:
                    output[out_pos++] = c;
                    break;
            }
        } else {
            output[out_pos++] = c;
        }
        
        lex->pos++;
        lex->col++;
    }
    
    output[out_pos] = '\0';
    return -1;  /* Unterminated string */
}

/* Parse number */
static int json_parse_number(struct json_lexer *lex, double *value) {
    char buf[128];
    int i = 0;
    char *end;
    
    /* Parse number characters */
    while (lex->pos < lex->input_len && i < sizeof(buf) - 1) {
        char c = lex->input[lex->pos];
        
        if ((c >= '0' && c <= '9') || c == '-' || c == '+' || 
            c == '.' || c == 'e' || c == 'E') {
            buf[i++] = c;
            lex->pos++;
            lex->col++;
        } else {
            break;
        }
    }
    
    buf[i] = '\0';
    
    *value = strtod(buf, &end);
    
    return (end == buf + i) ? 0 : -1;
}

/* Get next token */
static int json_next_token(struct json_lexer *lex, struct json_token *tok) {
    char c;
    
    memset(tok, 0, sizeof(*tok));
    
    json_skip_ws(lex);
    
    if (lex->pos >= lex->input_len) {
        tok->type = JSON_TOKEN_EOF;
        return 0;
    }
    
    c = lex->input[lex->pos];
    
    /* Single character tokens */
    switch (c) {
        case '{':
            tok->type = JSON_TOKEN_LBRACE;
            lex->pos++;
            lex->col++;
            return 0;
        case '}':
            tok->type = JSON_TOKEN_RBRACE;
            lex->pos++;
            lex->col++;
            return 0;
        case '[':
            tok->type = JSON_TOKEN_LBRACKET;
            lex->pos++;
            lex->col++;
            return 0;
        case ']':
            tok->type = JSON_TOKEN_RBRACKET;
            lex->pos++;
            lex->col++;
            return 0;
        case ':':
            tok->type = JSON_TOKEN_COLON;
            lex->pos++;
            lex->col++;
            return 0;
        case ',':
            tok->type = JSON_TOKEN_COMMA;
            lex->pos++;
            lex->col++;
            return 0;
    }
    
    /* String */
    if (c == '"') {
        tok->type = JSON_TOKEN_STRING;
        if (json_parse_string(lex, tok->value, sizeof(tok->value)) != 0) {
            tok->type = JSON_TOKEN_ERROR;
            json_set_error("Invalid string at line %d, col %d", lex->line, lex->col);
            return -1;
        }
        return 0;
    }
    
    /* Number */
    if ((c >= '0' && c <= '9') || c == '-') {
        tok->type = JSON_TOKEN_NUMBER;
        if (json_parse_number(lex, &tok->num_value) != 0) {
            tok->type = JSON_TOKEN_ERROR;
            json_set_error("Invalid number at line %d, col %d", lex->line, lex->col);
            return -1;
        }
        /* Also store as string for display */
        snprintf(tok->value, sizeof(tok->value), "%g", tok->num_value);
        return 0;
    }
    
    /* Keywords */
    if (strncmp(lex->input + lex->pos, "true", 4) == 0) {
        tok->type = JSON_TOKEN_TRUE;
        strcpy(tok->value, "true");
        lex->pos += 4;
        lex->col += 4;
        return 0;
    }
    
    if (strncmp(lex->input + lex->pos, "false", 5) == 0) {
        tok->type = JSON_TOKEN_FALSE;
        strcpy(tok->value, "false");
        lex->pos += 5;
        lex->col += 5;
        return 0;
    }
    
    if (strncmp(lex->input + lex->pos, "null", 4) == 0) {
        tok->type = JSON_TOKEN_NULL;
        strcpy(tok->value, "null");
        lex->pos += 4;
        lex->col += 4;
        return 0;
    }
    
    tok->type = JSON_TOKEN_ERROR;
    json_set_error("Unexpected character '%c' at line %d, col %d", c, lex->line, lex->col);
    return -1;
}

/* ============================================================
 * JSON PARSER
 * ============================================================ */

/* JSON value types */
typedef enum {
    JSON_VALUE_NULL = 0,
    JSON_VALUE_BOOL,
    JSON_VALUE_NUMBER,
    JSON_VALUE_STRING,
    JSON_VALUE_ARRAY,
    JSON_VALUE_OBJECT
} json_value_type;

/* Forward declaration */
struct json_value;

/* JSON array */
struct json_array {
    struct json_value **items;
    int count;
    int capacity;
};

/* JSON object member */
struct json_member {
    char key[JSON_MAX_KEY];
    struct json_value *value;
    struct json_member *next;
};

/* JSON object */
struct json_object {
    struct json_member *members;
    int count;
};

/* JSON value */
struct json_value {
    json_value_type type;
    union {
        int bool_val;
        double num_val;
        char str_val[MAKS_TEKS];
        struct json_array arr_val;
        struct json_object obj_val;
    } u;
};

/* Forward declarations for parser */
static struct json_value *json_parse_value(struct json_lexer *lex);

/* Create JSON value */
static struct json_value *json_create_value(json_value_type type) {
    struct json_value *val = calloc(1, sizeof(*val));
    if (val) {
        val->type = type;
    }
    return val;
}

/* Parse array */
static struct json_value *json_parse_array(struct json_lexer *lex) {
    struct json_value *val;
    struct json_token tok;
    int capacity = 16;
    
    val = json_create_value(JSON_VALUE_ARRAY);
    if (!val) return NULL;
    
    val->u.arr_val.items = malloc(capacity * sizeof(struct json_value *));
    if (!val->u.arr_val.items) {
        free(val);
        return NULL;
    }
    val->u.arr_val.capacity = capacity;
    
    /* Check for empty array */
    if (json_next_token(lex, &tok) != 0) {
        free(val->u.arr_val.items);
        free(val);
        return NULL;
    }
    
    if (tok.type == JSON_TOKEN_RBRACKET) {
        return val;  /* Empty array */
    }
    
    /* Put back token - parse first value */
    lex->pos -= (tok.type == JSON_TOKEN_STRING || tok.type == JSON_TOKEN_NUMBER) ? 
                (int)strlen(tok.value) + 2 : 1;
    
    while (1) {
        struct json_value *item;
        
        /* Parse value */
        item = json_parse_value(lex);
        if (!item) {
            /* Error already set */
            return NULL;
        }
        
        /* Expand array if needed */
        if (val->u.arr_val.count >= val->u.arr_val.capacity) {
            capacity *= 2;
            val->u.arr_val.items = realloc(val->u.arr_val.items, 
                                           capacity * sizeof(struct json_value *));
            if (!val->u.arr_val.items) {
                return NULL;
            }
            val->u.arr_val.capacity = capacity;
        }
        
        val->u.arr_val.items[val->u.arr_val.count++] = item;
        
        /* Check for comma or end */
        if (json_next_token(lex, &tok) != 0) {
            return NULL;
        }
        
        if (tok.type == JSON_TOKEN_RBRACKET) {
            return val;
        }
        
        if (tok.type != JSON_TOKEN_COMMA) {
            json_set_error("Expected ',' or ']' at line %d, col %d", lex->line, lex->col);
            return NULL;
        }
    }
}

/* Parse object */
static struct json_value *json_parse_object(struct json_lexer *lex) {
    struct json_value *val;
    struct json_token tok;
    struct json_member *last = NULL;
    
    val = json_create_value(JSON_VALUE_OBJECT);
    if (!val) return NULL;
    
    /* Check for empty object */
    if (json_next_token(lex, &tok) != 0) {
        return NULL;
    }
    
    if (tok.type == JSON_TOKEN_RBRACE) {
        return val;  /* Empty object */
    }
    
    /* Put back token */
    lex->pos -= (tok.type == JSON_TOKEN_STRING) ? (int)strlen(tok.value) + 2 : 1;
    
    while (1) {
        struct json_member *member;
        
        /* Parse key */
        if (json_next_token(lex, &tok) != 0) {
            return NULL;
        }
        
        if (tok.type != JSON_TOKEN_STRING) {
            json_set_error("Expected string key at line %d, col %d", lex->line, lex->col);
            return NULL;
        }
        
        member = calloc(1, sizeof(*member));
        if (!member) return NULL;
        
        strncpy(member->key, tok.value, JSON_MAX_KEY - 1);
        
        /* Parse colon */
        if (json_next_token(lex, &tok) != 0 || tok.type != JSON_TOKEN_COLON) {
            json_set_error("Expected ':' at line %d, col %d", lex->line, lex->col);
            free(member);
            return NULL;
        }
        
        /* Parse value */
        member->value = json_parse_value(lex);
        if (!member->value) {
            free(member);
            return NULL;
        }
        
        /* Add to object */
        if (!val->u.obj_val.members) {
            val->u.obj_val.members = member;
        } else {
            last->next = member;
        }
        last = member;
        val->u.obj_val.count++;
        
        /* Check for comma or end */
        if (json_next_token(lex, &tok) != 0) {
            return NULL;
        }
        
        if (tok.type == JSON_TOKEN_RBRACE) {
            return val;
        }
        
        if (tok.type != JSON_TOKEN_COMMA) {
            json_set_error("Expected ',' or '}' at line %d, col %d", lex->line, lex->col);
            return NULL;
        }
    }
}

/* Parse value */
static struct json_value *json_parse_value(struct json_lexer *lex) {
    struct json_token tok;
    struct json_value *val;
    
    if (json_next_token(lex, &tok) != 0) {
        return NULL;
    }
    
    switch (tok.type) {
        case JSON_TOKEN_NULL:
            val = json_create_value(JSON_VALUE_NULL);
            break;
            
        case JSON_TOKEN_TRUE:
            val = json_create_value(JSON_VALUE_BOOL);
            if (val) val->u.bool_val = 1;
            break;
            
        case JSON_TOKEN_FALSE:
            val = json_create_value(JSON_VALUE_BOOL);
            if (val) val->u.bool_val = 0;
            break;
            
        case JSON_TOKEN_NUMBER:
            val = json_create_value(JSON_VALUE_NUMBER);
            if (val) {
                val->u.num_val = tok.num_value;
                strncpy(val->u.str_val, tok.value, MAKS_TEKS - 1);
            }
            break;
            
        case JSON_TOKEN_STRING:
            val = json_create_value(JSON_VALUE_STRING);
            if (val) {
                strncpy(val->u.str_val, tok.value, MAKS_TEKS - 1);
            }
            break;
            
        case JSON_TOKEN_LBRACKET:
            val = json_parse_array(lex);
            break;
            
        case JSON_TOKEN_LBRACE:
            val = json_parse_object(lex);
            break;
            
        default:
            json_set_error("Unexpected token at line %d, col %d", lex->line, lex->col);
            return NULL;
    }
    
    return val;
}

/* Free JSON value recursively */
static void json_free_value(struct json_value *val) {
    int i;
    
    if (!val) return;
    
    switch (val->type) {
        case JSON_VALUE_ARRAY:
            for (i = 0; i < val->u.arr_val.count; i++) {
                json_free_value(val->u.arr_val.items[i]);
            }
            free(val->u.arr_val.items);
            break;
            
        case JSON_VALUE_OBJECT: {
            struct json_member *member = val->u.obj_val.members;
            while (member) {
                struct json_member *next = member->next;
                json_free_value(member->value);
                free(member);
                member = next;
            }
            break;
        }
        
        default:
            break;
    }
    
    free(val);
}

/* ============================================================
 * JSON TO TABLE CONVERSION
 * ============================================================ */

/* Convert JSON value to string */
static void json_value_to_string(struct json_value *val, char *buf, size_t buf_size) {
    if (!val || !buf || buf_size == 0) {
        if (buf && buf_size > 0) buf[0] = '\0';
        return;
    }
    
    switch (val->type) {
        case JSON_VALUE_NULL:
            strncpy(buf, "null", buf_size - 1);
            break;
        case JSON_VALUE_BOOL:
            strncpy(buf, val->u.bool_val ? "true" : "false", buf_size - 1);
            break;
        case JSON_VALUE_NUMBER:
            strncpy(buf, val->u.str_val, buf_size - 1);
            break;
        case JSON_VALUE_STRING:
            strncpy(buf, val->u.str_val, buf_size - 1);
            break;
        case JSON_VALUE_ARRAY:
            strncpy(buf, "[array]", buf_size - 1);
            break;
        case JSON_VALUE_OBJECT:
            strncpy(buf, "{object}", buf_size - 1);
            break;
        default:
            buf[0] = '\0';
            break;
    }
    
    buf[buf_size - 1] = '\0';
}

/* Convert JSON array of arrays to table */
static int json_array_to_table(struct json_value *arr, struct buffer_tabel **pbuf) {
    int rows, cols;
    int r, c;
    struct buffer_tabel *buf;
    
    if (!arr || arr->type != JSON_VALUE_ARRAY) return -1;
    
    /* Determine dimensions */
    rows = arr->u.arr_val.count;
    cols = 0;
    
    for (r = 0; r < rows; r++) {
        struct json_value *row = arr->u.arr_val.items[r];
        if (row && row->type == JSON_VALUE_ARRAY) {
            if (row->u.arr_val.count > cols) {
                cols = row->u.arr_val.count;
            }
        }
    }
    
    if (rows == 0 || cols == 0) {
        json_set_error("Empty array or no columns found");
        return -1;
    }
    
    /* Create buffer */
    bebas_buffer(*pbuf);
    buf = buat_buffer(rows, cols);
    if (!buf) return -1;
    
    /* Fill data */
    for (r = 0; r < rows && r < buf->cfg.baris; r++) {
        struct json_value *row = arr->u.arr_val.items[r];
        
        if (row && row->type == JSON_VALUE_ARRAY) {
            for (c = 0; c < row->u.arr_val.count && c < buf->cfg.kolom; c++) {
                json_value_to_string(row->u.arr_val.items[c], 
                                     buf->isi[r][c], MAKS_TEKS);
            }
        } else if (row) {
            /* Single value row */
            json_value_to_string(row, buf->isi[r][0], MAKS_TEKS);
        }
    }
    
    *pbuf = buf;
    return 0;
}

/* Convert JSON array of objects to table */
static int json_objects_to_table(struct json_value *arr, struct buffer_tabel **pbuf) {
    int rows, cols;
    int r, c;
    struct buffer_tabel *buf;
    struct json_member *member;
    char **headers;
    int num_headers;
    int header_capacity;
    
    if (!arr || arr->type != JSON_VALUE_ARRAY) return -1;
    
    /* Collect all unique keys as headers */
    header_capacity = 32;
    headers = calloc(header_capacity, sizeof(char *));
    num_headers = 0;
    
    rows = arr->u.arr_val.count;
    
    for (r = 0; r < rows; r++) {
        struct json_value *obj = arr->u.arr_val.items[r];
        
        if (obj && obj->type == JSON_VALUE_OBJECT) {
            member = obj->u.obj_val.members;
            while (member) {
                /* Check if key already exists */
                int found = 0;
                for (c = 0; c < num_headers; c++) {
                    if (strcmp(headers[c], member->key) == 0) {
                        found = 1;
                        break;
                    }
                }
                
                if (!found) {
                    /* Add new header */
                    if (num_headers >= header_capacity) {
                        header_capacity *= 2;
                        headers = realloc(headers, header_capacity * sizeof(char *));
                    }
                    headers[num_headers] = malloc(strlen(member->key) + 1);
                    strcpy(headers[num_headers], member->key);
                    num_headers++;
                }
                
                member = member->next;
            }
        }
    }
    
    cols = num_headers;
    
    if (rows == 0 || cols == 0) {
        json_set_error("No data found in JSON objects");
        for (c = 0; c < num_headers; c++) free(headers[c]);
        free(headers);
        return -1;
    }
    
    /* Create buffer with extra row for headers */
    bebas_buffer(*pbuf);
    buf = buat_buffer(rows + 1, cols);
    if (!buf) {
        for (c = 0; c < num_headers; c++) free(headers[c]);
        free(headers);
        return -1;
    }
    
    /* Fill headers */
    for (c = 0; c < num_headers && c < buf->cfg.kolom; c++) {
        strncpy(buf->isi[0][c], headers[c], MAKS_TEKS - 1);
    }
    
    /* Fill data */
    for (r = 0; r < rows && r + 1 < buf->cfg.baris; r++) {
        struct json_value *obj = arr->u.arr_val.items[r];
        
        if (obj && obj->type == JSON_VALUE_OBJECT) {
            for (c = 0; c < num_headers && c < buf->cfg.kolom; c++) {
                /* Find value for this column */
                member = obj->u.obj_val.members;
                while (member) {
                    if (strcmp(member->key, headers[c]) == 0) {
                        json_value_to_string(member->value, 
                                             buf->isi[r + 1][c], MAKS_TEKS);
                        break;
                    }
                    member = member->next;
                }
            }
        }
    }
    
    /* Cleanup */
    for (c = 0; c < num_headers; c++) free(headers[c]);
    free(headers);
    
    *pbuf = buf;
    return 0;
}

/* Main JSON open function */
int buka_json(struct buffer_tabel **pbuf, const char *fn) {
    FILE *fp;
    char *json_content;
    size_t json_len;
    struct json_lexer lex;
    struct json_value *root;
    int ret = -1;
    
    if (!pbuf || !fn) return -1;
    
    /* Baca file */
    fp = fopen(fn, "r");
    if (!fp) {
        json_set_error("Cannot open file: %s", fn);
        return -1;
    }
    
    /* Get file size */
    fseek(fp, 0, SEEK_END);
    json_len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    /* Allocate buffer */
    json_content = malloc(json_len + 1);
    if (!json_content) {
        fclose(fp);
        json_set_error("Memory allocation failed");
        return -1;
    }
    
    /* Read content */
    json_len = fread(json_content, 1, json_len, fp);
    fclose(fp);
    json_content[json_len] = '\0';
    
    /* Initialize lexer */
    json_lexer_init(&lex, json_content, (int)json_len);
    
    /* Parse JSON */
    root = json_parse_value(&lex);
    free(json_content);
    
    if (!root) {
        return -1;
    }
    
    /* Determine format and convert */
    if (root->type == JSON_VALUE_ARRAY) {
        if (root->u.arr_val.count > 0) {
            struct json_value *first = root->u.arr_val.items[0];
            
            if (first && first->type == JSON_VALUE_ARRAY) {
                /* Array of arrays */
                ret = json_array_to_table(root, pbuf);
            } else if (first && first->type == JSON_VALUE_OBJECT) {
                /* Array of objects */
                ret = json_objects_to_table(root, pbuf);
            } else {
                /* Single value array - convert to single column */
                struct buffer_tabel *buf;
                int r;
                
                bebas_buffer(*pbuf);
                buf = buat_buffer(root->u.arr_val.count, 1);
                if (buf) {
                    for (r = 0; r < root->u.arr_val.count; r++) {
                        json_value_to_string(root->u.arr_val.items[r],
                                             buf->isi[r][0], MAKS_TEKS);
                    }
                    *pbuf = buf;
                    ret = 0;
                }
            }
        } else {
            json_set_error("Empty JSON array");
        }
    } else if (root->type == JSON_VALUE_OBJECT) {
        /* Single object - convert to two columns (key, value) */
        struct buffer_tabel *buf;
        struct json_member *member;
        int r;
        
        bebas_buffer(*pbuf);
        buf = buat_buffer(root->u.obj_val.count + 1, 2);
        if (buf) {
            /* Headers */
            strcpy(buf->isi[0][0], "Key");
            strcpy(buf->isi[0][1], "Value");
            
            /* Data */
            member = root->u.obj_val.members;
            r = 1;
            while (member && r < buf->cfg.baris) {
                strncpy(buf->isi[r][0], member->key, MAKS_TEKS - 1);
                json_value_to_string(member->value, buf->isi[r][1], MAKS_TEKS);
                member = member->next;
                r++;
            }
            
            *pbuf = buf;
            ret = 0;
        }
    } else {
        json_set_error("JSON root must be array or object");
    }
    
    json_free_value(root);
    return ret;
}

/* ============================================================
 * JSON EXPORT
 * ============================================================ */

/* JSON export format options */
typedef enum {
    JSON_FORMAT_ARRAY = 0,    /* Array of arrays: [[...], [...]] */
    JSON_FORMAT_OBJECTS = 1,  /* Array of objects: [{...}, {...}] */
    JSON_FORMAT_KEYED = 2     /* Keyed object: {"A1": "...", "B1": "..."} */
} json_format_type;

/* JSON export options */
struct json_export_opts {
    json_format_type format;
    int pretty_print;         /* Add whitespace for readability */
    int include_headers;      /* For objects format, use first row as keys */
    int indent_size;          /* Spaces per indent level */
};

/* Set default options */
static void json_set_default_opts(struct json_export_opts *opts) {
    memset(opts, 0, sizeof(*opts));
    opts->format = JSON_FORMAT_ARRAY;
    opts->pretty_print = 1;
    opts->include_headers = 1;
    opts->indent_size = 2;
}

/* Write JSON string with escaping */
static void json_write_string(FILE *fp, const char *str) {
    const char *p;
    
    fputc('"', fp);
    
    for (p = str; *p; p++) {
        switch (*p) {
            case '"':  fprintf(fp, "\\\""); break;
            case '\\': fprintf(fp, "\\\\"); break;
            case '\b': fprintf(fp, "\\b"); break;
            case '\f': fprintf(fp, "\\f"); break;
            case '\n': fprintf(fp, "\\n"); break;
            case '\r': fprintf(fp, "\\r"); break;
            case '\t': fprintf(fp, "\\t"); break;
            default:
                if ((unsigned char)*p < 0x20) {
                    fprintf(fp, "\\u%04x", (unsigned char)*p);
                } else {
                    fputc(*p, fp);
                }
                break;
        }
    }
    
    fputc('"', fp);
}

/* Write indent */
static void json_write_indent(FILE *fp, int level, int indent_size) {
    int i;
    for (i = 0; i < level * indent_size; i++) {
        fputc(' ', fp);
    }
}

/* Export as array of arrays */
static int json_export_array(const struct buffer_tabel *buf, FILE *fp,
                             const struct json_export_opts *opts) {
    int r, c;
    int indent = opts->pretty_print ? 1 : 0;
    
    fprintf(fp, "[");
    
    for (r = 0; r < buf->cfg.baris; r++) {
        if (opts->pretty_print) {
            fprintf(fp, "\n");
            json_write_indent(fp, indent, opts->indent_size);
        }
        
        fprintf(fp, "[");
        
        for (c = 0; c < buf->cfg.kolom; c++) {
            if (c > 0) fprintf(fp, ", ");
            json_write_string(fp, buf->isi[r][c]);
        }
        
        fprintf(fp, "]");
        
        if (r < buf->cfg.baris - 1) fprintf(fp, ",");
    }
    
    if (opts->pretty_print) {
        fprintf(fp, "\n]");
    } else {
        fprintf(fp, "]");
    }
    
    return 0;
}

/* Export as array of objects */
static int json_export_objects(const struct buffer_tabel *buf, FILE *fp,
                               const struct json_export_opts *opts) {
    int r, c;
    int indent = opts->pretty_print ? 1 : 0;
    int start_row = opts->include_headers ? 1 : 0;
    char col_name[16];
    
    fprintf(fp, "[");
    
    for (r = start_row; r < buf->cfg.baris; r++) {
        if (opts->pretty_print) {
            fprintf(fp, "\n");
            json_write_indent(fp, indent, opts->indent_size);
        }
        
        fprintf(fp, "{");
        
        for (c = 0; c < buf->cfg.kolom; c++) {
            const char *key;
            
            if (opts->pretty_print && c > 0) {
                fprintf(fp, "\n");
                json_write_indent(fp, indent + 1, opts->indent_size);
            } else if (c > 0) {
                fprintf(fp, ", ");
            }
            
            /* Get key from header row or column letter */
            if (opts->include_headers && buf->cfg.baris > 0) {
                key = buf->isi[0][c];
            } else {
                /* Generate column name */
                xlsx_col_to_letters(c, col_name, sizeof(col_name));
                key = col_name;
            }
            
            json_write_string(fp, key);
            fprintf(fp, ": ");
            json_write_string(fp, buf->isi[r][c]);
        }
        
        fprintf(fp, "}");
        
        if (r < buf->cfg.baris - 1) fprintf(fp, ",");
    }
    
    if (opts->pretty_print) {
        fprintf(fp, "\n]");
    } else {
        fprintf(fp, "]");
    }
    
    return 0;
}

/* Export as keyed object (cell references) */
static int json_export_keyed(const struct buffer_tabel *buf, FILE *fp,
                             const struct json_export_opts *opts) {
    int r, c;
    int indent = opts->pretty_print ? 1 : 0;
    char cell_ref[16];
    int first = 1;
    
    fprintf(fp, "{");
    
    for (r = 0; r < buf->cfg.baris; r++) {
        for (c = 0; c < buf->cfg.kolom; c++) {
            /* Skip empty cells */
            if (buf->isi[r][c][0] == '\0') continue;
            
            if (opts->pretty_print && !first) {
                fprintf(fp, "\n");
                json_write_indent(fp, indent, opts->indent_size);
            } else if (!first) {
                fprintf(fp, ", ");
            }
            
            first = 0;
            
            /* Generate cell reference */
            xlsx_col_to_letters(c, cell_ref, sizeof(cell_ref));
            sprintf(cell_ref + strlen(cell_ref), "%d", r + 1);
            
            json_write_string(fp, cell_ref);
            fprintf(fp, ": ");
            json_write_string(fp, buf->isi[r][c]);
        }
    }
    
    if (opts->pretty_print) {
        fprintf(fp, "\n}");
    } else {
        fprintf(fp, "}");
    }
    
    return 0;
}

/* JSON save with options */
int simpan_json_opts(const struct buffer_tabel *buf, const char *fn,
                     const struct json_export_opts *user_opts) {
    FILE *fp;
    struct json_export_opts opts;
    int ret;
    
    if (!buf || !fn) return -1;
    
    /* Set options */
    if (user_opts) {
        memcpy(&opts, user_opts, sizeof(opts));
    } else {
        json_set_default_opts(&opts);
    }
    
    fp = fopen(fn, "w");
    if (!fp) {
        json_set_error("Cannot create file: %s", fn);
        return -1;
    }
    
    /* Export based on format */
    switch (opts.format) {
        case JSON_FORMAT_ARRAY:
            ret = json_export_array(buf, fp, &opts);
            break;
        case JSON_FORMAT_OBJECTS:
            ret = json_export_objects(buf, fp, &opts);
            break;
        case JSON_FORMAT_KEYED:
            ret = json_export_keyed(buf, fp, &opts);
            break;
        default:
            ret = -1;
            break;
    }
    
    fprintf(fp, "\n");
    
    if (fsync_berkas(fp) != 0) {
        fclose(fp);
        return -1;
    }
    
    fclose(fp);
    return ret;
}

/* Main JSON save function */
int simpan_json(const struct buffer_tabel *buf, const char *fn) {
    struct json_export_opts opts;
    json_set_default_opts(&opts);
    return simpan_json_opts(buf, fn, &opts);
}

/* ============================================================
 * DETEKSI DAN VALIDASI
 * ============================================================ */

/* Check if file is valid JSON */
int json_apakah_valid(const char *fn) {
    FILE *fp;
    char buf[64];
    size_t len;
    
    if (!fn) return 0;
    
    fp = fopen(fn, "r");
    if (!fp) return 0;
    
    /* Skip whitespace */
    while (1) {
        int c = fgetc(fp);
        if (c == EOF) {
            fclose(fp);
            return 0;
        }
        if (!isspace(c)) {
            ungetc(c, fp);
            break;
        }
    }
    
    /* Check first character */
    len = fread(buf, 1, 1, fp);
    fclose(fp);
    
    if (len != 1) return 0;
    
    /* JSON must start with { or [ */
    return (buf[0] == '{' || buf[0] == '[');
}

/* Get JSON info */
int json_info(const char *fn, int *is_array, int *row_estimate, int *col_estimate) {
    FILE *fp;
    char *content;
    size_t len;
    struct json_lexer lex;
    struct json_value *root;
    
    if (!fn) return -1;
    
    /* Initialize output */
    if (is_array) *is_array = 0;
    if (row_estimate) *row_estimate = 0;
    if (col_estimate) *col_estimate = 0;
    
    fp = fopen(fn, "r");
    if (!fp) return -1;
    
    fseek(fp, 0, SEEK_END);
    len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    content = malloc(len + 1);
    if (!content) {
        fclose(fp);
        return -1;
    }
    
    len = fread(content, 1, len, fp);
    fclose(fp);
    content[len] = '\0';
    
    json_lexer_init(&lex, content, (int)len);
    root = json_parse_value(&lex);
    free(content);
    
    if (!root) return -1;
    
    if (root->type == JSON_VALUE_ARRAY) {
        if (is_array) *is_array = 1;
        if (row_estimate) *row_estimate = root->u.arr_val.count;
        
        if (root->u.arr_val.count > 0) {
            struct json_value *first = root->u.arr_val.items[0];
            if (first && first->type == JSON_VALUE_ARRAY) {
                if (col_estimate) *col_estimate = first->u.arr_val.count;
            } else if (first && first->type == JSON_VALUE_OBJECT) {
                if (col_estimate) *col_estimate = first->u.obj_val.count;
            }
        }
    } else if (root->type == JSON_VALUE_OBJECT) {
        if (is_array) *is_array = 0;
        if (row_estimate) *row_estimate = root->u.obj_val.count;
        if (col_estimate) *col_estimate = 2;  /* Key, Value */
    }
    
    json_free_value(root);
    return 0;
}

/* ============================================================
 * INTERACTIVE EXPORT
 * ============================================================ */

/* Prompt user for JSON export options */
void prompt_json_opts(struct json_export_opts *opts) {
    char input[64];
    int len, cursor;
    int rows, cols;
    
    if (!opts) return;
    
    json_set_default_opts(opts);
    
    rows = ambil_tinggi_terminal();
    cols = ambil_lebar_terminal();
    
    /* Prompt format */
    atur_posisi_kursor(1, rows);
    for (int i = 0; i < cols; i++) write(STDOUT_FILENO, " ", 1);
    
    atur_posisi_kursor(2, rows);
    printf("Format [1=Array, 2=Objects, 3=Keyed]: ");
    fflush(stdout);
    
    len = 0;
    cursor = 0;
    if (baca_input_edit(input, &len, &cursor, 38, rows) != KEY_BATAL) {
        if (len > 0) {
            int choice = atoi(input);
            if (choice >= 1 && choice <= 3) {
                opts->format = (json_format_type)(choice - 1);
            }
        }
    }
    
    /* Prompt pretty print */
    atur_posisi_kursor(1, rows);
    for (int i = 0; i < cols; i++) write(STDOUT_FILENO, " ", 1);
    
    atur_posisi_kursor(2, rows);
    printf("Pretty print? [Y/n]: ");
    fflush(stdout);
    
    len = 0;
    cursor = 0;
    if (baca_input_edit(input, &len, &cursor, 21, rows) != KEY_BATAL) {
        if (len > 0 && (input[0] == 'n' || input[0] == 'N')) {
            opts->pretty_print = 0;
        }
    }
    
    /* Prompt include headers (only for objects format) */
    if (opts->format == JSON_FORMAT_OBJECTS) {
        atur_posisi_kursor(1, rows);
        for (int i = 0; i < cols; i++) write(STDOUT_FILENO, " ", 1);
        
        atur_posisi_kursor(2, rows);
        printf("Gunakan baris 1 sebagai key? [Y/n]: ");
        fflush(stdout);
        
        len = 0;
        cursor = 0;
        if (baca_input_edit(input, &len, &cursor, 32, rows) != KEY_BATAL) {
            if (len > 0 && (input[0] == 'n' || input[0] == 'N')) {
                opts->include_headers = 0;
            }
        }
    }
}

/* Interactive JSON export */
int export_json_interaktif(const struct buffer_tabel *buf, const char *fn) {
    struct json_export_opts opts;
    
    prompt_json_opts(&opts);
    
    return simpan_json_opts(buf, fn, &opts);
}
