#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>

#define MAX_PATH 1024
#define MAX_FILES 1000
#define FONT_SIZE 14
#define SCROLLBAR_WIDTH 20
#define SEARCHBAR_HEIGHT 30

typedef struct {
    char name[MAX_PATH];
    int is_dir;
} FileEntry;

typedef struct {
    int position;
    int size;
    int max_position;
} ScrollBar;

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    TTF_Font *font;
    char current_dir[MAX_PATH];
    FileEntry files[MAX_FILES];
    int file_count;
    int selected_index;
    ScrollBar scrollbar;
    char search_text[MAX_PATH];
    int width;
    int height;
} FilePicker;

// Core functions
FilePicker* initialize_file_picker(const char* initial_dir);
void cleanup_file_picker(FilePicker* picker);
void get_directory_contents(FilePicker* picker);
void render_file_picker(FilePicker* picker);
void handle_events(FilePicker* picker, SDL_Event* event, int* quit, char** selected_file);
void update_scroll(FilePicker* picker);

// Utility functions
int is_directory(const char* path);
void get_parent_directory(char* path);
void filter_files(FilePicker* picker);

// Main file picker function
char* show_file_picker(const char* initial_dir);

// Test main function
int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        SDL_Log("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    if (TTF_Init() == -1) {
        SDL_Log("TTF_Init: %s\n", TTF_GetError());
        return 1;
    }

    char* selected_file = show_file_picker(".");

    if (selected_file) {
        printf("Selected file: %s\n", selected_file);
        free(selected_file);
    } else {
        printf("No file selected.\n");
    }

    TTF_Quit();
    SDL_Quit();

    return 0;
}

int is_directory(const char* path) {
    struct stat statbuf;
    if (stat(path, &statbuf) != 0)
        return 0;
    return S_ISDIR(statbuf.st_mode);
}

void get_parent_directory(char* path) {
    char* last_slash = strrchr(path, '/');
    if (last_slash != NULL) {
        *last_slash = '\0';
    }
}

void filter_files(FilePicker* picker) {
    int j = 0;
    for (int i = 0; i < picker->file_count; i++) {
        if (strstr(picker->files[i].name, picker->search_text) != NULL) {
            picker->files[j] = picker->files[i];
            j++;
        }
    }
    picker->file_count = j;
}

FilePicker* initialize_file_picker(const char* initial_dir) {
    FilePicker* picker = (FilePicker*)malloc(sizeof(FilePicker));
    if (!picker) return NULL;

    picker->window = SDL_CreateWindow("File Picker", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 640, 480, SDL_WINDOW_RESIZABLE);
    if (!picker->window) {
        free(picker);
        return NULL;
    }

    picker->renderer = SDL_CreateRenderer(picker->window, -1, SDL_RENDERER_ACCELERATED);
    if (!picker->renderer) {
        SDL_DestroyWindow(picker->window);
        free(picker);
        return NULL;
    }

    picker->font = TTF_OpenFont("lemon.ttf", FONT_SIZE);
    if (!picker->font) {
        SDL_DestroyRenderer(picker->renderer);
        SDL_DestroyWindow(picker->window);
        free(picker);
        return NULL;
    }

    strncpy(picker->current_dir, initial_dir, MAX_PATH);
    picker->selected_index = 0;
    picker->search_text[0] = '\0';
    SDL_GetWindowSize(picker->window, &picker->width, &picker->height);

    get_directory_contents(picker);

    return picker;
}

void cleanup_file_picker(FilePicker* picker) {
    if (picker) {
        TTF_CloseFont(picker->font);
        SDL_DestroyRenderer(picker->renderer);
        SDL_DestroyWindow(picker->window);
        free(picker);
    }
}

void get_directory_contents(FilePicker* picker) {
    DIR* dir;
    struct dirent* entry;

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
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        strncpy(picker->files[picker->file_count].name, entry->d_name, MAX_PATH);
        char full_path[MAX_PATH];
        snprintf(full_path, MAX_PATH, "%s/%s", picker->current_dir, entry->d_name);
        picker->files[picker->file_count].is_dir = is_directory(full_path);
        picker->file_count++;
    }

    closedir(dir);

    if (picker->file_count == 0) {
        fprintf(stderr, "Warning: No files found in directory: %s\n", picker->current_dir);
    }
}

