#!/bin/bash

# Check if the number of instances was provided
if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <number_of_instances>"
    exit 1
fi

n=$1
sleep 0.5
# Compile the server
make server
sleep 0.5
# Start server instances
for ((i=0; i<n; i++))
do
    ./server server_config.txt $i -v &> "./server_outputs/server_$i.log" &
    echo "Server $i started, outputting to server_$i.log. PID:"
    echo $!
done
