#include <SDL2/SDL.h>
#include <SDL2/SDL2_gfxPrimitives.h>
#include <SDL2/SDL_audio.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL_pixels.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_video.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bell_wav.xxd"
#include "cJSON.h"

#define FPS 60
#define FRAME_DELAY (1000 / FPS)
#define MAX_NODES 1000
#define MAX_LABEL_LENGTH 64
#define SEARCH_BAR_HEIGHT 30
#define MAX_SEARCH_LENGTH 4096
#define RAND_XY_INIT_RANGE 500

#define LAYOUT_AREA_MULTIPLIER 1000
#define FORCE_ITERATIONS 100
#define FORCE_COOLING_FACTOR 1
#define FRUCHTERMAN_REINGOLD_INITIAL_TEMP 10.0
#define FRUCHTERMAN_REINGOLD_COOLING 0.80

#define SDL_BLACK                                                              \
  (SDL_Color) { 0, 0, 0, 255 }
#define SDL_WHITE                                                              \
  (SDL_Color) { 255, 255, 255, 255 }

typedef struct {
  float x, y;
} Vector2;

typedef struct {
  int id;
  int visible;
  Vector2 position;
  char *label;
} Node;

typedef struct {
  int source;
  int target;
  char *label;
} Edge;

typedef struct {
  Node *nodes;
  Edge *edges;
  int node_count;
  int edge_count;
} Graph;

enum SelectionMode { MODE_SINGLE, MODE_OUTGOING, MODE_INCOMING, MODE_COUNT };

const char *mode_names[] = {"Single", "Outgoing", "Incoming"};

Graph *graph;
float zoom = 1.0f;
Vector2 offset = {0, 0};
TTF_Font *font_15;
TTF_Font *font_45;
int WINDOW_WIDTH;
int WINDOW_HEIGHT;
char search_text[MAX_SEARCH_LENGTH] = "";
int search_cursor = 0;
int current_mode = MODE_SINGLE;
char *selected_nodes;

// Function to initialize a graph
static inline Graph *create_graph(int node_count, int edge_count) {
  Graph *graph = (Graph *)malloc(sizeof(Graph));
  graph->node_count = node_count;
  graph->edge_count = edge_count;
  graph->nodes = (Node *)malloc(node_count * sizeof(Node));
  graph->edges = (Edge *)malloc(edge_count * sizeof(Edge));
  return graph;
}

// Function to free a graph
static inline void free_graph(Graph *graph) {
  for (int i = 0; i < graph->node_count; i++) {
    free(graph->nodes[i].label);
  }
  for (int i = 0; i < graph->edge_count; i++) {
    free(graph->edges[i].label);
  }
  free(graph->nodes);
  free(graph->edges);
  free(graph);
}

// Function to load graph from JSON file
static inline Graph *load_graph(const char *filename) {
  FILE *file = fopen(filename, "r");
  if (!file) {
    printf("Failed to open file: %s\n", filename);
    exit(1);
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
    exit(1);
  }

  cJSON *nodes = cJSON_GetObjectItemCaseSensitive(json, "nodes");
  cJSON *edges = cJSON_GetObjectItemCaseSensitive(json, "edges");

  int node_count = cJSON_GetArraySize(nodes);
  int edge_count = cJSON_GetArraySize(edges);

  Graph *graph = create_graph(node_count, edge_count);

  for (int i = 0; i < node_count; i++) {
    cJSON *node = cJSON_GetArrayItem(nodes, i);
    graph->nodes[i].id = cJSON_GetObjectItemCaseSensitive(node, "id")->valueint;
    graph->nodes[i].position.x =
        (rand() % (2 * RAND_XY_INIT_RANGE)) - RAND_XY_INIT_RANGE;
    graph->nodes[i].position.y =
        (rand() % (2 * RAND_XY_INIT_RANGE)) - RAND_XY_INIT_RANGE;

    const char *label =
        cJSON_GetObjectItemCaseSensitive(node, "label")->valuestring;
    graph->nodes[i].label = strdup(label);

    graph->nodes[i].visible = 1;
  }

  for (int i = 0; i < edge_count; i++) {
    cJSON *edge = cJSON_GetArrayItem(edges, i);
    graph->edges[i].source =
        cJSON_GetObjectItemCaseSensitive(edge, "source")->valueint;
    graph->edges[i].target =
        cJSON_GetObjectItemCaseSensitive(edge, "target")->valueint;

    const char *label =
        cJSON_GetObjectItemCaseSensitive(edge, "label")->valuestring;
    graph->edges[i].label = strdup(label);
  }

  cJSON_Delete(json);
  return graph;
}

