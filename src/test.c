#include <SDL3/SDL.h>

int main(void)
{
    SDL_SetHint(SDL_HINT_CAMERA_DRIVER, "directshow");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_CAMERA) < 0) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow(
        "SDL3 Webcam",
        800, 600,
        SDL_WINDOW_RESIZABLE
    );

    if (!window) {
        SDL_Log("Window creation failed: %s", SDL_GetError());
        return 1;
    }

    /*SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL);
    if (!renderer) {
        SDL_Log("Renderer creation failed: %s", SDL_GetError());
        return 1;
    }*/

    /* Enumerate cameras */
    int num_cameras = 0;
    SDL_CameraID *cameras = SDL_GetCameras(&num_cameras);
    if (!cameras || num_cameras == 0) {
        SDL_Log("No cameras found");
        return 1;
    }
    SDL_Log("Camera count: %u\n", num_cameras);

    SDL_CameraSpec spec = {
        .format = SDL_PIXELFORMAT_ABGR8888,
        .width  = 640,
        .height = 480,
        .framerate_numerator = 30,
        .framerate_denominator = 1
    };

    SDL_Camera *camera = SDL_OpenCamera(cameras[0], &spec);
    SDL_free(cameras);

    if (!camera) {
        SDL_Log("Failed to open camera: %s", SDL_GetError());
        return 1;
    }

    /*SDL_Texture *texture = SDL_CreateTexture(
        renderer,
        spec.format,
        SDL_TEXTUREACCESS_STREAMING,
        spec.width,
        spec.height
    );

    if (!texture) {
        SDL_Log("Texture creation failed: %s", SDL_GetError());
        return 1;
    }*/

    int running = 1;
    SDL_Event e;

    while (running) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT)
                running = 0;
        }

        Uint64 timestamp_ns = 0;
        SDL_Surface *frame = SDL_AcquireCameraFrame(camera, &timestamp_ns);

        if (frame) {
            SDL_BlitSurfaceScaled(frame, NULL, SDL_GetWindowSurface(window), NULL, SDL_SCALEMODE_LINEAR);
            /*SDL_UpdateTexture(
                texture,
                NULL,
                frame->pixels,
                frame->pitch
            );*/
            SDL_ReleaseCameraFrame(camera, frame);
            SDL_UpdateWindowSurface(window);
        } else {
            SDL_Delay(1);
        }

        /*SDL_RenderClear(renderer);
        SDL_RenderTexture(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);*/
    }

    //SDL_DestroyTexture(texture);
    SDL_CloseCamera(camera);
    //SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
