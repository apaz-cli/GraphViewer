#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>

#define MAX_PATH_LENGTH 1024
#define MAX_FILES 1000
#define ITEM_HEIGHT 30

int WINDOW_WIDTH = 800;
int WINDOW_HEIGHT = 600;
int ITEMS_PER_PAGE;

typedef struct {
    char name[256];
    char full_path[MAX_PATH_LENGTH];
    int is_dir;
    off_t size;
    time_t modified_time;
} FileEntry;

typedef struct {
    FileEntry entries[MAX_FILES];
    int count;
    int scroll_offset;
    int selected;
    char current_path[MAX_PATH_LENGTH];
    char filter[256];
    int window_width;
    int window_height;
} FileList;

// Function prototypes
void init_file_list(FileList* list);
void update_file_list(FileList* list);
int file_name_compare(const void* a, const void* b);
void draw_file_list(SDL_Renderer* renderer, TTF_Font* font, FileList* list);
void handle_events(SDL_Event* event, FileList* list, int* quit, char** result);
char* format_size(off_t size);
char* format_time(time_t t);

char* show_file_picker(SDL_Renderer* renderer, TTF_Font* font, SDL_Window* window) {
    FileList file_list;
    init_file_list(&file_list, WINDOW_WIDTH, WINDOW_HEIGHT);

    SDL_Event event;
    int quit = 0;
    char* result = NULL;

    while (!quit) {
        while (SDL_PollEvent(&event)) {
            handle_events(&event, &file_list, &quit, &result, window);
        }

        SDL_SetRenderDrawColor(renderer, 240, 240, 240, 255);
        SDL_RenderClear(renderer);

        draw_file_list(renderer, font, &file_list);

        SDL_RenderPresent(renderer);
    }

    return result;
}

void init_file_list(FileList* list, int window_width, int window_height) {
    list->count = 0;
    list->scroll_offset = 0;
    list->selected = 0;
    strcpy(list->current_path, ".");
    strcpy(list->filter, "*");
    list->window_width = window_width;
    list->window_height = window_height;
    update_file_list(list);
}

void update_file_list(FileList* list) {
    DIR* dir = opendir(list->current_path);
    if (dir == NULL) {
        SDL_Log("Error opening directory: %s", list->current_path);
        return;
    }

    list->count = 0;
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL && list->count < MAX_FILES) {
        if (strcmp(entry->d_name, ".") == 0) continue;

        FileEntry* file = &list->entries[list->count];
        strncpy(file->name, entry->d_name, 255);
        snprintf(file->full_path, MAX_PATH_LENGTH, "%s/%s", list->current_path, file->name);

        struct stat st;
        if (stat(file->full_path, &st) == 0) {
            file->is_dir = S_ISDIR(st.st_mode);
            file->size = st.st_size;
            file->modified_time = st.st_mtime;
        } else {
            file->is_dir = 0;
            file->size = 0;
            file->modified_time = 0;
        }

        if (strcmp(list->filter, "*") == 0 || 
            (!file->is_dir && strstr(file->name, list->filter) != NULL)) {
            list->count++;
        }
    }
    closedir(dir);

    qsort(list->entries, list->count, sizeof(FileEntry), file_name_compare);
}

int file_name_compare(const void* a, const void* b) {
    FileEntry* fa = (FileEntry*)a;
    FileEntry* fb = (FileEntry*)b;

    if (fa->is_dir && !fb->is_dir) return -1;
    if (!fa->is_dir && fb->is_dir) return 1;
    return strcmp(fa->name, fb->name);
}

