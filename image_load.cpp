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
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

using Request_Queue = std::queue<Promised_Image*>;
using Lock = std::mutex;
using Lock_Guard = std::lock_guard<std::mutex>;
using Unique_Lock = std::unique_lock<std::mutex>;
using Thread = std::thread;
using Cond_Var = std::condition_variable;

struct Counting_Semaphore {
    public:
    void release() {
        Lock_Guard G(mtx);
        ++count;
        cv.notify_one();
    }
    
    void acquire() {
        Unique_Lock UL(mtx);
        while(!count) {
            cv.wait(UL);
        }
        count--;
    }
    
    Counting_Semaphore() : count(0) {}
    Counting_Semaphore(const Counting_Semaphore&) = delete;
    void operator=(const Counting_Semaphore&) = delete;
    private:
    Lock mtx;
    Cond_Var cv;
    unsigned long count;
};

struct Promised_Image {
    Promised_Image(std::string&& path)
        : path(path), processed(false), buffer(NULL),
    w(0), h(0) {}
    
    std::string path;
    volatile bool processed;
    
    void* buffer;
    int w, h;
};

static bool gShutdown = false;

static Lock gQueueLock;
static Request_Queue* gRequestQueue = NULL;

static Counting_Semaphore gSema;

static Thread* gThread = NULL;
static unsigned gThreadCount = 0;

static void SwapRedBlueChannels(uint8_t* rgba_buffer, unsigned width, unsigned height) {
    for(unsigned y = 0; y < height; y++) {
        for(unsigned x = 0; x < width; x++) {
            auto& c0 = rgba_buffer[y * width * 4 + x * 4 + 0];
            auto& c1 = rgba_buffer[y * width * 4 + x * 4 + 2];
            std::swap(c0, c1);
        }
    }
}

static void ThreadFunc(int i) {
    int w, h, channels;
    void *pixbuf;
    while(!gShutdown) {
        gSema.acquire();
        if(gShutdown) break; // shutdown
        gQueueLock.lock();
        auto P = gRequestQueue->front();
        //printf("Loader thread %d is loading '%s' (%p), N=%zu\n", i, P->path.c_str(), P, gRequestQueue->size());
        gRequestQueue->pop();
        gQueueLock.unlock();
        
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


static void CreateThreads() {
    auto N = std::thread::hardware_concurrency();
    if(N > 1) {
        if(N > 4) {
            // We don't want more than 3 threads doing I/O
            N = 3;
        } else {
            N -= 1;
        }
    } else {
        N = 2;
    }
    printf("Disk I/O on %u threads\n", N);
    gThread = new Thread[N];
    gThreadCount = N;
    for(unsigned i = 0; i < N; i++) {
        gThread[i] = Thread(ThreadFunc, i);
    }
}

static void CleanupThreads() {
    gShutdown = true;
    for(unsigned i = 0; i < gThreadCount; i++) {
        gSema.release();
    }
    for(unsigned i = 0; i < gThreadCount; i++) {
        gThread[i].join();
    }
    gThreadCount = 0;
    delete[] gThread;
}

void ImageLoader_Init() {
    gShutdown = false;
    gRequestQueue = new Request_Queue;
    CreateThreads();
}

void ImageLoader_Shutdown() {
    CleanupThreads();
    delete gRequestQueue;
    gRequestQueue = NULL;
}

Promised_Image* ImageLoader_Request(const char* path) {
    Promised_Image* ret = NULL;
    
    if(path && gRequestQueue) {
        ret = new Promised_Image(std::string(path));
        gQueueLock.lock();
        gRequestQueue->push(ret);
        gQueueLock.unlock();
        //printf("Client thread notifying threads about '%p'\n", ret);
        gSema.release();
    }
    
    return ret;
}

Loaded_Image* ImageLoader_Await(Promised_Image* pimg) {
    Loaded_Image* ret = NULL;
    
    if(pimg) {
        //printf("Client thread awaiting on '%p'\n", pimg);
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

void ImageLoader_Free(Loaded_Image* limg) {
    if(limg) {
        stbi_image_free(limg->buffer);
        delete limg;
    }
}
