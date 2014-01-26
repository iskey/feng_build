/**
 * Copyright (c) 2004, Jan Kneschke, incremental
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * - Neither the name of the 'incremental' nor the names of its contributors
 *   may be used to endorse or promote products derived from this software
 *   without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file configfile.c
 * Main configuration parsing functions
 */

#include <glib.h>

#include <sys/stat.h>

#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <assert.h>

#include "feng.h"
//#include "log.h"

#define log_error_write(...) {}

#include "stream.h"
//#include "plugin.h"
#include "network/rtp.h" // defaults
#include "configparser.h"
#include "configfile.h"
#include "proc_open.h"

/**
 * Insert the main configuration keys and their defaults
 */

static int config_insert(server *srv) {
    size_t i;
    int ret = 0;

    config_values_t cv[] = {
        { "server.bind", srv->srvconf.bindhost, T_CONFIG_STRING, T_CONFIG_SCOPE_SERVER },      /* 0 */
        { "server.errorlog", srv->srvconf.errorlog_file, T_CONFIG_STRING, T_CONFIG_SCOPE_SERVER },      /* 1 */
//        { "server.errorfile-prefix",     NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_SERVER },      /* 2 */
//        { "server.chroot",               NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_SERVER },      /* 3 */
        { "server.username", srv->srvconf.username, T_CONFIG_STRING, T_CONFIG_SCOPE_SERVER },      /* 2 */
        { "server.groupname", srv->srvconf.groupname, T_CONFIG_STRING, T_CONFIG_SCOPE_SERVER },      /* 3 */
        { "server.port", &srv->srvconf.port, T_CONFIG_SHORT,  T_CONFIG_SCOPE_SERVER },      /* 4 */
//        { "server.tag",                  NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_CONNECTION },  /* 6 */
        { "server.use-ipv6",             NULL, T_CONFIG_BOOLEAN, T_CONFIG_SCOPE_CONNECTION }, /* 5 */
        { "server.modules", srv->srvconf.modules, T_CONFIG_ARRAY, T_CONFIG_SCOPE_SERVER },       /* 6 */

        { "server.document-root", NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_CONNECTION },  /* 7 */
        { "server.name",                 NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_CONNECTION },  /* 8 */

        { "server.max-fds", &srv->srvconf.max_fds, T_CONFIG_SHORT, T_CONFIG_SCOPE_SERVER },       /* 9 */
#if 0 // HAVE_LSTAT
        { "server.follow-symlink",       NULL, T_CONFIG_BOOLEAN, T_CONFIG_SCOPE_CONNECTION }, /* 12 */
#else
        { "server.follow-symlink",
          (void *)"Unsupported for now",
          T_CONFIG_UNSUPPORTED, T_CONFIG_SCOPE_UNSET }, /* 10 */
#endif
//        { "server.kbytes-per-second",    NULL, T_CONFIG_SHORT, T_CONFIG_SCOPE_CONNECTION },   /* 25 */
//        { "connection.kbytes-per-second", NULL, T_CONFIG_SHORT, T_CONFIG_SCOPE_CONNECTION },  /* 26 */
        { "ssl.pemfile",                 NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_SERVER },      /* 11 */

        { "ssl.engine",                  NULL, T_CONFIG_BOOLEAN, T_CONFIG_SCOPE_SERVER },     /* 12 */

        { "ssl.ca-file",                 NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_SERVER },      /* 13 */

        { "server.errorlog-use-syslog", &srv->srvconf.errorlog_use_syslog, T_CONFIG_BOOLEAN, T_CONFIG_SCOPE_SERVER },     /* 14 */
        { "server.max-connections", &srv->srvconf.max_conns, T_CONFIG_SHORT, T_CONFIG_SCOPE_SERVER },       /* 15 */
//        { "server.upload-dirs",          NULL, T_CONFIG_ARRAY, T_CONFIG_SCOPE_CONNECTION },   /* 44 */
        { "ssl.cipher-list",             NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_SERVER },      /* 16 */
        { "sctp.protocol",               NULL, T_CONFIG_BOOLEAN, T_CONFIG_SCOPE_SERVER },
        { "sctp.max_streams",            NULL, T_CONFIG_SHORT, T_CONFIG_SCOPE_SERVER },

	// Metadata begin
#ifdef HAVE_METADATA
        { "cpd.port",                    NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_SERVER },    /* 19 */
        { "cpd.db.host",                    NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_SERVER },    /* 20 */
        { "cpd.db.user",                    NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_SERVER },    /* 21 */
        { "cpd.db.password",                    NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_SERVER },    /* 22 */
        { "cpd.db.name",                    NULL, T_CONFIG_STRING, T_CONFIG_SCOPE_SERVER },    /* 23 */
#endif
	// Metadata end

        { "server.first_udp_port",  &srv->srvconf.first_udp_port, T_CONFIG_SHORT, T_CONFIG_SCOPE_SERVER },
        { "server.buffered_frames", &srv->srvconf.buffered_frames, T_CONFIG_SHORT, T_CONFIG_SCOPE_SERVER },
        { "server.loglevel", &srv->srvconf.loglevel, T_CONFIG_SHORT, T_CONFIG_SCOPE_SERVER },
        { NULL,                          NULL, T_CONFIG_UNSET, T_CONFIG_SCOPE_UNSET }
    };

    srv->config_storage = calloc(srv->config_context->used, sizeof(specific_config));

    assert(srv->config_storage);

    for (i = 0; i < srv->config_context->used; i++) {
        specific_config *s = &srv->config_storage[i];

        s->document_root = buffer_init();
        s->server_name   = buffer_init();
        s->ssl_pemfile   = buffer_init();
        s->ssl_ca_file   = buffer_init();
        s->ssl_cipher_list = buffer_init();
        s->use_ipv6      = 0;
        s->is_ssl        = 0;
        s->is_sctp       = 0;
        s->sctp_max_streams = 16;

	// Metadata begin
#ifdef HAVE_METADATA
        s->cpd_port = buffer_init();
        s->cpd_db_host = buffer_init();
        s->cpd_db_user = buffer_init();
        s->cpd_db_password = buffer_init();
        s->cpd_db_name = buffer_init();
#endif
	// Metadata end

#ifdef HAVE_LSTAT
        s->follow_symlink = 1;
#endif
/*        s->kbytes_per_second = 0;
        s->global_kbytes_per_second = 0;
        s->global_bytes_per_second_cnt = 0;
        s->global_bytes_per_second_cnt_ptr = &s->global_bytes_per_second_cnt;
*/
        cv[5].destination = &s->use_ipv6;

        cv[7].destination = s->document_root;
        cv[8].destination = s->server_name;
        /* 9 -> max-fds */
#ifdef HAVE_LSTAT
        cv[10].destination = &s->follow_symlink;
#endif
//        cv[25].destination = &(s->global_kbytes_per_second);
//        cv[26].destination = &(s->kbytes_per_second);
        cv[11].destination = s->ssl_pemfile;

        cv[12].destination = &s->is_ssl;
        cv[13].destination = s->ssl_ca_file;
        cv[16].destination = s->ssl_cipher_list;

        cv[17].destination = &s->is_sctp;
        cv[18].destination = &s->sctp_max_streams;

	// Metadata begin
#ifdef HAVE_METADATA
        cv[19].destination = s->cpd_port;
        cv[20].destination = s->cpd_db_host;
        cv[21].destination = s->cpd_db_user;
        cv[22].destination = s->cpd_db_password;
        cv[23].destination = s->cpd_db_name;
#endif
	// Metadata end

        if (0 != (ret = config_insert_values_global(srv, ((data_config *)srv->config_context->data[i])->value, cv))) {
            break;
        }

    }

    return ret;

}

