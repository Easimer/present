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
#include <limits.h>
#include <stdint.h>
#include <errno.h>
#include <assert.h>
#include "present.h"
#include "arena.h"
#include "display.h" // display_swap_red_blue_channels
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define PF_MEM_SIZE (64 * 1024)

enum List_Node_Type {
    LNODE_INVALID = 0,
    LNODE_TEXT = 1,
    LNODE_IMAGE = 2,
    LNODE_MAX
};

enum Image_Alignment {
    IMGALIGN_INVALID = 0,
    IMGALIGN_LEFT = 1,
    IMGALIGN_INLINE = 1,
    IMGALIGN_RIGHT = 2,
    IMGALIGN_MAX
};

struct Present_List_Node {
    List_Node_Type type;
    Present_List_Node* next;
    Present_List_Node* children;
    Present_List_Node* parent; // this is not the prev->prev of next!
};

struct list_node_text {
    Present_List_Node hdr;
    
    unsigned text_length;
    const char* text;
};

struct list_node_image {
    Present_List_Node hdr;
    int path_len;
    const char* path;
    Image_Alignment alignment;
};

struct present_slide {
    int chapter_title_len;
    const char* chapter_title; // optional
    int subtitle_len;
    const char* subtitle; // optional
    present_slide* next;
    
    Present_List_Node* content;
    Present_List_Node* content_cur;
    int current_indent_level;
};

struct Present_File {
    const char* path;
    Mem_Arena* mem;
    
    unsigned title_len;
    const char* title;
    unsigned authors_len;
    const char* authors;
    // Slides
    int slide_count;
    int current_slide;
    present_slide* slides;
    present_slide* current_slide_data;
    
    const char* font_title; // Font used on the title slide
    const char* font_chapter; // Font used for chapter title
    const char* font_general; // Font used for content text and as a fallback
    
    // color of slide background (default: 255, 255, 255)
    RGBA_Color color_bg;
    // color of slide text (default: 0, 0, 0)
    RGBA_Color color_fg;
    // color of slide header (default: 43, 203, 186)
    RGBA_Color color_bg_header;
    // color of slide header text (default: 255, 255, 255)
    RGBA_Color color_fg_header;
};

struct Parse_State {
    present_slide* first;
    present_slide* last;
    
    int current_chapter_title_len;
    const char* current_chapter_title;
};

