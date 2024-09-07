#ifdef PYTHON_MODULE
#define PY_SSIZE_T_CLEAN
#include <Python.h>
#endif

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
#include "yyjson.h"
#include "lemon_ttf.xxd"

// Debug macro
#define DBG 1
#define DEBUG_PRINT(...) do { if (DBG) fprintf(stderr, "[DEBUG] %s:%d: ", __func__, __LINE__); if (DBG) fprintf(stderr, __VA_ARGS__); } while (0)


// Configuration constants
#define FPS 60
#define FRAME_DELAY (1000 / FPS)
#define MAX_NODES 1000
#define MAX_LABEL_LENGTH 4096
#define SEARCH_BAR_HEIGHT 30
#define MAX_SEARCH_LENGTH 4096
#define RAND_XY_INIT_RANGE 500
#define TOP_BAR_HEIGHT 40
#define OPEN_BUTTON_WIDTH 100

#define LAYOUT_AREA_MULTIPLIER 1000
#define FORCE_ITERATIONS 100
#define FORCE_COOLING_FACTOR 1
#define FRUCHTERMAN_REINGOLD_INITIAL_TEMP 10.0
#define FRUCHTERMAN_REINGOLD_COOLING 0.80

// Color definitions
#define COLOR_MENU_ITEM_1                                                      \
  (SDL_Color) { 55, 55, 55, 255 }
#define COLOR_MENU_ITEM_2                                                      \
  (SDL_Color) { 70, 70, 70, 255 }
#define COLOR_BLACK                                                            \
  (SDL_Color) { 0, 0, 0, 255 }
#define COLOR_WHITE                                                            \
  (SDL_Color) { 255, 255, 255, 255 }

typedef struct {
  float x, y;
} Vec2f;

typedef struct {
  int id;
  int visible;
  Vec2f position;
  char *label;
} GraphNode;

typedef struct {
  int source;
  int target;
  char *label;
} GraphEdge;

typedef struct {
  GraphNode *nodes;
  GraphEdge *edges;
  int node_count;
  int edge_count;
  yyjson_doc *doc;
} GraphData;

typedef enum {
  SELECT_SINGLE,
  SELECT_REFERENCES,
  SELECT_REFERENCED_BY,
  SELECT_REFERENCES_RECURSIVE,
  SELECT_REFERENCED_BY_RECURSIVE,
  SELECT_MODE_COUNT
} NodeSelectionMode;

typedef struct {
  float zoom;
  Vec2f position;
} Camera;

typedef struct {
  char text[MAX_SEARCH_LENGTH];
  int cursor_position;
} SearchBar;

typedef struct {
  GraphData *graph;
  Camera camera;
  TTF_Font *font_small;
  TTF_Font *font_medium;
  TTF_Font *font_large;
  int window_width;
  int window_height;
  SearchBar search_bar;
  NodeSelectionMode selection_mode;
  int *selected_nodes;
  int right_scroll_position;
  int right_menu_hovered_item;
  int left_scroll_position;
  int left_menu_hovered_item;
  int visible_nodes_count;
  int nodes_per_page;
  Vec2f mouse_position;
  int filter_referenced;
  int hovered_node;
  int hovered_edge;
  int is_dragging_left_scrollbar;
  int is_dragging_right_scrollbar;
  int drag_start_y;
  int drag_start_scroll;
  SDL_Rect open_button;
} AppState;

// Function declarations
static inline GraphData *create_graph(int node_count, int edge_count);
static inline void free_graph(GraphData *graph);
static inline GraphData *load_graph(const char *filename);
static inline void apply_force_directed_layout(GraphData *graph);
static inline void apply_fruchterman_reingold_layout(GraphData *graph);
static inline void update_node_visibility(AppState *app);
static inline void cycle_selection_mode(AppState *app);
static inline void update_open_button_position(AppState *app);
static inline char *handle_open_button_click(void);
static inline void set_node_selection(AppState *app, int node_id);
static inline void set_edge_selection(AppState *app, int edge_id);
static inline void render_top_bar(SDL_Renderer *renderer, AppState *app);
static inline void render_graph(SDL_Renderer *renderer, AppState *app);
static inline void render_left_menu(SDL_Renderer *renderer, AppState *app);
static inline void render_right_menu(SDL_Renderer *renderer, AppState *app);
static inline void handle_input(SDL_Event *event, AppState *app);
static inline void initialize_app(AppState *app, const char *graph_file);
static inline void cleanup_app(AppState *app);
static inline void reinitialize_app(AppState *app, const char *graph_file);
static inline int run_graph_viewer(const char *graph_file);

// Utility functions
static inline int get_left_menu_width(int window_width) {
  return window_width * 0.15;
}

static inline int get_right_menu_width(int window_width) {
  return window_width * 0.2;
}

static inline int get_graph_width(int window_width) {
  return window_width - get_left_menu_width(window_width) -
         get_right_menu_width(window_width);
}

// Implementation of core functions
static inline GraphData *create_graph(int node_count, int edge_count) {
  GraphData *graph = (GraphData *)malloc(sizeof(GraphData));
  if (!graph) {
    fprintf(stderr, "Failed to allocate memory for graph\n");
    return NULL;
  }
  graph->node_count = node_count;
  graph->edge_count = edge_count;
  graph->nodes = (GraphNode *)calloc(node_count, sizeof(GraphNode));
  graph->edges = (GraphEdge *)calloc(edge_count, sizeof(GraphEdge));
  if (!graph->nodes || !graph->edges) {
    fprintf(stderr, "Failed to allocate memory for nodes or edges\n");
    free(graph);
    return NULL;
  }
  return graph;
}

static inline void free_graph(GraphData *graph) {
  if (!graph)
    return;
  if (graph->doc) {
    yyjson_doc_free(graph->doc);
  }
  free(graph->nodes);
  free(graph->edges);
  free(graph);
}

