#pragma once
#include "render_queue.h"

struct present_file;

present_file* present_open(const char* filename);
void present_close(present_file* file);

bool present_over(present_file* file);

int present_seek(present_file* file, int off);
int present_seek_to(present_file* file, int idx);
void present_fill_render_queue(present_file* file, render_queue* rq);