static inline int get_left_menu_width(int window_width) {
  return window_width * 0.15; // 15% of window width
}

static inline int get_right_menu_width(int window_width) {
  return window_width * 0.2; // 20% of window width
}

static inline int get_graph_width(int window_width) {
  return window_width - get_left_menu_width(window_width) -
         get_right_menu_width(window_width);
}

static inline void render_graph(SDL_Renderer *renderer) {
  int left_menu_width = get_left_menu_width(WINDOW_WIDTH);
  int right_menu_width = get_right_menu_width(WINDOW_WIDTH);
  int graph_width = get_graph_width(WINDOW_WIDTH);

  for (int i = 0; i < graph->edge_count; i++) {
    Node *source = &graph->nodes[graph->edges[i].source];
    Node *target = &graph->nodes[graph->edges[i].target];

    if (!source->visible || !target->visible)
      continue;

    int x1 = (source->position.x + offset.x) * zoom + left_menu_width +
             (float)graph_width / 2;
    int y1 = (source->position.y + offset.y) * zoom + (float)WINDOW_HEIGHT / 2;
    int x2 = (target->position.x + offset.x) * zoom + left_menu_width +
             (float)graph_width / 2;
    int y2 = (target->position.y + offset.y) * zoom + (float)WINDOW_HEIGHT / 2;

    lineRGBA(renderer, x1, y1, x2, y2, 200, 200, 200, 255);
  }

  for (int i = 0; i < graph->node_count; i++) {
    if (!graph->nodes[i].visible)
      continue;

    int x = (graph->nodes[i].position.x + offset.x) * zoom + left_menu_width +
            (float)graph_width / 2;
    int y = (graph->nodes[i].position.y + offset.y) * zoom +
            (float)WINDOW_HEIGHT / 2;

    if (selected_nodes && selected_nodes[i]) {
      filledCircleRGBA(renderer, x, y, 5 * zoom, 255, 0, 0, 255);
    } else {
      filledCircleRGBA(renderer, x, y, 5 * zoom, 0, 0, 255, 255);
    }
  }
}

static inline void render_label(SDL_Renderer *renderer, const char *text, int x,
                                int y, TTF_Font *font, SDL_Color color) {

  SDL_Surface *surface = TTF_RenderText_Solid(font, text, color);
  SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);

  SDL_Rect rect;
  rect.x = x;
  rect.y = y;
  SDL_QueryTexture(texture, NULL, NULL, &rect.w, &rect.h);

  // Calculate the maximum width available for the label
  int left_menu_width = get_left_menu_width(WINDOW_WIDTH);
  int graph_width = get_graph_width(WINDOW_WIDTH);
  int right_begin = left_menu_width + graph_width;
  int max_width = WINDOW_WIDTH - right_begin - 10;

  if (rect.w > max_width) {
    // If the label is too wide, clip it
    SDL_Rect src_rect = {0, 0, max_width, rect.h};
    rect.w = max_width;
    SDL_RenderCopy(renderer, texture, &src_rect, &rect);
  } else {
    // If the label fits, render it normally
    SDL_RenderCopy(renderer, texture, NULL, &rect);
  }

  SDL_FreeSurface(surface);
  SDL_DestroyTexture(texture);
}