static inline GraphData *load_graph(const char *filename) {
  DEBUG_PRINT("Loading graph from file: %s\n", filename);

  // Read the entire file
  yyjson_read_flag flg = 0;
  yyjson_read_err err;
  yyjson_doc *doc = yyjson_read_file(filename, flg, NULL, &err);
  if (!doc) {
    DEBUG_PRINT("Error reading JSON file: %s at position %zu\n", err.msg, err.pos);
    return create_graph(0, 0);
  }

  // Get the root object
  yyjson_val *root = yyjson_doc_get_root(doc);
  if (!yyjson_is_obj(root)) {
    DEBUG_PRINT("Root is not an object\n");
    yyjson_doc_free(doc);
    return create_graph(0, 0);
  }

  // Get nodes and edges arrays
  yyjson_val *nodes = yyjson_obj_get(root, "nodes");
  yyjson_val *edges = yyjson_obj_get(root, "edges");

  if (!yyjson_is_arr(nodes) || !yyjson_is_arr(edges)) {
    DEBUG_PRINT("Nodes or edges is not an array\n");
    yyjson_doc_free(doc);
    return create_graph(0, 0);
  }

  size_t node_count = yyjson_arr_size(nodes);
  size_t edge_count = yyjson_arr_size(edges);
  DEBUG_PRINT("Node count: %zu, Edge count: %zu\n", node_count, edge_count);

  GraphData *graph = create_graph(node_count, edge_count);
  if (!graph) {
    DEBUG_PRINT("Failed to create graph\n");
    yyjson_doc_free(doc);
    return create_graph(0, 0);
  }

  graph->doc = doc;

  DEBUG_PRINT("Populating nodes\n");
  yyjson_arr_iter node_iter;
  yyjson_arr_iter_init(nodes, &node_iter);
  yyjson_val *node;
  size_t idx = 0;
  while ((node = yyjson_arr_iter_next(&node_iter))) {
    yyjson_val *id = yyjson_obj_get(node, "id");
    yyjson_val *label = yyjson_obj_get(node, "label");
    if (!yyjson_is_int(id) || !yyjson_is_str(label)) {
      DEBUG_PRINT("Invalid node data\n");
      continue;
    }
    graph->nodes[idx].id = yyjson_get_int(id);
    graph->nodes[idx].position.x = (rand() % (2 * RAND_XY_INIT_RANGE)) - RAND_XY_INIT_RANGE;
    graph->nodes[idx].position.y = (rand() % (2 * RAND_XY_INIT_RANGE)) - RAND_XY_INIT_RANGE;
    graph->nodes[idx].label = yyjson_get_str(label);
    graph->nodes[idx].visible = 1;
    DEBUG_PRINT("Node %zu: id=%d, label=%s\n", idx, graph->nodes[idx].id, graph->nodes[idx].label);
    idx++;
  }

  DEBUG_PRINT("Populating edges\n");
  yyjson_arr_iter edge_iter;
  yyjson_arr_iter_init(edges, &edge_iter);
  yyjson_val *edge;
  idx = 0;
  while ((edge = yyjson_arr_iter_next(&edge_iter))) {
    yyjson_val *source = yyjson_obj_get(edge, "source");
    yyjson_val *target = yyjson_obj_get(edge, "target");
    yyjson_val *label = yyjson_obj_get(edge, "label");
    if (!yyjson_is_int(source) || !yyjson_is_int(target) || !yyjson_is_str(label)) {
      DEBUG_PRINT("Invalid edge data\n");
      continue;
    }
    graph->edges[idx].source = yyjson_get_int(source);
    graph->edges[idx].target = yyjson_get_int(target);
    graph->edges[idx].label = yyjson_get_str(label);
    DEBUG_PRINT("Edge %zu: source=%d, target=%d, label=%s\n", idx, graph->edges[idx].source, graph->edges[idx].target, graph->edges[idx].label);
    idx++;
  }

  DEBUG_PRINT("Graph loading complete\n");
  return graph;
}

static inline void apply_force_directed_layout(GraphData *graph) {
  float width = sqrt(LAYOUT_AREA_MULTIPLIER * graph->node_count);
  float height = width;
  float area = width * height;
  float k = sqrt(area / graph->node_count);
  float t = width / 10;

  Vec2f *forces = calloc(graph->node_count, sizeof(Vec2f));
  if (!forces) {
    fprintf(stderr, "Failed to allocate memory for force calculation\n");
    return;
  }

  for (int iter = 0; iter < FORCE_ITERATIONS; iter++) {
    memset(forces, 0, graph->node_count * sizeof(Vec2f));

    // Calculate repulsive forces
    for (int i = 0; i < graph->node_count; i++) {
      for (int j = i + 1; j < graph->node_count; j++) {
        float dx = graph->nodes[i].position.x - graph->nodes[j].position.x;
        float dy = graph->nodes[i].position.y - graph->nodes[j].position.y;
        float distance = sqrt(dx * dx + dy * dy);
        if (distance == 0)
          distance = 0.01;

        float force = k * k / distance;
        float fx = dx / distance * force;
        float fy = dy / distance * force;

        forces[i].x += fx;
        forces[i].y += fy;
        forces[j].x -= fx;
        forces[j].y -= fy;
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
      float fx = dx / distance * force;
      float fy = dy / distance * force;

      forces[source].x -= fx;
      forces[source].y -= fy;
      forces[target].x += fx;
      forces[target].y += fy;
    }

    // Apply forces
    for (int i = 0; i < graph->node_count; i++) {
      float dx = forces[i].x;
      float dy = forces[i].y;
      float distance = sqrt(dx * dx + dy * dy);
      if (distance > 0) {
        float limiting_distance = fmin(distance, t);
        graph->nodes[i].position.x += dx / distance * limiting_distance;
        graph->nodes[i].position.y += dy / distance * limiting_distance;
      }

      graph->nodes[i].position.x =
          fmax(0, fmin(width, graph->nodes[i].position.x));
      graph->nodes[i].position.y =
          fmax(0, fmin(height, graph->nodes[i].position.y));
    }

    t *= FORCE_COOLING_FACTOR;
  }

  free(forces);
}

static inline void apply_fruchterman_reingold_layout(GraphData *graph) {
  int width = 1000;
  int height = 1000;
  float area = width * height * LAYOUT_AREA_MULTIPLIER;
  float k = sqrt(area / graph->node_count);
  float t = FRUCHTERMAN_REINGOLD_INITIAL_TEMP;

  Vec2f *displacement = calloc(graph->node_count, sizeof(Vec2f));
  if (!displacement) {
    fprintf(stderr, "Failed to allocate memory for displacement calculation\n");
    return;
  }

  for (int iter = 0; iter < FORCE_ITERATIONS; iter++) {
    memset(displacement, 0, graph->node_count * sizeof(Vec2f));

    // Calculate repulsive forces
    for (int i = 0; i < graph->node_count; i++) {
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

    // Apply displacement
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

      graph->nodes[i].position.x =
          fmin((float)width / 2,
               fmax(-(float)width / 2, graph->nodes[i].position.x));
      graph->nodes[i].position.y =
          fmin((float)height / 2,
               fmax(-(float)height / 2, graph->nodes[i].position.y));
    }

    t *= FRUCHTERMAN_REINGOLD_COOLING;
  }

  free(displacement);
}

static inline void update_node_visibility(AppState *app) {
  app->visible_nodes_count = 0;
  for (int i = 0; i < app->graph->node_count; i++) {
    if (strlen(app->search_bar.text) == 0) {
      app->graph->nodes[i].visible =
          !app->filter_referenced || app->selected_nodes[i];
    } else {
      char id_str[20];
      snprintf(id_str, sizeof(id_str), "%d", app->graph->nodes[i].id);
      app->graph->nodes[i].visible =
          ((strstr(app->graph->nodes[i].label, app->search_bar.text) != NULL) ||
           (strstr(id_str, app->search_bar.text) != NULL)) &&
          (!app->filter_referenced || app->selected_nodes[i]);
    }
    if (app->graph->nodes[i].visible) {
      app->visible_nodes_count++;
    }
  }
}

static inline void cycle_selection_mode(AppState *app) {
  app->selection_mode = (app->selection_mode + 1) % SELECT_MODE_COUNT;
}

static inline void select_references_recursive(AppState *app, int node_id) {
  for (int i = 0; i < app->graph->edge_count; i++) {
    if (app->graph->edges[i].source == node_id &&
        !app->selected_nodes[app->graph->edges[i].target]) {
      app->selected_nodes[app->graph->edges[i].target] = 1;
      select_references_recursive(app, app->graph->edges[i].target);
    }
  }
}

static inline void select_referenced_by_recursive(AppState *app, int node_id) {
  for (int i = 0; i < app->graph->edge_count; i++) {
    if (app->graph->edges[i].target == node_id &&
        !app->selected_nodes[app->graph->edges[i].source]) {
      app->selected_nodes[app->graph->edges[i].source] = 1;
      select_referenced_by_recursive(app, app->graph->edges[i].source);
    }
  }
}

static inline void set_node_selection(AppState *app, int node_id) {
  memset(app->selected_nodes, 0, app->graph->node_count * sizeof(int));

  switch (app->selection_mode) {
  case SELECT_SINGLE:
    app->selected_nodes[node_id] = 1;
    break;
  case SELECT_REFERENCES:
    app->selected_nodes[node_id] = 1;
    for (int i = 0; i < app->graph->edge_count; i++) {
      if (app->graph->edges[i].source == node_id) {
        app->selected_nodes[app->graph->edges[i].target] = 1;
      }
    }
    break;
  case SELECT_REFERENCED_BY:
    app->selected_nodes[node_id] = 1;
    for (int i = 0; i < app->graph->edge_count; i++) {
      if (app->graph->edges[i].target == node_id) {
        app->selected_nodes[app->graph->edges[i].source] = 1;
      }
    }
    break;
  case SELECT_REFERENCES_RECURSIVE:
    app->selected_nodes[node_id] = 1;
    select_references_recursive(app, node_id);
    break;
  case SELECT_REFERENCED_BY_RECURSIVE:
    app->selected_nodes[node_id] = 1;
    select_referenced_by_recursive(app, node_id);
    break;
  case SELECT_MODE_COUNT:
    printf("This should never happen.\n");
    exit(1);
  }
  update_node_visibility(app);
  app->left_scroll_position = 0; // Reset left menu scroll position
}

static inline void set_edge_selection(AppState *app, int edge_id) {
  memset(app->selected_nodes, 0, app->graph->node_count * sizeof(int));
  app->selected_nodes[app->graph->edges[edge_id].source] = 1;
  app->selected_nodes[app->graph->edges[edge_id].target] = 1;
  update_node_visibility(app);
  app->left_scroll_position = 0; // Reset left menu scroll position
}

static inline void render_label_background(SDL_Renderer *renderer, int x, int y,
                                           int width, int height) {
  SDL_Rect bg_rect = {x - 2, y - 2, width + 4, height + 4};
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 200);
  SDL_RenderFillRect(renderer, &bg_rect);
}

