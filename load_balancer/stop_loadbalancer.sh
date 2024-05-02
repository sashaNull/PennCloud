#!/bin/bash

# Stop the load balancer by killing its process
echo "Stopping load balancer..."
pkill -f './loadbalancer'
