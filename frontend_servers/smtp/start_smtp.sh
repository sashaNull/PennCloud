#!/bin/bash

# Start the smtp server
make
echo "Starting SMTP server..."
./smtp -v  > ../../logger/smtp_server.log 2>&1 &
echo "SMTP server tarted"