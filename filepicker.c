#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define MAX_PATH 1024
#define MAX_FILES 1000
#define FONT_SIZE 14
#define SCROLL_SPEED 20
#define SEARCHBAR_HEIGHT 30
#define SCROLLBAR_WIDTH 20

typedef struct {
    char name[256];
    int is_dir;
} FileEntry;

typedef struct {
    FileEntry entries[MAX_FILES];
    int count;
    int selected;
    int scroll;
    char current_path[MAX_PATH];
    char search[256];
    int searchbar_active;
} FilePicker;

typedef struct {
    SDL_Window* window;
    SDL_Renderer* renderer;
    TTF_Font* font;
    FilePicker picker;
} AppState;

AppState app = {0};

void log_error(const char* message) {
    fprintf(stderr, "Error: %s\n", message);
}

int init_sdl() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        log_error(SDL_GetError());
        return 0;
    }

    if (TTF_Init() == -1) {
        log_error(TTF_GetError());
        return 0;
    }

    app.window = SDL_CreateWindow("File Picker", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 
                                  640, 480, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (app.window == NULL) {
        log_error(SDL_GetError());
        return 0;
    }

    app.renderer = SDL_CreateRenderer(app.window, -1, SDL_RENDERER_ACCELERATED);
    if (app.renderer == NULL) {
        log_error(SDL_GetError());
        return 0;
    }

    app.font = TTF_OpenFont("lemon.ttf", FONT_SIZE);
    if (app.font == NULL) {
        log_error(TTF_GetError());
        return 0;
    }

    return 1;
}

void cleanup() {
    if (app.font) TTF_CloseFont(app.font);
    if (app.renderer) SDL_DestroyRenderer(app.renderer);
    if (app.window) SDL_DestroyWindow(app.window);
    TTF_Quit();
    SDL_Quit();
}

void load_directory() {
    DIR* dir;
    struct dirent* ent;
    struct stat st;
    char full_path[MAX_PATH];

    picker.count = 0;
    picker.selected = 0;
    picker.scroll = 0;

    // Add parent directory entry
    strcpy(picker.entries[picker.count].name, "..");
    picker.entries[picker.count].is_dir = 1;
    picker.count++;

    if ((dir = opendir(picker.current_path)) != NULL) {
        while ((ent = readdir(dir)) != NULL && picker.count < MAX_FILES) {
            if (strcmp(ent->d_name, ".") == 0) continue;

            snprintf(full_path, sizeof(full_path), "%s/%s", picker.current_path, ent->d_name);
            
            if (stat(full_path, &st) == 0) {
                if (S_ISDIR(st.st_mode) || S_ISREG(st.st_mode)) {
                    if (picker.search[0] == '\0' || strstr(ent->d_name, picker.search) != NULL) {
                        strcpy(picker.entries[picker.count].name, ent->d_name);
                        picker.entries[picker.count].is_dir = S_ISDIR(st.st_mode);
                        picker.count++;
                    }
                }
            }
        }
        closedir(dir);
    }
}