#if 0
#define PATCH(x) con->conf.x = s->x
int config_setup_connection(server *srv, connection *con) {
    specific_config *s = &srv->config_storage[0];

    PATCH(document_root);
#ifdef HAVE_LSTAT
    PATCH(follow_symlink);
#endif
    PATCH(kbytes_per_second);
    PATCH(global_kbytes_per_second);
    PATCH(global_bytes_per_second_cnt);

    con->conf.global_bytes_per_second_cnt_ptr = &s->global_bytes_per_second_cnt;
    buffer_copy_string_buffer(con->server_name, s->server_name);

    PATCH(is_ssl);

    PATCH(ssl_pemfile);
    PATCH(ssl_ca_file);
    PATCH(ssl_cipher_list);

    return 0;
}

int config_patch_connection(server *srv, connection *con, comp_key_t comp) {
    size_t i, j;

    con->conditional_is_valid[comp] = 1;

    /* skip the first, the global context */
    for (i = 1; i < srv->config_context->used; i++) {
        data_config *dc = (data_config *)srv->config_context->data[i];
        specific_config *s = &srv->config_storage[i];

        /* not our stage */
        if (comp != dc->comp) continue;

        /* condition didn't match */
        if (!config_check_cond(srv, con, dc)) continue;

        /* merge config */
        for (j = 0; j < dc->value->used; j++) {
            data_unset *du = dc->value->data[j];

            if (buffer_is_equal_string(du->key, CONST_STR_LEN("server.document-root"))) {
                PATCH(document_root);
            } else if (buffer_is_equal_string(du->key, CONST_STR_LEN("ssl.pemfile"))) {
                PATCH(ssl_pemfile);
            } else if (buffer_is_equal_string(du->key, CONST_STR_LEN("ssl.ca-file"))) {
                PATCH(ssl_ca_file);
            } else if (buffer_is_equal_string(du->key, CONST_STR_LEN("ssl.cipher-list"))) {
                PATCH(ssl_cipher_list);
            } else if (buffer_is_equal_string(du->key, CONST_STR_LEN("ssl.engine"))) {
                PATCH(is_ssl);
#ifdef HAVE_LSTAT
            } else if (buffer_is_equal_string(du->key, CONST_STR_LEN("server.follow-symlink"))) {
                PATCH(follow_symlink);
#endif
            } else if (buffer_is_equal_string(du->key, CONST_STR_LEN("server.name"))) {
                buffer_copy_string_buffer(con->server_name, s->server_name);
            } else if (buffer_is_equal_string(du->key, CONST_STR_LEN("connection.kbytes-per-second"))) {
                PATCH(kbytes_per_second);
            } else if (buffer_is_equal_string(du->key, CONST_STR_LEN("server.kbytes-per-second"))) {
                PATCH(global_kbytes_per_second);
                PATCH(global_bytes_per_second_cnt);
                con->conf.global_bytes_per_second_cnt_ptr = &s->global_bytes_per_second_cnt;
            }
        }
    }

    return 0;
}
#undef PATCH
#endif

