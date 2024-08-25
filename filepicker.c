#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>

#define MAX_FILES 100
#define MAX_PATH 1024

typedef struct {
    char name[256];
    int is_dir;
} FileEntry;

char* show_file_picker(SDL_Renderer* renderer, TTF_Font* font) {
    char current_dir[MAX_PATH];
    getcwd(current_dir, sizeof(current_dir));

    FileEntry files[MAX_FILES];
    int file_count = 0;
    int selected_index = 0;
    char* selected_file = NULL;

    SDL_Window* picker_window = SDL_CreateWindow("File Picker",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 400, 300, 0);
    SDL_Renderer* picker_renderer = SDL_CreateRenderer(picker_window, -1, SDL_RENDERER_ACCELERATED);

    int quit = 0;
    while (!quit) {
        // Read directory contents
        DIR* dir = opendir(current_dir);
        file_count = 0;
        if (dir) {
            struct dirent* entry;
            while ((entry = readdir(dir)) != NULL && file_count < MAX_FILES) {
                strcpy(files[file_count].name, entry->d_name);
                files[file_count].is_dir = (entry->d_type == DT_DIR);
                file_count++;
            }
            closedir(dir);
        }

        // Handle events
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                quit = 1;
            } else if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                    case SDLK_UP:
                        selected_index = (selected_index - 1 + file_count) % file_count;
                        break;
                    case SDLK_DOWN:
                        selected_index = (selected_index + 1) % file_count;
                        break;
                    case SDLK_RETURN:
                        if (files[selected_index].is_dir) {
                            if (strcmp(files[selected_index].name, "..") == 0) {
                                chdir("..");
                            } else {
                                chdir(files[selected_index].name);
                            }
                            getcwd(current_dir, sizeof(current_dir));
                            selected_index = 0;
                        } else {
                            selected_file = malloc(MAX_PATH);
                            snprintf(selected_file, MAX_PATH, "%s/%s", current_dir, files[selected_index].name);
                            quit = 1;
                        }
                        break;
                    case SDLK_ESCAPE:
                        quit = 1;
                        break;
                }
            }
        }

        // Render
        SDL_SetRenderDrawColor(picker_renderer, 255, 255, 255, 255);
        SDL_RenderClear(picker_renderer);

        SDL_Color text_color = {0, 0, 0, 255};
        for (int i = 0; i < file_count; i++) {
            SDL_Surface* surface = TTF_RenderText_Solid(font, files[i].name, text_color);
            SDL_Texture* texture = SDL_CreateTextureFromSurface(picker_renderer, surface);

            SDL_Rect dest_rect = {10, 10 + i * 30, surface->w, surface->h};
            SDL_RenderCopy(picker_renderer, texture, NULL, &dest_rect);

            if (i == selected_index) {
                SDL_SetRenderDrawColor(picker_renderer, 255, 0, 0, 255);
                SDL_RenderDrawRect(picker_renderer, &dest_rect);
            }

            SDL_FreeSurface(surface);
            SDL_DestroyTexture(texture);
        }

        SDL_RenderPresent(picker_renderer);
    }

    SDL_DestroyRenderer(picker_renderer);
    SDL_DestroyWindow(picker_window);

    return selected_file;
}

int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL initialization failed: %s\n", SDL_GetError());
        return 1;
    }

    if (TTF_Init() < 0) {
        fprintf(stderr, "SDL_ttf initialization failed: %s\n", TTF_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("SDL2 File Picker Test",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480, 0);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    TTF_Font* font = TTF_OpenFont("/path/to/your/font.ttf", 16);
    if (!font) {
        fprintf(stderr, "Failed to load font: %s\n", TTF_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    char* selected_file = show_file_picker(renderer, font);

    if (selected_file) {
        printf("Selected file: %s\n", selected_file);
        free(selected_file);
    } else {
        printf("No file selected.\n");
    }

    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();

    return 0;
}
