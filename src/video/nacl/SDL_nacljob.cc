/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include <assert.h>
#include <ppapi/cpp/instance.h>
#include <ppapi/cpp/graphics_2d.h>
#include <ppapi/cpp/completion_callback.h>
#include <ppapi/cpp/image_data.h>
#include <ppapi/cpp/rect.h>
#include <ppapi/c/pp_errors.h>
#include "SDL_nacljob.h"

void SDLNaclJob::Run(MainThreadRunner::JobEntry *e) {
  job_entry_ = e;
  factory_ = new pp::CompletionCallbackFactory<SDLNaclJob>(this);
  pp::CompletionCallback cc = factory_->NewCallback(&SDLNaclJob::Finish);
  pp::Instance *instance = MainThreadRunner::ExtractPepperInstance(e);

  int32_t rv;
  switch (op_) {
  case VIDEO_INIT:
    cc.Run(PP_OK);
    break;

  case CREATE_GRAPHICS_CONTEXT:
    if (device_->hidden->context2d)
      delete device_->hidden->context2d;
    device_->hidden->context2d =
      new pp::Graphics2D(instance,
			 pp::Size(device_->hidden->w, device_->hidden->h), false);
    assert(device_->hidden->context2d != NULL);
    
    if (!instance->BindGraphics(*device_->hidden->context2d)) {
      fprintf(stderr, "***** Couldn't bind the device context *****\n");
    }
    
    if (device_->hidden->image_data)
      delete device_->hidden->image_data;

    device_->hidden->image_data =
      new pp::ImageData(instance,
			PP_IMAGEDATAFORMAT_BGRA_PREMUL,
			device_->hidden->context2d->size(),
			false);
    assert(device_->hidden->image_data != NULL);
    // TODO: report any errors
    cc.Run(PP_OK);
    break;

  case VIDEO_FLUSH:
    for (int i = 0; i < device_->hidden->numrects; ++i) {
      SDL_Rect& r = device_->hidden->rects[i];
      device_->hidden->context2d->PaintImageData(*device_->hidden->image_data, pp::Point(), pp::Rect(r.x, r.y, r.w, r.h));
    }

    device_->hidden->context2d->Flush(cc);
    break;

  case VIDEO_QUIT:
    delete device_->hidden->context2d;
    delete device_->hidden->image_data;
    cc.Run(PP_OK);

  }
}

void SDLNaclJob::Finish(int32_t result) {
  MainThreadRunner::ResultCompletion(job_entry_, result);
}
