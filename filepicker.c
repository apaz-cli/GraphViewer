#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PATH_LENGTH 1024
#define MAX_FILES 1000
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define ITEM_HEIGHT 30

typedef struct {
    char name[256];
    int is_directory;
} FileEntry;

typedef struct {
    FileEntry entries[MAX_FILES];
    int count;
    int scroll_offset;
    int selected;
    char current_path[MAX_PATH_LENGTH];
    char filter[256];
} FileList;

void update_file_list(FileList* list) {
    DIR* dir;
    struct dirent* entry;

    list->count = 0;
    dir = opendir(list->current_path);
    if (dir == NULL) {
        printf("Error opening directory\n");
        return;
    }

    while ((entry = readdir(dir)) != NULL && list->count < MAX_FILES) {
        if (strcmp(entry->d_name, ".") == 0) continue;

        strcpy(list->entries[list->count].name, entry->d_name);
        list->entries[list->count].is_directory = (entry->d_type == DT_DIR);
        list->count++;
    }

    closedir(dir);
}

void init_file_list(FileList* list) {
    list->count = 0;
    list->scroll_offset = 0;
    list->selected = 0;
    strcpy(list->current_path, ".");
    strcpy(list->filter, "*");
    update_file_list(list);
}

void draw_file_list(SDL_Renderer* renderer, TTF_Font* font, FileList* list) {
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderClear(renderer);

    SDL_Color text_color = {0, 0, 0, 255};
    SDL_Color highlight_color = {200, 200, 255, 255};

    // Draw current path
    SDL_Surface* path_surface = TTF_RenderText_Blended(font, list->current_path, text_color);
    SDL_Texture* path_texture = SDL_CreateTextureFromSurface(renderer, path_surface);
    SDL_Rect path_rect = {10, 10, path_surface->w, path_surface->h};
    SDL_RenderCopy(renderer, path_texture, NULL, &path_rect);
    SDL_FreeSurface(path_surface);
    SDL_DestroyTexture(path_texture);

    // Draw file list
    for (int i = 0; i < 15 && i + list->scroll_offset < list->count; i++) {
        FileEntry* file = &list->entries[i + list->scroll_offset];
        char display_name[256];
        snprintf(display_name, sizeof(display_name), "%s%s", file->is_directory ? "[DIR] " : "", file->name);

        SDL_Surface* text_surface = TTF_RenderText_Blended(font, display_name, text_color);
        SDL_Texture* text_texture = SDL_CreateTextureFromSurface(renderer, text_surface);

        SDL_Rect item_rect = {10, 50 + i * ITEM_HEIGHT, WINDOW_WIDTH - 20, ITEM_HEIGHT};
        
        if (i + list->scroll_offset == list->selected) {
            SDL_SetRenderDrawColor(renderer, highlight_color.r, highlight_color.g, highlight_color.b, highlight_color.a);
            SDL_RenderFillRect(renderer, &item_rect);
        }

        SDL_Rect text_rect = {item_rect.x + 5, item_rect.y + 5, text_surface->w, text_surface->h};
        SDL_RenderCopy(renderer, text_texture, NULL, &text_rect);

        SDL_FreeSurface(text_surface);
        SDL_DestroyTexture(text_texture);
    }

    SDL_RenderPresent(renderer);
}

void handle_events(SDL_Event* event, FileList* list, int* quit, char** result) {
    switch (event->type) {
        case SDL_QUIT:
            *quit = 1;
            break;
        case SDL_KEYDOWN:
            switch (event->key.keysym.sym) {
                case SDLK_UP:
                    if (list->selected > 0) list->selected--;
                    break;
                case SDLK_DOWN:
                    if (list->selected < list->count - 1) list->selected++;
                    break;
                case SDLK_RETURN:
                    if (list->entries[list->selected].is_directory) {
                        char new_path[MAX_PATH_LENGTH];
                        snprintf(new_path, sizeof(new_path), "%s/%s", list->current_path, list->entries[list->selected].name);
                        strcpy(list->current_path, new_path);
                        list->selected = 0;
                        list->scroll_offset = 0;
                        update_file_list(list);
                    } else {
                        char* selected_file = malloc(MAX_PATH_LENGTH);
                        snprintf(selected_file, MAX_PATH_LENGTH, "%s/%s", list->current_path, list->entries[list->selected].name);
                        *result = selected_file;
                        *quit = 1;
                    }
                    break;
                case SDLK_ESCAPE:
                    *quit = 1;
                    break;
            }
            break;
    }
}

char* show_file_picker() {
    SDL_Window* window = SDL_CreateWindow("File Picker", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    TTF_Font* font = TTF_OpenFont("lemon.ttf", 16);
    if (!font) {
        printf("Failed to load font: %s\n", TTF_GetError());
        return NULL;
    }

    FileList file_list;
    init_file_list(&file_list);

    int quit = 0;
    SDL_Event event;
    char* result = NULL;

    while (!quit) {
        while (SDL_PollEvent(&event)) {
            handle_events(&event, &file_list, &quit, &result);
        }
        draw_file_list(renderer, font, &file_list);
    }

    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    return result;
}

int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    if (TTF_Init() == -1) {
        printf("TTF_Init: %s\n", TTF_GetError());
        return 1;
    }

    char* selected_file = show_file_picker();

    if (selected_file) {
        printf("Selected file: %s\n", selected_file);
        free(selected_file);
    } else {
        printf("No file selected\n");
    }

    TTF_Quit();
    SDL_Quit();

    return 0;
}