static unsigned ReadLine(char* buf, unsigned bufsiz, unsigned* indent_level, FILE* f) {
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
static bool IsDirective(const char** dirout, unsigned* dirlen, const char* buf, unsigned buflen) {
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

static void AppendSlide(Present_File* file, Parse_State* state) {
    assert(file && state);
    present_slide* next_slide = (present_slide*)Arena_Alloc(file->mem, sizeof(present_slide));
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

static void SetChapterTitle(Present_File* file, Parse_State* state, const char* title, unsigned title_len) {
    assert(file && state && title);
    
    if(title_len == 0) {
        state->current_chapter_title = NULL;
    } else {
        char* buf = (char*)Arena_Alloc(file->mem, title_len + 1);
        memcpy(buf, title, title_len);
        buf[title_len] = 0;
        state->current_chapter_title = buf;
        state->current_chapter_title_len = title_len;
    }
}

static void SetTitle(Present_File* file, Parse_State* state, const char* title, unsigned title_len) {
    assert(file && state && title);
    
    char* buf = (char*)Arena_Alloc(file->mem, title_len + 1);
    memcpy(buf, title, title_len);
    buf[title_len] = 0;
    file->title = buf;
    file->title_len = title_len;
}

static void SetAuthors(Present_File* file, Parse_State* state, const char* authors, unsigned authors_len) {
    assert(file && state && authors);
    
    char* buf = (char*)Arena_Alloc(file->mem, authors_len + 1);
    memcpy(buf, authors, authors_len);
    buf[authors_len] = 0;
    
    file->authors = buf;
    file->authors_len = authors_len;
}

#if _WIN32
#define WIN32_MEAN_AND_LEAN
#include <Windows.h>
inline int P_Chdir(const char* path) {
    int ret = 0;
    BOOL res;
    
    res = SetCurrentDirectory(path);
    
    if(!res) {
        ret = -1;
    }
    
    return ret;
}

inline char* P_Getcwd(char* buf, size_t size) {
    char* ret = NULL;
    DWORD res;
    
    res = GetCurrentDirectory((DWORD)size, buf);
    
    if(size >= res) {
        ret = buf;
    }
    
    return ret;
}

#define PATH_MAX MAX_PATH

inline char* P_Realpath(const char* path, char buf[PATH_MAX]) {
    char* ret = NULL;
    if(GetFullPathNameA(path, PATH_MAX, buf, NULL) > 0) {
        ret = buf;
    }
    return ret;
}
#else
#include <unistd.h>
inline int P_Chdir(const char* path) {
    return chdir(path);
}

inline char* P_Getcwd(char* buf, size_t size) {
    return getcwd(buf, size);
}

inline char* P_Realpath(const char* path, char buf[PATH_MAX]) {
    return realpath(path, buf);
}
#endif

static void SaveWorkDir(char** dst) {
    assert(dst);
    char *buf = NULL, *res = NULL;
    int bufsiz = 64;
    while(!buf) {
        buf = (char*)malloc(bufsiz);
        res = P_Getcwd(buf, bufsiz);
        if(!res) {
            free(buf);
            buf = NULL;
            bufsiz *= 2;
        }
    }
    *dst = buf;
}

static void RestoreWorkDir(char** path) {
    int res;
    assert(path && *path);
    res = P_Chdir(*path);
    *path = NULL;
    if(res) {
        fprintf(stderr, "Failed to chdir: %s\n", strerror(errno));
    }
}

static void ChangeToDirOfFile(const char* path) {
    int res;
    size_t len = strlen(path);
    char* buf = (char*)malloc(len + 1);
    memcpy(buf, path, len + 1);
    size_t i = len;
    while(buf[--i] != '/' && i > 0);
    if(i != 0) {
        buf[++i] = 0;
        res = P_Chdir(buf);
        buf[i] = '/';
        if(res) {
            fprintf(stderr, "Failed to chdir: %s\n", strerror(errno));
        }
    }
}

static void SwapRedBlueChannels(uint8_t* rgba_buffer, unsigned width, unsigned height) {
    for(unsigned y = 0; y < height; y++) {
        for(unsigned x = 0; x < width; x++) {
            auto& c0 = rgba_buffer[y * width * 4 + x * 4 + 0];
            auto& c1 = rgba_buffer[y * width * 4 + x * 4 + 2];
            uint8_t t = c0;
            c0 = c1;
            c1 = t;
        }
    }
}

static void AddInlineImage(Present_File* file, Parse_State* state, int indent_level, const char* path, unsigned path_len, Image_Alignment alignment) {
    char* prev_workdir = NULL;
    list_node_image* node;
    char full_path_buf[PATH_MAX];
    auto slide = state->last;
    if(!slide) {
        fprintf(stderr, "No #SLIDE directive before content!\n");
    }
    assert(slide);
    node = (list_node_image*)Arena_Alloc(file->mem, sizeof(list_node_image));
    node->hdr.type = LNODE_IMAGE;
    node->hdr.next = node->hdr.children = node->hdr.parent = NULL;
    node->alignment = alignment;
    
    SaveWorkDir(&prev_workdir);
    ChangeToDirOfFile(file->path);
    
    if(P_Realpath(path, full_path_buf)) {
        unsigned len = (unsigned)strlen(full_path_buf);
        node->path = (char*)Arena_Alloc(file->mem, len + 1);
        node->path_len = len;
        memcpy((char*)node->path, full_path_buf, len + 1);
    } else {
        node->path_len = 0;
        node->path = NULL;
    }
    
    RestoreWorkDir(&prev_workdir);
    
    if(slide->content_cur) {
        if(slide->current_indent_level < indent_level) {
            //fprintf(stderr, "Indent IN to %d from %d\n", indent_level, slide->current_indent_level);
            node->hdr.parent = slide->content_cur;
            slide->content_cur->children = (Present_List_Node*)node;
            slide->content_cur = (Present_List_Node*) node;
            slide->current_indent_level = indent_level;
        } else if(slide->current_indent_level > indent_level) {
            //fprintf(stderr, "Indent OUT to %d\n", indent_level);
            auto parent = slide->content_cur->parent;
            slide->content_cur = parent;
            parent->next = (Present_List_Node*)node;
            slide->current_indent_level = indent_level;
        } else {
            //fprintf(stderr, "Indent STAYS at %d\n", indent_level);
            slide->content_cur->next = (Present_List_Node*)node;
            node->hdr.parent = slide->content_cur->parent;
            slide->content_cur = (Present_List_Node*)node;
        }
    } else {
        //fprintf(stderr, "Indent ESTABLISHED at %d\n", indent_level);
        slide->content = slide->content_cur = (Present_List_Node*)node;
        slide->current_indent_level = indent_level;
    }
    //fprintf(stderr, "Appended image '%.*s'\n", path_len, path);
}

static void SetSubtitle(Present_File* file, Parse_State* state, const char* title, unsigned title_len) {
    assert(file && state && title);
    auto slide = state->last;
    if(!slide) {
        fprintf(stderr, "No #SLIDE directive before #SUBTITLE!\n");
    }
    assert(slide);
    slide->subtitle_len = title_len;
    char* buf = (char*)Arena_Alloc(file->mem, title_len + 1);
    memcpy(buf, title, title_len);
    buf[title_len] = 0;
    slide->subtitle = buf;
}

static void AppendToList(Present_File* file, Parse_State* state, int indent_level, const char* line, unsigned linelen) {
    auto slide = state->last;
    if(!slide) {
        fprintf(stderr, "No #SLIDE directive before content!\n");
    }
    assert(slide);
    auto node = (list_node_text*)Arena_Alloc(file->mem, sizeof(list_node_text));
    node->hdr.type = LNODE_TEXT;
    node->hdr.next = node->hdr.children = node->hdr.parent = NULL;
    node->text_length = linelen;
    char* buf = (char*)Arena_Alloc(file->mem, linelen + 1);
    memcpy(buf, line, linelen);
    buf[linelen] = 0;
    node->text = buf;
    
    if(slide->content_cur) {
        if(slide->current_indent_level < indent_level) {
            //fprintf(stderr, "Indent IN to %d\n", indent_level);
            node->hdr.parent = slide->content_cur;
            slide->content_cur->children = (Present_List_Node*)node;
            slide->content_cur = (Present_List_Node*) node;
            slide->current_indent_level = indent_level;
        } else if(slide->current_indent_level > indent_level) {
            //fprintf(stderr, "Indent OUT to %d\n", indent_level);
            auto parent = slide->content_cur->parent;
            parent->next = (Present_List_Node*)node;
            slide->content_cur = (Present_List_Node*)node;
            slide->current_indent_level = indent_level;
        } else {
            //fprintf(stderr, "Indent STAYS at %d\n", indent_level);
            slide->content_cur->next = (Present_List_Node*)node;
            node->hdr.parent = slide->content_cur->parent;
            slide->content_cur = (Present_List_Node*)node;
        }
    } else {
        //fprintf(stderr, "Indent ESTABLISHED at %d\n", indent_level);
        slide->content = slide->content_cur = (Present_List_Node*)node;
        slide->current_indent_level = indent_level;
    }
    
    //fprintf(stderr, "Appended list item %.*s\n", node->text_length, node->text);
}

static void SetFont(Present_File* file, const char** dst, const char* name, unsigned name_len) {
    char* buf;
    assert(file && dst && name && name_len > 0);
    if(file && dst && name && name_len > 0) {
        if(*dst) {
            fprintf(stderr, "Note: font was set multiple times!\n");
        }
        buf = (char*)Arena_Alloc(file->mem, name_len + 1);
        memcpy(buf, name, name_len);
        buf[name_len] = 0;
        *dst = buf;
    }
}

static void SetColor(Present_File* file, RGBA_Color* dst, const char* col, unsigned col_len) {
    assert(file && dst && col && col_len > 0);
    if(file && dst && col && col_len > 0) {
        if(col_len < 7) {
            fprintf(stderr, "Warning: hex color '%.*s' is too short, ignored!\n", col_len, col);
        } else if(col_len > 9) {
            fprintf(stderr, "Warning: hex color '%.*s' is too long, ignoring trailing characters (%.*s)!\n", col_len, col, 7, col);
        } else {
            unsigned hexcol = 0;
            assert(col[0] == '#' && "Not a hex color!");
            for(unsigned i = 1; i < col_len; i++) {
                hexcol <<= 4;
                if(col[i] >= '0' && col[i] <= '9') {
                    hexcol |= ((col[i] - '0') & 0xF);
                } else if((col[i] >= 'A' && col[i] <= 'F') || (col[i] >= 'a' && col[i] <= 'f')) {
                    hexcol |= (10 + ((col[i] - 'A') & 0xF));
                }
            }
            dst->r = ((hexcol & 0x00FF0000) >> 16) / 255.0f;
            dst->g = ((hexcol & 0x0000FF00) >>  8) / 255.0f;
            dst->b = ((hexcol & 0x000000FF) >>  0) / 255.0f;
            if(col_len >= 9) {
                dst->a = ((hexcol & 0xFF000000) >> 24) / 255.0f;
            } else {
                dst->a = 1;
            }
        }
    }
}

static bool ParseFile(Present_File* file, FILE* f) {
    bool ret = true;
    unsigned line_length;
    char line_buf[512];
    const unsigned line_siz = 512;
    unsigned indent_level;
    
    const char* directive;
    unsigned directive_len;
    const char* directive_arg;
    unsigned directive_arg_len;
    Parse_State pstate;
    
    pstate.first = pstate.last = NULL;
    
    assert(file && f);
    
    file->font_general = file->font_title = file->font_chapter = NULL;
    
    // First line must be a '#PRESENT'
    line_length = ReadLine(line_buf, line_siz, &indent_level, f);
    if(line_length && IsDirective(&directive, &directive_len, line_buf, line_length)) {
        if(strncmp(directive, "PRESENT", directive_len) == 0) {
            while(!feof(f)) {
                line_length = ReadLine(line_buf, line_siz, &indent_level, f);
                if(line_length) {
                    if(IsDirective(&directive, &directive_len, line_buf, line_length)) {
                        // Calculate directive argument ptr and len
                        directive_arg = directive + directive_len + 1;
                        directive_arg_len = line_length - directive_len - 1;
                        if(directive_arg_len != 0) {
                            directive_arg_len--;
                        }
                        
                        if(strncmp(directive, "SLIDE", directive_len) == 0) {
                            AppendSlide(file, &pstate);
                        } else if(strncmp(directive, "SUBTITLE", directive_len) == 0) {
                            SetSubtitle(file, &pstate, directive_arg, directive_arg_len);
                        } else if(strncmp(directive, "INLINE_IMAGE", directive_len) == 0) {
                            AddInlineImage(file, &pstate, indent_level, directive_arg, directive_arg_len, IMGALIGN_INLINE);
                        } else if(strncmp(directive, "RIGHT_IMAGE", directive_len) == 0) {
                            AddInlineImage(file, &pstate, indent_level, directive_arg, directive_arg_len, IMGALIGN_RIGHT);
                        } else if(strncmp(directive, "CHAPTER", directive_len) == 0) {
                            SetChapterTitle(file, &pstate, directive_arg, directive_arg_len);
                        } else if(strncmp(directive, "TITLE", directive_len) == 0) {
                            SetTitle(file, &pstate, directive_arg, directive_arg_len);
                        } else if(strncmp(directive, "AUTHORS", directive_len) == 0) {
                            SetAuthors(file, &pstate, directive_arg, directive_arg_len);
                        } else if(strncmp(directive, "FONT", directive_len) == 0) {
                            SetFont(file, &file->font_general, directive_arg, directive_arg_len);
                        } else if(strncmp(directive, "FONT_TITLE", directive_len) == 0) {
                            SetFont(file, &file->font_title, directive_arg, directive_arg_len);
                        } else if(strncmp(directive, "FONT_CHAPTER", directive_len) == 0) {
                            SetFont(file, &file->font_chapter, directive_arg, directive_arg_len);
                        } else if(strncmp(directive, "COLOR_BG", directive_len) == 0) {
                            SetColor(file, &file->color_bg, directive_arg, directive_arg_len);
                        } else if(strncmp(directive, "COLOR_FG", directive_len) == 0) {
                            SetColor(file, &file->color_fg, directive_arg, directive_arg_len);
                        } else if(strncmp(directive, "COLOR_BG_HEADER", directive_len) == 0) {
                            SetColor(file, &file->color_bg_header, directive_arg, directive_arg_len);
                        } else if(strncmp(directive, "COLOR_FG_HEADER", directive_len) == 0) {
                            SetColor(file, &file->color_fg_header, directive_arg, directive_arg_len);
                        } else {
                            fprintf(stderr, "Warning: unknown directive: '%.*s'\n",
                                    directive_len, directive);
                        }
                    } else {
                        AppendToList(file, &pstate, indent_level, line_buf, line_length);
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

Present_File* Present_Open(const char* filename) {
    Present_File* ret = NULL;
    FILE* f = NULL;
    
    assert(filename);
    if(filename) {
        f = fopen(filename, "r");
        if(f) {
            ret = (Present_File*)malloc(sizeof(Present_File));
            if(ret) {
                ret->path = filename;
                ret->mem = Arena_Create(PF_MEM_SIZE);
                ret->title = NULL;
                ret->authors = NULL;
                ret->slide_count = 1; // implicit title slide
                ret->current_slide = 0;
                ret->slides = NULL;
                SET_RGB(ret->color_bg, 255, 255, 255);
                SET_RGB(ret->color_fg, 0, 0, 0);
                SET_RGB(ret->color_bg_header, 43, 203, 186);
                SET_RGB(ret->color_fg_header, 255, 255, 255);
                if(!ParseFile(ret, f)) {
                    Arena_Destroy(ret->mem);
                    free(ret);
                    ret = NULL;
                    
                    fprintf(stderr, "Presentation parse error!\n");
                } else {
                    auto mmused = Arena_Used(ret->mem);
                    auto mmsize = Arena_Size(ret->mem);
                    auto mmperc = (float)mmused / (float)mmsize;
                    fprintf(stderr, "Presentation uses %u / %u bytes of memory (%f%%)\n", mmused, mmsize, mmperc * 100);
                }
            }
            fclose(f);
        } else {
            fprintf(stderr, "Failed to open presentation file '%s'!\n", filename);
        }
    }
    
    return ret;
}

void Present_Close(Present_File* file) {
    assert(file);
    if(file) {
        free(file);
    }
}

bool Present_Over(Present_File* file) {
    bool ret = true;
    assert(file);
    if(file) {
        ret = file->current_slide > file->slide_count;
    }
    return ret;
}

int Present_Seek(Present_File* file, int off) {
    int ret = 0;
    int abs;
    assert(file);
    if(file) {
        abs = file->current_slide + off;
        ret = Present_SeekTo(file, abs);
    }
    return ret;
}

int Present_SeekTo(Present_File* file, int abs) {
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

static void PresentClearScreen(Present_File* file, Render_Queue* rq, float r, float b, float g) {
    auto rect = RQ_NewCmd<RQ_Draw_Rect>(rq, RQCMD_DRAW_RECTANGLE);
    rect->x0 = rect->y0 = 0;
    rect->x1 = rect->y1 = 1;
    rect->color.r = r; rect->color.g = g;
    rect->color.b = b; rect->color.a = 1;
}

static void PresentFillRQTitleSlide(Present_File* file, Render_Queue* rq) {
    PresentClearScreen(file, rq, file->color_bg.r, file->color_bg.g, file->color_bg.b);
    if(file->title_len && file->title) {
        auto title = RQ_NewCmd<RQ_Draw_Text>(rq, RQCMD_DRAW_TEXT);
        title->x = VIRTUAL_X(24);
        title->y = VIRTUAL_Y((720 - 72) / 2);
        title->size = VIRTUAL_Y(72);
        title->text_len = file->title_len;
        title->text = file->title;
        title->font_name = file->font_title;
        title->color = file->color_fg;
    }
    
    if(file->authors_len && file->authors) {
        auto authors = RQ_NewCmd<RQ_Draw_Text>(rq, RQCMD_DRAW_TEXT);
        authors->x = VIRTUAL_X(36);
        authors->y = VIRTUAL_Y((720 + 20) / 2);
        authors->size = VIRTUAL_Y(20);
        authors->text_len = file->authors_len;
        authors->text = file->authors;
        authors->font_name = file->font_title;
        authors->color = file->color_fg;
    }
}

static void PresentFillRQEndSlide(Present_File* file, Render_Queue* rq) {
    // a filled rectangle covering the screen
    PresentClearScreen(file, rq, 0, 0, 0);
}

struct List_Processor_State {
    int x, y;
    int right_y;
};

static void ProcessListElement(Present_File* file, Present_List_Node* node, Render_Queue* rq,
                               List_Processor_State& state) {
    auto cur = node;
    while(cur) {
        if(cur->type == LNODE_TEXT) {
            RQ_Draw_Text* cmd = NULL;
            list_node_text* text = (list_node_text*)cur;
            cmd = RQ_NewCmd<RQ_Draw_Text>(rq, RQCMD_DRAW_TEXT);
            cmd->x = VIRTUAL_X(state.x);
            cmd->y = VIRTUAL_Y(state.y);
            cmd->size = VIRTUAL_Y(32);
            cmd->text_len = text->text_length;
            cmd->text = text->text;
            cmd->font_name = file->font_general;
            cmd->color = file->color_fg;
            state.y += 40;
        } else if(cur->type == LNODE_IMAGE) {
            RQ_Draw_Image* cmd = NULL;
            int w, h, channels;
            void *pixbuf, *pixbuf_final;
            list_node_image* img = (list_node_image*)cur;
            cmd = RQ_NewCmd<RQ_Draw_Image>(rq, RQCMD_DRAW_IMAGE);
            
            pixbuf = stbi_load(img->path, &w, &h, &channels, STBI_rgb_alpha);
            if(pixbuf) {
                unsigned pixbuf_siz = sizeof(stbi_uc) * w * h * 4;
                pixbuf_final = Arena_Alloc(rq->mem, pixbuf_siz);
                memcpy(pixbuf_final, pixbuf, pixbuf_siz);
                if(Display_SwapRedBlueChannels()) {
                    SwapRedBlueChannels((uint8_t*)pixbuf_final, w, h);
                }
                stbi_image_free(pixbuf);
                pixbuf = NULL;
                
                cmd->width = w;
                cmd->height = h;
                cmd->buffer = pixbuf_final;
                
                switch(img->alignment) {
                    case IMGALIGN_RIGHT:
                    cmd->x = 1 - VIRTUAL_X(w);
                    if(cmd->x < 0) {
                        cmd->x = 0.25f;
                        fprintf(stderr, "Warning: image '%s' is too wide to fit on the screen!\n", img->path);
                    }
                    cmd->y = VIRTUAL_Y(state.right_y);
                    state.right_y += cmd->height;
                    break;
                    default:
                    fprintf(stderr, "Unimplemented image alignment %d\n", img->alignment);
                    case IMGALIGN_INLINE:
                    cmd->x = 0;
                    cmd->y = VIRTUAL_Y(state.y);
                    state.y += cmd->height;
                    break;
                }
                
                assert(cmd->buffer);
                
            } else {
                fprintf(stderr, "Couldn't load image '%s': %s\n", img->path, stbi_failure_reason());
                cmd->width = cmd->height = 0;
                cmd->buffer = NULL;
            }
        }
        if(cur->children) {
            state.x += 24;
            ProcessListElement(file, cur->children, rq, state);
        }
        cur = cur->next;
    }
    state.x -= 24;
}

static void PresentFillRQRegularSlide(Present_File* file, present_slide* slide, Render_Queue* rq) {
    List_Processor_State lps = {8, 160, 80};
    RQ_Draw_Text* cmd = NULL;
    RQ_Draw_Rect* rect = NULL;
    
    PresentClearScreen(file, rq, file->color_bg.r, file->color_bg.g, file->color_bg.b);
    
    if(slide->chapter_title) {
        rect = RQ_NewCmd<RQ_Draw_Rect>(rq, RQCMD_DRAW_RECTANGLE);
        rect->x0 = 0; rect->y0 = 0;
        rect->x1 = 1; rect->y1 = VIRTUAL_Y(72);
        rect->color = file->color_bg_header;
        
        cmd = RQ_NewCmd<RQ_Draw_Text>(rq, RQCMD_DRAW_TEXT);
        cmd->x = VIRTUAL_X(10);
        cmd->y = VIRTUAL_Y(64);
        cmd->size = VIRTUAL_Y(64);
        cmd->text_len = slide->chapter_title_len;
        cmd->text = slide->chapter_title;
        cmd->font_name = file->font_chapter;
        cmd->color = file->color_fg_header;
    }
    if(slide->subtitle) {
        cmd = RQ_NewCmd<RQ_Draw_Text>(rq, RQCMD_DRAW_TEXT);
        cmd->x = VIRTUAL_X(10); cmd->y = VIRTUAL_Y(120);
        cmd->size = VIRTUAL_Y(44);
        cmd->text_len = slide->subtitle_len;
        cmd->text = slide->subtitle;
        cmd->font_name = file->font_general;
        cmd->color = file->color_fg;
    }
    
    ProcessListElement(file, slide->content, rq, lps);
    
    cmd = RQ_NewCmd<RQ_Draw_Text>(rq, RQCMD_DRAW_TEXT);
    int slide_num_len = snprintf(NULL, 0, "%d / %d", file->current_slide, file->slide_count);
    cmd->text = (char*)Arena_Alloc(rq->mem, slide_num_len + 1);
    cmd->text_len = slide_num_len;
    snprintf((char*)cmd->text, cmd->text_len + 1, "%d / %d", file->current_slide, file->slide_count);
    cmd->x = VIRTUAL_X(1280 - 50);
    cmd->y = VIRTUAL_Y(720 - 18);
    cmd->size = VIRTUAL_Y(18);
    cmd->color = file->color_fg;
    cmd->font_name = file->font_general;
}

void Present_FillRenderQueue(Present_File* file, Render_Queue* rq) {
    assert(file && rq);
    if(file && rq) {
        if(file->current_slide == 0) {
            PresentFillRQTitleSlide(file, rq);
        } else if(file->current_slide == file->slide_count) {
            PresentFillRQEndSlide(file, rq);
        } else {
            auto slide = file->current_slide_data;
            PresentFillRQRegularSlide(file, slide, rq);
        }
    }
}