typedef struct {
    int foo;
    int bar;

    const conf_buffer *source;
    const char *input;
    size_t offset;
    size_t size;

    int line_pos;
    int line;

    int in_key;
    int in_brace;
    int in_cond;
} tokenizer_t;

#if 0
static int tokenizer_open(server *srv, tokenizer_t *t, conf_buffer *basedir, const char *fn) {
    if (buffer_is_empty(basedir) &&
            (fn[0] == '/' || fn[0] == '\\') &&
            (fn[0] == '.' && (fn[1] == '/' || fn[1] == '\\'))) {
        t->file = buffer_init_string(fn);
    } else {
        t->file = buffer_init_buffer(basedir);
        buffer_append_string(t->file, fn);
    }

    if (0 != stream_open(&(t->s), t->file->ptr)) {
        log_error_write(srv, __FILE__, __LINE__, "sbss",
                "opening configfile ", t->file, "failed:", strerror(errno));
        buffer_free(t->file);
        return -1;
    }

    t->input = t->s.start;
    t->offset = 0;
    t->size = t->s.size;
    t->line = 1;
    t->line_pos = 1;

    t->in_key = 1;
    t->in_brace = 0;
    t->in_cond = 0;
    return 0;
}

static int tokenizer_close(server *srv, tokenizer_t *t) {
    UNUSED(srv);

    buffer_free(t->file);
    return stream_close(&(t->s));
}
#endif

/**
 * skip any newline (unix, windows, mac style)
 */

static int config_skip_newline(tokenizer_t *t) {
    int skipped = 1;
    assert(t->input[t->offset] == '\r' || t->input[t->offset] == '\n');
    if (t->input[t->offset] == '\r' && t->input[t->offset + 1] == '\n') {
        skipped ++;
        t->offset ++;
    }
    t->offset ++;
    return skipped;
}

