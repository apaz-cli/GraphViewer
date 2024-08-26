#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "lemon_ttf.xxd"

#define MAX_PATH 1024
#define COLOR_MENU_ITEM_1 (SDL_Color){55, 55, 55, 255}
#define COLOR_MENU_ITEM_2 (SDL_Color){70, 70, 70, 255}
#define COLOR_BLACK (SDL_Color){0, 0, 0, 255}
#define COLOR_WHITE (SDL_Color){255, 255, 255, 255}
#define COLOR_DIRECTORY (SDL_Color){0, 150, 255, 255}
#define MAX_FILES 1000
#define FONT_SIZE 14
#define SCROLLBAR_WIDTH 20
#define SEARCHBAR_HEIGHT 30
#define ITEM_HEIGHT (FONT_SIZE + 4)

typedef struct {
  char name[MAX_PATH];
  int is_dir;
} FileEntry;

typedef struct {
  int position;
  int size;
  int max_position;
} PickerScrollBar;

typedef struct {
  SDL_Window *window;
  SDL_Renderer *renderer;
  TTF_Font *font;
  SDL_RWops *font_rw;
  char current_dir[MAX_PATH];
  FileEntry files[MAX_FILES];
  int file_count;
  int selected_index;
  PickerScrollBar scrollbar;
  char search_text[MAX_PATH];
  int width;
  int height;
  int scroll_offset;
  int is_scrolling;
  int items_per_page;
} FilePicker;

// Core functions
static inline FilePicker *initialize_file_picker(const char *initial_dir);
static inline void cleanup_file_picker(FilePicker *picker);
static inline void get_directory_contents(FilePicker *picker);
static inline void render_file_picker(FilePicker *picker);
static inline void handle_events(FilePicker *picker, SDL_Event *event, int *quit,
                   char **selected_file);
static inline void update_scroll(FilePicker *picker);

// Utility functions
static inline int is_directory(const char *path);
static inline void get_parent_directory(char *path);
static inline void filter_files(FilePicker *picker);

// Case-insensitive string comparison
static inline int strcasecmp_custom(const char *s1, const char *s2) {
  while (*s1 && *s2) {
    char c1 = tolower((unsigned char)*s1);
    char c2 = tolower((unsigned char)*s2);
    if (c1 != c2) {
      return c1 - c2;
    }
    s1++;
    s2++;
  }
  return *s1 - *s2;
}

// Comparison function for sorting FileEntry structs
static inline int compare_file_entries(const void *a, const void *b) {
  const FileEntry *fa = (const FileEntry *)a;
  const FileEntry *fb = (const FileEntry *)b;

  // Sort directories first
  if (fa->is_dir && !fb->is_dir)
    return -1;
  if (!fa->is_dir && fb->is_dir)
    return 1;

  // Then sort alphabetically (case-insensitive)
  return strcasecmp_custom(fa->name, fb->name);
}

// Main file picker function
static inline char *show_file_picker(const char *initial_dir);

int main(int argc, char *argv[]) {
  const char *initial_dir = ".";

  // Check if an initial directory was provided as a command-line argument
  if (argc > 1) {
    initial_dir = argv[1];
  } else {
    // Use current working directory if no argument is provided
    char cwd[MAX_PATH];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
      initial_dir = cwd;
    } else {
      fprintf(stderr, "Error getting current working directory\n");
      return 1;
    }
  }

  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    fprintf(stderr, "SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
    return 1;
  }

  if (TTF_Init() == -1) {
    fprintf(stderr, "TTF_Init: %s\n", TTF_GetError());
    SDL_Quit();
    return 1;
  }

  char *selected_file = show_file_picker(initial_dir);

  if (selected_file) {
    printf("%s\n", selected_file);
    free(selected_file);
  }

  TTF_Quit();
  SDL_Quit();

  return 0;
}

static inline int is_directory(const char *path) {
  struct stat statbuf;
  if (stat(path, &statbuf) != 0)
    return 0;
  return S_ISDIR(statbuf.st_mode);
}

static inline void get_parent_directory(char *path) {
  char *last_slash = strrchr(path, '/');
  if (last_slash != NULL) {
    *last_slash = '\0';
  }
}

