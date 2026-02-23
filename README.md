# TrainGame – Railway Simulation

## Description
This project implements a train management and simulation game using the GFX Framework.  
Players manage trains, stations, and passengers on a dynamic grid. Features include:

- Procedurally generated terrain: grass, rivers, and mountains.
- Station placement and passenger spawning.
- Train movement along rails with realistic locomotive and wagon rendering.
- Score tracking and game-over conditions based on station capacity.

## Requirements
- C++17 or higher
- Visual Studio 2022 or equivalent
- CMake 3.16+ (for generating project files)
- OpenGL 3.3+ compatible GPU

## Build Instructions

1. Clone the repository:

git clone <your-repo-url>
cd TrainGame

2. Create a build folder:

mkdir build
cd build

3. Generate Visual Studio project files with CMake:

cmake ..

4. Build the project:

Open the generated .sln file in Visual Studio and build the project.

## Running the Game

After building, run the game by executing:

build\bin\Debug\GFXFramework.exe

## Gameplay:

Trains spawn at stations and pick up/drop off passengers.
Manage train routes to avoid overcrowding stations.
Score points by delivering passengers correctly. Game ends if any station becomes full for too long.

## Note

All assets (models, shaders, fonts) are included in the repository.

Code is organized around the TrainGame class.

Camera and rendering are handled via the GFX Framework.

The project uses a separate build folder to generate Visual Studio files and binaries.

## License

This project is licensed under the MIT License. See LICENSE.md for details.