/**
 * Skip comments
 * a comment starts with an # and last till a newline
 */

static int config_skip_comment(tokenizer_t *t) {
    int i;
    assert(t->input[t->offset] == '#');
    for (i = 1; t->input[t->offset + i] &&
         (t->input[t->offset + i] != '\n' && t->input[t->offset + i] != '\r');
         i++);
    t->offset += i;
    return i;
}

/**
 * Break the configuration in tokens
 */

static int config_tokenizer(server *srv, tokenizer_t *t, int *token_id, conf_buffer *token) {
    int tid = 0;
    size_t i;

    for (tid = 0; tid == 0 && t->offset < t->size && t->input[t->offset] ; ) {
        char c = t->input[t->offset];
        const char *start = NULL;

        switch (c) {
        case '=':
            if (t->in_brace) {
                if (t->input[t->offset + 1] == '>') {
                    t->offset += 2;

                    buffer_copy_string(token, "=>");

                    tid = TK_ARRAY_ASSIGN;
                } else {
                    log_error_write(srv, __FILE__, __LINE__, "sbsdsds",
                            "source:", t->source,
                            "line:", t->line, "pos:", t->line_pos,
                            "use => for assignments in arrays");

                    return -1;
                }
            } else if (t->in_cond) {
                if (t->input[t->offset + 1] == '=') {
                    t->offset += 2;

                    buffer_copy_string(token, "==");

                    tid = TK_EQ;
                } else if (t->input[t->offset + 1] == '~') {
                    t->offset += 2;

                    buffer_copy_string(token, "=~");

                    tid = TK_MATCH;
                } else {
                    log_error_write(srv, __FILE__, __LINE__, "sbsdsds",
                            "source:", t->source,
                            "line:", t->line, "pos:", t->line_pos,
                            "only =~ and == are allowed in the condition");
                    return -1;
                }
                t->in_key = 1;
                t->in_cond = 0;
            } else if (t->in_key) {
                tid = TK_ASSIGN;

                buffer_copy_string_len(token, t->input + t->offset, 1);

                t->offset++;
                t->line_pos++;
            } else {
                log_error_write(srv, __FILE__, __LINE__, "sbsdsds",
                        "source:", t->source,
                        "line:", t->line, "pos:", t->line_pos,
                        "unexpected equal-sign: =");
                return -1;
            }

            break;
        case '!':
            if (t->in_cond) {
                if (t->input[t->offset + 1] == '=') {
                    t->offset += 2;

                    buffer_copy_string(token, "!=");

                    tid = TK_NE;
                } else if (t->input[t->offset + 1] == '~') {
                    t->offset += 2;

                    buffer_copy_string(token, "!~");

                    tid = TK_NOMATCH;
                } else {
                    log_error_write(srv, __FILE__, __LINE__, "sbsdsds",
                            "source:", t->source,
                            "line:", t->line, "pos:", t->line_pos,
                            "only !~ and != are allowed in the condition");
                    return -1;
                }
                t->in_key = 1;
                t->in_cond = 0;
            } else {
                log_error_write(srv, __FILE__, __LINE__, "sbsdsds",
                        "source:", t->source,
                        "line:", t->line, "pos:", t->line_pos,
                        "unexpected exclamation-marks: !");
                return -1;
            }

            break;
        case '\t':
        case ' ':
            t->offset++;
            t->line_pos++;
            break;
        case '\n':
        case '\r':
            if (t->in_brace == 0) {
                int done = 0;
                while (!done && t->offset < t->size) {
                    switch (t->input[t->offset]) {
                    case '\r':
                    case '\n':
                        config_skip_newline(t);
                        t->line_pos = 1;
                        t->line++;
                        break;

                    case '#':
                        t->line_pos += config_skip_comment(t);
                        break;

                    case '\t':
                    case ' ':
                        t->offset++;
                        t->line_pos++;
                        break;

                    default:
                        done = 1;
                    }
                }
                t->in_key = 1;
                tid = TK_EOL;
                buffer_copy_string(token, "(EOL)");
            } else {
                config_skip_newline(t);
                t->line_pos = 1;
                t->line++;
            }
            break;
        case ',':
            if (t->in_brace > 0) {
                tid = TK_COMMA;

                buffer_copy_string(token, "(COMMA)");
            }

            t->offset++;
            t->line_pos++;
            break;
        case '"':
            /* search for the terminating " */
            start = t->input + t->offset + 1;
            buffer_copy_string(token, "");

            for (i = 1; t->input[t->offset + i]; i++) {
                if (t->input[t->offset + i] == '\\' &&
                    t->input[t->offset + i + 1] == '"') {

                    buffer_append_string_len(token, start, t->input + t->offset + i - start);

                    start = t->input + t->offset + i + 1;

                    /* skip the " */
                    i++;
                    continue;
                }


                if (t->input[t->offset + i] == '"') {
                    tid = TK_STRING;

                    buffer_append_string_len(token, start, t->input + t->offset + i - start);

                    break;
                }
            }

            if (t->input[t->offset + i] == '\0') {
                /* ERROR */

                log_error_write(srv, __FILE__, __LINE__, "sbsdsds",
                        "source:", t->source,
                        "line:", t->line, "pos:", t->line_pos,
                        "missing closing quote");

                return -1;
            }

            t->offset += i + 1;
            t->line_pos += i + 1;

            break;
        case '(':
            t->offset++;
            t->in_brace++;

            tid = TK_LPARAN;

            buffer_copy_string(token, "(");
            break;
        case ')':
            t->offset++;
            t->in_brace--;

            tid = TK_RPARAN;

            buffer_copy_string(token, ")");
            break;
        case '$':
            t->offset++;

            tid = TK_DOLLAR;
            t->in_cond = 1;
            t->in_key = 0;

            buffer_copy_string(token, "$");

            break;

        case '+':
            if (t->input[t->offset + 1] == '=') {
                t->offset += 2;
                buffer_copy_string(token, "+=");
                tid = TK_APPEND;
            } else {
                t->offset++;
                tid = TK_PLUS;
                buffer_copy_string(token, "+");
            }
            break;

        case '{':
            t->offset++;

            tid = TK_LCURLY;

            buffer_copy_string(token, "{");

            break;

        case '}':
            t->offset++;

            tid = TK_RCURLY;

            buffer_copy_string(token, "}");

            break;

        case '[':
            t->offset++;

            tid = TK_LBRACKET;

            buffer_copy_string(token, "[");

            break;

        case ']':
            t->offset++;

            tid = TK_RBRACKET;

            buffer_copy_string(token, "]");

            break;
        case '#':
            t->line_pos += config_skip_comment(t);

            break;
        default:
            if (t->in_cond) {
                for (i = 0; t->input[t->offset + i] &&
                     (isalpha((unsigned char)t->input[t->offset + i])
                      ); i++);

                if (i && t->input[t->offset + i]) {
                    tid = TK_SRVVARNAME;
                    buffer_copy_string_len(token, t->input + t->offset, i);

                    t->offset += i;
                    t->line_pos += i;
                } else {
                    /* ERROR */
                    log_error_write(srv, __FILE__, __LINE__, "sbsdsds",
                            "source:", t->source,
                            "line:", t->line, "pos:", t->line_pos,
                            "invalid character in condition");
                    return -1;
                }
            } else if (isdigit((unsigned char)c)) {
                /* take all digits */
                for (i = 0; t->input[t->offset + i] && isdigit((unsigned char)t->input[t->offset + i]);  i++);

                /* was there it least a digit ? */
                if (i) {
                    tid = TK_INTEGER;

                    buffer_copy_string_len(token, t->input + t->offset, i);

                    t->offset += i;
                    t->line_pos += i;
                }
            } else {
                /* the key might consist of [-.0-9a-z] */
                for (i = 0; t->input[t->offset + i] &&
                     (isalnum((unsigned char)t->input[t->offset + i]) ||
                      t->input[t->offset + i] == '.' ||
                      t->input[t->offset + i] == '_' || /* for env.* */
                      t->input[t->offset + i] == '-'
                      ); i++);

                if (i && t->input[t->offset + i]) {
                    buffer_copy_string_len(token, t->input + t->offset, i);

                    if (strcmp(token->ptr, "include") == 0) {
                        tid = TK_INCLUDE;
                    } else if (strcmp(token->ptr, "include_shell") == 0) {
                        tid = TK_INCLUDE_SHELL;
                    } else if (strcmp(token->ptr, "global") == 0) {
                        tid = TK_GLOBAL;
                    } else if (strcmp(token->ptr, "else") == 0) {
                        tid = TK_ELSE;
                    } else {
                        tid = TK_LKEY;
                    }

                    t->offset += i;
                    t->line_pos += i;
                } else {
                    /* ERROR */
                    log_error_write(srv, __FILE__, __LINE__, "sbsdsds",
                            "source:", t->source,
                            "line:", t->line, "pos:", t->line_pos,
                            "invalid character in variable name");
                    return -1;
                }
            }
            break;
        }
    }

    if (tid) {
        *token_id = tid;
#if 0
        log_error_write(srv, __FILE__, __LINE__, "sbsdsdbdd",
                "source:", t->source,
                "line:", t->line, "pos:", t->line_pos,
                token, token->used - 1, tid);
#endif

        return 1;
    } else if (t->offset < t->size) {
        fprintf(stderr, "%s.%d: %d, %s\n",
            __FILE__, __LINE__,
            tid, token->ptr);
    }
    return 0;
}