static inline void render_hover_label(SDL_Renderer *renderer, AppState *app,
                                      const char *label, int x, int y) {
  if (label == NULL || strlen(label) == 0) {
    return;
  }

  int max_width = 300;
  SDL_Surface *text_surface = TTF_RenderText_Blended_Wrapped(
      app->font_small, label, COLOR_WHITE, max_width);
  if (!text_surface) {
    fprintf(stderr, "Failed to render text: %s\n", TTF_GetError());
    return;
  }

  SDL_Texture *text_texture =
      SDL_CreateTextureFromSurface(renderer, text_surface);
  if (!text_texture) {
    fprintf(stderr, "Failed to create texture: %s\n", SDL_GetError());
    SDL_FreeSurface(text_surface);
    return;
  }

  int text_width = text_surface->w;
  int text_height = text_surface->h;

  // Use cursor position (stored in app->mouse_position)
  x = app->mouse_position.x;
  y = app->mouse_position.y;

  // Adjust position to keep label on screen
  if (x + text_width > app->window_width) {
    x = app->window_width - text_width - 5;
  }
  if (y + text_height > app->window_height) {
    y = app->window_height - text_height - 5;
  }

  render_label_background(renderer, x, y, text_width, text_height);

  SDL_Rect text_rect = {x, y, text_width, text_height};
  SDL_RenderCopy(renderer, text_texture, NULL, &text_rect);

  SDL_FreeSurface(text_surface);
  SDL_DestroyTexture(text_texture);
}

static inline void render_graph(SDL_Renderer *renderer, AppState *app) {
  int left_menu_width = get_left_menu_width(app->window_width);
  int graph_width = get_graph_width(app->window_width);

  // First pass: Render non-highlighted edges
  for (int i = 0; i < app->graph->edge_count; i++) {
    GraphNode *source = &app->graph->nodes[app->graph->edges[i].source];
    GraphNode *target = &app->graph->nodes[app->graph->edges[i].target];

    if (!source->visible || !target->visible)
      continue;

    int both_selected = app->selected_nodes[app->graph->edges[i].source] &&
                        app->selected_nodes[app->graph->edges[i].target];

    if (both_selected)
      continue; // Skip highlighted edges in this pass

    float x1 =
        (source->position.x + app->camera.position.x) * app->camera.zoom +
        left_menu_width + (float)graph_width / 2;
    float y1 =
        (source->position.y + app->camera.position.y) * app->camera.zoom +
        (float)app->window_height / 2;
    float x2 =
        (target->position.x + app->camera.position.x) * app->camera.zoom +
        left_menu_width + (float)graph_width / 2;
    float y2 =
        (target->position.y + app->camera.position.y) * app->camera.zoom +
        (float)app->window_height / 2;

    float angle = atan2(y2 - y1, x2 - x1);
    float circle_radius = 5 * app->camera.zoom;
    float x2_adj = x2 - circle_radius * cos(angle);
    float y2_adj = y2 - circle_radius * sin(angle);

    lineRGBA(renderer, x1, y1, x2_adj, y2_adj, 200, 200, 200, 255);

    float arrow_size = 10 * app->camera.zoom;
    float x3 = x2_adj - arrow_size * cos(angle - M_PI / 12);
    float y3 = y2_adj - arrow_size * sin(angle - M_PI / 12);
    float x4 = x2_adj - arrow_size * cos(angle + M_PI / 12);
    float y4 = y2_adj - arrow_size * sin(angle + M_PI / 12);

    filledTrigonRGBA(renderer, x2_adj, y2_adj, x3, y3, x4, y4, 200, 200, 200,
                     255);
  }

  // Second pass: Render non-highlighted nodes
  for (int i = 0; i < app->graph->node_count; i++) {
    if (!app->graph->nodes[i].visible || app->selected_nodes[i])
      continue;

    int x = (app->graph->nodes[i].position.x + app->camera.position.x) *
                app->camera.zoom +
            left_menu_width + (float)graph_width / 2;
    int y = (app->graph->nodes[i].position.y + app->camera.position.y) *
                app->camera.zoom +
            (float)app->window_height / 2;

    filledCircleRGBA(renderer, x, y, 5 * app->camera.zoom, 0, 0, 255, 255);
  }

  // Third pass: Render highlighted edges
  for (int i = 0; i < app->graph->edge_count; i++) {
    GraphNode *source = &app->graph->nodes[app->graph->edges[i].source];
    GraphNode *target = &app->graph->nodes[app->graph->edges[i].target];

    if (!source->visible || !target->visible)
      continue;

    int both_selected = app->selected_nodes[app->graph->edges[i].source] &&
                        app->selected_nodes[app->graph->edges[i].target];

    if (!both_selected)
      continue; // Skip non-highlighted edges in this pass

    float x1 =
        (source->position.x + app->camera.position.x) * app->camera.zoom +
        left_menu_width + (float)graph_width / 2;
    float y1 =
        (source->position.y + app->camera.position.y) * app->camera.zoom +
        (float)app->window_height / 2;
    float x2 =
        (target->position.x + app->camera.position.x) * app->camera.zoom +
        left_menu_width + (float)graph_width / 2;
    float y2 =
        (target->position.y + app->camera.position.y) * app->camera.zoom +
        (float)app->window_height / 2;

    float angle = atan2(y2 - y1, x2 - x1);
    float circle_radius = 5 * app->camera.zoom;
    float x2_adj = x2 - circle_radius * cos(angle);
    float y2_adj = y2 - circle_radius * sin(angle);

    lineRGBA(renderer, x1, y1, x2_adj, y2_adj, 255, 0, 0, 255);

    float arrow_size = 10 * app->camera.zoom;
    float x3 = x2_adj - arrow_size * cos(angle - M_PI / 12);
    float y3 = y2_adj - arrow_size * sin(angle - M_PI / 12);
    float x4 = x2_adj - arrow_size * cos(angle + M_PI / 12);
    float y4 = y2_adj - arrow_size * sin(angle + M_PI / 12);

    filledTrigonRGBA(renderer, x2_adj, y2_adj, x3, y3, x4, y4, 255, 0, 0, 255);
  }

  // Fourth pass: Render highlighted nodes
  for (int i = 0; i < app->graph->node_count; i++) {
    if (!app->graph->nodes[i].visible || !app->selected_nodes[i])
      continue;

    int x = (app->graph->nodes[i].position.x + app->camera.position.x) *
                app->camera.zoom +
            left_menu_width + (float)graph_width / 2;
    int y = (app->graph->nodes[i].position.y + app->camera.position.y) *
                app->camera.zoom +
            (float)app->window_height / 2;

    filledCircleRGBA(renderer, x, y, 5 * app->camera.zoom, 255, 0, 0, 255);
  }

  // Final pass: Render hover labels
  if (app->hovered_node != -1) {
    int x = (app->graph->nodes[app->hovered_node].position.x +
             app->camera.position.x) *
                app->camera.zoom +
            left_menu_width + (float)graph_width / 2;
    int y = (app->graph->nodes[app->hovered_node].position.y +
             app->camera.position.y) *
                app->camera.zoom +
            (float)app->window_height / 2;
    render_hover_label(renderer, app,
                       app->graph->nodes[app->hovered_node].label, x + 10,
                       y - 20);
  } else if (app->hovered_edge != -1) {
    GraphNode *source =
        &app->graph->nodes[app->graph->edges[app->hovered_edge].source];
    GraphNode *target =
        &app->graph->nodes[app->graph->edges[app->hovered_edge].target];

    float x1 =
        (source->position.x + app->camera.position.x) * app->camera.zoom +
        left_menu_width + (float)graph_width / 2;
    float y1 =
        (source->position.y + app->camera.position.y) * app->camera.zoom +
        (float)app->window_height / 2;
    float x2 =
        (target->position.x + app->camera.position.x) * app->camera.zoom +
        left_menu_width + (float)graph_width / 2;
    float y2 =
        (target->position.y + app->camera.position.y) * app->camera.zoom +
        (float)app->window_height / 2;

    int label_x = (x1 + x2) / 2;
    int label_y = (y1 + y2) / 2;
    render_hover_label(renderer, app,
                       app->graph->edges[app->hovered_edge].label, label_x,
                       label_y);
  }
}

