#!/bin/bash

# Configuration
SOURCE_FILE="graph_viewer.c"
EXECUTABLE="graph_viewer"
#SAMPLE_FILE="data.json"
SAMPLE_FILE="object_graph.json"

COMPILE_COMMAND="./build.sh"
RUN_COMMAND="./$EXECUTABLE $SAMPLE_FILE &"


# Function to check if file has changed
file_changed() {
    local file=$1
    local last_mod=$2
    local current_mod=$(stat -c %Y "$file" 2>/dev/null || stat -f %m "$file" 2>/dev/null)
    [ "$current_mod" != "$last_mod" ]
}

# Initial compilation and run
echo "Compiling..."
$COMPILE_COMMAND
if [ $? -eq 0 ]; then
    echo "Starting program..."
    $RUN_COMMAND
    PROGRAM_PID=$!
else
    echo "Initial compilation failed. Please fix the errors and try again."
    exit 1
fi

LAST_MODIFIED=$(stat -c %Y "$SOURCE_FILE" 2>/dev/null || stat -f %m "$SOURCE_FILE" 2>/dev/null)

# Watch for changes and hot reload
while true; do
    sleep 1  # Check for changes every second
    
    if file_changed "$SOURCE_FILE" "$LAST_MODIFIED"; then
        echo "Change detected. Recompiling..."
        LAST_MODIFIED=$(stat -c %Y "$SOURCE_FILE" 2>/dev/null || stat -f %m "$SOURCE_FILE" 2>/dev/null)
        
        # Attempt to compile
        echo "Compiling..."
        $COMPILE_COMMAND
        if [ $? -eq 0 ]; then
            echo "Compilation successful. Restarting program..."
            kill $PROGRAM_PID 2>/dev/null
            wait $PROGRAM_PID 2>/dev/null
            echo "Starting program..."
            $RUN_COMMAND &
            PROGRAM_PID=$!
        else
            echo "Compilation failed. Keeping the current version running."
        fi
    fi
done