static inline void filter_files(FilePicker *picker) {
  int j = 0;
  for (int i = 0; i < picker->file_count; i++) {
    if (strstr(picker->files[i].name, picker->search_text) != NULL) {
      picker->files[j] = picker->files[i];
      j++;
    }
  }
  picker->file_count = j;
}

static inline FilePicker *initialize_file_picker(const char *initial_dir) {
  FilePicker *picker = (FilePicker *)malloc(sizeof(FilePicker));
  if (!picker)
    return NULL;

  picker->window =
      SDL_CreateWindow("File Picker", SDL_WINDOWPOS_UNDEFINED,
                       SDL_WINDOWPOS_UNDEFINED, 640, 480, SDL_WINDOW_RESIZABLE);
  if (!picker->window) {
    free(picker);
    return NULL;
  }

  picker->renderer =
      SDL_CreateRenderer(picker->window, -1, SDL_RENDERER_ACCELERATED);
  if (!picker->renderer) {
    SDL_DestroyWindow(picker->window);
    free(picker);
    return NULL;
  }

  picker->font_rw = SDL_RWFromMem(lemon_ttf, lemon_ttf_len);
  if (!picker->font_rw) {
    fprintf(stderr, "Failed to create RWops for font: %s\n", SDL_GetError());
    SDL_DestroyRenderer(picker->renderer);
    SDL_DestroyWindow(picker->window);
    free(picker);
    return NULL;
  }

  picker->font = TTF_OpenFontRW(picker->font_rw, 0, FONT_SIZE);
  if (!picker->font) {
    fprintf(stderr, "Failed to load font: %s\n", TTF_GetError());
    SDL_FreeRW(picker->font_rw);
    SDL_DestroyRenderer(picker->renderer);
    SDL_DestroyWindow(picker->window);
    free(picker);
    return NULL;
  }

  strncpy(picker->current_dir, initial_dir, MAX_PATH);
  picker->selected_index = 0;
  picker->search_text[0] = '\0';
  SDL_GetWindowSize(picker->window, &picker->width, &picker->height);
  picker->scroll_offset = 0;
  picker->is_scrolling = 0;
  picker->items_per_page = (picker->height - SEARCHBAR_HEIGHT) / ITEM_HEIGHT;

  get_directory_contents(picker);
  update_scroll(picker);

  return picker;
}

static inline void cleanup_file_picker(FilePicker *picker) {
  if (picker) {
    TTF_CloseFont(picker->font);
    SDL_FreeRW(picker->font_rw);
    SDL_DestroyRenderer(picker->renderer);
    SDL_DestroyWindow(picker->window);
    free(picker);
  }
}

static inline void get_directory_contents(FilePicker *picker) {
  DIR *dir;
  struct dirent *entry;

  picker->file_count = 0;

  dir = opendir(picker->current_dir);
  if (dir == NULL) {
    fprintf(stderr, "Error opening directory: %s\n", picker->current_dir);
    return;
  }

  // Always add ".." as the first entry
  strncpy(picker->files[picker->file_count].name, "..", MAX_PATH);
  picker->files[picker->file_count].is_dir = 1;
  picker->file_count++;

  while ((entry = readdir(dir)) != NULL && picker->file_count < MAX_FILES) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
      continue;

    strncpy(picker->files[picker->file_count].name, entry->d_name, MAX_PATH);
    char full_path[MAX_PATH];
    snprintf(full_path, MAX_PATH, "%s/%s", picker->current_dir, entry->d_name);
    picker->files[picker->file_count].is_dir = is_directory(full_path);
    picker->file_count++;
  }

  closedir(dir);

  // Sort the files (excluding the ".." entry at index 0)
  qsort(&picker->files[1], picker->file_count - 1, sizeof(FileEntry),
        compare_file_entries);

  if (picker->file_count == 0) {
    fprintf(stderr, "Warning: No files found in directory: %s\n",
            picker->current_dir);
  }
}