static inline void render_label(SDL_Renderer *renderer, const char *text, int x,
                                int y, TTF_Font *font, SDL_Color color,
                                int max_width) {
  SDL_Surface *surface = TTF_RenderText_Solid(font, text, color);
  SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);

  SDL_Rect rect;
  rect.x = x;
  rect.y = y;
  SDL_QueryTexture(texture, NULL, NULL, &rect.w, &rect.h);

  if (rect.w > max_width) {
    SDL_Rect src_rect = {0, 0, max_width, rect.h};
    rect.w = max_width;
    SDL_RenderCopy(renderer, texture, &src_rect, &rect);
  } else {
    SDL_RenderCopy(renderer, texture, NULL, &rect);
  }

  SDL_FreeSurface(surface);
  SDL_DestroyTexture(texture);
}

static inline SDL_Rect render_scrollbar(SDL_Renderer *renderer, int x, int y,
                                        int width, int height, int total_items,
                                        int visible_items,
                                        int scroll_position) {
  int content_height = total_items * 20; // Assuming each item is 20 pixels high
  float visible_ratio = (float)height / content_height;
  int scrollbar_height = (int)(visible_ratio * height);
  scrollbar_height = (scrollbar_height < 20)
                         ? 20
                         : scrollbar_height; // Minimum scrollbar height

  int max_scroll = content_height - height;
  float scroll_ratio =
      (max_scroll > 0) ? (float)scroll_position / max_scroll : 0;
  int scrollbar_y = y + (int)(scroll_ratio * (height - scrollbar_height));

  SDL_Rect scrollbar_bg = {x, y, width, height};
  if (renderer) {
    SDL_SetRenderDrawColor(renderer, 70, 70, 70, 255);
    SDL_RenderFillRect(renderer, &scrollbar_bg);

    SDL_Rect scrollbar_handle = {x, scrollbar_y, width, scrollbar_height};
    SDL_SetRenderDrawColor(renderer, 150, 150, 150, 255);
    SDL_RenderFillRect(renderer, &scrollbar_handle);
  }

  return scrollbar_bg; // Return the entire scrollbar area
}

static inline void handle_menu_scroll(int *scroll_position, int scroll_amount,
                                      int total_items, int visible_items,
                                      int item_height) {
  int max_scroll = fmax(0, (total_items - visible_items) * item_height);
  *scroll_position =
      fmax(0, fmin(*scroll_position - scroll_amount, max_scroll));

  // Ensure we don't scroll past the end of the content
  if (*scroll_position > max_scroll) {
    *scroll_position = max_scroll;
  }
}

static inline int is_mouse_over_menu_item(int mouseX, int mouseY, int itemY,
                                          int menuX, int menuWidth,
                                          int itemHeight) {
  return mouseX >= menuX && mouseX <= menuX + menuWidth && mouseY >= itemY &&
         mouseY < itemY + itemHeight;
}

static inline void render_menu_item(SDL_Renderer *renderer, const char *text,
                                    int x, int y, int width, int height,
                                    SDL_Color bg_color, SDL_Color text_color,
                                    TTF_Font *font) {
  SDL_Rect bg_rect = {x, y, width, height};
  SDL_SetRenderDrawColor(renderer, bg_color.r, bg_color.g, bg_color.b,
                         bg_color.a);
  SDL_RenderFillRect(renderer, &bg_rect);
  render_label(renderer, text, x + 5, y + (height - TTF_FontHeight(font)) / 2,
               font, text_color, width - 10);
}

