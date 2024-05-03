#!/bin/bash

# Start the load balancer
make
echo "Starting load balancer..."
./loadbalancer -v ../frontend_servers/server_config.txt > ../logger/load_balancer.log 2>&1 &
echo "Load Balancer Started"