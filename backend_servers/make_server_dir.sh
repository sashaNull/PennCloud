#!/bin/bash

# Check if the correct number of arguments is provided
if [ "$#" -ne 3 ]; then
    echo "Usage: $0 start_index end_index directory"
    exit 1
fi

start=$1
end=$2
directory=$3

# Validate indices
if ! [[ "$start" =~ ^[0-9]+$ ]] || ! [[ "$end" =~ ^[0-9]+$ ]]; then
    echo "Error: start_index and end_index must be integers."
    exit 1
fi

# Validate and create the base directory if it does not exist
if [ ! -d "$directory" ]; then
    mkdir -p "$directory"
fi

# Create directories from serveri to serverj including serveri/logs for each
for (( i=start; i<=end; i++ ))
do
    mkdir -p "$directory/server$i/logs"
    echo "Created $directory/server$i/logs"
done