static inline void render_right_menu(SDL_Renderer *renderer) {
  int right_menu_width = get_right_menu_width(WINDOW_WIDTH);

  // Render menu background
  SDL_Rect menu_rect = {WINDOW_WIDTH - right_menu_width, 0, right_menu_width,
                        WINDOW_HEIGHT};
  SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
  SDL_RenderFillRect(renderer, &menu_rect);

  // Render search bar
  SDL_Rect search_rect = {WINDOW_WIDTH - right_menu_width + 5, 5,
                          right_menu_width - 10, SEARCH_BAR_HEIGHT};
  SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
  SDL_RenderFillRect(renderer, &search_rect);

  // Render search text
  render_label(renderer, search_text, WINDOW_WIDTH - right_menu_width + 10, 10,
               font_15, SDL_BLACK);

  // Render node list
  int y_offset = SEARCH_BAR_HEIGHT + 10;
  for (int i = 0; i < graph->node_count; i++) {
    if (graph->nodes[i].visible) {
      char node_text[MAX_LABEL_LENGTH + 10];
      snprintf(node_text, sizeof(node_text), "%d: %s", graph->nodes[i].id,
               graph->nodes[i].label);
      render_label(renderer, node_text, WINDOW_WIDTH - right_menu_width + 10,
                   y_offset, font_15, SDL_WHITE);
      y_offset += 20;
    }
  }
}

static inline void filter_nodes() {
  if (strlen(search_text) == 0) {
    for (int i = 0; i < graph->node_count; i++) {
      graph->nodes[i].visible = 1;
    }
  } else {
    for (int i = 0; i < graph->node_count; i++) {
      char id_str[20];
      snprintf(id_str, sizeof(id_str), "%d", graph->nodes[i].id);

      graph->nodes[i].visible =
          (strstr(graph->nodes[i].label, search_text) != NULL) ||
          (strstr(id_str, search_text) != NULL);
    }
  }
}

static inline void toggle_mode() {
  current_mode = (current_mode + 1) % MODE_COUNT;
}

static inline void select_node(int node_id, int alloc) {
  if (alloc) {
    free(selected_nodes);
    selected_nodes = calloc(graph->node_count, sizeof(int));
    if (selected_nodes == NULL) {
      printf("Failed to allocate memory for selected nodes\n");
      exit(1);
    }
  }

  switch (current_mode) {
  case MODE_SINGLE:
    selected_nodes[node_id] = 1;
    break;
  case MODE_OUTGOING:
    selected_nodes[node_id] = 1;
    for (int i = 0; i < graph->edge_count; i++) {
      if (graph->edges[i].source == node_id) {
        selected_nodes[graph->edges[i].target] = 1;
      }
    }
    break;
  case MODE_INCOMING:
    selected_nodes[node_id] = 1;
    for (int i = 0; i < graph->edge_count; i++) {
      if (graph->edges[i].target == node_id) {
        selected_nodes[graph->edges[i].source] = 1;
      }
    }
    break;
  }
}

static inline void select_edge(int edge_id) {
  select_node(graph->edges[edge_id].source, 1);
  select_node(graph->edges[edge_id].target, 0);
}

static inline void render_left_menu(SDL_Renderer *renderer) {
  int left_menu_width = get_left_menu_width(WINDOW_WIDTH);

  // Render left menu background
  SDL_Rect menu_rect = {0, 0, left_menu_width, WINDOW_HEIGHT};
  SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
  SDL_RenderFillRect(renderer, &menu_rect);

  // Render mode button
  SDL_Rect button_rect = {10, 10, left_menu_width - 20, 30};
  SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
  SDL_RenderFillRect(renderer, &button_rect);

  char button_text[20];
  snprintf(button_text, sizeof(button_text), "Mode: %s",
           mode_names[current_mode]);
  render_label(renderer, button_text, 15, 15, font_15, SDL_WHITE);
}

