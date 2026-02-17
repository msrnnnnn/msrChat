# msrChat

A chat application with a Qt client and C++ server.

## Project Structure

- `client/`: Qt client source code
- `server/`: C++ backend services (GateServer, etc.)
- `shared/`: Shared protocol definitions (Protobuf)

## Build Instructions

### Server
See `server/GateServer/README.md` (if available) or check `server/GateServer/CMakeLists.txt`.

### Client
Open `client/QmsrChat/CMakeLists.txt` (or .pro) with Qt Creator.