/**
 * Parse the configuration
 * @return 0 on success, -1 on failure
 */

static int config_parse(server *srv, config_t *context, tokenizer_t *t) {
    void *pParser;
    int token_id;
    conf_buffer *token, *lasttoken;
    int ret;

    pParser = configparserAlloc( malloc );
    lasttoken = buffer_init();
    token = buffer_init();
    while((1 == (ret = config_tokenizer(srv, t, &token_id, token))) && context->ok) {
        buffer_copy_string_buffer(lasttoken, token);
        configparser(pParser, token_id, token, context);

        token = buffer_init();
    }
    buffer_free(token);

    if (ret != -1 && context->ok) {
        /* add an EOL at EOF, better than say sorry */
        configparser(pParser, TK_EOL, buffer_init_string("(EOL)"), context);
        if (context->ok) {
            configparser(pParser, 0, NULL, context);
        }
    }
    configparserFree(pParser, free);

    if (ret == -1) {
        log_error_write(srv, __FILE__, __LINE__, "sb",
                "configfile parser failed at:", lasttoken);
    } else if (context->ok == 0) {
        log_error_write(srv, __FILE__, __LINE__, "sbsdsdsb",
                "source:", t->source,
                "line:", t->line, "pos:", t->line_pos,
                "parser failed somehow near here:", lasttoken);
        ret = -1;
    }
    buffer_free(lasttoken);

    return ret == -1 ? -1 : 0;
}

