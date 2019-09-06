#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "present.h"
#include "arena.h"

#define PF_MEM_SIZE (8 * 1024)

enum list_node_type {
    LNODE_INVALID = 0,
    LNODE_TEXT = 1,
    LNODE_IMAGE = 2,
    LNODE_MAX
};

struct present_list_node {
    list_node_type type;
    present_list_node* next;
    present_list_node* children;
};

struct list_node_text {
    present_list_node hdr;

    unsigned text_length;
    const char* text;
};

struct list_node_image {
    present_list_node hdr;
    int width, height;
    void* buffer;
};

struct present_slide {
    const char* chapter_title; // optional
    const char* subtitle; // optional
    present_slide* next;
};

struct present_file {
    mem_arena* mem;

    const char* title;
    const char* authors;
    // Slides
    int slide_count;
    int current_slide;
    present_slide* slides;
};

static unsigned read_line(char* buf, unsigned bufsiz, unsigned* indent_level, FILE* f) {
    unsigned ret = 0;
    unsigned i = 0;
    char c = 0;
    assert(buf && bufsiz > 0 && indent_level && f);

    *indent_level = 0;
    fread(&c, 1, 1, f);
    while(c != '\n' && (c == ' ' || c == '\t') && !feof(f)) {
        if(c == ' ' || c == '\t') {
            (*indent_level)++; // count every space and tab as a new indent level
        }
        fread(&c, 1, 1, f);
    }
    if(c == '\n' || feof(f)) {
        // barring some whitespace, this was an empty line
        buf[0] = 0;
    } else {
        buf[i++] = c;
        fread(&c, 1, 1, f);
        while(c != '\n' && !feof(f) && i < bufsiz) {
            buf[i++] = c;
            fread(&c, 1, 1, f);
        }

        // 'Remove' trailing whitespace
        while((buf[i] == ' ' || buf[i] == '\t') && i > 0) {
            i--;
        }
        ret = i;
    }

    return ret;
}

// dir, dirlen are outputs
// buf, buflen are inputs
static bool is_directive(const char** dirout, unsigned* dirlen, const char* buf, unsigned buflen) {
    bool ret = false;
    unsigned len;
    const char* dir;

    assert(buf && buflen > 0 && dirout && dirlen);

    if(buf && buflen > 0) {
        *dirout = NULL;
        *dirlen = 0;
        if(buf[0] == '#') {
            ret = true;
            dir = buf + 1;
            len = 0;
            while(dir[len] != ' ' && dir[len] != '\t' && len + 1 < buflen) {
                len++;
            }
            *dirout = dir;
            *dirlen = len;
        }
    }
    return ret;
}

static bool parse_file(present_file* file, FILE* f) {
    bool ret = true;
    unsigned line_length;
    char line_buf[512];
    const unsigned line_siz = 512;
    unsigned indent_level;

    const char* directive;
    unsigned directive_len;

    assert(file && f);

    // First line must be a '#PRESENT'
    line_length = read_line(line_buf, line_siz, &indent_level, f);
    if(line_length && is_directive(&directive, &directive_len, line_buf, line_length)) {
        if(strncmp(directive, "PRESENT", directive_len) == 0) {
            while(!feof(f)) {
                line_length = read_line(line_buf, line_siz, &indent_level, f);
                if(line_length) {
                    if(is_directive(&directive, &directive_len, line_buf, line_length)) {
                    }
                }
            }
        } else {
            fprintf(stderr, "#PRESENT header is missing from presentation file!\n");
        }
    }

    return ret;
}

present_file* present_open(const char* filename) {
    present_file* ret = NULL;
    FILE* f = NULL;

    assert(filename);
    if(filename) {
        f = fopen(filename, "r");
        if(f) {
            ret = (present_file*)malloc(sizeof(present_file));
            if(ret) {
                ret->mem = arena_create(PF_MEM_SIZE);
                ret->title = NULL;
                ret->authors = NULL;
                ret->slide_count = 0;
                ret->current_slide = 0;
                ret->slides = NULL;
                if(!parse_file(ret, f)) {
                    arena_destroy(ret->mem);
                    free(ret);
                    ret = NULL;

                    fprintf(stderr, "Presentation parse error!\n");
                }
            }
            fclose(f);
        } else {
            fprintf(stderr, "Failed to open presentation file '%s'!\n", filename);
        }
    }

    return ret;
}

void present_close(present_file* file) {
    assert(file);
    if(file) {
        free(file);
    }
}

bool present_over(present_file* file) {
    bool ret = true;
    assert(file);
    if(file) {
    }
    return ret;
}

int present_seek(present_file* file, int off) {
    int ret = 0;
    assert(file);
    if(file) {
    }
    return ret;
}

int present_seek_to(present_file* file, int idx) {
    int ret = 0;
    assert(file);
    if(file) {
    }
    return ret;
}

void present_fill_render_queue(present_file* file, render_queue* rq) {
    assert(file);
    if(file) {
    }
}
