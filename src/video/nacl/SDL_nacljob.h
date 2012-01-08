/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef _SDL_nacljob_h
#define _SDL_nacljob_h

#include <ppapi/c/pp_errors.h>
#include <ppapi/c/pp_file_info.h>
#include <ppapi/cpp/completion_callback.h>
#include <ppapi/cpp/file_io.h>
#include <ppapi/cpp/file_ref.h>
#include <ppapi/cpp/file_system.h>
#include <ppapi/cpp/url_loader.h>
#include <ppapi/cpp/url_request_info.h>
#include <ppapi/cpp/url_response_info.h>
#include <string>
#include <vector>
#include <set>
#include "MainThreadRunner.h"

#include "SDL_naclvideo.h"
extern "C" {
#include "../SDL_sysvideo.h"
}

typedef enum {
  NO_OP = 0,
  VIDEO_INIT,
  CREATE_GRAPHICS_CONTEXT,
  VIDEO_FLUSH,
  VIDEO_QUIT
} SDLNaclOperation;

class SDLNaclJob : public MainThreadJob {
 public:
 SDLNaclJob(SDLNaclOperation op, SDL_VideoDevice* device) : op_(op), device_(device) {}
  ~SDLNaclJob() {}

  void Run(MainThreadRunner::JobEntry *e);
  
 private:

  SDLNaclOperation op_;
  SDL_VideoDevice *device_;

  pp::CompletionCallbackFactory<SDLNaclJob> *factory_;

  MainThreadRunner::JobEntry *job_entry_;

  void Finish(int32_t result);
};

#endif