/**
 * tokenizer initial state
 */

static int tokenizer_init(tokenizer_t *t, const conf_buffer *source, const char *input, size_t size) {

    t->source = source;
    t->input = input;
    t->size = size;
    t->offset = 0;
    t->line = 1;
    t->line_pos = 1;

    t->in_key = 1;
    t->in_brace = 0;
    t->in_cond = 0;
    return 0;
}

/**
 * Parse a configuration file
 * @param srv instance variable
 * @param context configuration context
 * @param fn file name, to be searched in context->basedir if set
 * @return -1 on failure
 */

int config_parse_file(server *srv, config_t *context, const char *fn) {
    tokenizer_t t;
    stream s;
    int ret;
    conf_buffer *filename;

    if (buffer_is_empty(context->basedir) &&
            (fn[0] == '/' || fn[0] == '\\') &&
            (fn[0] == '.' && (fn[1] == '/' || fn[1] == '\\'))) {
        filename = buffer_init_string(fn);
    } else {
        filename = buffer_init_buffer(context->basedir);
        buffer_append_string(filename, fn);
    }

    if (0 != stream_open(&s, filename->ptr)) {
        if (s.size == 0) {
            /* the file was empty, nothing to parse */
            ret = 0;
        } else {
            log_error_write(srv, __FILE__, __LINE__, "sbss",
                    "opening configfile ", filename, "failed:", strerror(errno));
            ret = -1;
        }
    } else {
        tokenizer_init(&t, filename, s.start, s.size);
        ret = config_parse(srv, context, &t);
    }

    stream_close(&s);
    buffer_free(filename);
    return ret;
}

