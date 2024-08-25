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

// TODO: Implement the stubbed functions
FilePicker* initialize_file_picker(const char* initial_dir) {
    // Implementation
}

void cleanup_file_picker(FilePicker* picker) {
    // Implementation
}

void get_directory_contents(FilePicker* picker) {
    // Implementation
}

void render_file_picker(FilePicker* picker) {
    // Implementation
}

void handle_events(FilePicker* picker, SDL_Event* event, int* quit, char** selected_file) {
    // Implementation
}

void update_scroll(FilePicker* picker) {
    // Implementation
}

int is_directory(const char* path) {
    // Implementation
}

void get_parent_directory(char* path) {
    // Implementation
}

void filter_files(FilePicker* picker) {
    // Implementation
}

char* show_file_picker(const char* initial_dir) {
    // Implementation
}
