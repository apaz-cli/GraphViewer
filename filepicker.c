#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>

#define MAX_PATH 1024
#define MAX_FILES 1000
#define FONT_SIZE 14
#define ITEM_HEIGHT 20
#define SCROLLBAR_WIDTH 20
#define SEARCHBAR_HEIGHT 30

typedef struct {
    char name[256];
    int is_dir;
} FileItem;

typedef struct {
    FileItem items[MAX_FILES];
    int count;
    int selected;
    int scroll;
    char current_path[MAX_PATH];
    char search[256];
    SDL_Rect searchbar;
    int active;
} FilePicker;

SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;
TTF_Font* font = NULL;
FilePicker picker;

void init_file_picker(const char* initial_path) {
    strncpy(picker.current_path, initial_path, MAX_PATH);
    picker.count = 0;
    picker.selected = 0;
    picker.scroll = 0;
    picker.search[0] = '\0';
    picker.active = 0;
}

void load_directory() {
    DIR* dir;
    struct dirent* entry;
    struct stat st;
    char full_path[MAX_PATH];

    picker.count = 0;

    // Add parent directory
    strcpy(picker.items[picker.count].name, "..");
    picker.items[picker.count].is_dir = 1;
    picker.count++;

    dir = opendir(picker.current_path);
    if (dir == NULL) return;

    while ((entry = readdir(dir)) != NULL && picker.count < MAX_FILES) {
        if (strcmp(entry->d_name, ".") == 0) continue;

        snprintf(full_path, MAX_PATH, "%s/%s", picker.current_path, entry->d_name);
        if (stat(full_path, &st) == 0) {
            strcpy(picker.items[picker.count].name, entry->d_name);
            picker.items[picker.count].is_dir = S_ISDIR(st.st_mode);
            picker.count++;
        }
    }

    closedir(dir);
}

void draw_file_picker() {
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderClear(renderer);

    int window_height;
    SDL_GetWindowSize(window, NULL, &window_height);
    int visible_items = (window_height - SEARCHBAR_HEIGHT) / ITEM_HEIGHT;

    SDL_Color text_color = {0, 0, 0, 255};
    SDL_Color highlight_color = {200, 200, 255, 255};

    // Draw searchbar
    SDL_SetRenderDrawColor(renderer, 240, 240, 240, 255);
    SDL_RenderFillRect(renderer, &picker.searchbar);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderDrawRect(renderer, &picker.searchbar);

    SDL_Surface* search_surface = TTF_RenderText_Blended(font, picker.search, text_color);
    SDL_Texture* search_texture = SDL_CreateTextureFromSurface(renderer, search_surface);
    SDL_Rect search_rect = {picker.searchbar.x + 5, picker.searchbar.y + 5, search_surface->w, search_surface->h};
    SDL_RenderCopy(renderer, search_texture, NULL, &search_rect);
    SDL_FreeSurface(search_surface);
    SDL_DestroyTexture(search_texture);

    for (int i = 0; i < picker.count; i++) {
        if (strstr(picker.items[i].name, picker.search) == NULL) continue;

        int y = SEARCHBAR_HEIGHT + (i - picker.scroll) * ITEM_HEIGHT;
        if (y >= SEARCHBAR_HEIGHT && y < window_height) {
            SDL_Rect item_rect = {0, y, picker.searchbar.w - SCROLLBAR_WIDTH, ITEM_HEIGHT};
            
            if (i == picker.selected) {
                SDL_SetRenderDrawColor(renderer, highlight_color.r, highlight_color.g, highlight_color.b, highlight_color.a);
                SDL_RenderFillRect(renderer, &item_rect);
            }

            char display_name[256];
            snprintf(display_name, sizeof(display_name), "%s%s", picker.items[i].is_dir ? "[D] " : "", picker.items[i].name);
            
            SDL_Surface* surface = TTF_RenderText_Blended(font, display_name, text_color);
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_Rect text_rect = {5, y + (ITEM_HEIGHT - surface->h) / 2, surface->w, surface->h};
            SDL_RenderCopy(renderer, texture, NULL, &text_rect);
            SDL_FreeSurface(surface);
            SDL_DestroyTexture(texture);
        }
    }

    // Draw scrollbar
    int total_height = picker.count * ITEM_HEIGHT;
    int scrollbar_height = (window_height - SEARCHBAR_HEIGHT) * (window_height - SEARCHBAR_HEIGHT) / total_height;
    int scrollbar_y = SEARCHBAR_HEIGHT + (window_height - SEARCHBAR_HEIGHT - scrollbar_height) * picker.scroll / (picker.count - visible_items);
    
    SDL_Rect scrollbar_rect = {picker.searchbar.w - SCROLLBAR_WIDTH, scrollbar_y, SCROLLBAR_WIDTH, scrollbar_height};
    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
    SDL_RenderFillRect(renderer, &scrollbar_rect);

    SDL_RenderPresent(renderer);
}

