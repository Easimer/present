// present
// Copyright (C) 2019 Daniel Meszaros <easimer@gmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include "present.h"
#include "arena.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define PF_MEM_SIZE (8 * 1024 * 1024)

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
    present_list_node* parent; // this is not the prev->prev of next!
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
    int chapter_title_len;
    const char* chapter_title; // optional
    int subtitle_len;
    const char* subtitle; // optional
    present_slide* next;

    present_list_node* content;
    present_list_node* content_cur;
    int current_indent_level;
};

struct present_file {
    const char* path;
    mem_arena* mem;

    unsigned title_len;
    const char* title;
    unsigned authors_len;
    const char* authors;
    // Slides
    int slide_count;
    int current_slide;
    present_slide* slides;
    present_slide* current_slide_data;
};

struct parse_state {
    present_slide* first;
    present_slide* last;

    int current_chapter_title_len;
    const char* current_chapter_title;
};

static unsigned read_line(char* buf, unsigned bufsiz, unsigned* indent_level, FILE* f) {
    unsigned ret = 0;
    unsigned i = 0;
    char c = 0;
    assert(buf && bufsiz > 0 && indent_level && f);

    memset(buf, 0, bufsiz);

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

static void append_slide(present_file* file, parse_state* state) {
    assert(file && state);
    present_slide* next_slide = (present_slide*)arena_alloc(file->mem, sizeof(present_slide));
    next_slide->chapter_title = state->current_chapter_title;
    next_slide->chapter_title_len = state->current_chapter_title_len;
    next_slide->subtitle = NULL;
    next_slide->next = NULL;
    //fprintf(stderr, "=== SLIDE ===\n");
    if(state->last != NULL) {
        state->last->next = next_slide;
        state->last = next_slide;
    } else {
        state->first = state->last = next_slide;
    }
    file->slide_count++;
}

static void set_chapter_title(present_file* file, parse_state* state, const char* title, unsigned title_len) {
    assert(file && state && title);

    if(title_len == 0) {
        state->current_chapter_title = NULL;
    } else {
        char* buf = (char*)arena_alloc(file->mem, title_len + 1);
        memcpy(buf, title, title_len);
        buf[title_len] = 0;
        state->current_chapter_title = buf;
        state->current_chapter_title_len = title_len;
    }
}

static void set_title(present_file* file, parse_state* state, const char* title, unsigned title_len) {
    assert(file && state && title);

    char* buf = (char*)arena_alloc(file->mem, title_len + 1);
    memcpy(buf, title, title_len);
    buf[title_len] = 0;
    file->title = buf;
    file->title_len = title_len;
}

static void set_authors(present_file* file, parse_state* state, const char* authors, unsigned authors_len) {
    assert(file && state && authors);

    char* buf = (char*)arena_alloc(file->mem, authors_len + 1);
    memcpy(buf, authors, authors_len);
    buf[authors_len] = 0;
    
    file->authors = buf;
    file->authors_len = authors_len;
}

static void save_workdir(char** dst) {
    assert(dst);
    char *buf = NULL, *res = NULL;
    int bufsiz = 64;
    while(!buf) {
        buf = (char*)malloc(bufsiz);
        res = getcwd(buf, bufsiz);
        if(!res) {
            free(buf);
            buf = NULL;
            bufsiz *= 2;
        }
    }
    *dst = buf;
}

static void restore_workdir(char** path) {
    int res;
    assert(path && *path);
    res = chdir(*path);
    *path = NULL;
    if(res) {
        fprintf(stderr, "Failed to chdir: %s\n", strerror(errno));
    }
}

static void change_to_dir_of_file(const char* path) {
    int res;
    unsigned len = strlen(path);
    char* buf = (char*)malloc(len + 1);
    memcpy(buf, path, len + 1);
    unsigned i = len;
    while(buf[--i] != '/' && i > 0);
    if(i != 0) {
        buf[++i] = 0;
        res = chdir(buf);
        buf[i] = '/';
        if(res) {
            fprintf(stderr, "Failed to chdir: %s\n", strerror(errno));
        }
    }
}

static void add_inline_image(present_file* file, parse_state* state, int indent_level, const char* path, unsigned path_len) {
    char* prev_workdir = NULL;
    int x, y, channels;
    list_node_image* node;
    char* temp_path;
    auto slide = state->last;
    stbi_uc* pixbuf = NULL;
    stbi_uc* pixbuf_final = NULL;
    if(!slide) {
        fprintf(stderr, "No #SLIDE directive before content!\n");
    }
    assert(slide);
    node = (list_node_image*)arena_alloc(file->mem, sizeof(list_node_image));
    node->hdr.type = LNODE_IMAGE;
    node->hdr.next = node->hdr.children = node->hdr.parent = NULL;

    save_workdir(&prev_workdir);
    change_to_dir_of_file(file->path);

    // Copy path to a temp buf and make it zero
    // terminated because stbi wants it that way :(
    //arena_push_frame(file->mem);
    //temp_path = (char*)arena_alloc(file->mem, path_len + 1);
    temp_path = (char*)malloc(path_len + 1);
    memcpy(temp_path, path, path_len);
    temp_path[path_len] = 0;

    // Load pixel data
    pixbuf = stbi_load(temp_path, &x, &y, &channels, STBI_rgb_alpha);

    free(temp_path);
    temp_path = NULL;
    //arena_pop_frame(file->mem);

    restore_workdir(&prev_workdir);

    if(pixbuf) {
        unsigned pixbuf_siz = sizeof(stbi_uc) * x * y * 4;
        pixbuf_final = (stbi_uc*)arena_alloc(file->mem, pixbuf_siz);
        memcpy(pixbuf_final, pixbuf, pixbuf_siz);
        stbi_image_free(pixbuf);
        pixbuf = NULL;
    } else {
        fprintf(stderr, "Failed to load image '%.*s': %s!\n", path_len, path, stbi_failure_reason());
    }
    assert(pixbuf_final);

    node->width = x;
    node->height = y;
    node->buffer = pixbuf_final;

    if(slide->content_cur) {
        if(slide->current_indent_level < indent_level) {
            //fprintf(stderr, "Indent IN to %d from %d\n", indent_level, slide->current_indent_level);
            node->hdr.parent = slide->content_cur;
            slide->content_cur->children = (present_list_node*)node;
            slide->content_cur = (present_list_node*) node;
            slide->current_indent_level = indent_level;
        } else if(slide->current_indent_level > indent_level) {
            //fprintf(stderr, "Indent OUT to %d\n", indent_level);
            auto parent = slide->content_cur->parent;
            slide->content_cur = parent;
            parent->next = (present_list_node*)node;
            slide->current_indent_level = indent_level;
        } else {
            //fprintf(stderr, "Indent STAYS at %d\n", indent_level);
            slide->content_cur->next = (present_list_node*)node;
            node->hdr.parent = slide->content_cur->parent;
            slide->content_cur = (present_list_node*)node;
        }
    } else {
        //fprintf(stderr, "Indent ESTABLISHED at %d\n", indent_level);
        slide->content = slide->content_cur = (present_list_node*)node;
        slide->current_indent_level = indent_level;
    }
    //fprintf(stderr, "Appended image '%.*s'\n", path_len, path);
}

static void set_subtitle(present_file* file, parse_state* state, const char* title, unsigned title_len) {
    assert(file && state && title);
    auto slide = state->last;
    if(!slide) {
        fprintf(stderr, "No #SLIDE directive before #SUBTITLE!\n");
    }
    assert(slide);
    slide->subtitle_len = title_len;
    char* buf = (char*)arena_alloc(file->mem, title_len + 1);
    memcpy(buf, title, title_len);
    buf[title_len] = 0;
    slide->subtitle = buf;
}

static void append_to_list(present_file* file, parse_state* state, int indent_level, const char* line, unsigned linelen) {
    auto slide = state->last;
    if(!slide) {
        fprintf(stderr, "No #SLIDE directive before content!\n");
    }
    assert(slide);
    auto node = (list_node_text*)arena_alloc(file->mem, sizeof(list_node_text));
    node->hdr.type = LNODE_TEXT;
    node->hdr.next = node->hdr.children = node->hdr.parent = NULL;
    node->text_length = linelen;
    char* buf = (char*)arena_alloc(file->mem, linelen + 1);
    memcpy(buf, line, linelen);
    buf[linelen] = 0;
    node->text = buf;

    if(slide->content_cur) {
        if(slide->current_indent_level < indent_level) {
            //fprintf(stderr, "Indent IN to %d\n", indent_level);
            node->hdr.parent = slide->content_cur;
            slide->content_cur->children = (present_list_node*)node;
            slide->content_cur = (present_list_node*) node;
            slide->current_indent_level = indent_level;
        } else if(slide->current_indent_level > indent_level) {
            //fprintf(stderr, "Indent OUT to %d\n", indent_level);
            auto parent = slide->content_cur->parent;
            slide->content_cur = parent;
            parent->next = (present_list_node*)node;
            slide->current_indent_level = indent_level;
        } else {
            //fprintf(stderr, "Indent STAYS at %d\n", indent_level);
            slide->content_cur->next = (present_list_node*)node;
            node->hdr.parent = slide->content_cur->parent;
            slide->content_cur = (present_list_node*)node;
        }
    } else {
        //fprintf(stderr, "Indent ESTABLISHED at %d\n", indent_level);
        slide->content = slide->content_cur = (present_list_node*)node;
        slide->current_indent_level = indent_level;
    }

    //fprintf(stderr, "Appended list item %.*s\n", node->text_length, node->text);
}

static bool parse_file(present_file* file, FILE* f) {
    bool ret = true;
    unsigned line_length;
    char line_buf[512];
    const unsigned line_siz = 512;
    unsigned indent_level;

    const char* directive;
    unsigned directive_len;
    const char* directive_arg;
    unsigned directive_arg_len;
    parse_state pstate;

    pstate.first = pstate.last = NULL;

    assert(file && f);

    // First line must be a '#PRESENT'
    line_length = read_line(line_buf, line_siz, &indent_level, f);
    if(line_length && is_directive(&directive, &directive_len, line_buf, line_length)) {
        if(strncmp(directive, "PRESENT", directive_len) == 0) {
            while(!feof(f)) {
                line_length = read_line(line_buf, line_siz, &indent_level, f);
                if(line_length) {
                    if(is_directive(&directive, &directive_len, line_buf, line_length)) {
                        // Calculate directive argument ptr and len
                        directive_arg = directive + directive_len + 1;
                        directive_arg_len = line_length - directive_len - 1;
                        if(directive_arg_len != 0) {
                            directive_arg_len--;
                        }

                        if(strncmp(directive, "SLIDE", directive_len) == 0) {
                            append_slide(file, &pstate);
                        } else if(strncmp(directive, "SUBTITLE", directive_len) == 0) {
                            set_subtitle(file, &pstate, directive_arg, directive_arg_len);
                        } else if(strncmp(directive, "INLINE_IMAGE", directive_len) == 0) {
                            add_inline_image(file, &pstate, indent_level, directive_arg, directive_arg_len);
                        } else if(strncmp(directive, "CHAPTER", directive_len) == 0) {
                            set_chapter_title(file, &pstate, directive_arg, directive_arg_len);
                        } else if(strncmp(directive, "TITLE", directive_len) == 0) {
                            set_title(file, &pstate, directive_arg, directive_arg_len);
                        } else if(strncmp(directive, "AUTHORS", directive_len) == 0) {
                            set_authors(file, &pstate, directive_arg, directive_arg_len);
                        } else {
                            fprintf(stderr, "Warning: unknown directive: '%.*s'\n",
                                    directive_len, directive);
                        }
                    } else {
                        append_to_list(file, &pstate, indent_level, line_buf, line_length);
                    }
                }
            }
            file->slides = pstate.first;
        } else {
            fprintf(stderr, "#PRESENT header is missing from presentation file!\n");
            ret = false;
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
                ret->path = filename;
                ret->mem = arena_create(PF_MEM_SIZE);
                ret->title = NULL;
                ret->authors = NULL;
                ret->slide_count = 1; // implicit title slide
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
        ret = file->current_slide > file->slide_count;
    }
    return ret;
}

int present_seek(present_file* file, int off) {
    int ret = 0;
    int abs;
    assert(file);
    if(file) {
        abs = file->current_slide + off;
        ret = present_seek_to(file, abs);
    }
    return ret;
}

int present_seek_to(present_file* file, int abs) {
    int ret = 0;
    assert(file);
    if(file) {
        if(abs == -1) {
            abs = file->slide_count - 1;
        }

        fprintf(stderr, "Jumping to slide %d/%d\n", abs, file->slide_count);
        if(abs < 0) {
            abs = 0;
            file->current_slide_data = NULL;
        } else if(abs > file->slide_count) {
            abs = file->slide_count;
            file->current_slide_data = NULL;
        } else {
            file->current_slide_data = file->slides;
            for(int i = 1; i < abs; i++) {
                file->current_slide_data = file->current_slide_data->next;
            }
            file->current_slide = abs;
        }
        ret = abs;
    }
    return ret;
}

static void present_fill_rq_title_slide(present_file* file, render_queue* rq) {
    if(file->title_len && file->title) {
        auto title = rq_new_cmd<rq_draw_text>(rq, RQCMD_DRAW_TEXT);
        title->x = 10;
        title->y = 32;
        title->size = 20;
        title->text_len = file->title_len;
        title->text = file->title;
    }

    if(file->authors_len && file->authors) {
        auto authors = rq_new_cmd<rq_draw_text>(rq, RQCMD_DRAW_TEXT);
        authors->x = 14;
        authors->y = 52;
        authors->size = 20;
        authors->text_len = file->authors_len;
        authors->text = file->authors;
    }
}

static void present_fill_rq_end_slide(present_file* file, render_queue* rq) {
    // a filled rectangle covering the screen
    auto rect = rq_new_cmd<rq_draw_rect>(rq, RQCMD_DRAW_RECTANGLE);
    rect->x0 = rect->y0 = 0;
    rect->x1 = rect->y1 = 1;
}

static void present_fill_rq_regular_slide(present_slide* slide, render_queue* rq) {
    int x = 8;
    int y = 96;
    rq_draw_text* cmd = NULL;

    present_list_node* cur = slide->content;

    if(slide->chapter_title) {
        cmd = rq_new_cmd<rq_draw_text>(rq, RQCMD_DRAW_TEXT);
        cmd->x = 10; cmd->y = 32;
        cmd->size = 32;
        cmd->text_len = slide->chapter_title_len;
        cmd->text = slide->chapter_title;
    }
    if(slide->subtitle) {
        cmd = rq_new_cmd<rq_draw_text>(rq, RQCMD_DRAW_TEXT);
        cmd->x = 10; cmd->y = 64;
        cmd->size = 32;
        cmd->text_len = slide->subtitle_len;
        cmd->text = slide->subtitle;
    }

    while(cur) {
        if(cur->type == LNODE_TEXT) {
            list_node_text* text = (list_node_text*)cur;
            cmd = rq_new_cmd<rq_draw_text>(rq, RQCMD_DRAW_TEXT);
            cmd->x = x;
            cmd->y = y;
            cmd->size = 20;
            cmd->text_len = text->text_length;
            cmd->text = text->text;
            y += 22;
        } else if(cur->type == LNODE_IMAGE) {
            rq_draw_image* cmd = NULL;
            list_node_image* img = (list_node_image*)cur;
            cmd = rq_new_cmd<rq_draw_image>(rq, RQCMD_DRAW_IMAGE);
            cmd->width = img->width;
            cmd->height = img->height;
            cmd->buffer = img->buffer;
            assert(img->buffer);
            y += img->height;
        }
        if(cur->children) {
            x += 24;
            cur = cur->children;
        } else {
        }
        if(cur->next) {
            cur = cur->next;
        } else {
            x -= 24;
            if(cur->parent) {
                cur = cur->parent->next;
            } else {
                cur = NULL;
            }
        }
    }
}

void present_fill_render_queue(present_file* file, render_queue* rq) {
    assert(file);
    if(file) {
        if(file->current_slide == 0) {
            present_fill_rq_title_slide(file, rq);
        } else if(file->current_slide == file->slide_count) {
            present_fill_rq_end_slide(file, rq);
        } else {
            auto slide = file->current_slide_data;
            present_fill_rq_regular_slide(slide, rq);
        }
    }
}
