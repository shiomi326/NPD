#include "vlccapture.h"
#include <iostream>

#include "opencv2/core/core.hpp"
#include "opencv2/videoio/videoio.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "vlc/vlc.h"
#include "vlc/libvlc.h"
#include "SDL.h"
#include "SDL_mutex.h"

//#define VIDEO_WIDTH     960
//#define VIDEO_HEIGHT    540
#define VIDEO_WIDTH     1024
#define VIDEO_HEIGHT    576

struct ctx
{
   cv::Mat* image;
   SDL_mutex* mutex;
   uchar*    pixels;
};


void* lock(void *data, void**p_pixels)
{
   struct ctx *ctx = (struct ctx*)data;

//   WaitForSingleObject(ctx->mutex, INFINITE);
   SDL_LockMutex(ctx->mutex);

   // pixel will be stored on image pixel space
   *p_pixels = ctx->pixels;

   return NULL;

}

void display(void *data, void *id){
   (void) data;
   assert(id == NULL);
}

void unlock(void *data, void *id, void *const *p_pixels){

   // get back data structure
   struct ctx *ctx = (struct ctx*)data;

   /* VLC just rendered the video, but we can also render stuff */
   cv::Mat img = *ctx->image;
   // show rendered image
   imshow("test", img);

   SDL_UnlockMutex(ctx->mutex);
   std::cout << "Image" << img.cols << std::endl;
//   ReleaseMutex(ctx->mutex);
}

int lvc_capture(){

    // VLC pointers
     libvlc_instance_t *vlcInstance;
     libvlc_media_player_t *mp;
     libvlc_media_t *media;

     const char * const vlc_args[] = {
        "-I", "dummy", // Don't use any interface
        "--ignore-config", // Don't use VLC's config
        "--extraintf=logger", // Log anything
        "--verbose=2", // Be much more verbose then normal for debugging purpose
//         "--no-audio",
     };

     vlcInstance = libvlc_new(sizeof(vlc_args) / sizeof(vlc_args[0]), vlc_args);
     if ( vlcInstance == NULL ) {
            std::cout << "Create Media Stream Error" << std::endl;
            return 0;
        }

     // Read a distant video stream
     media = libvlc_media_new_location(vlcInstance, "rtsp://192.168.5.7:554/s2");
//     media = libvlc_media_new_location(vlcInstance, "rtsp://192.168.5.5:8554/CH001.sdp");
     // Read a local video file
//     media = libvlc_media_new_path(vlcInstance, "/home/shiomi/QtProjects/NpDetectio/detected_images/video/camera1.avi");
     if ( media == NULL ) {
         std::cout << "Media Stream is Null" << std::endl;
         return 0;
     }
     mp = libvlc_media_player_new_from_media(media);

     libvlc_media_release(media);

     struct ctx* context = ( struct ctx* )malloc( sizeof( *context ) );
//     context->mutex = CreateMutex(NULL, false, NULL);
     context->mutex = SDL_CreateMutex();

     context->image = new cv::Mat(VIDEO_HEIGHT, VIDEO_WIDTH, CV_8UC3);
//     context->image = new Mat(VIDEO_HEIGHT, VIDEO_WIDTH, CV_8UC4);
     context->pixels = (unsigned char *)context->image->data;
     // show blank image
//     namedWindow("test");
     imshow("test", *context->image);

     libvlc_video_set_callbacks(mp, lock, unlock, display, context);
     libvlc_video_set_format(mp, "RV24", VIDEO_WIDTH, VIDEO_HEIGHT, VIDEO_WIDTH * 24 / 8); // pitch = width * BitsPerPixel / 8
//     libvlc_video_set_format(mp, "RV32", VIDEO_WIDTH, VIDEO_HEIGHT, VIDEO_WIDTH<<2); //

     libvlc_media_player_play(mp);

     int ii = 0;
     int key = 0;
     while(key != 27)
     {
//        ii++;
//        if (ii > 5)
//        {
//           libvlc_media_player_play(mp);
//        }
//        float fps =  libvlc_media_player_get_fps(mp);
//        printf("fps:%f\r\n",fps);
        key = cv::waitKey(100); // wait 100ms for Esc key
     }

     libvlc_media_player_stop(mp);
     libvlc_media_player_release( mp );
     libvlc_release( vlcInstance );

     return 0;
}