static inline void render_file_picker(FilePicker *picker) {
  SDL_SetRenderDrawColor(picker->renderer, 50, 50, 50, 255);
  SDL_RenderClear(picker->renderer);

  int y = SEARCHBAR_HEIGHT;
  for (int i = picker->scroll_offset;
       i < picker->file_count &&
       i < picker->scroll_offset + picker->items_per_page;
       i++) {
    SDL_Color bg_color = (i == picker->selected_index)
                             ? (SDL_Color){100, 100, 100, 255}
                             : (i % 2 == 0 ? COLOR_MENU_ITEM_1 : COLOR_MENU_ITEM_2);
    SDL_Rect bg_rect = {0, y, picker->width - SCROLLBAR_WIDTH, ITEM_HEIGHT};
    SDL_SetRenderDrawColor(picker->renderer, bg_color.r, bg_color.g, bg_color.b,
                           bg_color.a);
    SDL_RenderFillRect(picker->renderer, &bg_rect);

    char display_name[MAX_PATH];
    snprintf(display_name, MAX_PATH, "%s%s", picker->files[i].name,
             picker->files[i].is_dir ? "/" : "");

    SDL_Color item_color =
        picker->files[i].is_dir ? COLOR_DIRECTORY : COLOR_WHITE;
    SDL_Surface *surface =
        TTF_RenderText_Solid(picker->font, display_name, item_color);
    if (surface) {
      SDL_Texture *texture =
          SDL_CreateTextureFromSurface(picker->renderer, surface);
      if (texture) {
        SDL_Rect dest = {5, y + (ITEM_HEIGHT - FONT_SIZE) / 2, surface->w,
                         surface->h};
        SDL_RenderCopy(picker->renderer, texture, NULL, &dest);
        SDL_DestroyTexture(texture);
      } else {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to create texture: %s", SDL_GetError());
      }
      SDL_FreeSurface(surface);
    } else {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to render text: %s",
                   TTF_GetError());
    }

    y += ITEM_HEIGHT;
  }

  // Render scrollbar
  SDL_Rect scrollbar_bg = {picker->width - SCROLLBAR_WIDTH, SEARCHBAR_HEIGHT,
                           SCROLLBAR_WIDTH, picker->height - SEARCHBAR_HEIGHT};
  SDL_SetRenderDrawColor(picker->renderer, 70, 70, 70, 255);
  SDL_RenderFillRect(picker->renderer, &scrollbar_bg);

  if (picker->file_count > picker->items_per_page) {
    float scrollbar_height = (float)(picker->height - SEARCHBAR_HEIGHT) *
                             picker->items_per_page / picker->file_count;
    float scrollbar_y = SEARCHBAR_HEIGHT +
                        (picker->height - SEARCHBAR_HEIGHT - scrollbar_height) *
                            picker->scroll_offset /
                            (picker->file_count - picker->items_per_page);
    SDL_Rect scrollbar = {picker->width - SCROLLBAR_WIDTH, (int)scrollbar_y,
                          SCROLLBAR_WIDTH, (int)scrollbar_height};
    SDL_SetRenderDrawColor(picker->renderer, 150, 150, 150, 255);
    SDL_RenderFillRect(picker->renderer, &scrollbar);
  }

  // Render search bar
  SDL_Rect searchbar_bg = {0, 0, picker->width, SEARCHBAR_HEIGHT};
  SDL_SetRenderDrawColor(picker->renderer, 70, 70, 70, 255);
  SDL_RenderFillRect(picker->renderer, &searchbar_bg);

  const char *render_text =
      strlen(picker->search_text) > 0 ? picker->search_text : "Search...";
  SDL_Color render_color = strlen(picker->search_text) > 0
                               ? COLOR_WHITE
                               : (SDL_Color){150, 150, 150, 255};

  SDL_Surface *search_surface =
      TTF_RenderText_Solid(picker->font, render_text, render_color);
  if (search_surface) {
    SDL_Texture *search_texture =
        SDL_CreateTextureFromSurface(picker->renderer, search_surface);
    if (search_texture) {
      SDL_Rect search_dest = {5, 5, search_surface->w, search_surface->h};
      SDL_RenderCopy(picker->renderer, search_texture, NULL, &search_dest);
      SDL_DestroyTexture(search_texture);
    } else {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                   "Failed to create search texture: %s", SDL_GetError());
    }
    SDL_FreeSurface(search_surface);
  } else {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Failed to render search text: %s", TTF_GetError());
  }

  // Render "No files found" message if file_count is 0
  if (picker->file_count == 0) {
    const char *no_files_text = "No files found";
    SDL_Surface *no_files_surface =
        TTF_RenderText_Solid(picker->font, no_files_text, COLOR_WHITE);
    if (no_files_surface) {
      SDL_Texture *no_files_texture =
          SDL_CreateTextureFromSurface(picker->renderer, no_files_surface);
      if (no_files_texture) {
        SDL_Rect no_files_dest = {(picker->width - no_files_surface->w) / 2,
                                  (picker->height - no_files_surface->h) / 2,
                                  no_files_surface->w, no_files_surface->h};
        SDL_RenderCopy(picker->renderer, no_files_texture, NULL,
                       &no_files_dest);
        SDL_DestroyTexture(no_files_texture);
      }
      SDL_FreeSurface(no_files_surface);
    }
  }

  SDL_RenderPresent(picker->renderer);
}