static inline void render_left_menu(SDL_Renderer *renderer, AppState *app) {
  int left_menu_width = get_left_menu_width(app->window_width);
  int detail_area_height = app->window_height * 0.4;
  int scrollbar_width = 15;
  int title_height = 50;
  int padding = 10;
  int button_height = 30;

  // Render left menu background
  SDL_Rect left_menu_rect = {0, 0, left_menu_width, app->window_height};
  SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
  SDL_RenderFillRect(renderer, &left_menu_rect);

  // Render mode button
  SDL_Rect mode_button_rect = {10, 10, left_menu_width - 20, button_height};
  SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
  SDL_RenderFillRect(renderer, &mode_button_rect);

  const char *mode_names[] = {
      "Single",
      "References",
      "Referenced By",
      "References (Recursive)",
      "Referenced By (Recursive)",
  };
  char mode_text[50];
  snprintf(mode_text, sizeof(mode_text), "Mode: %s",
           mode_names[app->selection_mode]);
  render_label(renderer, mode_text, 15, 15, app->font_small, COLOR_WHITE,
               left_menu_width - 30);

  // Render filter referenced button
  SDL_Rect filter_button_rect = {10, 50, left_menu_width - 20, button_height};
  SDL_SetRenderDrawColor(renderer, app->filter_referenced ? 150 : 100, 100, 100,
                         255);
  SDL_RenderFillRect(renderer, &filter_button_rect);
  render_label(renderer, "Show only selected", 15, 55, app->font_small,
               COLOR_WHITE, left_menu_width - 30);

  // Render detail area
  SDL_Rect detail_rect = {0, app->window_height - detail_area_height,
                          left_menu_width, detail_area_height};
  SDL_SetRenderDrawColor(renderer, 70, 70, 70, 255);
  SDL_RenderFillRect(renderer, &detail_rect);

  // Render "Selected Objects" title
  SDL_Rect title_bg = {0, app->window_height - detail_area_height,
                       left_menu_width, title_height};
  SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
  SDL_RenderFillRect(renderer, &title_bg);
  render_label(renderer, "Selected Objects", padding,
               app->window_height - detail_area_height + padding,
               app->font_small, COLOR_WHITE, left_menu_width - 2 * padding);

  // Render selected nodes details
  int y_offset = app->window_height - detail_area_height + title_height;
  int total_content_height = 0;
  int selected_count = 0;
  int item_height = 20;

  for (int i = 0; i < app->graph->node_count; i++) {
    if (app->selected_nodes[i] && app->graph->nodes[i].visible) {
      selected_count++;
      char detail_text[MAX_LABEL_LENGTH * 2];
      snprintf(detail_text, sizeof(detail_text), "%d: %s",
               app->graph->nodes[i].id, app->graph->nodes[i].label);

      SDL_Surface *text_surface = TTF_RenderText_Blended_Wrapped(
          app->font_small, detail_text, COLOR_WHITE,
          left_menu_width - 2 * padding - scrollbar_width);
      total_content_height += text_surface->h + padding;
      SDL_FreeSurface(text_surface);
    }
  }

  // Render scrollbar
  int scroll_area_height = detail_area_height - title_height;
  render_scrollbar(renderer, left_menu_width - scrollbar_width, y_offset,
                   scrollbar_width, scroll_area_height, selected_count,
                   scroll_area_height / item_height, app->left_scroll_position);

  // Render visible content
  SDL_Rect content_area = {0, y_offset, left_menu_width - scrollbar_width,
                           scroll_area_height};
  SDL_RenderSetViewport(renderer, &content_area);

  y_offset = -app->left_scroll_position;
  for (int i = 0; i < app->graph->node_count; i++) {
    if (app->selected_nodes[i] && app->graph->nodes[i].visible) {
      char detail_text[MAX_LABEL_LENGTH * 2];
      snprintf(detail_text, sizeof(detail_text), "%d: %s",
               app->graph->nodes[i].id, app->graph->nodes[i].label);

      // Calculate available width for text
      int available_width = content_area.w - 10; // Subtract padding

      // Truncate the text if it's too long
      char truncated_text[MAX_LABEL_LENGTH * 2];
      int max_chars = available_width / (TTF_FontHeight(app->font_small) /
                                         2); // More accurate estimate
      if ((int)strlen(detail_text) > max_chars) {
        strncpy(truncated_text, detail_text, max_chars);
        truncated_text[max_chars] = '\0';
      } else {
        strcpy(truncated_text, detail_text);
      }

      SDL_Color bg_color =
          (selected_count % 2 == 0) ? COLOR_MENU_ITEM_1 : COLOR_MENU_ITEM_2;
      render_menu_item(renderer, truncated_text, 0, y_offset, content_area.w,
                       item_height, bg_color, COLOR_WHITE, app->font_small);

      y_offset += item_height;
      selected_count++;
    }
  }

  SDL_RenderSetViewport(renderer, NULL);
}

static inline void render_right_menu(SDL_Renderer *renderer, AppState *app) {
  int right_menu_width = get_right_menu_width(app->window_width);
  int right_menu_x = app->window_width - right_menu_width;
  int scrollbar_width = 15;
  int search_icon_size = SEARCH_BAR_HEIGHT;

  // Render right menu background
  SDL_Rect menu_rect = {right_menu_x, 0, right_menu_width, app->window_height};
  SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
  SDL_RenderFillRect(renderer, &menu_rect);

  // Render search box
  SDL_Rect search_rect = {right_menu_x + 5, 5,
                          right_menu_width - 10 - search_icon_size,
                          SEARCH_BAR_HEIGHT};
  SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
  SDL_RenderFillRect(renderer, &search_rect);

  render_label(renderer, app->search_bar.text, right_menu_x + 10, 10,
               app->font_small, COLOR_BLACK,
               right_menu_width - 20 - search_icon_size);

  // Render search icon
  SDL_Rect search_icon_rect = {app->window_width - search_icon_size, 5,
                               search_icon_size, search_icon_size};
  SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
  SDL_RenderFillRect(renderer, &search_icon_rect);

  // Draw a simple magnifying glass icon
  int icon_center_x = search_icon_rect.x + search_icon_size / 2;
  int icon_center_y = search_icon_rect.y + search_icon_size / 2;
  int circle_radius = search_icon_size / 3;
  circleRGBA(renderer, icon_center_x, icon_center_y, circle_radius, 100, 100,
             100, 255);
  thickLineRGBA(renderer, icon_center_x + circle_radius - 2,
                icon_center_y + circle_radius - 2,
                icon_center_x + search_icon_size / 2 - 2,
                icon_center_y + search_icon_size / 2 - 2, 3, 100, 100, 100,
                255);

  // Render node list
  int y_offset = SEARCH_BAR_HEIGHT + 10;
  int nodes_rendered = 0;
  int item_height = 20;

  // Render scrollbar
  int scroll_area_height = app->window_height - SEARCH_BAR_HEIGHT - 20;
  render_scrollbar(renderer, app->window_width - scrollbar_width,
                   SEARCH_BAR_HEIGHT + 10, scrollbar_width, scroll_area_height,
                   app->visible_nodes_count, scroll_area_height / item_height,
                   app->right_scroll_position);

  // Render visible content
  SDL_Rect content_area = {right_menu_x, y_offset,
                           right_menu_width - scrollbar_width,
                           scroll_area_height};
  SDL_RenderSetViewport(renderer, &content_area);

  y_offset = -app->right_scroll_position;
  for (int i = 0; i < app->graph->node_count; i++) {
    if (app->graph->nodes[i].visible) {
      char node_text[MAX_LABEL_LENGTH + 10];
      snprintf(node_text, sizeof(node_text), "%d: %s", app->graph->nodes[i].id,
               app->graph->nodes[i].label);

      SDL_Color bg_color =
          (nodes_rendered % 2 == 0) ? COLOR_MENU_ITEM_1 : COLOR_MENU_ITEM_2;

      // Highlight hovered item
      if (nodes_rendered == app->right_menu_hovered_item) {
        bg_color = (SDL_Color){100, 100, 100, 255}; // Lighter color for hover
      }

      render_menu_item(renderer, node_text, 0, y_offset, content_area.w,
                       item_height, bg_color, COLOR_WHITE, app->font_small);

      y_offset += item_height;
      nodes_rendered++;
    }
  }

  SDL_RenderSetViewport(renderer, NULL);
}

