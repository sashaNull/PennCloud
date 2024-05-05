#!/bin/bash

# Navigate to the directory containing the backend_servers folder if it's not in the same directory
# cd /path/to/your/backend_servers/parent/directory

echo "Resetting data directories..."

# Remove all files and directories within backend_servers/data recursively
rm -rf backend_servers/data/*

# Check for errors in removal
if [ $? -ne 0 ]; then
    echo "Failed to remove data."
    exit 1
fi

# Run the make_server_dir.sh script with required arguments
./backend_servers/make_server_dir.sh 0 8 ./backend_servers/data/

# Check for successful execution of the make_server_dir.sh script
if [ $? -ne 0 ]; then
    echo "Failed to execute make_server_dir.sh."
    exit 1
fi

echo "Data directories reset successfully."
