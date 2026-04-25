# Air Defense System

An interactive 3D graphics simulation of an automated air defense system built with OpenGL. The project features a military air defense vehicle equipped with radar detection, missile tracking, and guidance systems to neutralize incoming aerial threats.

## 📋 Overview

This is a **Computer Graphics (CG) project** that simulates real-time air defense operations. The application renders a night-time urban environment with:

- **Defense Vehicle**: A tracked military platform with rotating radar dish and three-tube missile launcher
- **Radar System**: Rotating antenna with sweeping detection range di
- **Dynamic Effects**: Explosions, particle systems, environmental details

## ✨ Features
splay
- **Enemy Drones**: AI-controlled aerial targets with flight animations
- **Missile Guidance**: Guided missiles with smoke trails targeting detected threats
- **Real-time Radar Detection**: Rotating radar dish detects incoming drones within a defined range
- **Automated Missile Guidance**: Missiles automatically track and intercept detected targets
- **Particle Effects**: Realistic explosion particles (sparks, smoke, debris)
- **Immersive Environment**:
  - Night sky with stars and moon
  - Urban cityscape with buildings
  - Parallax scrolling clouds
  - Street lights and road infrastructure
  - Traffic vehicles (cars and motorcycles)
- **Game Controls**: Spawn enemies, pause simulation, manual missile launch
- **HUD Display**: Real-time status information and system telemetry

## 🛠️ Prerequisites

- **C++ Compiler** (supporting C++11 or later)
- **OpenGL 2.1+** and GLUT (FreeGLUT)
- **CMake** or direct compilation with g++/clang

### Installation Requirements

**Linux (Ubuntu/Debian):**

```bash
sudo apt-get install freeglut3-dev libglu1-mesa-dev
```

**Windows (MSVC):**

- Install Visual Studio with OpenGL support
- Link GLUT libraries in your project

**macOS:**

```bash
brew install freeglut
```

## 🚀 Build & Run

### Using direct compilation (Linux/macOS):

```bash
g++ -o air-defense-system main.cpp -lGL -lglut -lm
./air-defense-system
```

### Using CMake:

```bash
mkdir build
cd build
cmake ..
make
./air-defense-system
```

## 🎮 Controls

| Key   | Action                  |
| ----- | ----------------------- |
| `S`   | Spawn a new enemy drone |
| `P`   | Pause/Resume simulation |
| `ESC` | Exit program            |

## 📐 Project Structure

### Core Components

- **Game Entities**:
  - `Drone`: Enemy aircraft with detection states
  - `Missile`: Guided projectiles with trajectory calculations
  - `Explosion`: Dynamic blast effects with expanding radius
  - `Particle`: Individual particle effects (sparks, smoke, debris)
  - `Vehicle`: Traffic simulation (cars, motorcycles)
  - `Cloud`: Parallax background elements

- **Systems**:
  - **Radar System**: Detection and sweep animation with configurable range
  - **Missile Launcher**: 3-tube launcher with guidance logic
  - **Particle Engine**: Real-time particle generation and physics
  - **Rendering Pipeline**: 2D orthographic projection with layered drawing

### Key Functions

- `drawDefenseVehicle()`: Renders the main defense platform
- `drawRadar()`: Renders radar display with sweep animation
- `drawDrone()`: Renders enemy aircraft with banking animation
- `drawExplosion()`: Renders blast effects with color gradients
- `update()`: Main game loop logic (~60 FPS)
- `display()`: Scene rendering pipeline

## 📊 Technical Details

- **Rendering**: 2D orthographic projection (1200×700 window)
- **Architecture**: Procedural generation with pool-based entity allocation
- **Animation**: Frame-based timing with sine/cosine functions
- **Collision**: Distance-based hit detection between missiles and drones

## 🎯 Gameplay Mechanics

1. Drones spawn periodically and fly from right to left
2. Radar detects drones within range (580 units)
3. Detected drones appear as blips on radar display
4. Missiles automatically launch to intercept targets
5. On impact, explosions generate particle effects
6. Destroyed drones are removed from the simulation

## � কন্ট্রিবিউশন গাইড

এই প্রজেক্টে কন্ট্রিবিউট করতে নিচের ধাপগুলো অনুসরণ করো:

### Step 1: রিপোজিটরি ক্লোন করো

```bash
git clone https://github.com/Ashraf1551/air-defense-system.git
```

### Step 2: নতুন ব্রাঞ্চ তৈরি করো (তোমার nickname ব্যবহার করো)

```bash
git checkout -b your_nickname
```

### Step 3: ব্রাঞ্চ পুশ করো

```bash
git push -u origin your_nickname
```

### Step 4: Main থেকে latest update নেওয়া (Merge)

```bash
git checkout your_nickname
git merge main
```

### Step 5: পরিবর্তন commit এবং push করা

```bash
git add .
git commit -m "give a meaningful commit message"
git push -u origin your_branch_name
```

## 📋 নিয়মাবলী

- কাজ করার আগে অবশ্যই নিজের ব্রাঞ্চ তৈরি করতে হবে
- সরাসরি `main` ব্রাঞ্চে push করা যাবে না
- প্রতিটি পরিবর্তনের আগে `main` থেকে latest update নিয়ে নিতে হবে
- commit message পরিষ্কার এবং meaningful হতে হবে

## �📜 License

[Specify your license here]

## 👥 Authors

- Ashraf1551 and Contributors

এই প্রজেক্টে কন্ট্রিবিউট করতে নিচের ধাপগুলো অনুসরণ করো:

---

### Step 1: রিপোজিটরি ক্লোন করো

```bash
git clone https://github.com/Ashraf1551/air-defense-system.git
```

---

### Step 2: নতুন ব্রাঞ্চ তৈরি করো (তোমার nickname ব্যবহার করো)

```bash
git checkout -b your_nickname
```

উদাহরণ:

```bash
git checkout -b ashraf
```

---

### Step 3: ব্রাঞ্চ পুশ করো

```bash
git push -u origin your_nickname
```

উদাহরণ:

```bash
git push -u origin ashraf
```

---

### Step 4: Main থেকে latest update নেওয়া (Merge)

কাজ শুরু করার আগে বা নিয়মিতভাবে main branch এর নতুন পরিবর্তন নিজের ব্রাঞ্চে আনতে হবে:

```bash
git checkout your_nickname
git merge main
```

উদাহরণ:

```bash
git checkout ashraf
git merge main
```

---

### Step 5: পরিবর্তন commit এবং push করা

কোনো কাজ করার পর নিচের কমান্ডগুলো ব্যবহার করো:

```bash
git add .
git commit -m "give a meaningful commit message"
git push -u origin your_branch_name
```

## নিয়মাবলী

- কাজ করার আগে অবশ্যই নিজের ব্রাঞ্চ তৈরি করতে হবে
- সরাসরি `main` ব্রাঞ্চে push করা যাবে না
- প্রতিটি পরিবর্তনের আগে `main` থেকে latest update নিয়ে নিতে হবে
- commit message পরিষ্কার এবং meaningful হতে হবে
