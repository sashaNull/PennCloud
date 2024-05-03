#!/bin/bash

# Start the coordinator
make
echo "Starting coordinator..."
./coordinator -v ../backend_servers/server_config.txt > ../logger/coordinator.log 2>&1 &
echo "Coordinator Started"