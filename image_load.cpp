// present
// Copyright (C) 2020 Daniel Meszaros <easimer@gmail.com>
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

#include "arena.h"
#include "image_load.h"
#include "display.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <string>
#include <queue>
#include <list>

#include "stb_image.h"

#define THREAD_COUNT (2)

struct Promised_Image {
    Promised_Image(std::string&& path)
        : path(path), processed(false), buffer(NULL),
        w(0), h(0) {}

    std::string path;
    bool processed;

    void* buffer;
    int w, h;
};

using Request_Queue = std::queue<Promised_Image*>;
using Lock = std::mutex;
using Lock_Guard = std::lock_guard<std::mutex>;
using Unique_Lock = std::unique_lock<std::mutex>;
using Thread = std::thread;
using Cond_Var = std::condition_variable;

static bool gShutdown = false;

static Lock gQueueLock;
static Request_Queue* gRequestQueue = NULL;

static Cond_Var gCV;
static Lock gCVLock;

static Thread gThread[THREAD_COUNT];

static void SwapRedBlueChannels(uint8_t* rgba_buffer, unsigned width, unsigned height) {
    for(unsigned y = 0; y < height; y++) {
        for(unsigned x = 0; x < width; x++) {
            auto& c0 = rgba_buffer[y * width * 4 + x * 4 + 0];
            auto& c1 = rgba_buffer[y * width * 4 + x * 4 + 2];
            std::swap(c0, c1);
        }
    }
}

static void ThreadFunc() {
    int w, h, channels;
    void *pixbuf, *pixbuf_final;
    while(!gShutdown) {
        Unique_Lock UL(gCVLock);
        gCV.wait(UL);
        Lock_Guard G(gQueueLock);
        auto P = gRequestQueue->back();
        gRequestQueue->pop();
        pixbuf = stbi_load(P->path.c_str(), &w, &h, &channels, STBI_rgb_alpha);
        if(pixbuf) {
            if(Display_SwapRedBlueChannels()) {
                SwapRedBlueChannels((uint8_t*)pixbuf, w, h);
            }
            P->buffer = pixbuf;
            P->w = w;
            P->h = h;
        } else {
            printf("ImageLoader: couldn't load '%s'\n", P->path.c_str());
        }
        
        P->processed = true;
    }
}

void ImageLoader_Init() {
    gShutdown = false;
    gRequestQueue = new Request_Queue;
    for(int i = 0; i < THREAD_COUNT; i++) {
        gThread[i] = Thread(ThreadFunc);
    }
}

void ImageLoader_Shutdown() {
    gShutdown = true;
    gCV.notify_all();
    for(int i = 0; i < THREAD_COUNT; i++) {
        gThread[i].join();
    }
    delete gRequestQueue;
    gRequestQueue = NULL;
}

Promised_Image* ImageLoader_Request(const char* path) {
    Promised_Image* ret = NULL;

    if(path && gRequestQueue) {
        Lock_Guard G(gQueueLock);
        ret = new Promised_Image(std::string(path));
        gRequestQueue->push(ret);
        gCV.notify_one();
    }

    return ret;
}

Loaded_Image* ImageLoader_Await(Promised_Image* pimg) {
    Loaded_Image* ret = NULL;

    if(pimg) {
        while(!pimg->processed) {
            std::this_thread::yield();
        }
        if(pimg->buffer) {
            ret = new Loaded_Image;
            ret->buffer = (char*)pimg->buffer;
            ret->width = pimg->w;
            ret->height = pimg->h;
        }
    }

    return ret;
}

//void ImageLoader_Free(Promised_Image* pimg);
void ImageLoader_Free(Loaded_Image* limg) {
    if(limg) {
        stbi_image_free(limg->buffer);
        delete limg;
    }
}
