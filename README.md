🚦 Animated City Traffic Simulation

Overview
Animated City Traffic Simulation is a dynamic 2D city scene built using C++ and OpenGL (GLUT). It showcases animated vehicles, pedestrians, environmental elements, and a realistic day/night cycle — all combined to bring a lively urban setting to life.

This project demonstrates animation principles, object interaction logic, time-based events, and simple AI behavior in an OpenGL environment.

✨ Features
Realistic Day/Night Cycle

Smooth sky color transitions from morning to night.

Sun and Moon movement across the sky.

Buildings light up at night; vehicle headlights and streetlights activate automatically.

Animated Traffic

Multiple vehicle types (cars, buses, trucks) with different speeds and sizes.

Two-way traffic flow with basic predictive braking and lane behavior.

Traffic lights controlling vehicle movement with realistic stop/go rules.

Pedestrian Movement

Pedestrians walk on sidewalks during the day.

Crosswalk logic where pedestrians wait for a green signal to cross.

Pedestrian behavior changes at night (walking stops, pedestrians become semi-transparent).

Dynamic Sky Elements

Clouds move across the sky (daytime only).

Birds flying animations during daytime.

Cityscape Details

Buildings of various shapes and heights.

Trees, roads with lane markings, sidewalks, a control tower structure.

Performance Optimized

Uses basic OpenGL primitives (quads, triangles, circles) for smooth, efficient rendering.

🛠 Requirements
C++ Compiler (e.g., g++)

OpenGL and GLUT (or FreeGLUT)

📦 Installation
Linux (Ubuntu/Debian):

bash
Copy
Edit
sudo apt update
sudo apt install freeglut3-dev g++
Fedora:

bash
Copy
Edit
sudo dnf install freeglut-devel gcc-c++
macOS (with Homebrew):

bash
Copy
Edit
brew install freeglut gcc
Windows:

Install MinGW or use Visual Studio.

Manually configure GLUT/FreeGLUT libraries or use a package manager like vcpkg.

🚀 How to Build and Run
Clone or download the source code.

Open a terminal in the project folder.

Compile the project:

bash
Copy
Edit
g++ main.cpp -o AnimatedCityTrafficSim -lGL -lglut -lGLU -lm
Run the executable:

bash
Copy
Edit
./AnimatedCityTrafficSim
🧩 Code Structure
Global Variables & Configs: Window settings, timing, animation states.

Structs:

Vehicle, Pedestrian, Tree, StreetLight, Cloud, Bird, Color, Point

Helper Functions: Drawing shapes, interpolations (lerp), collision prediction, time handling.

Drawing Functions:

Buildings, roads, vehicles, pedestrians, streetlights, sky, clouds, sun, moon.

Animation/Update Logic:

Traffic light logic, vehicle acceleration, pedestrian movement, day/night cycle progression.

Main Loop & Setup:

OpenGL/GLUT setup, event handlers, refresh timers.

🌟 Future Enhancements
Smarter vehicle AI (dynamic speed changes, collision avoidance, lane switching).

Enhanced pedestrian AI (group crossing, waiting time variation).

Weather effects like rain, thunder, or snowfall.

Interactive controls (camera movement, speed adjustment).

Optimized lighting effects for better realism.


👤 Author
Created by Md Seyam Ali Biswas.
Feel free to connect or contribute!
