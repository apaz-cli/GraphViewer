#include <SDL2/SDL.h>
#include <SDL2/SDL2_gfxPrimitives.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FPS 60
#define FRAME_DELAY (1000 / FPS)
#define MAX_NODES 1000
#define MAX_LABEL_LENGTH 256

typedef struct {
    float x, y;
} Vec2f;

typedef struct {
    int id;
    Vec2f position;
    char label[MAX_LABEL_LENGTH];
} GraphNode;

typedef struct {
    int source;
    int target;
    char label[MAX_LABEL_LENGTH];
} GraphEdge;

typedef struct {
    GraphNode nodes[MAX_NODES];
    GraphEdge *edges;
    int node_count;
    int edge_count;
} GraphData;

typedef struct {
    float zoom;
    Vec2f position;
} Camera;

typedef struct {
    GraphData *graph;
    Camera camera;
    TTF_Font *font;
    int window_width;
    int window_height;
} AppState;

// Function declarations
GraphData *load_graph(const char *filename);
void free_graph(GraphData *graph);
void render_graph(SDL_Renderer *renderer, AppState *app);
void handle_input(SDL_Event *event, AppState *app);
void initialize_app(AppState *app, const char *graph_file);
void cleanup_app(AppState *app);

GraphData *load_graph(const char *filename) {
    // TODO: Implement graph loading
    return NULL;
}

void free_graph(GraphData *graph) {
    // TODO: Implement graph freeing
}

void render_graph(SDL_Renderer *renderer, AppState *app) {
    // TODO: Implement graph rendering
}

void handle_input(SDL_Event *event, AppState *app) {
    // TODO: Implement input handling
}

void initialize_app(AppState *app, const char *graph_file) {
    // TODO: Implement app initialization
}

void cleanup_app(AppState *app) {
    // TODO: Implement app cleanup
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <graph_file.json>\n", argv[0]);
        return 1;
    }

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    if (TTF_Init() == -1) {
        fprintf(stderr, "TTF_Init: %s\n", TTF_GetError());
        return 1;
    }

    AppState app;
    initialize_app(&app, argv[1]);

    SDL_Window *window = SDL_CreateWindow("Graph Viewer", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          app.window_width, app.window_height, SDL_WINDOW_RESIZABLE);
    if (!window) {
        fprintf(stderr, "Window could not be created! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        fprintf(stderr, "Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Event event;
    int quit = 0;
    Uint32 frameStart;
    int frameTime;

    while (!quit) {
        frameStart = SDL_GetTicks();

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                quit = 1;
            } else {
                handle_input(&event, &app);
            }
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        render_graph(renderer, &app);

        SDL_RenderPresent(renderer);

        frameTime = SDL_GetTicks() - frameStart;
        if (frameTime < FRAME_DELAY) {
            SDL_Delay(FRAME_DELAY - frameTime);
        }
    }

    cleanup_app(&app);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();

    return 0;
}