/**
 * read configuration from output of a command
 * @return -1 on failure, 0 on success
 */

int config_parse_cmd(server *srv, config_t *context, const char *cmd) {
    proc_handler_t proc;
    tokenizer_t t;
    int ret;
    conf_buffer *source;
    conf_buffer *out;
    char oldpwd[PATH_MAX];

    if (NULL == getcwd(oldpwd, sizeof(oldpwd))) {
        log_error_write(srv, __FILE__, __LINE__, "s",
                "cannot get cwd", strerror(errno));
        return -1;
    }

    source = buffer_init_string(cmd);
    out = buffer_init();

    if (!buffer_is_empty(context->basedir)) {
        chdir(context->basedir->ptr);
    }

    if (0 != proc_open_buffer(&proc, cmd, NULL, out, NULL)) {
        log_error_write(srv, __FILE__, __LINE__, "sbss",
                "opening", source, "failed:", strerror(errno));
        ret = -1;
    } else {
        tokenizer_init(&t, source, out->ptr, out->used);
        ret = config_parse(srv, context, &t);
    }

    buffer_free(source);
    buffer_free(out);
    chdir(oldpwd);
    return ret;
}

/**
 * init context
 */

static void context_init(server *srv, config_t *context) {
    context->srv = srv;
    context->ok = 1;
    context->configs_stack = array_init();
    context->configs_stack->is_weakref = 1;
    context->basedir = buffer_init();
}

/**
 * free context
 */

static void context_free(config_t *context) {
    array_free(context->configs_stack);
    buffer_free(context->basedir);
}

/**
 * load configuration for global
 * sets the module list.
 */

int config_read(server *srv, const char *fn) {
    config_t context;
    data_config *dc;
    data_integer *dpid;
    data_string *dcwd;
    int ret;
    char *pos;
    data_array *modules;

    context_init(srv, &context);
    context.all_configs = srv->config_context;

    pos = strrchr(fn,
#ifdef __WIN32
            '\\'
#else
            '/'
#endif
            );
    if (pos) {
        buffer_copy_string_len(context.basedir, fn, pos - fn + 1);
        fn = pos + 1;
    }

    dc = data_config_init();
    buffer_copy_string(dc->key, "global");

    assert(context.all_configs->used == 0);
    dc->context_ndx = context.all_configs->used;
    array_insert_unique(context.all_configs, (data_unset *)dc);
    context.current = dc;

    /* default context */
    srv->config = dc->value;
    dpid = data_integer_init();
    dpid->value = getpid();
    buffer_copy_string(dpid->key, "var.PID");
    array_insert_unique(srv->config, (data_unset *)dpid);

    dcwd = data_string_init();
    buffer_prepare_copy(dcwd->value, 1024);
    if (NULL != getcwd(dcwd->value->ptr, dcwd->value->size - 1)) {
        dcwd->value->used = strlen(dcwd->value->ptr) + 1;
        buffer_copy_string(dcwd->key, "var.CWD");
        array_insert_unique(srv->config, (data_unset *)dcwd);
    }

    ret = config_parse_file(srv, &context, fn);

    /* remains nothing if parser is ok */
    assert(!(0 == ret && context.ok && 0 != context.configs_stack->used));
    context_free(&context);

    if (0 != ret) {
        return ret;
    }

    if (NULL != (dc = (data_config *)array_get_element(srv->config_context, "global"))) {
        srv->config = dc->value;
    } else {
        return -1;
    }

    if (NULL != (modules = (data_array *)array_get_element(srv->config, "server.modules"))) {
        data_string *ds;
        data_array *prepends;

        if (modules->type != TYPE_ARRAY) {
            fprintf(stderr, "server.modules must be an array");
            return -1;
        }

        prepends = data_array_init();

        /* prepend default modules */
        if (NULL == array_get_element(modules->value, "mod_indexfile")) {
            ds = data_string_init();
            buffer_copy_string(ds->value, "mod_indexfile");
            array_insert_unique(prepends->value, (data_unset *)ds);
        }

        prepends = (data_array *)configparser_merge_data((data_unset *)prepends, (data_unset *)modules);
        buffer_copy_string_buffer(prepends->key, modules->key);
        array_replace(srv->config, (data_unset *)prepends);
        modules->free((data_unset *)modules);
        modules = prepends;

        /* append default modules */
        if (NULL == array_get_element(modules->value, "mod_dirlisting")) {
            ds = data_string_init();
            buffer_copy_string(ds->value, "mod_dirlisting");
            array_insert_unique(modules->value, (data_unset *)ds);
        }

        if (NULL == array_get_element(modules->value, "mod_staticfile")) {
            ds = data_string_init();
            buffer_copy_string(ds->value, "mod_staticfile");
            array_insert_unique(modules->value, (data_unset *)ds);
        }
    } else {
        data_string *ds;

        modules = data_array_init();

        /* server.modules is not set */
        ds = data_string_init();
        buffer_copy_string(ds->value, "mod_indexfile");
        array_insert_unique(modules->value, (data_unset *)ds);

        ds = data_string_init();
        buffer_copy_string(ds->value, "mod_dirlisting");
        array_insert_unique(modules->value, (data_unset *)ds);

        ds = data_string_init();
        buffer_copy_string(ds->value, "mod_staticfile");
        array_insert_unique(modules->value, (data_unset *)ds);

        buffer_copy_string(modules->key, "server.modules");
        array_insert_unique(srv->config, (data_unset *)modules);
    }


    if (0 != config_insert(srv)) {
        return -1;
    }

    return 0;
}