void render_text(const char* text, int x, int y, SDL_Color color) {
    if (!app.font || !app.renderer) {
        log_error("Font or renderer not initialized");
        return;
    }

    SDL_Surface* surface = TTF_RenderText_Solid(app.font, text, color);
    if (!surface) {
        log_error(TTF_GetError());
        return;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(app.renderer, surface);
    if (!texture) {
        log_error(SDL_GetError());
        SDL_FreeSurface(surface);
        return;
    }
    
    SDL_Rect rect = {x, y, surface->w, surface->h};
    SDL_RenderCopy(app.renderer, texture, NULL, &rect);
    
    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}

void render_file_picker() {
    int window_width, window_height;
    SDL_GetWindowSize(app.window, &window_width, &window_height);

    SDL_SetRenderDrawColor(app.renderer, 255, 255, 255, 255);
    SDL_RenderClear(app.renderer);

    // Render search bar
    SDL_SetRenderDrawColor(app.renderer, 200, 200, 200, 255);
    SDL_Rect searchbar_rect = {0, 0, window_width, SEARCHBAR_HEIGHT};
    SDL_RenderFillRect(app.renderer, &searchbar_rect);

    SDL_Color text_color = {0, 0, 0, 255};
    render_text(app.picker.search, 10, 5, text_color);

    // Render file list
    int y = SEARCHBAR_HEIGHT;
    for (int i = app.picker.scroll; i < app.picker.count && y < window_height - FONT_SIZE; i++) {
        SDL_Color color = (i == app.picker.selected) ? (SDL_Color){255, 0, 0, 255} : (SDL_Color){0, 0, 0, 255};
        char display_name[256];
        snprintf(display_name, sizeof(display_name), "%s%s", 
                 app.picker.entries[i].is_dir ? "[DIR] " : "", 
                 app.picker.entries[i].name);
        render_text(display_name, 10, y, color);
        y += FONT_SIZE + 5;
    }

    // Render scrollbar
    int content_height = app.picker.count * (FONT_SIZE + 5);
    int visible_height = window_height - SEARCHBAR_HEIGHT;
    if (content_height > visible_height) {
        float scroll_ratio = (float)app.picker.scroll / (app.picker.count - visible_height / (FONT_SIZE + 5));
        int scrollbar_height = (visible_height * visible_height) / content_height;
        int scrollbar_y = SEARCHBAR_HEIGHT + scroll_ratio * (visible_height - scrollbar_height);

        SDL_SetRenderDrawColor(app.renderer, 200, 200, 200, 255);
        SDL_Rect scrollbar_rect = {window_width - SCROLLBAR_WIDTH, scrollbar_y, SCROLLBAR_WIDTH, scrollbar_height};
        SDL_RenderFillRect(app.renderer, &scrollbar_rect);
    }

    SDL_RenderPresent(app.renderer);
}

void handle_key_event(SDL_KeyboardEvent* event) {
    if (event->type == SDL_KEYDOWN) {
        if (picker.searchbar_active) {
            if (event->keysym.sym == SDLK_BACKSPACE && strlen(picker.search) > 0) {
                picker.search[strlen(picker.search) - 1] = '\0';
                load_directory();
            } else if (event->keysym.sym == SDLK_RETURN) {
                picker.searchbar_active = 0;
            }
        } else {
            switch (event->keysym.sym) {
                case SDLK_UP:
                    if (picker.selected > 0) picker.selected--;
                    break;
                case SDLK_DOWN:
                    if (picker.selected < picker.count - 1) picker.selected++;
                    break;
                case SDLK_RETURN:
                    if (picker.entries[picker.selected].is_dir) {
                        if (strcmp(picker.entries[picker.selected].name, "..") == 0) {
                            char* last_slash = strrchr(picker.current_path, '/');
                            if (last_slash) *last_slash = '\0';
                        } else {
                            strcat(picker.current_path, "/");
                            strcat(picker.current_path, picker.entries[picker.selected].name);
                        }
                        load_directory();
                    }
                    break;
                case SDLK_ESCAPE:
                    SDL_Event quit_event;
                    quit_event.type = SDL_QUIT;
                    SDL_PushEvent(&quit_event);
                    break;
            }
        }
    }
}

void handle_text_input(SDL_TextInputEvent* event) {
    if (picker.searchbar_active && strlen(picker.search) < sizeof(picker.search) - 1) {
        strcat(picker.search, event->text);
        load_directory();
    }
}

void handle_mouse_wheel(SDL_MouseWheelEvent* event) {
    picker.scroll -= event->y * SCROLL_SPEED;
    if (picker.scroll < 0) picker.scroll = 0;
    int max_scroll = picker.count - (SDL_GetWindowSurface(window)->h - SEARCHBAR_HEIGHT) / (FONT_SIZE + 5);
    if (picker.scroll > max_scroll) picker.scroll = max_scroll;
}

void handle_mouse_button(SDL_MouseButtonEvent* event) {
    if (event->type == SDL_MOUSEBUTTONDOWN) {
        int x = event->x;
        int y = event->y;

        if (y < SEARCHBAR_HEIGHT) {
            picker.searchbar_active = 1;
        } else {
            picker.searchbar_active = 0;
            int selected = (y - SEARCHBAR_HEIGHT) / (FONT_SIZE + 5) + picker.scroll;
            if (selected >= 0 && selected < picker.count) {
                picker.selected = selected;
            }
        }
    }
}

char* run_file_picker(const char* initial_path) {
    strcpy(picker.current_path, initial_path);
    memset(picker.search, 0, sizeof(picker.search));
    picker.searchbar_active = 0;

    load_directory();

    SDL_StartTextInput();

    SDL_Event event;
    int running = 1;

    while (running) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running = 0;
                    break;
                case SDL_KEYDOWN:
                    handle_key_event(&event.key);
                    break;
                case SDL_TEXTINPUT:
                    handle_text_input(&event.text);
                    break;
                case SDL_MOUSEWHEEL:
                    handle_mouse_wheel(&event.wheel);
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    handle_mouse_button(&event.button);
                    break;
            }
        }

        render_file_picker();
        SDL_Delay(10);
    }

    SDL_StopTextInput();

    if (picker.selected >= 0 && picker.selected < picker.count && !picker.entries[picker.selected].is_dir) {
        char* result = malloc(MAX_PATH);
        snprintf(result, MAX_PATH, "%s/%s", picker.current_path, picker.entries[picker.selected].name);
        return result;
    }

    return NULL;
}

int main(int argc, char* argv[]) {
    if (!init_sdl()) {
        log_error("Failed to initialize SDL");
        return 1;
    }

    char* selected_file = run_file_picker(argc > 1 ? argv[1] : ".");

    if (selected_file) {
        printf("Selected file: %s\n", selected_file);
        free(selected_file);
    } else {
        printf("No file selected.\n");
    }

    cleanup();
    return 0;
}