void draw_file_list(SDL_Renderer* renderer, TTF_Font* font, FileList* list) {
    SDL_Color text_color = {0, 0, 0, 255};
    SDL_Color highlight_color = {200, 200, 255, 255};

    // Draw current path
    SDL_Surface* path_surface = TTF_RenderText_Blended(font, list->current_path, text_color);
    SDL_Texture* path_texture = SDL_CreateTextureFromSurface(renderer, path_surface);
    SDL_Rect path_rect = {10, 10, path_surface->w, path_surface->h};
    SDL_RenderCopy(renderer, path_texture, NULL, &path_rect);
    SDL_FreeSurface(path_surface);
    SDL_DestroyTexture(path_texture);

    // Calculate ITEMS_PER_PAGE based on current window height
    int ITEMS_PER_PAGE = (list->window_height - 100) / ITEM_HEIGHT;

    // Draw file list
    for (int i = 0; i < ITEMS_PER_PAGE && i + list->scroll_offset < list->count; i++) {
        FileEntry* file = &list->entries[i + list->scroll_offset];
        
        SDL_Rect item_rect = {10, 50 + i * ITEM_HEIGHT, list->window_width - 20, ITEM_HEIGHT};
        
        if (i + list->scroll_offset == list->selected) {
            SDL_SetRenderDrawColor(renderer, highlight_color.r, highlight_color.g, highlight_color.b, highlight_color.a);
            SDL_RenderFillRect(renderer, &item_rect);
        }

        char display_text[512];
        char* size_str = format_size(file->size);
        char* time_str = format_time(file->modified_time);
        snprintf(display_text, sizeof(display_text), "%-30s %10s %20s %s", 
                 file->name, file->is_dir ? "<DIR>" : size_str, time_str, file->is_dir ? "/" : "");
        free(size_str);
        free(time_str);

        SDL_Surface* text_surface = TTF_RenderText_Blended(font, display_text, text_color);
        SDL_Texture* text_texture = SDL_CreateTextureFromSurface(renderer, text_surface);
        SDL_RenderCopy(renderer, text_texture, NULL, &item_rect);
        SDL_FreeSurface(text_surface);
        SDL_DestroyTexture(text_texture);
    }

    // Draw scrollbar if necessary
    if (list->count > ITEMS_PER_PAGE) {
        int scrollbar_height = (list->window_height - 100) * ITEMS_PER_PAGE / list->count;
        int scrollbar_y = 50 + (list->window_height - 100) * list->scroll_offset / list->count;
        SDL_Rect scrollbar = {list->window_width - 20, scrollbar_y, 10, scrollbar_height};
        SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
        SDL_RenderFillRect(renderer, &scrollbar);
    }
}

void handle_events(SDL_Event* event, FileList* list, int* quit, char** result, SDL_Window* window) {
    switch (event->type) {
        case SDL_QUIT:
            *quit = 1;
            break;
        case SDL_KEYDOWN:
            switch (event->key.keysym.sym) {
                case SDLK_UP:
                    if (list->selected > 0) {
                        list->selected--;
                        if (list->selected < list->scroll_offset) {
                            list->scroll_offset = list->selected;
                        }
                    }
                    break;
                case SDLK_DOWN:
                    if (list->selected < list->count - 1) {
                        list->selected++;
                        if (list->selected >= list->scroll_offset + ITEMS_PER_PAGE) {
                            list->scroll_offset = list->selected - ITEMS_PER_PAGE + 1;
                        }
                    }
                    break;
                case SDLK_RETURN: {
                    FileEntry* selected_file = &list->entries[list->selected];
                    if (selected_file->is_dir) {
                        if (strcmp(selected_file->name, "..") == 0) {
                            char* last_slash = strrchr(list->current_path, '/');
                            if (last_slash != NULL) {
                                *last_slash = '\0';
                            }
                        } else {
                            strcat(list->current_path, "/");
                            strcat(list->current_path, selected_file->name);
                        }
                        list->selected = 0;
                        list->scroll_offset = 0;
                        update_file_list(list);
                    } else {
                        *result = strdup(selected_file->full_path);
                        *quit = 1;
                    }
                    break;
                }
                case SDLK_ESCAPE:
                    *quit = 1;
                    break;
            }
            break;
        case SDL_MOUSEWHEEL:
            if (event->wheel.y > 0 && list->scroll_offset > 0) {
                list->scroll_offset--;
            } else if (event->wheel.y < 0 && list->scroll_offset < list->count - ITEMS_PER_PAGE) {
                list->scroll_offset++;
            }
            break;
        case SDL_WINDOWEVENT:
            if (event->window.event == SDL_WINDOWEVENT_RESIZED) {
                list->window_width = event->window.data1;
                list->window_height = event->window.data2;
                SDL_GetWindowSize(window, &WINDOW_WIDTH, &WINDOW_HEIGHT);
            }
            break;
    }
}

char* format_size(off_t size) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit = 0;
    double size_d = size;

    while (size_d >= 1024 && unit < 4) {
        size_d /= 1024;
        unit++;
    }

    char* result = malloc(20);
    snprintf(result, 20, "%.2f %s", size_d, units[unit]);
    return result;
}

char* format_time(time_t t) {
    char* result = malloc(20);
    struct tm* tm_info = localtime(&t);
    strftime(result, 20, "%Y-%m-%d %H:%M:%S", tm_info);
    return result;
}

int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        SDL_Log("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    if (TTF_Init() < 0) {
        SDL_Log("SDL_ttf could not initialize! SDL_ttf Error: %s\n", TTF_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("File Picker", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (window == NULL) {
        SDL_Log("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == NULL) {
        SDL_Log("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    TTF_Font* font = TTF_OpenFont("lemon.ttf", 15);
    if (font == NULL) {
        SDL_Log("Failed to load font! SDL_ttf Error: %s\n", TTF_GetError());
        return 1;
    }

    char* selected_file = show_file_picker(renderer, font, window);

    if (selected_file != NULL) {
        SDL_Log("Selected file: %s", selected_file);
        free(selected_file);
    } else {
        SDL_Log("No file selected");
    }

    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();

    return 0;
}