static inline void handle_input(SDL_Event *event, AppState *app) {
  int left_menu_width = get_left_menu_width(app->window_width);
  int right_menu_width = get_right_menu_width(app->window_width);
  int graph_width = get_graph_width(app->window_width);

  switch (event->type) {
  case SDL_MOUSEMOTION:
    app->mouse_position.x = event->motion.x;
    app->mouse_position.y = event->motion.y;

    // Handle scrollbar dragging
    if (app->is_dragging_left_scrollbar) {
      int drag_distance = event->motion.y - app->drag_start_y;
      int scroll_area_height = app->window_height * 0.4 - 50;
      int max_scroll = app->visible_nodes_count * 20 - scroll_area_height;
      app->left_scroll_position =
          app->drag_start_scroll +
          (drag_distance * max_scroll) / scroll_area_height;
      app->left_scroll_position =
          fmax(0, fmin(app->left_scroll_position, max_scroll));
    } else if (app->is_dragging_right_scrollbar) {
      int drag_distance = event->motion.y - app->drag_start_y;
      int scroll_area_height = app->window_height - SEARCH_BAR_HEIGHT - 20;
      int max_scroll = app->visible_nodes_count * 20 - scroll_area_height;
      app->right_scroll_position =
          app->drag_start_scroll +
          (drag_distance * max_scroll) / scroll_area_height;
      app->right_scroll_position =
          fmax(0, fmin(app->right_scroll_position, max_scroll));
    } else {
      // Reset hover states
      app->hovered_node = -1;
      app->hovered_edge = -1;
      app->right_menu_hovered_item = -1;

      int right_menu_x = app->window_width - right_menu_width;

      // Check for right menu hover
      if (app->mouse_position.x >= right_menu_x) {
        int y_offset = SEARCH_BAR_HEIGHT + 10 - app->right_scroll_position;
        int item_height = 20;
        int nodes_rendered = 0;
        for (int i = 0; i < app->graph->node_count; i++) {
          if (app->graph->nodes[i].visible) {
            if (app->mouse_position.y >= y_offset &&
                app->mouse_position.y < y_offset + item_height) {
              app->right_menu_hovered_item = nodes_rendered;
              break;
            }
            y_offset += item_height;
            nodes_rendered++;
          }
        }
      }

      // Check for node hover (in graph area)
      if (app->right_menu_hovered_item == -1 &&
          app->mouse_position.x >= left_menu_width &&
          app->mouse_position.x < right_menu_x) {
        for (int i = 0; i < app->graph->node_count; i++) {
          if (!app->graph->nodes[i].visible)
            continue;

          int nx = (app->graph->nodes[i].position.x + app->camera.position.x) *
                       app->camera.zoom +
                   left_menu_width + (float)graph_width / 2;
          int ny = (app->graph->nodes[i].position.y + app->camera.position.y) *
                       app->camera.zoom +
                   (float)app->window_height / 2;

          if (sqrt(pow(app->mouse_position.x - nx, 2) +
                   pow(app->mouse_position.y - ny, 2)) <=
              5 * app->camera.zoom) {
            app->hovered_node = i;
            break;
          }
        }
      }

      // Check for edge hover (in graph area)
      if (app->hovered_node == -1 && app->right_menu_hovered_item == -1 &&
          app->mouse_position.x >= left_menu_width &&
          app->mouse_position.x < right_menu_x) {
        for (int i = 0; i < app->graph->edge_count; i++) {
          GraphNode *source = &app->graph->nodes[app->graph->edges[i].source];
          GraphNode *target = &app->graph->nodes[app->graph->edges[i].target];

          if (!source->visible || !target->visible)
            continue;

          int x1 =
              (source->position.x + app->camera.position.x) * app->camera.zoom +
              left_menu_width + (float)graph_width / 2;
          int y1 =
              (source->position.y + app->camera.position.y) * app->camera.zoom +
              (float)app->window_height / 2;
          int x2 =
              (target->position.x + app->camera.position.x) * app->camera.zoom +
              left_menu_width + (float)graph_width / 2;
          int y2 =
              (target->position.y + app->camera.position.y) * app->camera.zoom +
              (float)app->window_height / 2;

          float d =
              fabs((y2 - y1) * app->mouse_position.x -
                   (x2 - x1) * app->mouse_position.y + x2 * y1 - y2 * x1) /
              sqrt(pow(y2 - y1, 2) + pow(x2 - x1, 2));

          if (d <= 5 * app->camera.zoom &&
              app->mouse_position.x >= fmin(x1, x2) - 5 * app->camera.zoom &&
              app->mouse_position.x <= fmax(x1, x2) + 5 * app->camera.zoom &&
              app->mouse_position.y >= fmin(y1, y2) - 5 * app->camera.zoom &&
              app->mouse_position.y <= fmax(y1, y2) + 5 * app->camera.zoom) {
            app->hovered_edge = i;
            break;
          }
        }
      }

      if (event->motion.state & SDL_BUTTON_LMASK) {
        // Only move the graph if the mouse is in the graph area
        int left_menu_width = get_left_menu_width(app->window_width);
        int right_menu_width = get_right_menu_width(app->window_width);
        if (app->mouse_position.x > left_menu_width &&
            app->mouse_position.x < app->window_width - right_menu_width &&
            app->mouse_position.y > TOP_BAR_HEIGHT) {
          app->camera.position.x += event->motion.xrel / app->camera.zoom;
          app->camera.position.y += event->motion.yrel / app->camera.zoom;
        }
      }
    }
    break;

  case SDL_MOUSEWHEEL:
    if (app->mouse_position.x > app->window_width - right_menu_width) {
      handle_menu_scroll(&app->right_scroll_position, event->wheel.y * 20,
                         app->visible_nodes_count, app->nodes_per_page, 20);
    } else if (app->mouse_position.x < left_menu_width &&
               app->mouse_position.y >
                   app->window_height - app->window_height * 0.4) {
      int selected_count = 0;
      for (int i = 0; i < app->graph->node_count; i++) {
        if (app->selected_nodes[i] && app->graph->nodes[i].visible) {
          selected_count++;
        }
      }
      int visible_items = (app->window_height * 0.4 - 50) / 20;
      handle_menu_scroll(&app->left_scroll_position, event->wheel.y * 20,
                         selected_count, visible_items, 20);
    } else {
      app->camera.zoom *= (event->wheel.y > 0) ? 1.1f : 0.9f;
    }
    break;

  case SDL_MOUSEBUTTONDOWN:
    if (event->button.button == SDL_BUTTON_LEFT) {
      int x = event->button.x;
      int y = event->button.y;

      if (x >= app->open_button.x &&
          x <= app->open_button.x + app->open_button.w &&
          y >= app->open_button.y &&
          y <= app->open_button.y + app->open_button.h) {
        const char *selected_file = handle_open_button_click();
        reinitialize_app(app, selected_file);
      } else if (x >= 10 && x <= left_menu_width - 10 && y >= 10 && y <= 40) {
        cycle_selection_mode(app);
      } else if (x >= 10 && x <= left_menu_width - 10 && y >= 50 && y <= 80) {
        app->filter_referenced = !app->filter_referenced;
        update_node_visibility(app);
      } else if (x >= app->window_width - right_menu_width) {
        int scrollbar_width = 15;
        // Check if clicking on right scrollbar
        SDL_Rect right_scrollbar = render_scrollbar(
            NULL, app->window_width - scrollbar_width, SEARCH_BAR_HEIGHT + 10,
            scrollbar_width, app->window_height - SEARCH_BAR_HEIGHT - 20,
            app->visible_nodes_count * 20,
            app->window_height - SEARCH_BAR_HEIGHT - 20,
            app->right_scroll_position);
        if (x >= right_scrollbar.x &&
            x <= right_scrollbar.x + right_scrollbar.w &&
            y >= right_scrollbar.y &&
            y <= right_scrollbar.y + right_scrollbar.h) {
          app->is_dragging_right_scrollbar = 1;
          app->drag_start_y = y;
          app->drag_start_scroll = app->right_scroll_position;
        } else if (x < app->window_width - scrollbar_width) {
          // Clicking in the right menu
          int y_offset = SEARCH_BAR_HEIGHT + 10 - app->right_scroll_position;
          int nodes_rendered = 0;
          for (int i = 0; i < app->graph->node_count; i++) {
            if (app->graph->nodes[i].visible) {
              if (y >= y_offset && y < y_offset + 20) {
                set_node_selection(app, i);
                break;
              }
              y_offset += 20;
              nodes_rendered++;
            }
          }
        }
      } else if (x < left_menu_width &&
                 y > app->window_height - app->window_height * 0.4) {
        int scrollbar_width = 15;
        // Check if clicking on left scrollbar
        SDL_Rect left_scrollbar = render_scrollbar(
            NULL, left_menu_width - scrollbar_width,
            app->window_height - app->window_height * 0.4 + 50, scrollbar_width,
            app->window_height * 0.4 - 50, app->visible_nodes_count * 20,
            app->window_height * 0.4 - 50, app->left_scroll_position);
        if (x >= left_scrollbar.x && x <= left_scrollbar.x + left_scrollbar.w &&
            y >= left_scrollbar.y && y <= left_scrollbar.y + left_scrollbar.h) {
          app->is_dragging_left_scrollbar = 1;
          app->drag_start_y = y;
          app->drag_start_scroll = app->left_scroll_position;
        } else if (x < left_menu_width - scrollbar_width) {
          // Clicking in the left menu's "Selected Objects" section
          int y_offset = app->window_height - app->window_height * 0.4 + 50 -
                         app->left_scroll_position;
          for (int i = 0; i < app->graph->node_count; i++) {
            if (app->selected_nodes[i] && app->graph->nodes[i].visible) {
              if (y >= y_offset && y < y_offset + 20) {
                set_node_selection(app, i);
                break;
              }
              y_offset += 20;
            }
          }
        }
      } else {
        // Clicking in the graph area
        if (app->hovered_node != -1) {
          set_node_selection(app, app->hovered_node);
        } else if (app->hovered_edge != -1) {
          set_edge_selection(app, app->hovered_edge);
        }
      }
    }
    break;

  case SDL_MOUSEBUTTONUP:
    if (event->button.button == SDL_BUTTON_LEFT) {
      app->is_dragging_left_scrollbar = 0;
      app->is_dragging_right_scrollbar = 0;
    }
    break;

  case SDL_TEXTINPUT:
    if (strlen(app->search_bar.text) < MAX_SEARCH_LENGTH - 1) {
      strcat(app->search_bar.text, event->text.text);
      update_node_visibility(app);
    }
    break;

  case SDL_KEYDOWN:
    switch (event->key.keysym.sym) {
    case SDLK_BACKSPACE:
      if (strlen(app->search_bar.text) > 0) {
        app->search_bar.text[strlen(app->search_bar.text) - 1] = '\0';
        update_node_visibility(app);
      }
      break;
    case SDLK_TAB:
      cycle_selection_mode(app);
      break;
    case SDLK_PAGEUP:
    case SDLK_PAGEDOWN:
    case SDLK_HOME:
    case SDLK_END: {
      int scroll_amount = 0;
      int *scroll_position = NULL;
      int total_items = 0;
      int visible_items = 0;

      if (app->mouse_position.x > app->window_width - right_menu_width) {
        scroll_position = &app->right_scroll_position;
        total_items = app->visible_nodes_count;
        visible_items = app->nodes_per_page;
      } else if (app->mouse_position.x < left_menu_width &&
                 app->mouse_position.y >
                     app->window_height - app->window_height * 0.4) {
        scroll_position = &app->left_scroll_position;
        // Count selected and visible nodes for the left menu
        for (int i = 0; i < app->graph->node_count; i++) {
          if (app->selected_nodes[i] && app->graph->nodes[i].visible) {
            total_items++;
          }
        }
        // Approximate number of visible items in left menu
        visible_items = (app->window_height * 0.4 - 50) / 20;
      }

      if (scroll_position) {
        switch (event->key.keysym.sym) {
        case SDLK_PAGEUP:
          scroll_amount = 1;
          break;
        case SDLK_PAGEDOWN:
          scroll_amount = -1;
          break;
        case SDLK_HOME:
          *scroll_position = 0;
          break;
        case SDLK_END:
          *scroll_position = fmax(0, (total_items - visible_items) * 20);
          break;
        }

        if (scroll_amount != 0) {
          int scroll_pixels = scroll_amount * visible_items * 20;
          handle_menu_scroll(scroll_position, scroll_pixels, total_items,
                             visible_items, 20);
        }
      }
    } break;
    }
    break;

  case SDL_WINDOWEVENT:
    if (event->window.event == SDL_WINDOWEVENT_RESIZED) {
      app->window_width = event->window.data1;
      app->window_height = event->window.data2;
      app->nodes_per_page = (app->window_height - SEARCH_BAR_HEIGHT - 20) / 20;
      update_open_button_position(app);
    }
    break;
  }
}