static inline void apply_force_directed_layout(Graph *graph) {
  float width = sqrt(LAYOUT_AREA_MULTIPLIER * graph->node_count);
  float height = width;
  float area = width * height;
  float k = sqrt(area / graph->node_count);
  float t = width / 10; // Initial temperature

  size_t forces_size = graph->node_count * sizeof(float);
  float *forces_x = malloc(forces_size * 2);
  float *forces_y = forces_x + graph->node_count;

  for (int iter = 0; iter < FORCE_ITERATIONS; iter++) {
    // Reset forces
    for (int i = 0; i < graph->node_count; i++) {
      forces_x[i] = 0;
      forces_y[i] = 0;
    }

    // Calculate repulsive forces
    for (int i = 0; i < graph->node_count; i++) {
      for (int j = i + 1; j < graph->node_count; j++) {
        float dx = graph->nodes[i].position.x - graph->nodes[j].position.x;
        float dy = graph->nodes[i].position.y - graph->nodes[j].position.y;
        float distance = sqrt(dx * dx + dy * dy);
        if (distance == 0)
          distance = 0.01;

        float force = k * k / distance;
        forces_x[i] += (dx / distance) * force;
        forces_y[i] += (dy / distance) * force;
        forces_x[j] -= (dx / distance) * force;
        forces_y[j] -= (dy / distance) * force;
      }
    }

    // Calculate attractive forces
    for (int i = 0; i < graph->edge_count; i++) {
      int source = graph->edges[i].source;
      int target = graph->edges[i].target;
      float dx =
          graph->nodes[source].position.x - graph->nodes[target].position.x;
      float dy =
          graph->nodes[source].position.y - graph->nodes[target].position.y;
      float distance = sqrt(dx * dx + dy * dy);
      if (distance == 0)
        distance = 0.01;

      float force = (distance * distance) / k;
      forces_x[source] -= (dx / distance) * force;
      forces_y[source] -= (dy / distance) * force;
      forces_x[target] += (dx / distance) * force;
      forces_y[target] += (dy / distance) * force;
    }

    // Apply forces
    for (int i = 0; i < graph->node_count; i++) {
      float dx = forces_x[i];
      float dy = forces_y[i];
      float distance = sqrt(dx * dx + dy * dy);
      if (distance > 0) {
        float limiting_distance = fmin(distance, t);
        graph->nodes[i].position.x += dx / distance * limiting_distance;
        graph->nodes[i].position.y += dy / distance * limiting_distance;
      }

      // Keep nodes within bounds
      graph->nodes[i].position.x =
          fmax(0, fmin(width, graph->nodes[i].position.x));
      graph->nodes[i].position.y =
          fmax(0, fmin(height, graph->nodes[i].position.y));
    }

    // Cool temperature
    t *= FORCE_COOLING_FACTOR;
  }

  free(forces_x);
}

