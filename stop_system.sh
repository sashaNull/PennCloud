#!/bin/bash

echo "Stopping all services..."

(cd backend_servers && ./stop_servers.sh)
(cd frontend_servers && ./stop_servers.sh)
(cd coordinator_node && ./stop_coordinator.sh)
(cd load_balancer && ./stop_loadbalancer.sh)
(cd frontend_servers && cd smtp && ./stop_smtp.sh)


echo "All services have been stopped."
