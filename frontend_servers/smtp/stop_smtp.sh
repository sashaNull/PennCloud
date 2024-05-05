#!/bin/bash

# Stop the smtp server by killing its process
echo "Stopping smtp server..."
pkill -f './smtp'