char* run_file_picker(const char* initial_path) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return NULL;
    }

    if (TTF_Init() == -1) {
        printf("TTF_Init: %s\n", TTF_GetError());
        return NULL;
    }

    window = SDL_CreateWindow("File Picker", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 400, 300, SDL_WINDOW_RESIZABLE);
    if (window == NULL) {
        printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        return NULL;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == NULL) {
        printf("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        return NULL;
    }

    font = TTF_OpenFont("lemon.ttf", FONT_SIZE);
    if (font == NULL) {
        printf("Failed to load font! SDL_ttf Error: %s\n", TTF_GetError());
        return NULL;
    }

    init_file_picker(initial_path);
    load_directory();

    SDL_Event e;
    int quit = 0;
    char* result = NULL;

    while (!quit) {
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                quit = 1;
            } else if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                int w, h;
                SDL_GetWindowSize(window, &w, &h);
                picker.searchbar = (SDL_Rect){0, 0, w, SEARCHBAR_HEIGHT};
            } else if (e.type == SDL_MOUSEBUTTONDOWN) {
                int x, y;
                SDL_GetMouseState(&x, &y);
                if (y > SEARCHBAR_HEIGHT) {
                    int new_selected = picker.scroll + (y - SEARCHBAR_HEIGHT) / ITEM_HEIGHT;
                    if (new_selected < picker.count) {
                        picker.selected = new_selected;
                    }
                } else if (SDL_PointInRect(&(SDL_Point){x, y}, &picker.searchbar)) {
                    picker.active = 1;
                } else {
                    picker.active = 0;
                }
            } else if (e.type == SDL_MOUSEWHEEL) {
                picker.scroll -= e.wheel.y;
                int max_scroll = picker.count - (picker.searchbar.h - SEARCHBAR_HEIGHT) / ITEM_HEIGHT;
                if (picker.scroll < 0) picker.scroll = 0;
                if (picker.scroll > max_scroll) picker.scroll = max_scroll;
            } else if (e.type == SDL_KEYDOWN) {
                if (picker.active) {
                    if (e.key.keysym.sym == SDLK_BACKSPACE && strlen(picker.search) > 0) {
                        picker.search[strlen(picker.search) - 1] = '\0';
                    } else if (e.key.keysym.sym == SDLK_RETURN) {
                        picker.active = 0;
                    }
                } else {
                    if (e.key.keysym.sym == SDLK_UP && picker.selected > 0) {
                        picker.selected--;
                    } else if (e.key.keysym.sym == SDLK_DOWN && picker.selected < picker.count - 1) {
                        picker.selected++;
                    } else if (e.key.keysym.sym == SDLK_RETURN) {
                        if (picker.items[picker.selected].is_dir) {
                            if (strcmp(picker.items[picker.selected].name, "..") == 0) {
                                char* last_slash = strrchr(picker.current_path, '/');
                                if (last_slash != NULL) {
                                    *last_slash = '\0';
                                }
                            } else {
                                strcat(picker.current_path, "/");
                                strcat(picker.current_path, picker.items[picker.selected].name);
                            }
                            load_directory();
                            picker.selected = 0;
                            picker.scroll = 0;
                        } else {
                            char full_path[MAX_PATH];
                            snprintf(full_path, MAX_PATH, "%s/%s", picker.current_path, picker.items[picker.selected].name);
                            result = strdup(full_path);
                            quit = 1;
                        }
                    }
                }
            } else if (e.type == SDL_TEXTINPUT && picker.active) {
                strcat(picker.search, e.text.text);
            }
        }

        draw_file_picker();
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_CloseFont(font);
    TTF_Quit();
    SDL_Quit();

    return result;
}

int main(int argc, char* argv[]) {
    char* selected_file = run_file_picker(argc > 1 ? argv[1] : ".");
    if (selected_file) {
        printf("Selected file: %s\n", selected_file);
        free(selected_file);
    } else {
        printf("No file selected.\n");
    }
    return 0;
}