static inline void fruchterman_reingold_layout(Graph *graph) {
  int width = 1000;
  int height = 1000;
  float area = width * height * LAYOUT_AREA_MULTIPLIER;
  float k = sqrt(area / graph->node_count);
  float t = FRUCHTERMAN_REINGOLD_INITIAL_TEMP;

  Vector2 *displacement = malloc(graph->node_count * sizeof(Vector2));

  for (int iter = 0; iter < FORCE_ITERATIONS; iter++) {
    // Calculate repulsive forces
    for (int i = 0; i < graph->node_count; i++) {
      displacement[i].x = 0;
      displacement[i].y = 0;
      for (int j = 0; j < graph->node_count; j++) {
        if (i != j) {
          float dx = graph->nodes[i].position.x - graph->nodes[j].position.x;
          float dy = graph->nodes[i].position.y - graph->nodes[j].position.y;
          float distance = sqrt(dx * dx + dy * dy);
          if (distance > 0) {
            float repulsive_force = (k * k) / distance;
            displacement[i].x += dx / distance * repulsive_force;
            displacement[i].y += dy / distance * repulsive_force;
          }
        }
      }
    }

    // Calculate attractive forces
    for (int e = 0; e < graph->edge_count; e++) {
      int i = graph->edges[e].source;
      int j = graph->edges[e].target;
      float dx = graph->nodes[i].position.x - graph->nodes[j].position.x;
      float dy = graph->nodes[i].position.y - graph->nodes[j].position.y;
      float distance = sqrt(dx * dx + dy * dy);
      float attractive_force = distance * distance / k;
      if (distance > 0) {
        displacement[i].x -= dx / distance * attractive_force;
        displacement[i].y -= dy / distance * attractive_force;
        displacement[j].x += dx / distance * attractive_force;
        displacement[j].y += dy / distance * attractive_force;
      }
    }

    // Apply displacement with temperature
    for (int i = 0; i < graph->node_count; i++) {
      float disp_length = sqrt(displacement[i].x * displacement[i].x +
                               displacement[i].y * displacement[i].y);
      if (disp_length > 0) {
        float capped_disp_length = fmin(disp_length, t);
        graph->nodes[i].position.x +=
            displacement[i].x / disp_length * capped_disp_length;
        graph->nodes[i].position.y +=
            displacement[i].y / disp_length * capped_disp_length;
      }

      // Keep nodes within the frame
      graph->nodes[i].position.x =
          fmin((float)width / 2,
               fmax(-(float)width / 2, graph->nodes[i].position.x));
      graph->nodes[i].position.y =
          fmin((float)height / 2,
               fmax(-(float)height / 2, graph->nodes[i].position.y));
    }

    // Cool down
    t *= FRUCHTERMAN_REINGOLD_COOLING;
  }

  free(displacement);
}