static inline void initialize_app(AppState *app, const char *graph_file) {
  DEBUG_PRINT("Initializing app with graph file: %s\n", graph_file);

  DEBUG_PRINT("Loading graph\n");
  app->graph = load_graph(graph_file);
  if (!app->graph) {
    fprintf(stderr, "Failed to load graph\n");
    exit(1);
  }
  DEBUG_PRINT("Graph loaded successfully. Node count: %d, Edge count: %d\n", app->graph->node_count, app->graph->edge_count);

  DEBUG_PRINT("Initializing camera\n");
  app->camera.zoom = 1.0f;
  app->camera.position = (Vec2f){0, 0};

  DEBUG_PRINT("Getting display mode\n");
  SDL_DisplayMode dm;
  if (SDL_GetCurrentDisplayMode(0, &dm) != 0) {
    fprintf(stderr, "SDL_GetCurrentDisplayMode failed: %s\n", SDL_GetError());
    exit(1);
  }
  DEBUG_PRINT("Display mode: %dx%d\n", dm.w, dm.h);

  app->window_width = dm.w / 2;
  app->window_height = dm.h / 2;
  app->nodes_per_page = (app->window_height - SEARCH_BAR_HEIGHT - 20) / 20;
  DEBUG_PRINT("Window size set to %dx%d, Nodes per page: %d\n", app->window_width, app->window_height, app->nodes_per_page);

  DEBUG_PRINT("Allocating memory for selected nodes\n");
  app->selected_nodes = calloc(app->graph->node_count, sizeof(int));
  if (!app->selected_nodes) {
    fprintf(stderr, "Failed to allocate memory for selected nodes\n");
    exit(1);
  }

  DEBUG_PRINT("Initializing app state variables\n");
  app->selection_mode = SELECT_SINGLE;
  app->right_scroll_position = 0;
  app->left_scroll_position = 0;
  app->visible_nodes_count = app->graph->node_count;
  app->mouse_position = (Vec2f){0, 0};
  app->filter_referenced = 0;
  app->hovered_edge = -1;
  app->hovered_node = -1;
  app->is_dragging_left_scrollbar = 0;
  app->is_dragging_right_scrollbar = 0;
  app->drag_start_y = 0;
  app->drag_start_scroll = 0;

  DEBUG_PRINT("Loading fonts\n");
  SDL_RWops *font_rw = SDL_RWFromMem(lemon_ttf, lemon_ttf_len);
  if (!font_rw) {
    fprintf(stderr, "Failed to create RWops for font: %s\n", SDL_GetError());
    exit(1);
  }

  app->font_small = TTF_OpenFontRW(font_rw, 0, 15);
  SDL_RWseek(font_rw, 0, RW_SEEK_SET);
  app->font_medium = TTF_OpenFontRW(font_rw, 0, 30);
  SDL_RWseek(font_rw, 0, RW_SEEK_SET);
  app->font_large = TTF_OpenFontRW(font_rw, 1, 45);

  if (!app->font_small || !app->font_medium || !app->font_large) {
    fprintf(stderr, "TTF_OpenFontRW: %s\n", TTF_GetError());
    exit(1);
  }

  memset(app->search_bar.text, 0, MAX_SEARCH_LENGTH);

  int left_menu_width = get_left_menu_width(app->window_width);
  app->open_button = (SDL_Rect){left_menu_width + 10, 5, OPEN_BUTTON_WIDTH,
                                TOP_BAR_HEIGHT - 10};
  update_open_button_position(app);

  DEBUG_PRINT("App initialization complete\n");
}