static inline void handle_events(FilePicker *picker, SDL_Event *event, int *quit,
                   char **selected_file) {
  int mouse_x, mouse_y;
  SDL_GetMouseState(&mouse_x, &mouse_y);

  switch (event->type) {
  case SDL_QUIT:
    *quit = 1;
    break;
  case SDL_KEYDOWN:
    switch (event->key.keysym.sym) {
    case SDLK_UP:
      if (picker->selected_index > 0) {
        picker->selected_index--;
        if (picker->selected_index < picker->scroll_offset) {
          picker->scroll_offset = picker->selected_index;
        }
      }
      break;
    case SDLK_DOWN:
      if (picker->selected_index < picker->file_count - 1) {
        picker->selected_index++;
        if (picker->selected_index >=
            picker->scroll_offset + picker->items_per_page) {
          picker->scroll_offset =
              picker->selected_index - picker->items_per_page + 1;
        }
      }
      break;
    case SDLK_PAGEUP:
      picker->selected_index -= picker->items_per_page;
      if (picker->selected_index < 0)
        picker->selected_index = 0;
      picker->scroll_offset = picker->selected_index;
      break;
    case SDLK_PAGEDOWN:
      picker->selected_index += picker->items_per_page;
      if (picker->selected_index >= picker->file_count)
        picker->selected_index = picker->file_count - 1;
      picker->scroll_offset =
          picker->selected_index - picker->items_per_page + 1;
      if (picker->scroll_offset < 0)
        picker->scroll_offset = 0;
      break;
    case SDLK_HOME:
      picker->selected_index = 0;
      picker->scroll_offset = 0;
      break;
    case SDLK_END:
      picker->selected_index = picker->file_count - 1;
      picker->scroll_offset = picker->file_count - picker->items_per_page;
      if (picker->scroll_offset < 0)
        picker->scroll_offset = 0;
      break;
    case SDLK_RETURN:
      if (picker->files[picker->selected_index].is_dir) {
        if (strcmp(picker->files[picker->selected_index].name, "..") == 0) {
          if (strcmp(picker->current_dir, ".") == 0) {
            // If current dir is ".", get the absolute path of the parent
            char abs_path[MAX_PATH];
            if (realpath("..", abs_path) != NULL) {
              strncpy(picker->current_dir, abs_path, MAX_PATH);
            }
          } else {
            get_parent_directory(picker->current_dir);
          }
        } else {
          char new_dir[MAX_PATH];
          snprintf(new_dir, MAX_PATH, "%s/%s", picker->current_dir,
                   picker->files[picker->selected_index].name);
          strncpy(picker->current_dir, new_dir, MAX_PATH);
        }
        get_directory_contents(picker);
        picker->selected_index = 0;
        picker->scroll_offset = 0;
      } else {
        char full_path[MAX_PATH];
        snprintf(full_path, MAX_PATH, "%s/%s", picker->current_dir,
                 picker->files[picker->selected_index].name);
        *selected_file = strdup(full_path);
        *quit = 1;
      }
      break;
    case SDLK_BACKSPACE: {
      int len = strlen(picker->search_text);
      if (len > 0) {
        picker->search_text[len - 1] = '\0';
        get_directory_contents(picker);
        filter_files(picker);
        picker->selected_index = 0;
        picker->scroll_offset = 0;
      }
    } break;
    case SDLK_ESCAPE:
      *quit = 1;
      break;
    }
    break;
  case SDL_TEXTINPUT:
    strcat(picker->search_text, event->text.text);
    get_directory_contents(picker);
    filter_files(picker);
    picker->selected_index = 0;
    picker->scroll_offset = 0;
    break;
  case SDL_MOUSEBUTTONDOWN:
    if (event->button.button == SDL_BUTTON_LEFT) {
      if (mouse_x >= picker->width - SCROLLBAR_WIDTH) {
        picker->is_scrolling = 1;
      } else if (mouse_y >= SEARCHBAR_HEIGHT) {
        int clicked_index =
            picker->scroll_offset + (mouse_y - SEARCHBAR_HEIGHT) / ITEM_HEIGHT;
        if (clicked_index < picker->file_count) {
          picker->selected_index = clicked_index;
        }
      }
    }
    break;
  case SDL_MOUSEBUTTONUP:
    if (event->button.button == SDL_BUTTON_LEFT) {
      picker->is_scrolling = 0;
    }
    break;
  case SDL_MOUSEMOTION:
    if (picker->is_scrolling) {
      float scroll_ratio = (float)(mouse_y - SEARCHBAR_HEIGHT) /
                           (picker->height - SEARCHBAR_HEIGHT);
      picker->scroll_offset =
          (int)(scroll_ratio * (picker->file_count - picker->items_per_page));
      if (picker->scroll_offset < 0)
        picker->scroll_offset = 0;
      if (picker->scroll_offset > picker->file_count - picker->items_per_page)
        picker->scroll_offset = picker->file_count - picker->items_per_page;
    }
    break;
  case SDL_MOUSEWHEEL:
    picker->scroll_offset -= event->wheel.y * 3;
    if (picker->scroll_offset < 0)
      picker->scroll_offset = 0;
    if (picker->scroll_offset > picker->file_count - picker->items_per_page)
      picker->scroll_offset = picker->file_count - picker->items_per_page;
    break;
  case SDL_WINDOWEVENT:
    if (event->window.event == SDL_WINDOWEVENT_RESIZED) {
      picker->width = event->window.data1;
      picker->height = event->window.data2;
      picker->items_per_page =
          (picker->height - SEARCHBAR_HEIGHT) / ITEM_HEIGHT;
    }
    break;
  }
  update_scroll(picker);
}