/* MAIN */

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("Usage: %s <graph_file.json>\n", argv[0]);
    return 1;
  }

  graph = load_graph(argv[1]);
  selected_nodes = calloc(graph->node_count, sizeof(int));

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
    printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
    return 1;
  }

  if (TTF_Init() == -1) {
    printf("TTF_Init: %s\n", TTF_GetError());
    return 1;
  }

  // TODO: Load from .h file like bell_wav.xxd
  // Use SDL_IOFromDynamicMem and TTF_OpenFontIndexDPIIO.
  font_15 = TTF_OpenFont("lemon.ttf", 15);
  if (!font_15) {
    printf("TTF_OpenFont: %s\n", TTF_GetError());
    return 1;
  }

  font_45 = TTF_OpenFont("lemon.ttf", 45);
  if (!font_45) {
    printf("TTF_OpenFont: %s\n", TTF_GetError());
    return 1;
  }

  SDL_DisplayMode dm;
  if (SDL_GetCurrentDisplayMode(0, &dm) != 0) {
    printf("SDL_GetCurrentDisplayMode failed: %s\n", SDL_GetError());
    return 1;
  }

  WINDOW_WIDTH = dm.w / 2;
  WINDOW_HEIGHT = dm.h / 2;

  SDL_Window *window = SDL_CreateWindow("Graph Viewer", SDL_WINDOWPOS_CENTERED,
                                        SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH,
                                        WINDOW_HEIGHT, SDL_WINDOW_RESIZABLE);
  if (window == NULL) {
    printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
    return 1;
  }

  SDL_Renderer *renderer =
      SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
  if (renderer == NULL) {
    printf("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
    return 1;
  }

  // Initialize SDL_mixer
  if (Mix_OpenAudio(44100, AUDIO_S16SYS, 2, 512) < 0) {
    printf("SDL_mixer could not initialize! SDL_mixer Error: %s\n",
           Mix_GetError());
    return 1;
  }

  // Create channels
  if (Mix_AllocateChannels(4) < 0) {
    printf("SDL_mixer could not allocate channels! SDL_mixer Error: %s\n",
           Mix_GetError());
    return 1;
  }

  // Load sound from memory
  SDL_RWops *rw = SDL_RWFromMem(bell_wav, bell_wav_len);
  Mix_Chunk *sound = Mix_LoadWAV_RW(rw, 1);
  if (!sound) {
    printf("Failed to load sound! SDL_mixer Error: %s\n", Mix_GetError());
    return 1;
  }

  // Play the sound
  /*
  int channel = Mix_PlayChannel(-1, sound, 0);
  if (channel == -1) {
    printf("Failed to play sound! SDL_mixer Error: %s\n", Mix_GetError());
    return 1;
  }
  */

  SDL_Event event;
  int quit = 0;
  Uint32 frameStart;
  int frameTime;
  int mouseX, mouseY;

  while (!quit) {
    frameStart = SDL_GetTicks();

    SDL_GetRendererOutputSize(renderer, &WINDOW_WIDTH, &WINDOW_HEIGHT);

    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_MOUSEMOTION) {
        if (event.motion.state & SDL_BUTTON_LMASK) {
          offset.x += event.motion.xrel / zoom;
          offset.y += event.motion.yrel / zoom;
        }
        mouseX = event.motion.x;
        mouseY = event.motion.y;
      } else if (event.type == SDL_MOUSEWHEEL) {
        zoom *= (event.wheel.y > 0) ? 1.1f : 0.9f;
      } else if (event.type == SDL_QUIT) {
        quit = 1;
      } else if (event.type == SDL_WINDOWEVENT) {
        if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
          WINDOW_WIDTH = event.window.data1;
          WINDOW_HEIGHT = event.window.data2;
        }
      } else if (event.type == SDL_TEXTINPUT) {
        if (strlen(search_text) < MAX_SEARCH_LENGTH - 1) {
          strcat(search_text, event.text.text);
          filter_nodes();
        }
      } else if (event.type == SDL_KEYDOWN) {
        if (event.key.keysym.sym == SDLK_BACKSPACE && strlen(search_text) > 0) {
          search_text[strlen(search_text) - 1] = '\0';
          filter_nodes();
        }
      } else if (event.type == SDL_MOUSEBUTTONDOWN) {
        if (event.button.button == SDL_BUTTON_LEFT) {
          int x = event.button.x;
          int y = event.button.y;
          int left_menu_width = get_left_menu_width(WINDOW_WIDTH);

          // Check if the mode button was clicked
          if (x >= 10 && x <= left_menu_width - 10 && y >= 10 && y <= 40) {
            toggle_mode();
          } else {
            // Check for node or edge clicks
            int clicked_node = -1;
            int clicked_edge = -1;
            int graph_width = get_graph_width(WINDOW_WIDTH);

            for (int i = 0; i < graph->node_count; i++) {
              if (!graph->nodes[i].visible)
                continue;

              int nx = (graph->nodes[i].position.x + offset.x) * zoom +
                       left_menu_width + (float)graph_width / 2;
              int ny = (graph->nodes[i].position.y + offset.y) * zoom +
                       (float)WINDOW_HEIGHT / 2;

              if (sqrt(pow(x - nx, 2) + pow(y - ny, 2)) <= 5 * zoom) {
                clicked_node = i;
                break;
              }
            }

            if (clicked_node == -1) {
              for (int i = 0; i < graph->edge_count; i++) {
                Node *source = &graph->nodes[graph->edges[i].source];
                Node *target = &graph->nodes[graph->edges[i].target];

                if (!source->visible || !target->visible)
                  continue;

                int x1 = (source->position.x + offset.x) * zoom +
                         left_menu_width + (float)graph_width / 2;
                int y1 = (source->position.y + offset.y) * zoom +
                         (float)WINDOW_HEIGHT / 2;
                int x2 = (target->position.x + offset.x) * zoom +
                         left_menu_width + (float)graph_width / 2;
                int y2 = (target->position.y + offset.y) * zoom +
                         (float)WINDOW_HEIGHT / 2;

                float d =
                    abs((y2 - y1) * x - (x2 - x1) * y + x2 * y1 - y2 * x1) /
                    sqrt(pow(y2 - y1, 2) + pow(x2 - x1, 2));
                if (d <= 5 && x >= fmin(x1, x2) - 5 && x <= fmax(x1, x2) + 5 &&
                    y >= fmin(y1, y2) - 5 && y <= fmax(y1, y2) + 5) {
                  clicked_edge = i;
                  break;
                }
              }
            }

            if (clicked_node != -1) {
              select_node(clicked_node, 1);
            } else if (clicked_edge != -1) {
              select_edge(clicked_edge);
            }
          }
        }
      }
    }

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    render_graph(renderer);
    render_right_menu(renderer);
    render_left_menu(renderer);

    int hoveredNode = -1;
    int left_menu_width = get_left_menu_width(WINDOW_WIDTH);
    int graph_width = get_graph_width(WINDOW_WIDTH);

    // Check for node hover
    for (int i = 0; i < graph->node_count; i++) {
      if (!graph->nodes[i].visible)
        continue;

      int x = (graph->nodes[i].position.x + offset.x) * zoom + left_menu_width +
              (float)graph_width / 2;
      int y = (graph->nodes[i].position.y + offset.y) * zoom +
              (float)WINDOW_HEIGHT / 2;

      if (sqrt(pow(mouseX - x, 2) + pow(mouseY - y, 2)) <= 5 * zoom) {
        // Highlight hovered node in yellow
        filledCircleRGBA(renderer, x, y, 5 * zoom, 255, 255, 0, 255);
        SDL_RenderDrawPoint(renderer, x, y);
        hoveredNode = i;
        break;
      }
    }

    if (hoveredNode != -1) {
      render_label(renderer, graph->nodes[hoveredNode].label, mouseX,
                   mouseY - 20, font_45, SDL_WHITE);
    } else {
      // Check for edge hover only if not hovering over a node
      for (int i = 0; i < graph->edge_count; i++) {
        Node *source = &graph->nodes[graph->edges[i].source];
        Node *target = &graph->nodes[graph->edges[i].target];

        if (!source->visible || !target->visible)
          continue;

        int x1 = (source->position.x + offset.x) * zoom + left_menu_width +
                 (float)graph_width / 2;
        int y1 =
            (source->position.y + offset.y) * zoom + (float)WINDOW_HEIGHT / 2;
        int x2 = (target->position.x + offset.x) * zoom + left_menu_width +
                 (float)graph_width / 2;
        int y2 =
            (target->position.y + offset.y) * zoom + (float)WINDOW_HEIGHT / 2;

        float d =
            abs((y2 - y1) * mouseX - (x2 - x1) * mouseY + x2 * y1 - y2 * x1) /
            sqrt(pow(y2 - y1, 2) + pow(x2 - x1, 2));
        if (d <= 5 && mouseX >= fmin(x1, x2) - 5 &&
            mouseX <= fmax(x1, x2) + 5 && mouseY >= fmin(y1, y2) - 5 &&
            mouseY <= fmax(y1, y2) + 5) {
          render_label(renderer, graph->edges[i].label, mouseX, mouseY - 20,
                       font_45, SDL_WHITE);
          break;
        }
      }
    }

    SDL_RenderPresent(renderer);

    frameTime = SDL_GetTicks() - frameStart;
    if (frameTime < FRAME_DELAY) {
      SDL_Delay(FRAME_DELAY - frameTime);
    }
  }

  Mix_FreeChunk(sound);
  Mix_CloseAudio();
  TTF_CloseFont(font_15);
  TTF_CloseFont(font_45);
  TTF_Quit();
  free_graph(graph);
  free(selected_nodes);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}