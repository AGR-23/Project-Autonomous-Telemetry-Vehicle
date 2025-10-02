#!/bin/bash

# Autonomous Vehicle Telemetry Observer - Startup Script

# Script directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
CLIENT_DIR="$SCRIPT_DIR/client"

# Default values
DEFAULT_HOST="127.0.0.1"
DEFAULT_PORT="9000"
DEFAULT_NAME="observer"

# Parse command line arguments
HOST=${1:-$DEFAULT_HOST}
PORT=${2:-$DEFAULT_PORT}
CLIENT_NAME=${3:-$DEFAULT_NAME}

echo "==========================================="
echo " Autonomous Vehicle Telemetry Observer"
echo "==========================================="
echo "Connecting to: $HOST:$PORT"
echo "Client name: $CLIENT_NAME"
echo "==========================================="

# Check if client directory exists
if [ ! -d "$CLIENT_DIR" ]; then
    echo "Error: Client directory not found at $CLIENT_DIR"
    exit 1
fi

# Change to client directory
cd "$CLIENT_DIR"

# Check if Maven is available
if ! command -v mvn &> /dev/null; then
    echo "Error: Maven is not installed or not in PATH"
    echo "Please install Maven to run this application"
    exit 1
fi

# Run the JavaFX application
echo "Starting JavaFX application..."
mvn javafx:run -Djavafx.args="$HOST $PORT $CLIENT_NAME"