/**
 * set defaults
 */

int config_set_defaults(server *srv) {
    specific_config *s = &srv->config_storage[0];
#if 0
    size_t i;
    struct stat st1, st2;
#endif

    if (buffer_is_empty(s->document_root)) {
        log_error_write(srv, __FILE__, __LINE__, "s",
                "a default document-root has to be set");

        return -1;
    }

#if 0
    buffer_copy_string_buffer(srv->tmp_buf, s->document_root);

    buffer_to_lower(srv->tmp_buf);

    if (0 == stat(srv->tmp_buf->ptr, &st1)) {
        int is_lower = 0;

        is_lower = buffer_is_equal(srv->tmp_buf, s->document_root);

        /* lower-case existed, check upper-case */
        buffer_copy_string_buffer(srv->tmp_buf, s->document_root);

        buffer_to_upper(srv->tmp_buf);

        /* we have to handle the special case that upper and lower-casing results in the same filename
         * as in server.document-root = "/" or "/12345/" */

        if (is_lower && buffer_is_equal(srv->tmp_buf, s->document_root)) {
            /* lower-casing and upper-casing didn't result in
             * an other filename, no need to stat(),
             * just assume it is case-sensitive. */

            s->force_lowercase_filenames = 0;
        } else if (0 == stat(srv->tmp_buf->ptr, &st2)) {

            /* upper case exists too, doesn't the FS handle this ? */

            /* upper and lower have the same inode -> case-insensitve FS */

            if (st1.st_ino == st2.st_ino) {
                /* upper and lower have the same inode -> case-insensitve FS */

                s->force_lowercase_filenames = 1;
            }
        }
    }
#endif

    if (srv->srvconf.port == 0) {
        srv->srvconf.port = s->is_ssl ? 322 : 554;
    }

    if (srv->srvconf.max_conns == 0)
        srv->srvconf.max_conns = 100;

    if (srv->srvconf.first_udp_port == 0)
        srv->srvconf.first_udp_port = RTP_DEFAULT_PORT;
    if (srv->srvconf.buffered_frames == 0)
        srv->srvconf.buffered_frames = BUFFERED_FRAMES_DEFAULT;

    if (s->is_ssl) {
        if (buffer_is_empty(s->ssl_pemfile)) {
            /* PEM file is require */

            log_error_write(srv, __FILE__, __LINE__, "s",
                    "ssl.pemfile has to be set");
            return -1;
        }

#ifndef USE_OPENSSL
        log_error_write(srv, __FILE__, __LINE__, "s",
                "ssl support is missing, recompile with --with-openssl");

        return -1;
#endif
    }

    return 0;
}