void render_file_picker(FilePicker* picker) {
    SDL_SetRenderDrawColor(picker->renderer, 255, 255, 255, 255);
    SDL_RenderClear(picker->renderer);

    SDL_Color text_color = {0, 0, 0, 255};
    SDL_Color highlight_color = {200, 200, 255, 255};

    int y = SEARCHBAR_HEIGHT;
    for (int i = 0; i < picker->file_count; i++) {
        SDL_Color bg_color = (i == picker->selected_index) ? highlight_color : (SDL_Color){255, 255, 255, 255};
        SDL_Rect bg_rect = {0, y, picker->width - SCROLLBAR_WIDTH, FONT_SIZE + 4};
        SDL_SetRenderDrawColor(picker->renderer, bg_color.r, bg_color.g, bg_color.b, bg_color.a);
        SDL_RenderFillRect(picker->renderer, &bg_rect);

        SDL_Surface* surface = TTF_RenderText_Solid(picker->font, picker->files[i].name, text_color);
        SDL_Texture* texture = SDL_CreateTextureFromSurface(picker->renderer, surface);
        SDL_Rect dest = {5, y, surface->w, surface->h};
        SDL_RenderCopy(picker->renderer, texture, NULL, &dest);
        SDL_FreeSurface(surface);
        SDL_DestroyTexture(texture);

        y += FONT_SIZE + 4;
    }

    // Render scrollbar
    SDL_Rect scrollbar_bg = {picker->width - SCROLLBAR_WIDTH, SEARCHBAR_HEIGHT, SCROLLBAR_WIDTH, picker->height - SEARCHBAR_HEIGHT};
    SDL_SetRenderDrawColor(picker->renderer, 200, 200, 200, 255);
    SDL_RenderFillRect(picker->renderer, &scrollbar_bg);

    if (picker->file_count > 0) {
        int scrollbar_height = (picker->height - SEARCHBAR_HEIGHT) * (picker->height - SEARCHBAR_HEIGHT) / (picker->file_count * (FONT_SIZE + 4));
        int scrollbar_y = SEARCHBAR_HEIGHT;
        if (picker->scrollbar.max_position > 0) {
            scrollbar_y += (picker->height - SEARCHBAR_HEIGHT - scrollbar_height) * picker->scrollbar.position / picker->scrollbar.max_position;
        }
        SDL_Rect scrollbar = {picker->width - SCROLLBAR_WIDTH, scrollbar_y, SCROLLBAR_WIDTH, scrollbar_height};
        SDL_SetRenderDrawColor(picker->renderer, 100, 100, 100, 255);
        SDL_RenderFillRect(picker->renderer, &scrollbar);
    }

    // Render search bar
    SDL_Rect searchbar_bg = {0, 0, picker->width, SEARCHBAR_HEIGHT};
    SDL_SetRenderDrawColor(picker->renderer, 240, 240, 240, 255);
    SDL_RenderFillRect(picker->renderer, &searchbar_bg);

    SDL_Surface* search_surface = TTF_RenderText_Solid(picker->font, picker->search_text, text_color);
    SDL_Texture* search_texture = SDL_CreateTextureFromSurface(picker->renderer, search_surface);
    SDL_Rect search_dest = {5, 5, search_surface->w, search_surface->h};
    SDL_RenderCopy(picker->renderer, search_texture, NULL, &search_dest);
    SDL_FreeSurface(search_surface);
    SDL_DestroyTexture(search_texture);

    SDL_RenderPresent(picker->renderer);
}

void handle_events(FilePicker* picker, SDL_Event* event, int* quit, char** selected_file) {
    switch (event->type) {
        case SDL_QUIT:
            *quit = 1;
            break;
        case SDL_KEYDOWN:
            if (event->key.keysym.sym == SDLK_UP) {
                if (picker->selected_index > 0) picker->selected_index--;
            } else if (event->key.keysym.sym == SDLK_DOWN) {
                if (picker->selected_index < picker->file_count - 1) picker->selected_index++;
            } else if (event->key.keysym.sym == SDLK_RETURN) {
                if (picker->files[picker->selected_index].is_dir) {
                    if (strcmp(picker->files[picker->selected_index].name, "..") == 0) {
                        get_parent_directory(picker->current_dir);
                    } else {
                        char new_dir[MAX_PATH];
                        snprintf(new_dir, MAX_PATH, "%s/%s", picker->current_dir, picker->files[picker->selected_index].name);
                        strncpy(picker->current_dir, new_dir, MAX_PATH);
                    }
                    get_directory_contents(picker);
                    picker->selected_index = 0;
                } else {
                    char full_path[MAX_PATH];
                    snprintf(full_path, MAX_PATH, "%s/%s", picker->current_dir, picker->files[picker->selected_index].name);
                    *selected_file = strdup(full_path);
                    *quit = 1;
                }
            } else if (event->key.keysym.sym == SDLK_BACKSPACE) {
                int len = strlen(picker->search_text);
                if (len > 0) {
                    picker->search_text[len - 1] = '\0';
                    get_directory_contents(picker);
                    filter_files(picker);
                }
            } else if (event->key.keysym.sym == SDLK_ESCAPE) {
                *quit = 1;
            }
            break;
        case SDL_TEXTINPUT:
            strcat(picker->search_text, event->text.text);
            get_directory_contents(picker);
            filter_files(picker);
            break;
        case SDL_WINDOWEVENT:
            if (event->window.event == SDL_WINDOWEVENT_RESIZED) {
                picker->width = event->window.data1;
                picker->height = event->window.data2;
            }
            break;
    }
    update_scroll(picker);
}

void update_scroll(FilePicker* picker) {
    int total_height = picker->file_count * (FONT_SIZE + 4);
    int visible_height = picker->height - SEARCHBAR_HEIGHT;
    picker->scrollbar.max_position = (total_height > visible_height) ? (total_height - visible_height) : 0;
    picker->scrollbar.size = (visible_height * visible_height) / total_height;
    picker->scrollbar.position = picker->selected_index * (FONT_SIZE + 4);
    if (picker->scrollbar.position > picker->scrollbar.max_position)
        picker->scrollbar.position = picker->scrollbar.max_position;
}

char* show_file_picker(const char* initial_dir) {
    FilePicker* picker = initialize_file_picker(initial_dir);
    if (!picker) return NULL;

    char* selected_file = NULL;
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