static inline void render_top_bar(SDL_Renderer *renderer, AppState *app) {
  int left_menu_width = get_left_menu_width(app->window_width);
  int graph_width = get_graph_width(app->window_width);

  // Render top bar background
  SDL_Rect top_bar_rect = {left_menu_width, 0, graph_width, TOP_BAR_HEIGHT};
  SDL_SetRenderDrawColor(renderer, 70, 70, 70, 255);
  SDL_RenderFillRect(renderer, &top_bar_rect);

  // Render open button
  SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
  SDL_RenderFillRect(renderer, &app->open_button);
  render_label(renderer, "Open", app->open_button.x + 5, app->open_button.y + 5,
               app->font_small, COLOR_WHITE, OPEN_BUTTON_WIDTH - 10);

  // Render "apaz's heap viewer" text
  render_label(renderer, "apaz's heap viewer",
               left_menu_width + graph_width - 200, 10, app->font_small,
               (SDL_Color){0, 255, 0, 255}, 190);
}

static inline void update_open_button_position(AppState *app) {
  int left_menu_width = get_left_menu_width(app->window_width);
  app->open_button = (SDL_Rect){left_menu_width + 10, 5, OPEN_BUTTON_WIDTH,
                                TOP_BAR_HEIGHT - 10};
}

static inline char *handle_open_button_click(void) {
  static char selected_file[4096] = {0};
  FILE *fp;
  char command[256];

  // Construct the command to run the filepicker binary
  snprintf(command, sizeof(command), "./filepicker");

  fp = popen(command, "r");
  if (fp == NULL) {
    fprintf(stderr, "Failed to run filepicker command\n");
    return NULL;
  }

  if (fgets(selected_file, sizeof(selected_file) - 1, fp) != NULL) {
    // Remove newline character if present
    size_t len = strlen(selected_file);
    if (len > 0 && selected_file[len - 1] == '\n') {
      selected_file[len - 1] = '\0';
    }
  } else {
    // No file selected
    selected_file[0] = '\0';
  }

  pclose(fp);

  return selected_file[0] != '\0' ? selected_file : NULL;
}

static inline void cleanup_app(AppState *app) {
  free_graph(app->graph);
  free(app->selected_nodes);
  TTF_CloseFont(app->font_small);
  TTF_CloseFont(app->font_medium);
  TTF_CloseFont(app->font_large);
}

static inline void reinitialize_app(AppState *app, const char *graph_file) {
  // Clean up existing resources
  free_graph(app->graph);
  free(app->selected_nodes);

  // Reinitialize the application
  app->graph = load_graph(graph_file);
  if (!app->graph) {
    fprintf(stderr, "Failed to load graph\n");
    exit(1);
  }

  app->camera.zoom = 1.0f;
  app->camera.position = (Vec2f){0, 0};

  app->selected_nodes = calloc(app->graph->node_count, sizeof(int));
  if (!app->selected_nodes) {
    fprintf(stderr, "Failed to allocate memory for selected nodes\n");
    exit(1);
  }

  app->selection_mode = SELECT_SINGLE;
  app->right_scroll_position = 0;
  app->left_scroll_position = 0;
  app->visible_nodes_count = app->graph->node_count;
  app->filter_referenced = 0;

  app->hovered_edge = -1;
  app->hovered_node = -1;

  app->is_dragging_left_scrollbar = 0;
  app->is_dragging_right_scrollbar = 0;
  app->drag_start_y = 0;
  app->drag_start_scroll = 0;

  memset(app->search_bar.text, 0, MAX_SEARCH_LENGTH);

  update_node_visibility(app);
  update_open_button_position(app);
}

static inline int run_graph_viewer(const char *graph_file) {
  DEBUG_PRINT("Starting run_graph_viewer\n");

  DEBUG_PRINT("Initializing SDL\n");
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
    fprintf(stderr, "SDL could not initialize! SDL_Error: %s\n",
            SDL_GetError());
    return 1;
  }

  DEBUG_PRINT("Initializing TTF\n");
  if (TTF_Init() == -1) {
    fprintf(stderr, "TTF_Init: %s\n", TTF_GetError());
    return 1;
  }

  DEBUG_PRINT("Initializing app\n");
  AppState app;
  initialize_app(&app, graph_file);

  DEBUG_PRINT("Creating window\n");
  SDL_Window *window = SDL_CreateWindow(
      "Graph Viewer", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
      app.window_width, app.window_height, SDL_WINDOW_RESIZABLE);
  if (!window) {
    fprintf(stderr, "Window could not be created! SDL_Error: %s\n",
            SDL_GetError());
    return 1;
  }

  DEBUG_PRINT("Creating renderer\n");
  SDL_Renderer *renderer =
      SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
  if (!renderer) {
    fprintf(stderr, "Renderer could not be created! SDL_Error: %s\n",
            SDL_GetError());
    return 1;
  }

  DEBUG_PRINT("Initializing audio\n");
  if (Mix_OpenAudio(44100, AUDIO_S16SYS, 2, 512) < 0) {
    fprintf(stderr, "SDL_mixer could not initialize! SDL_mixer Error: %s\n",
            Mix_GetError());
    return 1;
  }

  DEBUG_PRINT("Loading sound\n");
  SDL_RWops *rw = SDL_RWFromMem(bell_wav, bell_wav_len);
  Mix_Chunk *sound = Mix_LoadWAV_RW(rw, 1);
  if (!sound) {
    fprintf(stderr, "Failed to load sound! SDL_mixer Error: %s\n",
            Mix_GetError());
    return 1;
  }

  SDL_Event event;
  int quit = 0;
  Uint32 frameStart;
  int frameTime;

  DEBUG_PRINT("Entering main loop\n");
  while (!quit) {
    frameStart = SDL_GetTicks();

    DEBUG_PRINT("Handling events\n");
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) {
        quit = 1;
      } else {
        handle_input(&event, &app);
      }
    }

    DEBUG_PRINT("Clearing renderer\n");
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    DEBUG_PRINT("Rendering graph\n");
    render_graph(renderer, &app);
    DEBUG_PRINT("Rendering left menu\n");
    render_left_menu(renderer, &app);
    DEBUG_PRINT("Rendering right menu\n");
    render_right_menu(renderer, &app);
    DEBUG_PRINT("Rendering top bar\n");
    render_top_bar(renderer, &app);

    DEBUG_PRINT("Presenting renderer\n");
    SDL_RenderPresent(renderer);

    frameTime = SDL_GetTicks() - frameStart;
    if (frameTime < FRAME_DELAY) {
      SDL_Delay(FRAME_DELAY - frameTime);
    }
  }

  DEBUG_PRINT("Cleaning up\n");
  Mix_FreeChunk(sound);
  Mix_CloseAudio();
  cleanup_app(&app);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  TTF_Quit();
  SDL_Quit();

  DEBUG_PRINT("Exiting run_graph_viewer\n");
  return 0;
}

#ifndef PYTHON_MODULE

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <graph_file.json>\n", argv[0]);
    return 1;
  }

  return run_graph_viewer(argv[1]);
}
#error "PYTHON_MODULE not defined"
#else

static PyObject *py_run_graph_viewer(PyObject *self, PyObject *args) {
  const char *filename;
  if (!PyArg_ParseTuple(args, "s", &filename)) {
    return NULL;
  }

  printf("Running graph viewer with file: %s\n", filename);
  fflush(stdout);

  int result = run_graph_viewer(filename);
  return PyLong_FromLong(result);
}

static PyMethodDef GraphViewerMethods[] = {
    {"run_graph_viewer", py_run_graph_viewer, METH_VARARGS,
     "Run the graph viewer with the given JSON file."},
    {NULL, NULL, 0, NULL}};

static struct PyModuleDef graphviewermodule = {
    PyModuleDef_HEAD_INIT, "graph_viewer", "Graph Viewer module", -1,
    GraphViewerMethods};

PyMODINIT_FUNC PyInit_graph_viewer(void) {
  return PyModule_Create(&graphviewermodule);
}
#endif