static inline void update_scroll(FilePicker *picker) {
  picker->items_per_page = (picker->height - SEARCHBAR_HEIGHT) / ITEM_HEIGHT;

  if (picker->selected_index < picker->scroll_offset) {
    picker->scroll_offset = picker->selected_index;
  } else if (picker->selected_index >=
             picker->scroll_offset + picker->items_per_page) {
    picker->scroll_offset = picker->selected_index - picker->items_per_page + 1;
  }

  if (picker->scroll_offset < 0) {
    picker->scroll_offset = 0;
  }

  int max_scroll = picker->file_count - picker->items_per_page;
  if (picker->scroll_offset > max_scroll) {
    picker->scroll_offset = max_scroll;
  }

  if (picker->scroll_offset < 0) {
    picker->scroll_offset = 0;
  }
}

char *show_file_picker(const char *initial_dir) {
  FilePicker *picker = initialize_file_picker(initial_dir);
  if (!picker)
    return NULL;

  char *selected_file = NULL;
  int quit = 0;
  SDL_Event event;

  while (!quit) {
    while (SDL_PollEvent(&event)) {
      handle_events(picker, &event, &quit, &selected_file);
    }
    render_file_picker(picker);
    SDL_Delay(10);
  }

  cleanup_file_picker(picker);
  return selected_file;
}
