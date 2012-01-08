#include "SDL_config.h"

#include <assert.h>

#include "SDL_naclvideo.h"
#include "SDL_naclevents_c.h"
#include "MainThreadRunner.h"
#include "SDL_nacljob.h"

#include <ppapi/cpp/instance.h>
#include <ppapi/cpp/graphics_2d.h>
#include <ppapi/cpp/completion_callback.h>
#include <ppapi/cpp/image_data.h>
#include <ppapi/cpp/rect.h>
#include <ppapi/c/pp_errors.h>

pp::Instance* gNaclPPInstance;
static int gNaclVideoWidth;
static int gNaclVideoHeight;
static MainThreadRunner* gNaclMainThreadRunner;

static int kNaClFlushDelayMs = 20;

#include "SDL_nacl.h"

extern "C" {

#include "SDL_video.h"
#include "SDL_mouse.h"
#include "../SDL_sysvideo.h"
#include "../SDL_pixels_c.h"
#include "../../events/SDL_events_c.h"

#define NACLVID_DRIVER_NAME "nacl"

void SDL_NACL_SetInstance(PP_Instance instance, int width, int height) {
  bool is_resize = gNaclPPInstance &&
    (width != gNaclVideoWidth || height != gNaclVideoHeight);
  if (!gNaclPPInstance) {
    gNaclPPInstance = pp::Module::Get()->InstanceForPPInstance(instance);
    gNaclMainThreadRunner = new MainThreadRunner(gNaclPPInstance);
  }
  gNaclVideoWidth = width;
  gNaclVideoHeight = height;
  if (is_resize && current_video) {
    current_video->hidden->ow = width;
    current_video->hidden->oh = height;
    SDL_PrivateResize(width, height);
  }
}

static void flush(void* data, int32_t unused);

/* Initialization/Query functions */
static int NACL_VideoInit(_THIS, SDL_PixelFormat *vformat);
static SDL_Rect **NACL_ListModes(_THIS, SDL_PixelFormat *format, Uint32 flags);
static SDL_Surface *NACL_SetVideoMode(_THIS, SDL_Surface *current, int width, int height, int bpp, Uint32 flags);
static void NACL_VideoQuit(_THIS);
static void NACL_UpdateRects(_THIS, int numrects, SDL_Rect *rects);

/* The implementation dependent data for the window manager cursor */
struct WMcursor {
  // Fake cursor data to fool SDL into not using its broken (as it seems) software cursor emulation.
};

static void NACL_FreeWMCursor(_THIS, WMcursor *cursor);
static WMcursor *NACL_CreateWMCursor(_THIS,
                Uint8 *data, Uint8 *mask, int w, int h, int hot_x, int hot_y);
static int NACL_ShowWMCursor(_THIS, WMcursor *cursor);
static void NACL_WarpWMCursor(_THIS, Uint16 x, Uint16 y);


static int NACL_Available(void) {
  return gNaclPPInstance != 0;
}

static void NACL_DeleteDevice(SDL_VideoDevice *device) {
  SDL_free(device->hidden);
  SDL_free(device);
}

static SDL_VideoDevice *NACL_CreateDevice(int devindex) {
  SDL_VideoDevice *device;

  assert(gNaclPPInstance);

  /* Initialize all variables that we clean on shutdown */
  device = (SDL_VideoDevice *)SDL_malloc(sizeof(SDL_VideoDevice));
  if ( device ) {
    SDL_memset(device, 0, (sizeof *device));
    device->hidden = (struct SDL_PrivateVideoData *)
        SDL_malloc((sizeof *device->hidden));
  }
  if ( (device == NULL) || (device->hidden == NULL) ) {
    SDL_OutOfMemory();
    if ( device ) {
      SDL_free(device);
    }
    return(0);
  }
  SDL_memset(device->hidden, 0, (sizeof *device->hidden));

  device->hidden->ow = gNaclVideoWidth;
  device->hidden->oh = gNaclVideoHeight;

  // TODO: query the fullscreen size

  /* Set the function pointers */
  device->VideoInit = NACL_VideoInit;
  device->ListModes = NACL_ListModes;
  device->SetVideoMode = NACL_SetVideoMode;
  device->UpdateRects = NACL_UpdateRects;
  device->VideoQuit = NACL_VideoQuit;
  device->InitOSKeymap = NACL_InitOSKeymap;
  device->PumpEvents = NACL_PumpEvents;

  device->FreeWMCursor = NACL_FreeWMCursor;
  device->CreateWMCursor = NACL_CreateWMCursor;
  device->ShowWMCursor = NACL_ShowWMCursor;
  device->WarpWMCursor = NACL_WarpWMCursor;

  device->free = NACL_DeleteDevice;

  return device;
}

VideoBootStrap NACL_bootstrap = {
  NACLVID_DRIVER_NAME, "SDL Native Client video driver",
  NACL_Available, NACL_CreateDevice
};


int NACL_VideoInit(_THIS, SDL_PixelFormat *vformat) {
  fprintf(stderr, "CONGRATULATIONS: You are using the SDL nacl video driver!\n");

  /* Determine the screen depth (use default 8-bit depth) */
  /* we change this during the SDL_SetVideoMode implementation... */
  vformat->BitsPerPixel = 32;
  vformat->BytesPerPixel = 4;

  _this->info.current_w = gNaclVideoWidth;
  _this->info.current_h = gNaclVideoHeight;

  /* We're done! */
  return(0);
}

SDL_Rect **NACL_ListModes(_THIS, SDL_PixelFormat *format, Uint32 flags) {
  // TODO: list modes
  return (SDL_Rect **) -1;
}


SDL_Surface *NACL_SetVideoMode(_THIS, SDL_Surface *current,
    int width, int height, int bpp, Uint32 flags) {

  fprintf(stderr, "setvideomode %d %d %d %u\n", width, height, bpp, flags);
  fflush(stderr);

  if (width > _this->hidden->ow || height > _this->hidden->oh)
    return NULL;
  _this->hidden->bpp = bpp = 32; // Let SDL handle pixel format conversion.
  _this->hidden->w = width;
  _this->hidden->h = height;

  SDLNaclJob* job = new SDLNaclJob(CREATE_GRAPHICS_CONTEXT, _this);
  int32_t rv = gNaclMainThreadRunner->RunJob(job);
  if (rv != PP_OK)
    return NULL;

  /* Allocate the new pixel format for the screen */
  if ( ! SDL_ReallocFormat(current, bpp, 0xFF0000, 0xFF00, 0xFF, 0) ) {
    SDL_SetError("Couldn't allocate new pixel format for requested mode");
    return(NULL);
  }

  /* Set up the new mode framebuffer */
  current->flags = flags & SDL_FULLSCREEN;
  _this->hidden->bpp = bpp;
  _this->hidden->w = current->w = width;
  _this->hidden->h = current->h = height;
  current->pitch = current->w * (bpp / 8);
  current->pixels = _this->hidden->image_data->data();

  /* We're done */
  return(current);
}

static void NACL_UpdateRects(_THIS, int numrects, SDL_Rect *rects) {
  if (_this->hidden->bpp == 0) // not initialized yet
    return;

  assert(_this->hidden->image_data);
  assert(_this->hidden->w > 0);
  assert(_this->hidden->h > 0);

  // Clear alpha channel in the ImageData.
  unsigned char* start = (unsigned char*)_this->hidden->image_data->data();
  unsigned char* end = start + (_this->hidden->w * _this->hidden->h * _this->hidden->bpp / 8);
  for (unsigned char* p = start + 3; p < end; p +=4)
    *p = 0xFF;

  _this->hidden->numrects = numrects;
  _this->hidden->rects = rects;

  // Flush on the main thread.
  SDLNaclJob* job = new SDLNaclJob(VIDEO_FLUSH, _this);
  gNaclMainThreadRunner->RunJob(job);

  _this->hidden->numrects = 0; // sanity
  _this->hidden->rects = NULL;
}

static void NACL_FreeWMCursor(_THIS, WMcursor *cursor) {
  delete cursor;
}

static WMcursor *NACL_CreateWMCursor(_THIS,
    Uint8 *data, Uint8 *mask, int w, int h, int hot_x, int hot_y) {
  return new WMcursor();
}

static int NACL_ShowWMCursor(_THIS, WMcursor *cursor) {
  return 1; // Success!
}

static void NACL_WarpWMCursor(_THIS, Uint16 x, Uint16 y) {
}

/* Note:  If we are terminated, this could be called in the middle of
   another SDL video routine -- notably UpdateRects.
*/
void NACL_VideoQuit(_THIS) {
  if (_this->screen->pixels != NULL)
  {
    SDL_free(_this->screen->pixels);
    _this->screen->pixels = NULL;
  }

  SDLNaclJob* job = new SDLNaclJob(VIDEO_QUIT, _this);
  gNaclMainThreadRunner->RunJob(job);
}
} // extern "C"
