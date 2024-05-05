#!/bin/bash

# Check if two arguments are given
if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <backend_server_arg> <frontend_server_arg>"
    exit 1
fi

backend_arg=$1
frontend_arg=$2

# Create a directory to store logs if it doesn't already exist
mkdir -p logger

# Start services using the new scripts

(cd coordinator_node && ./start_coordinator.sh)
(cd load_balancer && ./start_loadbalancer.sh)
(cd backend_servers && ./start_servers.sh $backend_arg)
(cd frontend_servers && ./start_servers.sh $frontend_arg)
(cd frontend_servers && cd smtp && ./start_smtp.sh)

echo "All services have been started and are logging to the 'logger' folder."
