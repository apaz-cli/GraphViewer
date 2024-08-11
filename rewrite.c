#include <SDL2/SDL.h>
#include <SDL2/SDL2_gfxPrimitives.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "cJSON.h"

#define FPS 60
#define FRAME_DELAY (1000 / FPS)
#define MAX_NODES 1000
#define MAX_LABEL_LENGTH 4096
#define RAND_XY_INIT_RANGE 500

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
    FILE *file = fopen(filename, "r");
    if (!file) {
        printf("Failed to open file: %s\n", filename);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *json_string = malloc(file_size + 1);
    fread(json_string, 1, file_size, file);
    json_string[file_size] = '\0';

    fclose(file);

    cJSON *json = cJSON_Parse(json_string);
    free(json_string);

    if (!json) {
        printf("Error parsing JSON\n");
        return NULL;
    }

    cJSON *nodes = cJSON_GetObjectItemCaseSensitive(json, "nodes");
    cJSON *edges = cJSON_GetObjectItemCaseSensitive(json, "edges");

    int node_count = cJSON_GetArraySize(nodes);
    int edge_count = cJSON_GetArraySize(edges);

    GraphData *graph = malloc(sizeof(GraphData));
    graph->node_count = node_count;
    graph->edge_count = edge_count;
    graph->edges = malloc(edge_count * sizeof(GraphEdge));

    for (int i = 0; i < node_count; i++) {
        cJSON *node = cJSON_GetArrayItem(nodes, i);
        graph->nodes[i].id = cJSON_GetObjectItemCaseSensitive(node, "id")->valueint;
        graph->nodes[i].position.x = (rand() % (2 * RAND_XY_INIT_RANGE)) - RAND_XY_INIT_RANGE;
        graph->nodes[i].position.y = (rand() % (2 * RAND_XY_INIT_RANGE)) - RAND_XY_INIT_RANGE;

        const char *label = cJSON_GetObjectItemCaseSensitive(node, "label")->valuestring;
        strncpy(graph->nodes[i].label, label, MAX_LABEL_LENGTH - 1);
        graph->nodes[i].label[MAX_LABEL_LENGTH - 1] = '\0';
    }

    for (int i = 0; i < edge_count; i++) {
        cJSON *edge = cJSON_GetArrayItem(edges, i);
        graph->edges[i].source = cJSON_GetObjectItemCaseSensitive(edge, "source")->valueint;
        graph->edges[i].target = cJSON_GetObjectItemCaseSensitive(edge, "target")->valueint;

        const char *label = cJSON_GetObjectItemCaseSensitive(edge, "label")->valuestring;
        strncpy(graph->edges[i].label, label, MAX_LABEL_LENGTH - 1);
        graph->edges[i].label[MAX_LABEL_LENGTH - 1] = '\0';
    }

    cJSON_Delete(json);
    return graph;
}

void free_graph(GraphData *graph) {
    if (graph) {
        free(graph->edges);
        free(graph);
    }
}

void render_graph(SDL_Renderer *renderer, AppState *app) {
    for (int i = 0; i < app->graph->edge_count; i++) {
        GraphNode *source = &app->graph->nodes[app->graph->edges[i].source];
        GraphNode *target = &app->graph->nodes[app->graph->edges[i].target];

        float x1 = (source->position.x + app->camera.position.x) * app->camera.zoom + app->window_width / 2;
        float y1 = (source->position.y + app->camera.position.y) * app->camera.zoom + app->window_height / 2;
        float x2 = (target->position.x + app->camera.position.x) * app->camera.zoom + app->window_width / 2;
        float y2 = (target->position.y + app->camera.position.y) * app->camera.zoom + app->window_height / 2;

        lineRGBA(renderer, x1, y1, x2, y2, 200, 200, 200, 255);
    }

    for (int i = 0; i < app->graph->node_count; i++) {
        int x = (app->graph->nodes[i].position.x + app->camera.position.x) * app->camera.zoom + app->window_width / 2;
        int y = (app->graph->nodes[i].position.y + app->camera.position.y) * app->camera.zoom + app->window_height / 2;

        filledCircleRGBA(renderer, x, y, 5 * app->camera.zoom, 0, 0, 255, 255);
    }
}

void handle_input(SDL_Event *event, AppState *app) {
    switch (event->type) {
    case SDL_MOUSEMOTION:
        if (event->motion.state & SDL_BUTTON_LMASK) {
            app->camera.position.x += event->motion.xrel / app->camera.zoom;
            app->camera.position.y += event->motion.yrel / app->camera.zoom;
        }
        break;

    case SDL_MOUSEWHEEL:
        app->camera.zoom *= (event->wheel.y > 0) ? 1.1f : 0.9f;
        break;

    case SDL_WINDOWEVENT:
        if (event->window.event == SDL_WINDOWEVENT_RESIZED) {
            app->window_width = event->window.data1;
            app->window_height = event->window.data2;
        }
        break;
    }
}

void initialize_app(AppState *app, const char *graph_file) {
    app->graph = load_graph(graph_file);
    if (!app->graph) {
        fprintf(stderr, "Failed to load graph\n");
        exit(1);
    }

    app->camera.zoom = 1.0f;
    app->camera.position = (Vec2f){0, 0};

    SDL_DisplayMode dm;
    if (SDL_GetCurrentDisplayMode(0, &dm) != 0) {
        fprintf(stderr, "SDL_GetCurrentDisplayMode failed: %s\n", SDL_GetError());
        exit(1);
    }

    app->window_width = dm.w / 2;
    app->window_height = dm.h / 2;

    app->font = TTF_OpenFont("lemon.ttf", 15);
    if (!app->font) {
        fprintf(stderr, "TTF_OpenFont: %s\n", TTF_GetError());
        exit(1);
    }
}

void cleanup_app(AppState *app) {
    free_graph(app->graph);
    TTF_CloseFont(app->font);
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
