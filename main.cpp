#include <GL/glut.h>          // OpenGL utility toolkit for window and graphics
#include <cmath>              // Mathematical functions (sin, cos, sqrt, etc.)
#include <cstdio>             // Standard input/output
#include <cstdlib>            // Standard library utilities
#include <cstring>            // String manipulation
#include <ctime>              // Time functions for random seeding
#include <algorithm>          // STL algorithms (swap, etc.)

// ─── Mathematical & Display Constants ────────────────────────────────────────
#define PI              3.14159265358979323846f   // PI constant
#define WIN_W           1200                      // Window width in pixels
#define WIN_H           700                       // Window height in pixels
#define MAX_DRONES      6                         // Maximum number of drones
#define MAX_MISSILES    10                        // Maximum number of missiles
#define MAX_EXPLOSIONS  10                        // Maximum number of explosions
#define MAX_PARTICLES   80                        // Maximum number of particles

// ─── Data Structures ─────────────────────────────────────────────────────────

// Drone structure - represents enemy aircraft
struct Drone {
    float x, y;              // Position coordinates
    float speed;             // Movement speed
    bool  active;            // Whether drone is currently active
    bool  detected;          // Whether drone is detected by radar
    int   missileAssigned;   // Index of assigned missile (-1 if none)
};

// Missile structure - represents launched air defense missiles
struct Missile {
    float x, y;              // Current position
    float speed;             // Movement speed
    bool  active;            // Whether missile is active
    int   targetIdx;         // Index of target drone
};

// Explosion structure - represents blast effects
struct Explosion {
    float x, y;              // Center position of explosion
    float radius;            // Current radius of blast
    float maxRadius;         // Maximum expansion radius
    int   frame;             // Frame counter for animation
    bool  active;            // Whether explosion is active
};

// Particle structure - represents individual particles from explosions
struct Particle {
    float x, y;              // Position coordinates
    float vx, vy;            // Velocity components
    float life;              // Life span (0.0-1.0)
    float r, g, b;           // RGB color components
    int   type;              // Particle type (affects rendering)
    bool  active;            // Whether particle is alive
};

// Vehicle structure - represents ground vehicles (cars, motorcycles)
struct Vehicle {
    float x, y;              // Position coordinates
    float speed;             // Movement speed
    int   type;              // Vehicle type (0=car, 1=motorcycle)
    float r, g, b;           // RGB color components
};

// Cloud structure - represents atmospheric clouds
struct Cloud {
    float x, y;              // Position coordinates
    float speed;             // Horizontal movement speed
    float scale;             // Size scale factor
    bool  active;            // Whether cloud is visible
};

// ─── Global Variables ────────────────────────────────────────────────────────

// Radar system variables
float gRadarAngle   = 0.0f;                     // Current sweep angle in degrees
const float RADAR_X = 100.0f;                   // Radar center X coordinate
const float RADAR_Y = 580.0f;                   // Radar center Y coordinate
const float RADAR_R = 88.0f;                    // Radar display radius
const float RADAR_DETECT_RANGE = 580.0f;       // Detection range for drones

// Game object arrays
Drone      gDrones    [MAX_DRONES];             // Array of drone objects
Missile    gMissiles  [MAX_MISSILES];           // Array of missile objects
Explosion  gExplosions[MAX_EXPLOSIONS];         // Array of explosion effects
Particle   gParticles [MAX_PARTICLES];          // Array of particle effects
Vehicle    gVehicles  [5];                      // Array of ground vehicles
Cloud      gClouds    [20];                     // Array of cloud objects

// Game state variables
int  gSpawnTimer     = 0;                       // Timer for drone spawning
int  gSpawnInterval  = 200;                     // Frames between drone spawns
bool gPaused         = false;                   // Game pause state
int  gFrameCounter   = 0;                       // Total frames rendered

// Background elements
float gStarX[200], gStarY[200];                 // Star field coordinates
float gStreetLights[20][2];                     // Street light positions

// ─── Helper Functions ────────────────────────────────────────────────────────

// Calculate Euclidean distance between two points
static inline float fDist(float x1,float y1,float x2,float y2) {
    float dx=x2-x1, dy=y2-y1;
    return sqrtf(dx*dx+dy*dy);
}

// Set RGB color for OpenGL rendering
static void col3(float r,float g,float b){ glColor3f(r,g,b); }

// Set RGBA color with alpha channel for transparency
static void col4(float r,float g,float b,float a){ glColor4f(r,g,b,a); }

// ─── Line Drawing Algorithms ────────────────────────────────────────────────

// Bresenham's Line Algorithm (integer-based, optimal for raster graphics)
// Uses incremental error calculation for efficient line rasterization
static void drawLineBresenham(float x1, float y1, float x2, float y2)
{
    int x0 = (int)roundf(x1);
    int y0 = (int)roundf(y1);
    int x1_int = (int)roundf(x2);
    int y1_int = (int)roundf(y2);
    
    int dx = abs(x1_int - x0);
    int dy = abs(y1_int - y0);
    int sx = (x0 < x1_int) ? 1 : -1;
    int sy = (y0 < y1_int) ? 1 : -1;
    int err = dx - dy;
    
    glBegin(GL_POINTS);
    while (true) {
        glVertex2i(x0, y0);
        if (x0 == x1_int && y0 == y1_int) break;
        
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
    glEnd();
}

// DDA (Digital Differential Analyzer) Algorithm
// Uses floating-point increments - smoother but slower than Bresenham
static void drawLineDDA(float x1, float y1, float x2, float y2)
{
    float dx = x2 - x1;
    float dy = y2 - y1;
    float steps = fabs(dx) > fabs(dy) ? fabs(dx) : fabs(dy);
    
    if (steps < 0.001f) {
        glBegin(GL_POINTS);
        glVertex2f(x1, y1);
        glEnd();
        return;
    }
    
    float xInc = dx / steps;
    float yInc = dy / steps;
    float x = x1;
    float y = y1;
    
    glBegin(GL_POINTS);
    for (int i = 0; i <= (int)steps; i++) {
        glVertex2f(x, y);
        x += xInc;
        y += yInc;
    }
    glEnd();
}

// Wu's Anti-aliasing Algorithm
// Produces smooth lines through alpha blending at sub-pixel precision
static void drawLineWu(float x1, float y1, float x2, float y2)
{
    bool steep = fabs(y2 - y1) > fabs(x2 - x1);
    
    if (steep) {
        std::swap(x1, y1);
        std::swap(x2, y2);
    }
    
    if (x1 > x2) {
        std::swap(x1, x2);
        std::swap(y1, y2);
    }
    
    float dx = x2 - x1;
    float dy = y2 - y1;
    float gradient = (dx == 0) ? 1.0f : dy / dx;
    
    float xend = roundf(x1);
    float yend = y1 + gradient * (xend - x1);
    float xgap = 1.0f - fmodf(x1 + 0.5f, 1.0f);
    int xpxl1 = (int)xend;
    int ypxl1 = (int)yend;
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    if (steep) {
        glBegin(GL_POINTS);
        col4(1.0f, 1.0f, 1.0f, (1.0f - fmodf(yend, 1.0f)) * xgap);
        glVertex2i(ypxl1, xpxl1);
        col4(1.0f, 1.0f, 1.0f, fmodf(yend, 1.0f) * xgap);
        glVertex2i(ypxl1 + 1, xpxl1);
        glEnd();
    } else {
        glBegin(GL_POINTS);
        col4(1.0f, 1.0f, 1.0f, (1.0f - fmodf(yend, 1.0f)) * xgap);
        glVertex2i(xpxl1, ypxl1);
        col4(1.0f, 1.0f, 1.0f, fmodf(yend, 1.0f) * xgap);
        glVertex2i(xpxl1, ypxl1 + 1);
        glEnd();
    }
    
    float intery = yend + gradient;
    
    xend = roundf(x2);
    yend = y2 + gradient * (xend - x2);
    xgap = fmodf(x2 + 0.5f, 1.0f);
    int xpxl2 = (int)xend;
    int ypxl2 = (int)yend;
    
    if (steep) {
        glBegin(GL_POINTS);
        col4(1.0f, 1.0f, 1.0f, (1.0f - fmodf(yend, 1.0f)) * xgap);
        glVertex2i(ypxl2, xpxl2);
        col4(1.0f, 1.0f, 1.0f, fmodf(yend, 1.0f) * xgap);
        glVertex2i(ypxl2 + 1, xpxl2);
        glEnd();
    } else {
        glBegin(GL_POINTS);
        col4(1.0f, 1.0f, 1.0f, (1.0f - fmodf(yend, 1.0f)) * xgap);
        glVertex2i(xpxl2, ypxl2);
        col4(1.0f, 1.0f, 1.0f, fmodf(yend, 1.0f) * xgap);
        glVertex2i(xpxl2, ypxl2 + 1);
        glEnd();
    }
    
    for (int x = xpxl1 + 1; x <= xpxl2 - 1; x++) {
        if (steep) {
            glBegin(GL_POINTS);
            col4(1.0f, 1.0f, 1.0f, 1.0f - fmodf(intery, 1.0f));
            glVertex2i((int)intery, x);
            col4(1.0f, 1.0f, 1.0f, fmodf(intery, 1.0f));
            glVertex2i((int)intery + 1, x);
            glEnd();
        } else {
            glBegin(GL_POINTS);
            col4(1.0f, 1.0f, 1.0f, 1.0f - fmodf(intery, 1.0f));
            glVertex2i(x, (int)intery);
            col4(1.0f, 1.0f, 1.0f, fmodf(intery, 1.0f));
            glVertex2i(x, (int)intery + 1);
            glEnd();
        }
        intery += gradient;
    }
    
    glDisable(GL_BLEND);
}

// Midpoint Circle Algorithm
// Uses symmetry to draw circles efficiently with only one octant calculation
static void drawCircleMidpoint(float cx, float cy, float r)
{
    int xc = (int)roundf(cx);
    int yc = (int)roundf(cy);
    int radius = (int)roundf(r);
    
    if (radius <= 0) return;
    
    int x = 0;
    int y = radius;
    int d = 1 - radius;
    int deltaE = 3;
    int deltaSE = -2 * radius + 5;
    
    glBegin(GL_POINTS);
    
    // Plot initial points
    glVertex2i(xc + x, yc + y);
    glVertex2i(xc - x, yc + y);
    glVertex2i(xc + x, yc - y);
    glVertex2i(xc - x, yc - y);
    glVertex2i(xc + y, yc + x);
    glVertex2i(xc - y, yc + x);
    glVertex2i(xc + y, yc - x);
    glVertex2i(xc - y, yc - x);
    
    while (y > x) {
        if (d < 0) {
            d += deltaE;
            deltaE += 2;
            deltaSE += 2;
        } else {
            d += deltaSE;
            deltaE += 2;
            deltaSE += 4;
            y--;
        }
        x++;
        
        glVertex2i(xc + x, yc + y);
        glVertex2i(xc - x, yc + y);
        glVertex2i(xc + x, yc - y);
        glVertex2i(xc - x, yc - y);
        glVertex2i(xc + y, yc + x);
        glVertex2i(xc - y, yc + x);
        glVertex2i(xc + y, yc - x);
        glVertex2i(xc - y, yc - x);
    }
    
    glEnd();
}

// Bresenham's Circle Algorithm
// Alternative to midpoint - integer-based error calculation
static void drawCircleBresenham(float cx, float cy, float r)
{
    int xc = (int)roundf(cx);
    int yc = (int)roundf(cy);
    int radius = (int)roundf(r);
    
    if (radius <= 0) return;
    
    int x = 0;
    int y = radius;
    int d = 3 - 2 * radius;
    
    glBegin(GL_POINTS);
    
    glVertex2i(xc + x, yc + y);
    glVertex2i(xc - x, yc + y);
    glVertex2i(xc + x, yc - y);
    glVertex2i(xc - x, yc - y);
    glVertex2i(xc + y, yc + x);
    glVertex2i(xc - y, yc + x);
    glVertex2i(xc + y, yc - x);
    glVertex2i(xc - y, yc - x);
    
    while (y >= x) {
        x++;
        if (d > 0) {
            y--;
            d = d + 4 * (x - y) + 10;
        } else {
            d = d + 4 * x + 6;
        }
        
        glVertex2i(xc + x, yc + y);
        glVertex2i(xc - x, yc + y);
        glVertex2i(xc + x, yc - y);
        glVertex2i(xc - x, yc - y);
        glVertex2i(xc + y, yc + x);
        glVertex2i(xc - y, yc + x);
        glVertex2i(xc + y, yc - x);
        glVertex2i(xc - y, yc - x);
    }
    
    glEnd();
}

// Unified line drawing function with algorithm selection
// algorithm: 0=Bresenham, 1=DDA, 2=Wu's anti-aliasing
static void drawLine(float x1, float y1, float x2, float y2, int algorithm = 0)
{
    switch(algorithm) {
        case 0: drawLineBresenham(x1, y1, x2, y2); break;
        case 1: drawLineDDA(x1, y1, x2, y2); break;
        case 2: drawLineWu(x1, y1, x2, y2); break;
        default: drawLineBresenham(x1, y1, x2, y2);
    }
}

// Backward compatible line drawing (uses Bresenham by default)
static void drawLine(float x1, float y1, float x2, float y2)
{
    drawLineBresenham(x1, y1, x2, y2);
}

// Unified circle outline drawing with algorithm selection
// algorithm: 0=Midpoint, 1=Bresenham
static void drawCircleOutline(float cx, float cy, float r, int algorithm = 0)
{
    switch(algorithm) {
        case 0: drawCircleMidpoint(cx, cy, r); break;
        case 1: drawCircleBresenham(cx, cy, r); break;
        default: drawCircleMidpoint(cx, cy, r);
    }
}

// Backward compatible circle outline drawing (uses Midpoint by default)
static void drawCircleOutline(float cx, float cy, float r)
{
    drawCircleMidpoint(cx, cy, r);
}

// Draw filled circle using triangle fan (smooth, filled appearance)
// seg: number of segments for approximation (higher = smoother)
static void drawFilledCircle(float cx, float cy, float r, int seg = 60)
{
    if (r <= 0) return;
    
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);
    for (int i = 0; i <= seg; i++) {
        float a = 2.0f * PI * i / seg;
        glVertex2f(cx + r * cosf(a), cy + r * sinf(a));
    }
    glEnd();
}

// Draw horizontal line from x1 to x2 at Y coordinate
static void drawHorizontalLine(int x1, int x2, int y)
{
    if (x1 > x2) {
        std::swap(x1, x2);
    }
    
    glBegin(GL_POINTS);
    for (int x = x1; x <= x2; x++) {
        glVertex2i(x, y);
    }
    glEnd();
}

// Draw vertical line from y1 to y2 at X coordinate
static void drawVerticalLine(int x, int y1, int y2)
{
    if (y1 > y2) {
        std::swap(y1, y2);
    }
    
    glBegin(GL_POINTS);
    for (int y = y1; y <= y2; y++) {
        glVertex2i(x, y);
    }
    glEnd();
}

// Draw filled rectangle (quad) at position (x,y) with dimensions w,h
static void drawRect(float x,float y,float w,float h){
    glBegin(GL_QUADS);
    glVertex2f(x,  y);   glVertex2f(x+w,y);
    glVertex2f(x+w,y+h); glVertex2f(x,  y+h);
    glEnd();
}

// Draw text at screen position using specified bitmap font
static void drawText(float x,float y,const char* s,void* font=GLUT_BITMAP_HELVETICA_18){
    glRasterPos2f(x,y);
    for(;*s;s++) glutBitmapCharacter(font,*s);
}

// ─── Background & Environment ────────────────────────────────────────────────

// Draw individual cloud with multiple overlapping circles for organic shape
static void drawCloud(float cx, float cy, float scale)
{
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    col4(200.0f/255.0f, 232.0f/255.0f, 245.0f/255.0f, 0.65f);
    
    float radius = 18.0f * scale;
    
    drawFilledCircle(cx + 0*radius, cy + 0.08f*radius, 0.07f*radius, 20);
    drawFilledCircle(cx - 0.06f*radius, cy + 0.08f*radius, 0.07f*radius, 20);
    drawFilledCircle(cx + 0.06f*radius, cy + 0.085f*radius, 0.07f*radius, 20);
    drawFilledCircle(cx - 0.06f*2*radius, cy + 0.08f*radius, 0.07f*radius, 20);
    drawFilledCircle(cx + 0.05f*2*radius, cy + 0.08f*radius, 0.07f*radius, 20);
    drawFilledCircle(cx - 0.04f*2*radius, cy + 0.06f*2*radius, 0.07f*radius, 20);
    drawFilledCircle(cx + 0.05f*2*radius, cy + 0.05f*2*radius, 0.07f*radius, 20);
    drawFilledCircle(cx + 0.03f*radius, cy + 0.065f*2*radius, 0.07f*radius, 20);
    drawFilledCircle(cx - 0.05f*3*radius, cy + 0.04f*2*radius, 0.07f*radius, 20);
    
    glDisable(GL_BLEND);
}

// Draw all active clouds on screen
static void drawClouds()
{
    for(int i = 0; i < 20; i++){
        if(gClouds[i].active){
            drawCloud(gClouds[i].x, gClouds[i].y, gClouds[i].scale);
        }
    }
}

// Update cloud positions (horizontal scrolling)
static void updateClouds()
{
    for(int i = 0; i < 20; i++){
        if(gClouds[i].active){
            gClouds[i].x += gClouds[i].speed;
            if(gClouds[i].x > WIN_W + 150) gClouds[i].x = -150;  // Wrap around
        }
    }
}

// Draw street lights with poles and illumination glow
static void drawStreetLights()
{
    col3(0.15f, 0.15f, 0.18f);
    for(int i = 0; i < 20; i++){
        float sx = gStreetLights[i][0];
        float sy = gStreetLights[i][1];
        
        drawRect(sx - 2, sy, 4, 32);
        
        col3(0.30f, 0.30f, 0.35f);
        drawFilledCircle(sx, sy + 32, 8);
        
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        col4(1.00f, 0.95f, 0.70f, 0.15f);
        drawFilledCircle(sx, sy + 32, 16);
        col4(1.00f, 0.95f, 0.70f, 0.08f);
        drawFilledCircle(sx, sy + 32, 24);
        glDisable(GL_BLEND);
    }
}

// Draw complete background scene: sky, moon, stars, roads, buildings, lights
static void drawBackground()
{
    glBegin(GL_QUADS);
    col3(0.01f,0.04f,0.16f); glVertex2f(0,WIN_H); glVertex2f(WIN_W,WIN_H);
    col3(0.04f,0.09f,0.26f); glVertex2f(WIN_W,230); glVertex2f(0,230);
    glEnd();

    col3(0.12f,0.12f,0.14f); drawRect(0,0,WIN_W,230);
    col3(0.18f,0.18f,0.20f); drawRect(0,75,WIN_W,65);
    col3(0.22f,0.22f,0.25f); drawRect(0,140,WIN_W,65);
    col3(0.70f,0.70f,0.75f); drawRect(0,73,WIN_W,4);
    col3(0.70f,0.70f,0.75f); drawRect(0,201,WIN_W,4);

    col3(0.90f,0.85f,0.15f);
    glLineWidth(2.0f);
    for(int i=0;i<WIN_W;i+=80) drawLineBresenham((float)i,138,(float)(i+50),138);
    
    col3(0.60f,0.58f,0.10f);
    glLineWidth(1.0f);
    for(int i=0;i<WIN_W;i+=100) {
        drawLineBresenham((float)i,105,(float)(i+40),105);
        drawLineBresenham((float)i,171,(float)(i+40),171);
    }
    glLineWidth(1.0f);

    glPointSize(2.0f);
    glBegin(GL_POINTS);
    for(int i=0;i<200;i++){
        float brt=0.5f+0.5f*(i%3)/2.0f;
        col3(brt,brt,brt);
        glVertex2f(gStarX[i],gStarY[i]);
    }
    glEnd();
    glPointSize(1.0f);

    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    
    col4(0.98f,0.95f,0.80f,0.08f); drawFilledCircle(1110,630,58);
    col4(0.98f,0.95f,0.80f,0.04f); drawFilledCircle(1110,630,70);
    col3(0.93f,0.93f,0.78f); drawFilledCircle(1110,630,42);
    col4(0.98f,0.98f,0.88f,0.6f); drawFilledCircle(1095,640,12);
    col4(0.95f,0.95f,0.85f,0.4f); drawFilledCircle(1115,620,8);
    col4(0.96f,0.96f,0.86f,0.35f); drawFilledCircle(1105,615,6);
    col4(0.75f,0.75f,0.68f,0.7f); drawFilledCircle(1120,635,5);
    col4(0.78f,0.78f,0.70f,0.6f); drawFilledCircle(1100,625,4);
    col4(0.76f,0.76f,0.69f,0.5f); drawFilledCircle(1115,610,3);
    col4(0.80f,0.80f,0.72f,0.55f); drawFilledCircle(1095,615,3.5f);
    col4(0.77f,0.77f,0.70f,0.6f); drawFilledCircle(1125,620,2.5f);
    col4(0.79f,0.79f,0.71f,0.5f); drawFilledCircle(1105,642,3);
    col4(0.75f,0.75f,0.67f,0.65f); drawFilledCircle(1118,642,2);
    col4(0.65f,0.65f,0.58f,0.8f); drawFilledCircle(1120,636,2);
    col4(0.68f,0.68f,0.60f,0.7f); drawFilledCircle(1100,626,1.5f);
    col4(0.10f,0.14f,0.34f,0.9f); drawFilledCircle(1128,637,37);
    col4(0.85f,0.85f,0.75f,0.2f); drawFilledCircle(1120,630,20);
    
    glDisable(GL_BLEND);
    
    drawClouds();
}

// ─── City Skyline ────────────────────────────────────────────────────────────

// Draw individual window that can be lit or dark
static void drawWindow(float x,float y,float w,float h,bool lit)
{
    if(lit){ col3(0.95f,0.85f,0.40f); }
    else   { col3(0.04f,0.07f,0.18f); }
    drawRect(x,y,w,h);
}

// Draw building with procedurally generated lit windows
// seed: used for window pattern variation
static void drawBuilding(float x,float baseY,float w,float h,float dr,float dg,float db)
{
    col3(dr,dg,db); drawRect(x,baseY,w,h);
    int seed=(int)(x*31+h*7);
    // Draw window grid
    for(float wy=baseY+8; wy<baseY+h-8; wy+=20){
        for(float wx=x+5; wx<x+w-10; wx+=16){
            bool lit=(((int)(wx+wy+seed))%3!=0);  // Procedural lighting
            drawWindow(wx,wy,9,12,lit);
        }
    }
    // Draw antenna on tall buildings
    if(h>160){
        col3(dr,dg,db);
        drawRect(x+w/2-2,baseY+h,4,20);
        col3(1.0f,0.2f,0.2f);
        drawFilledCircle(x+w/2,baseY+h+22,4);
    }
}

// Draw complete city skyline with multiple buildings
static void drawCitySkyline()
{
    float B[][6]={
        {  0,  55,190, 0.07f,0.11f,0.26f},
        { 50,  48,155, 0.08f,0.13f,0.29f},
        { 95,  65,215, 0.06f,0.10f,0.24f},
        {155,  42,175, 0.09f,0.13f,0.28f},
        {193,  58,200, 0.07f,0.11f,0.26f},
        {248,  38,145, 0.10f,0.14f,0.30f},
        {284,  75,235, 0.06f,0.10f,0.23f},
        {358,  48,168, 0.08f,0.12f,0.27f},
        {700,  72,218, 0.07f,0.11f,0.25f},
        {768,  50,180, 0.09f,0.13f,0.29f},
        {814,  80,248, 0.06f,0.10f,0.23f},
        {890,  62,198, 0.08f,0.12f,0.27f},
        {948,  44,170, 0.10f,0.14f,0.30f},
        {988,  70,215, 0.07f,0.11f,0.26f},
        {1055, 56,188, 0.08f,0.12f,0.28f},
        {1108, 65,208, 0.07f,0.11f,0.25f},
        {1170, 48,178, 0.09f,0.13f,0.29f},
    };
    for(auto& b:B) drawBuilding(b[0],230.0f,b[1],b[2],b[3],b[4],b[5]);
}

// ─── Defense Vehicle (ZAM-7 System) ──────────────────────────────────────────

// Draw the main defense vehicle with radar dish, missiles, and chassis
// Includes animated rotating radar dish and complex mechanical details
static void drawDefenseVehicle()
{
    const float BX=65.0f, BY=200.0f;

    col3(0.12f,0.12f,0.12f); drawRect(BX-12,BY,220,5);
    col3(0.12f,0.12f,0.12f); drawRect(BX-12,BY+35,220,5);
    
    col3(0.16f,0.16f,0.16f);
    for(int i=0;i<9;i++){
        drawRect(BX+2+i*21.0f,BY+8,18,3);
        drawRect(BX+2+i*21.0f,BY+28,18,3);
    }
    
    for(int i=0;i<8;i++){
        col3(0.22f,0.22f,0.22f); drawFilledCircle(BX+5+i*25.0f,BY+15,13);
        col3(0.10f,0.10f,0.10f); drawFilledCircle(BX+5+i*25.0f,BY+15, 8);
        col3(0.18f,0.18f,0.18f); drawFilledCircle(BX+5+i*25.0f,BY+15, 4);
    }
    
    col3(0.20f,0.20f,0.20f);
    for(int i=1;i<7;i++) drawFilledCircle(BX+5+i*25.0f,BY-3,6);

    col3(0.16f,0.28f,0.10f); drawRect(BX-2,BY+26,200,12);
    col3(0.18f,0.31f,0.11f); drawRect(BX-5,BY+38,205,32);
    col3(0.22f,0.36f,0.14f);
    drawRect(BX+5,BY+40,185,8);
    drawRect(BX+5,BY+58,185,8);
    col3(0.12f,0.20f,0.08f);
    for(int i=0;i<6;i++){
        drawRect(BX+15+i*28.0f,BY+32,6,4);
        drawRect(BX+15+i*28.0f,BY+62,6,4);
    }

    col3(0.20f,0.35f,0.14f); drawRect(BX+15,BY+70,165,42);
    
    glBegin(GL_QUADS);
    col3(0.19f,0.33f,0.13f);
    glVertex2f(BX+15,BY+70); glVertex2f(BX+32,BY+112);
    glVertex2f(BX+80,BY+112); glVertex2f(BX+80,BY+70);
    glEnd();
    
    col3(0.22f,0.37f,0.15f);
    drawRect(BX+20,BY+108,155,8);
    col3(0.14f,0.24f,0.10f);
    drawRect(BX+130,BY+110,12,10);
    col3(0.25f,0.40f,0.18f);
    drawRect(BX+131,BY+111,10,8);
    col3(0.18f,0.30f,0.12f); drawRect(BX+155,BY+108,6,8);
    col3(0.10f,0.16f,0.06f); drawFilledCircle(BX+158,BY+114,2);
    col3(0.28f,0.42f,0.18f);
    for(int i=0;i<6;i++){
        drawFilledCircle(BX+30+i*28.0f,BY+75,2);
        drawFilledCircle(BX+30+i*28.0f,BY+105,2);
    }

    col3(0.24f,0.40f,0.16f); drawRect(BX+22,BY+106,8,38);
    col3(0.18f,0.30f,0.12f);
    drawRect(BX+20,BY+140,12,2);
    drawRect(BX+20,BY+144,12,2);
    col3(0.20f,0.34f,0.14f);
    for(int i=0;i<4;i++) drawRect(BX+20,BY+112+i*8.0f,12,1);
    
    float dishRotation = gFrameCounter * 2.2f;
    
    glPushMatrix();
    glTranslatef(BX+25, BY+150, 0);
    glRotatef(dishRotation, 0, 0, 1);
    glTranslatef(-(BX+25), -(BY+150), 0);
    
    col3(0.35f,0.42f,0.24f);
    glBegin(GL_TRIANGLE_STRIP);
    for(int i=0;i<=32;i++){
        float a=PI+PI*i/32.0f;
        glVertex2f(BX+25+34.0f*cosf(a), BY+150+22.0f*sinf(a));
        glVertex2f(BX+25+35.5f*cosf(a), BY+150+23.5f*sinf(a));
    }
    glEnd();
    
    col3(0.52f,0.64f,0.36f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(BX+25,BY+150);
    for(int i=0;i<=24;i++){
        float a=PI+PI*i/24.0f;
        glVertex2f(BX+25+34.0f*cosf(a), BY+150+22.0f*sinf(a));
    }
    glEnd();
    
    col3(0.58f,0.70f,0.40f);
    for(int i=0;i<12;i++){
        float a1=PI+PI*i/12.0f;
        float a2=PI+PI*(i+1)/12.0f;
        glBegin(GL_TRIANGLES);
        glVertex2f(BX+25,BY+150);
        glVertex2f(BX+25+34*cosf(a1), BY+150+22*sinf(a1));
        glVertex2f(BX+25+34*cosf(a2), BY+150+22*sinf(a2));
        glEnd();
        
        col3(0.68f,0.78f,0.52f);
        glLineWidth(1.5f);
        glBegin(GL_LINES);
        glVertex2f(BX+25,BY+150);
        glVertex2f(BX+25+34*cosf(a1), BY+150+22*sinf(a1));
        glEnd();
        glLineWidth(1.0f);
        col3(0.58f,0.70f,0.40f);
    }
    
    glLineWidth(1.0f);
    col3(0.50f,0.60f,0.32f);
    for(int ring=1;ring<=2;ring++){
        float rRatio = 0.7f * ring / 2.0f;
        glBegin(GL_LINE_STRIP);
        for(int i=0;i<=32;i++){
            float a=PI+PI*i/32.0f;
            glVertex2f(BX+25+34.0f*rRatio*cosf(a), BY+150+22.0f*rRatio*sinf(a));
        }
        glEnd();
    }
    
    col3(0.65f,0.75f,0.48f);
    glLineWidth(3.5f);
    glBegin(GL_LINE_STRIP);
    for(int i=0;i<=32;i++){
        float a=PI+PI*i/32.0f;
        glVertex2f(BX+25+34.0f*cosf(a), BY+150+22.0f*sinf(a));
    }
    glEnd();
    
    glLineWidth(3.5f);
    glBegin(GL_LINES);
    glVertex2f(BX+25+34.0f*cosf(PI), BY+150+22.0f*sinf(PI));
    glVertex2f(BX+25, BY+150);
    glVertex2f(BX+25+34.0f*cosf(2*PI), BY+150+22.0f*sinf(2*PI));
    glVertex2f(BX+25, BY+150);
    glEnd();
    glLineWidth(1.0f);
    
    col3(0.72f,0.82f,0.55f);
    glLineWidth(2.0f);
    glBegin(GL_LINE_STRIP);
    for(int i=0;i<=32;i++){
        float a=PI+PI*i/32.0f;
        glVertex2f(BX+25+33.0f*cosf(a), BY+150+21.0f*sinf(a));
    }
    glEnd();
    glLineWidth(1.0f);
    
    glPopMatrix();
    
    col3(0.25f, 0.35f, 0.15f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(BX+25,BY+150-8);
    for(int i=0;i<=16;i++){
        float a=2.0f*PI*i/16.0f;
        glVertex2f(BX+25+8.0f*cosf(a), BY+150+8.0f*sinf(a)-8);
    }
    glEnd();
    
    col3(0.35f, 0.50f, 0.25f);
    glLineWidth(2.0f);
    glBegin(GL_LINE_LOOP);
    for(int i=0;i<=16;i++){
        float a=2.0f*PI*i/16.0f;
        glVertex2f(BX+25+8.0f*cosf(a), BY+150+8.0f*sinf(a)-8);
    }
    glEnd();
    
    col3(0.58f,0.68f,0.40f);
    drawRect(BX+22,BY+142,6,8);
    col3(0.68f, 0.78f, 0.52f);
    drawFilledCircle(BX+25,BY+150-6,4);
    col3(0.58f, 0.70f, 0.42f);
    drawFilledCircle(BX+25,BY+150-6,2);
    glLineWidth(1.0f);

    col3(0.20f,0.34f,0.14f);
    drawRect(BX+95,BY+80,48,35);
    
    for(int t=0;t<3;t++){
        glPushMatrix();
        glTranslatef(BX+119,BY+95,0);
        glRotatef(50.0f+t*10.0f,0,0,1);

        col3(0.22f,0.36f,0.16f); drawRect(0,-6,90,12);
        col3(0.18f,0.30f,0.13f);
        for(int r=0;r<8;r++) drawRect(10+r*8.0f,-6.5f,1.5f,13);
        col3(0.16f,0.27f,0.11f); drawRect(87,-7,5,14);
        col3(0.20f,0.33f,0.14f); drawRect(88,-6,3,12);
        col3(0.24f,0.38f,0.17f);
        glBegin(GL_TRIANGLES);
        glVertex2f(93,-8); glVertex2f(100,-10); glVertex2f(100,-6);
        glEnd();
        glBegin(GL_TRIANGLES);
        glVertex2f(93,8); glVertex2f(100,10); glVertex2f(100,6);
        glEnd();

        col3(0.68f,0.68f,0.72f); drawRect(45,-5,38,10);
        col3(0.82f,0.12f,0.08f);
        glBegin(GL_TRIANGLES);
        glVertex2f(83,0); glVertex2f(72,-5); glVertex2f(72,5);
        glEnd();
        col3(0.88f,0.85f,0.20f);
        drawRect(60,-5,4,10);
        drawRect(50,-5,4,10);
        col3(0.48f,0.50f,0.56f);
        glBegin(GL_POLYGON);
        glVertex2f(45,0); glVertex2f(40,-6); glVertex2f(48,-10); glVertex2f(50,-8);
        glEnd();
        glBegin(GL_POLYGON);
        glVertex2f(45,0); glVertex2f(40,6); glVertex2f(48,10); glVertex2f(50,8);
        glEnd();

        glPopMatrix();
    }    
    col3(0.22f,0.36f,0.16f);
    glBegin(GL_QUADS);
    glVertex2f(BX+22,BY+144); glVertex2f(BX+28,BY+144);
    glVertex2f(BX+30,BY+148); glVertex2f(BX+20,BY+148);
    glEnd();
    
    col3(0.16f,0.26f,0.10f);
    drawRect(BX+90,BY+75,2,35);
    drawRect(BX+145,BY+75,2,35);

    col3(0.28f,0.44f,0.20f); drawRect(BX+175,BY+50,8,8);
    col3(0.60f,0.75f,0.90f); drawFilledCircle(BX+179,BY+54,3);
    col3(0.20f,0.34f,0.14f); drawRect(BX+50,BY+114,2,12);
    col3(0.25f,0.40f,0.18f); drawFilledCircle(BX+51,BY+128,2);

    col3(0.88f,0.88f,0.22f);
    drawText(BX+110,BY+32,"ZAM-7",GLUT_BITMAP_HELVETICA_12);
}

// ─── Radar Display ───────────────────────────────────────────────────────────

// Draw radar screen with scanning sweep, concentric circles, and detected drones
// Includes animated sweep line and drone blips
static void drawRadar()
{
    float cx=RADAR_X, cy=RADAR_Y, r=RADAR_R;
    float sweepRad = gRadarAngle * PI/180.0f;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

    float bgPulse = 0.95f + 0.05f * sinf(gFrameCounter * 0.05f);
    col4(0.02f,0.12f,0.02f,bgPulse); drawFilledCircle(cx,cy,r);
    
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    col4(0.05f,0.18f,0.05f,0.15f);
    drawFilledCircle(cx,cy,r);

    glLineWidth(1.0f);
    for(int i=1;i<=4;i++){
        col4(0.10f,0.55f,0.20f,0.65f);
        drawCircleOutline(cx,cy,r*(float)i/4.0f, 0);
    }

    col4(0.00f,0.42f,0.12f,0.6f);
    drawLineDDA(cx-r,cy,cx+r,cy);
    drawLineDDA(cx,cy-r,cx,cy+r);

    int TRAIL=50;
    for(int i=0;i<TRAIL;i++){
        float a=sweepRad - i*(PI/60.0f);
        float alpha=(float)(TRAIL-i)/(float)TRAIL * 0.65f;
        col4(0.00f,0.90f,0.25f,alpha);
        glBegin(GL_TRIANGLES);
        glVertex2f(cx,cy);
        glVertex2f(cx+r*cosf(a),          cy+r*sinf(a));
        glVertex2f(cx+r*cosf(a-PI/60.0f), cy+r*sinf(a-PI/60.0f));
        glEnd();
    }
    
    col4(0.00f,0.80f,0.20f,0.3f);
    glLineWidth(4.0f);
    drawLineBresenham(cx,cy, cx+r*cosf(sweepRad), cy+r*sinf(sweepRad));
    col4(0.20f,1.00f,0.40f,1.0f);
    glLineWidth(2.0f);
    drawLineBresenham(cx,cy, cx+r*cosf(sweepRad), cy+r*sinf(sweepRad));
    glLineWidth(1.0f);

    for(int i=0;i<MAX_DRONES;i++){
        if(gDrones[i].active && gDrones[i].detected){
            float wx=(gDrones[i].x - cx)/(RADAR_DETECT_RANGE)*r;
            float wy=(gDrones[i].y - cy)/(RADAR_DETECT_RANGE)*r;
            float bd=sqrtf(wx*wx+wy*wy);
            if(bd>r-5){ wx=wx/bd*(r-5); wy=wy/bd*(r-5); }
            float blipPulse = 0.5f + 0.5f * sinf(gFrameCounter * 0.15f);
            if(blipPulse > 0.3f){
                col4(1.0f,0.25f,0.05f,1.0f);
                drawFilledCircle(cx+wx,cy+wy,5);
                col4(1.0f,0.60f,0.30f,0.6f);
                drawFilledCircle(cx+wx,cy+wy,8);
            }
        }
    }

    col4(0.10f,0.70f,0.25f,1.0f);
    glLineWidth(3.0f);
    drawCircleOutline(cx,cy,r, 0);
    glLineWidth(1.0f);

    col4(0.60f,1.00f,0.70f,0.5f);
    drawFilledCircle(cx,cy,8);
    col4(0.30f,1.00f,0.50f,1.0f);
    drawFilledCircle(cx,cy,5);

    glDisable(GL_BLEND);
}

// ─── Drone ───────────────────────────────────────────────────────────────────

// Draw enemy drone aircraft with banking animation and detection glow
static void drawDrone(float x,float y,bool detected)
{
    float bankAngle = sinf(gFrameCounter * 0.05f) * 8.0f;
    
    glPushMatrix();
    glTranslatef(x, y, 0);
    glRotatef(bankAngle, 0, 0, 1);
    
    if(detected){
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        float pulse = 0.5f + 0.3f * sinf(gFrameCounter * 0.1f);
        col4(1.0f,0.3f,0.0f,pulse*0.25f); drawFilledCircle(0,0,30);
        glDisable(GL_BLEND);
    }

    col3(0.48f,0.50f,0.55f);
    glBegin(GL_POLYGON);
    glVertex2f(28,0);
    glVertex2f(15,6);
    glVertex2f(-28,5);
    glVertex2f(-28,-5);
    glVertex2f(15,-6);
    glEnd();

    col3(0.42f,0.44f,0.50f);
    glBegin(GL_TRIANGLES);
    glVertex2f(5,2);   glVertex2f(-15,24); glVertex2f(-20,2);
    glVertex2f(5,-2);   glVertex2f(-15,-24); glVertex2f(-20,-2);
    glEnd();

    col3(0.38f,0.40f,0.46f);
    glBegin(GL_TRIANGLES);
    glVertex2f(-22,0);  glVertex2f(-34,12); glVertex2f(-22,5);
    glEnd();

    col3(0.20f,0.65f,0.82f);
    drawFilledCircle(18,0,7);
    col3(0.55f,0.85f,0.95f);
    drawFilledCircle(20,2,3);

    float glowPulse = 0.7f + 0.3f * sinf(gFrameCounter * 0.15f);
    col3(0.85f,0.45f,0.05f * glowPulse); drawFilledCircle(-28,0,5);
    col3(1.00f,0.85f,0.10f * glowPulse); drawFilledCircle(-28,0,2.5f);

    col3(1.0f,0.0f,0.0f); drawFilledCircle(-15,24,2.5f);
    col3(0.0f,1.0f,0.0f); drawFilledCircle(-15,-24,2.5f);
    
    glPopMatrix();
}

// ─── Missile ─────────────────────────────────────────────────────────────────

// Draw projectile missile with exhaust trail and guidance markings
// Rotates toward target direction with animated flame trail
static void drawMissileProj(float x,float y,float tx,float ty)
{
    float angle=atan2f(ty-y,tx-x);
    glPushMatrix();
    glTranslatef(x,y,0);
    glRotatef(angle*180.0f/PI,0,0,1);

    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

    for(int i = 1; i <= 5; i++){
        float alpha = 0.4f * (1.0f - i/5.0f);
        col4(0.85f,0.70f,0.40f,alpha);
        drawFilledCircle(-8.0f*i,-0.5f,3.0f + i*0.5f, 16);
    }

    col4(0.70f,0.30f,0.00f,0.70f);
    glBegin(GL_TRIANGLES);
    glVertex2f(-14,-3); glVertex2f(-45,0); glVertex2f(-14,3);
    glEnd();
    col4(1.00f,0.65f,0.10f,0.50f);
    glBegin(GL_TRIANGLES);
    glVertex2f(-14,-2); glVertex2f(-30,0); glVertex2f(-14,2);
    glEnd();

    glDisable(GL_BLEND);

    col3(0.72f,0.72f,0.78f); drawRect(-13,-4,28,8);

    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    col4(1.0f, 0.30f, 0.10f, 0.4f);
    drawFilledCircle(15, 0, 5);
    glDisable(GL_BLEND);

    col3(0.90f,0.18f,0.10f);
    glBegin(GL_TRIANGLES);
    glVertex2f(15,0); glVertex2f(5,-4); glVertex2f(5,4);
    glEnd();

    col3(0.50f,0.52f,0.60f);
    glBegin(GL_TRIANGLES);
    glVertex2f(-10,0); glVertex2f(-18,-10); glVertex2f(-5,0);
    glEnd();
    glBegin(GL_TRIANGLES);
    glVertex2f(-10,0); glVertex2f(-18, 10); glVertex2f(-5,0);
    glEnd();

    col3(0.85f,0.80f,0.10f);
    drawRect(0,-4,4,8);
    drawRect(-7,-4,4,8);

    glPopMatrix();
}

// ─── Explosions & Particle Effects ───────────────────────────────────────────

// Draw single explosion with expanding concentric circles
// t: normalized expansion (0.0 = initial, 1.0 = fully expanded)
static void drawExplosion(const Explosion& e)
{
    float t=e.radius/e.maxRadius;
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

    col4(1.0f,0.20f,0.00f,1.0f-t*0.6f); drawFilledCircle(e.x,e.y,e.radius);
    col4(1.0f,0.60f,0.00f,1.0f-t*0.7f); drawFilledCircle(e.x,e.y,e.radius*0.68f);
    col4(1.0f,0.95f,0.40f,1.0f-t*0.8f); drawFilledCircle(e.x,e.y,e.radius*0.40f);
    col4(1.0f,1.00f,1.00f,1.0f-t*0.9f); drawFilledCircle(e.x,e.y,e.radius*0.18f);

    glDisable(GL_BLEND);
}

// ─── Particles ───────────────────────────────────────────────────────────────

// Spawn particles at explosion location with random velocities and colors
// Types: 0=bright sparks, 1=white smoke, 2=debris
static void spawnParticles(float x,float y)
{
    for(int i=0;i<MAX_PARTICLES;i++){
        if(!gParticles[i].active){
            float a=(float)(rand()%360)*PI/180.0f;
            float spd=1.5f+(rand()%40)/10.0f;
            int ptype = rand()%3;
            float r = 1.0f, g = (rand()%10)/10.0f, b = 0.0f;
            
            if(ptype == 1){
                r = 0.8f; g = 0.8f; b = 0.8f;
                spd *= 0.6f;
            } else if(ptype == 2){
                r = 0.7f; g = 0.5f; b = 0.3f;
                spd *= 1.4f;
            }
            
            gParticles[i]={x,y,cosf(a)*spd,sinf(a)*spd,1.0f,r,g,b,ptype,true};
            static int cnt=0; if(++cnt>=12){ cnt=0; return; }
        }
    }
}

// Render all active particles with type-specific visual effects
static void drawParticles()
{
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    
    glPointSize(4.5f);
    glBegin(GL_POINTS);
    for(auto& p:gParticles){
        if(p.active && p.type == 0){
            col4(p.r,p.g,p.b,p.life);
            glVertex2f(p.x,p.y);
        }
    }
    glEnd();
    
    for(auto& p:gParticles){
        if(p.active && p.type == 1){
            col4(p.r,p.g,p.b,p.life*0.4f);
            drawFilledCircle(p.x,p.y,2.5f+p.life*2.0f,12);
        }
    }
    
    glPointSize(6.0f);
    glBegin(GL_POINTS);
    for(auto& p:gParticles){
        if(p.active && p.type == 2){
            col4(p.r,p.g,p.b,p.life*0.8f);
            glVertex2f(p.x,p.y);
        }
    }
    glEnd();
    
    glPointSize(1.0f);
    glDisable(GL_BLEND);
}

// Update particle physics: position, velocity, gravity, and lifetime
static void updateParticles()
{
    for(auto& p:gParticles){
        if(!p.active) continue;
        p.x+=p.vx; p.y+=p.vy;
        
        if(p.type == 1){
            p.vy += 0.04f;
            p.vx *= 0.98f;
        } else {
            p.vy-=0.08f;
        }
        
        p.life-=0.025f;
        if(p.life<=0) p.active=false;
    }
}

// ─── Ground Vehicles ─────────────────────────────────────────────────────────

// Draw car with detailed interior, windows, headlights, taillights
static void drawCar(float x,float y,float r,float g,float b)
{
    glPushMatrix();
    glTranslatef(x+41,y+26,0);
    
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    col4(0.0f,0.0f,0.0f,0.2f);
    glBegin(GL_POLYGON);
    glVertex2f(-40,-16); glVertex2f(40,-16);
    glVertex2f(40,-14); glVertex2f(-40,-14);
    glEnd();
    glDisable(GL_BLEND);
    
    col3(r,g,b);
    glBegin(GL_POLYGON);
    glVertex2f(-38,0);
    glVertex2f(-36,8);
    glVertex2f(-20,10);
    glVertex2f(0,12);
    glVertex2f(20,10);
    glVertex2f(36,8);
    glVertex2f(38,0);
    glVertex2f(36,-6);
    glVertex2f(-36,-6);
    glEnd();
    
    col3(r*0.65f,g*0.65f,b*0.65f);
    glBegin(GL_QUADS);
    glVertex2f(-36,-5); glVertex2f(36,-5);
    glVertex2f(36,-2); glVertex2f(-36,-2);
    glEnd();
    
    col3(r*0.75f,g*0.75f,b*0.75f);
    glBegin(GL_POLYGON);
    glVertex2f(-20,10);
    glVertex2f(-22,22);
    glVertex2f(22,22);
    glVertex2f(20,10);
    glEnd();
    
    col3(0.40f,0.70f,0.95f);
    glBegin(GL_POLYGON);
    glVertex2f(-16,10);
    glVertex2f(-12,22);
    glVertex2f(12,22);
    glVertex2f(16,10);
    glEnd();
    col3(0.60f,0.80f,1.00f);
    glBegin(GL_POLYGON);
    glVertex2f(-15,11);
    glVertex2f(-11,21);
    glVertex2f(11,21);
    glVertex2f(15,11);
    glEnd();
    
    col3(0.40f,0.70f,0.95f);
    glBegin(GL_POLYGON);
    glVertex2f(-16,10);
    glVertex2f(-20,22);
    glVertex2f(-8,22);
    glVertex2f(0,10);
    glEnd();
    col3(0.50f,0.75f,0.92f);
    glBegin(GL_POLYGON);
    glVertex2f(-15,11);
    glVertex2f(-19,21);
    glVertex2f(-9,21);
    glVertex2f(-1,11);
    glEnd();
    
    col3(0.35f,0.35f,0.40f);
    glBegin(GL_QUADS);
    glVertex2f(-22,12); glVertex2f(-20,12);
    glVertex2f(-20,18); glVertex2f(-22,18);
    glEnd();
    col3(0.55f,0.70f,0.85f);
    glBegin(GL_QUADS);
    glVertex2f(-21,13); glVertex2f(-20.5f,13);
    glVertex2f(-20.5f,17); glVertex2f(-21,17);
    glEnd();
    
    col3(0.25f,0.25f,0.30f);
    glBegin(GL_QUADS);
    glVertex2f(-36,0); glVertex2f(36,0);
    glVertex2f(36,2); glVertex2f(-36,2);
    glEnd();
    
    glBegin(GL_QUADS);
    glVertex2f(-36,-6); glVertex2f(36,-6);
    glVertex2f(36,-4); glVertex2f(-36,-4);
    glEnd();
    
    glPushMatrix();
    glTranslatef(-22,-15,0);
    col3(0.12f,0.12f,0.12f);
    drawFilledCircle(0,0,14);
    col3(0.55f,0.55f,0.60f);
    drawFilledCircle(0,0,9);
    col3(0.85f,0.85f,0.90f);
    drawFilledCircle(0,0,5);
    col3(0.70f,0.70f,0.75f);
    drawFilledCircle(0,0,2);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(22,-15,0);
    col3(0.12f,0.12f,0.12f);
    drawFilledCircle(0,0,14);
    col3(0.55f,0.55f,0.60f);
    drawFilledCircle(0,0,9);
    col3(0.85f,0.85f,0.90f);
    drawFilledCircle(0,0,5);
    col3(0.70f,0.70f,0.75f);
    drawFilledCircle(0,0,2);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(-32,4,0);
    col3(1.00f,1.00f,0.70f);
    drawFilledCircle(0,0,5);
    col3(0.95f,0.90f,0.40f);
    drawFilledCircle(0,0,3);
    glPopMatrix();
    
    glPushMatrix();
    glTranslatef(32,-2,0);
    col3(0.90f,0.15f,0.15f);
    drawFilledCircle(0,0,4);
    col3(1.00f,0.40f,0.40f);
    drawFilledCircle(0,0,2);
    glPopMatrix();
    
    col3(0.20f,0.20f,0.25f);
    for(int i=0; i<4; i++){
        glBegin(GL_LINES);
        glVertex2f(-28,2+i*2); glVertex2f(-20,2+i*2);
        glEnd();
    }
    
    col3(0.95f,0.95f,0.95f);
    glBegin(GL_QUADS);
    glVertex2f(28,-8); glVertex2f(38,-8);
    glVertex2f(38,-4); glVertex2f(28,-4);
    glEnd();
    
    col3(0.15f,0.15f,0.15f);
    drawText(30,-6,"ABC",GLUT_BITMAP_HELVETICA_12);
    
    glPopMatrix();
}

// Draw motorcycle with engine, rider silhouette, and headlight glow
static void drawMotorcycle(float x,float y)
{
    glPushMatrix();
    glTranslatef(x+28.0f, y+20.0f, 0.0f);

    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    col4(0.0f,0.0f,0.0f,0.18f);
    drawRect(-25.0f,-16.0f,54.0f,3.0f);
    glDisable(GL_BLEND);

    col3(0.10f,0.10f,0.12f);
    drawFilledCircle(-16.0f,-8.0f,11.0f);
    drawFilledCircle(18.0f,-8.0f,11.0f);
    col3(0.50f,0.52f,0.56f);
    drawFilledCircle(-16.0f,-8.0f,6.5f);
    drawFilledCircle(18.0f,-8.0f,6.5f);
    col3(0.76f,0.76f,0.80f);
    drawFilledCircle(-16.0f,-8.0f,2.5f);
    drawFilledCircle(18.0f,-8.0f,2.5f);

    col3(0.20f,0.20f,0.24f);
    glLineWidth(3.0f);
    drawLineBresenham(-12.0f,-5.0f, 6.0f,-2.0f);
    drawLineBresenham( 6.0f,-2.0f,18.0f,-5.0f);
    drawLineBresenham(-12.0f,-5.0f,-2.0f, 8.0f);
    drawLineBresenham(-2.0f, 8.0f,12.0f,10.0f);
    drawLineBresenham(12.0f,10.0f,18.0f,-5.0f);
    glLineWidth(1.0f);

    col3(0.25f,0.25f,0.30f);
    drawRect(-2.0f,-2.0f,10.0f,8.0f);
    col3(0.35f,0.35f,0.40f);
    drawRect(-1.0f,-1.0f,8.0f,3.0f);

    col3(0.42f,0.42f,0.46f);
    drawRect(8.0f,-3.0f,16.0f,2.8f);
    col3(0.58f,0.58f,0.62f);
    drawRect(23.0f,-3.2f,3.0f,3.2f);

    col3(0.66f,0.12f,0.12f);
    glBegin(GL_POLYGON);
    glVertex2f(-1.0f,10.0f);
    glVertex2f(10.5f,10.0f);
    glVertex2f(12.5f,15.0f);
    glVertex2f(2.0f,16.0f);
    glVertex2f(-2.0f,13.0f);
    glEnd();
    col3(0.78f,0.24f,0.24f);
    drawRect(1.0f,12.4f,8.0f,1.8f);

    col3(0.12f,0.12f,0.14f);
    glBegin(GL_POLYGON);
    glVertex2f(-8.0f,11.0f);
    glVertex2f(0.5f,11.0f);
    glVertex2f(1.5f,13.6f);
    glVertex2f(-7.2f,13.4f);
    glEnd();

    col3(0.45f,0.45f,0.50f);
    glLineWidth(2.5f);
    drawLineBresenham(14.0f,12.0f,20.0f,-1.0f);
    drawLineBresenham(15.5f,12.0f,21.5f,-1.0f);
    glLineWidth(1.0f);
    col3(0.28f,0.28f,0.32f);
    drawRect(10.0f,15.0f,9.0f,1.8f);

    col3(1.0f,1.0f,0.70f);
    drawFilledCircle(20.0f,12.0f,3.0f);
    col3(0.95f,0.88f,0.40f);
    drawFilledCircle(20.0f,12.0f,1.5f);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    col4(1.0f,0.95f,0.70f,0.12f);
    glBegin(GL_TRIANGLES);
    glVertex2f(21.0f,12.0f);
    glVertex2f(33.0f, 9.5f);
    glVertex2f(33.0f,14.5f);
    glEnd();
    glDisable(GL_BLEND);

    col3(0.22f,0.24f,0.34f);
    glBegin(GL_POLYGON);
    glVertex2f(-2.0f,14.0f);
    glVertex2f( 5.0f,14.0f);
    glVertex2f( 7.0f,24.0f);
    glVertex2f( 0.0f,24.0f);
    glVertex2f(-3.0f,18.0f);
    glEnd();
    col3(0.80f,0.62f,0.44f);
    drawFilledCircle(3.5f,27.0f,4.3f);

    col3(0.08f,0.08f,0.12f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(3.5f,28.0f);
    for(int i=0;i<=18;i++){
        float a=PI+PI*i/18.0f;
        glVertex2f(3.5f+5.2f*cosf(a), 28.0f+4.8f*sinf(a));
    }
    glEnd();
    col3(0.18f,0.58f,0.80f);
    drawRect(-1.0f,26.0f,7.0f,2.5f);

    col3(0.76f,0.56f,0.36f);
    glLineWidth(3.0f);
    drawLineBresenham(1.0f,20.0f, 9.5f,16.0f);
    drawLineBresenham(5.0f,20.0f,13.0f,15.8f);
    glLineWidth(1.0f);

    col3(0.90f,0.14f,0.14f);
    drawRect(-10.2f,10.8f,2.2f,1.8f);

    glPopMatrix();
}

// Render all active vehicles based on their type
static void drawVehicles()
{
    for(auto& v:gVehicles){
        if(v.type==0) drawCar(v.x,v.y,v.r,v.g,v.b);
        else          drawMotorcycle(v.x,v.y);
    }
}

// ─── Head-Up Display (HUD) ───────────────────────────────────────────────────

// Draw pause menu overlay with semi-transparent background
static void drawHUD()
{
    if(gPaused){
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        col4(0,0,0,0.6f); drawRect(0,0,WIN_W,WIN_H);
        glDisable(GL_BLEND);
        col3(1.0f,1.0f,0.0f);
        drawText(530,370,"-- PAUSED --");
        drawText(460,320,"[P] Resume | [ESC] Quit",GLUT_BITMAP_HELVETICA_12);
    }
}

// ─── Initialization & Game Management ──────────────────────────────────────

// Initialize all game objects, background elements, and system state
static void initAll()
{
    srand((unsigned)time(nullptr));

    for(int i=0;i<200;i++){
        gStarX[i]=(float)(rand()%WIN_W);
        gStarY[i]=230+(float)(rand()%(WIN_H-230));
    }

    float slData[20][2] = {
        {60,138}, {180,138}, {300,138}, {420,138}, {540,138},
        {660,138}, {780,138}, {900,138}, {1020,138}, {1140,138},
        {120,108}, {240,108}, {360,108}, {480,108}, {600,108},
        {720,108}, {840,108}, {960,108}, {1080,108}, {1200,108}
    };
    for(int i=0; i<20; i++){
        gStreetLights[i][0] = slData[i][0];
        gStreetLights[i][1] = slData[i][1];
    }

    for(auto& d:gDrones)    { d.active=false; d.detected=false; d.missileAssigned=-1; }
    for(auto& m:gMissiles)  { m.active=false; }
    for(auto& e:gExplosions){ e.active=false; }
    for(auto& p:gParticles) { p.active=false; }
    
    float cloudData[][4] = {
        {50,600,0.08f,1.2f}, {280,610,0.06f,1.4f}, {520,590,0.07f,1.2f},
        {750,605,0.075f,1.28f}, {950,615,0.065f,1.12f}, {1100,600,0.07f,1.2f},
        {100,520,0.15f,2.8f}, {350,550,0.12f,3.2f}, {600,510,0.14f,3.0f},
        {880,535,0.13f,3.12f}, {1150,520,0.16f,2.88f}, {150,450,0.35f,5.6f},
        {400,480,0.32f,6.0f}, {750,430,0.38f,6.4f}, {1000,470,0.34f,5.8f},
        {200,350,0.12f,8.8f}, {700,380,0.1f,8.0f}, {1050,360,0.11f,8.4f},
        {50,420,0.16f,5.2f}, {900,520,0.18f,5.0f},
    };
    for(int i=0;i<20;i++){
        gClouds[i]={cloudData[i][0],cloudData[i][1],cloudData[i][2],cloudData[i][3],true};
    }

    float vd[][7]={
        {220,88,1.5f,0, 0.70f,0.10f,0.12f},
        {480,88,2.0f,1, 0.00f,0.00f,0.00f},
        {700,88,1.8f,0, 0.10f,0.22f,0.72f},
        {900,88,2.2f,0, 0.08f,0.55f,0.18f},
        {1060,88,1.6f,0,0.50f,0.10f,0.50f},
    };
    for(int i=0;i<5;i++){
        gVehicles[i]={vd[i][0],vd[i][1],vd[i][2],(int)vd[i][3],vd[i][4],vd[i][5],vd[i][6]};
    }
}

// Spawn a new drone from the right side of screen with random properties
static void spawnDrone()
{
    for(auto& d:gDrones){
        if(!d.active){
            d.x       = (float)(WIN_W+60);          // Start off-screen right
            d.y       = 420.0f+(rand()%180);        // Random vertical position
            d.speed   = 1.4f+(rand()%25)/10.0f;    // Random speed variation
            d.active  = true;
            d.detected= false;
            d.missileAssigned=-1;
            return;
        }
    }
}

// Launch a missile from defense vehicle to intercept target drone
static void launchMissile(int droneIdx)
{
    for(int i=0;i<MAX_MISSILES;i++){
        if(!gMissiles[i].active){
            gMissiles[i].x        = 185.0f;         // Defense vehicle launcher X
            gMissiles[i].y        = 318.0f;         // Defense vehicle launcher Y
            gMissiles[i].speed    = 5.5f;           // Missile velocity
            gMissiles[i].active   = true;
            gMissiles[i].targetIdx= droneIdx;       // Link missile to target
            gDrones[droneIdx].missileAssigned=i;    // Link drone to missile
            return;
        }
    }
}

// Create explosion effect at given coordinates with particle burst
static void addExplosion(float x,float y)
{
    for(auto& e:gExplosions){
        if(!e.active){
            e={x,y,4.0f,38.0f,0,true};            // Initial radius 4, max 38
            spawnParticles(x,y);                   // Generate particle burst
            return;
        }
    }
}

// ─── Update Loop (called every 16ms for 60 FPS) ──────────────────────────────

// Main game update logic: drone spawning, missile tracking, collision detection
static void update(int)
{
    if(!gPaused){
        // Increment frame counter for animations
        gFrameCounter++;
        if(gFrameCounter > 10000) gFrameCounter = 0;

        // Rotate radar sweep
        gRadarAngle+=2.2f;
        if(gRadarAngle>=360.0f) gRadarAngle-=360.0f;

        // Update background elements
        updateClouds();

        // Drone spawning logic - progressively increase spawn rate
        gSpawnTimer++;
        if(gSpawnTimer>=gSpawnInterval){
            spawnDrone();
            gSpawnTimer=0;
            if(gSpawnInterval>80) gSpawnInterval--;  // Increase difficulty
        }

        // Update drone positions and detect with radar
        for(int i=0;i<MAX_DRONES;i++){
            auto& d=gDrones[i];
            if(!d.active) continue;
            d.x-=d.speed;                               // Move left across screen
            if(d.x<-80){ d.active=false; continue; }    // Remove off-screen

            // Radar detection and missile launch
            if(!d.detected){
                float dist=fDist(d.x,d.y,RADAR_X,RADAR_Y);
                if(dist<RADAR_DETECT_RANGE){
                    d.detected=true;
                    launchMissile(i);
                }
            }
        }

        // Update missile positions and track targets
        for(auto& m:gMissiles){
            if(!m.active) continue;
            int ti=m.targetIdx;
            if(ti<0||!gDrones[ti].active){ m.active=false; continue; }

            float tx=gDrones[ti].x, ty=gDrones[ti].y;
            float d=fDist(m.x,m.y,tx,ty);
            
            // Collision detection
            if(d<18.0f){
                addExplosion(tx,ty);               // Create explosion effect
                gDrones[ti].active=false;          // Destroy drone
                m.active=false;                    // Remove missile
            } else {
                // Missile guidance - move toward target
                m.x+=(tx-m.x)/d*m.speed;
                m.y+=(ty-m.y)/d*m.speed;
            }
        }

        // Update explosion animations (expanding blast radius)
        for(auto& e:gExplosions){
            if(!e.active) continue;
            e.radius+=1.8f; e.frame++;
            if(e.radius>e.maxRadius) e.active=false;
        }

        // Update particle physics and animations
        updateParticles();

        // Update vehicle positions (scrolling)
        for(auto& v:gVehicles){
            v.x+=v.speed;
            if(v.x>WIN_W+120) v.x=-120;           // Wrap around screen
        }
    }

    glutPostRedisplay();
    glutTimerFunc(16,update,0);                    // Schedule next update in 16ms
}

// ─── Display / Rendering ────────────────────────────────────────────────────

// Render complete scene: background, buildings, vehicles, defense system,
// drones, missiles, explosions, particles, and HUD
static void display()
{
    glClear(GL_COLOR_BUFFER_BIT);                // Clear screen

    // Draw environment layers from back to front
    drawBackground();                             // Night sky, stars, moon
    drawCitySkyline();                            // Urban buildings
    drawVehicles();                               // Ground traffic
    drawDefenseVehicle();                         // ZAM-7 defense system
    drawRadar();                                  // Radar display

    // Draw all active drones
    for(auto& d:gDrones)
        if(d.active) drawDrone(d.x,d.y,d.detected);

    // Draw all active missiles with trajectories
    for(auto& m:gMissiles){
        if(!m.active) continue;
        int ti=m.targetIdx;
        if(ti>=0&&gDrones[ti].active)
            drawMissileProj(m.x,m.y,gDrones[ti].x,gDrones[ti].y);
    }

    // Draw explosion effects
    for(auto& e:gExplosions)
        if(e.active) drawExplosion(e);

    // Draw particle effects
    drawParticles();
    
    // Draw UI overlays (pause menu)
    drawHUD();

    glutSwapBuffers();                           // Display rendered frame
}

// ─── Window Reshape ──────────────────────────────────────────────────────────

// Handle window resizing and update OpenGL projection matrix
static void reshape(int w,int h)
{
    glViewport(0,0,w,h);                         // Update viewport to window size
    glMatrixMode(GL_PROJECTION); 
    glLoadIdentity();
    gluOrtho2D(0,WIN_W,0,WIN_H);                 // Orthographic 2D projection
    glMatrixMode(GL_MODELVIEW);  
    glLoadIdentity();
}

// ─── Keyboard Input ──────────────────────────────────────────────────────────

// Handle keyboard input for game controls
// ESC: quit, S: spawn drone, P: pause/resume
static void keyboard(unsigned char key,int,int)
{
    switch(key){
        case 27:  exit(0); break;                // ESC - Quit application
        case 's': case 'S': spawnDrone(); break; // S - Manually spawn drone
        case 'p': case 'P': gPaused=!gPaused; break; // P - Toggle pause
    }
}

// ─── Main Entry Point ────────────────────────────────────────────────────────

// Initialize GLUT window, OpenGL settings, and start the game loop
int main(int argc,char** argv)
{
    glutInit(&argc,argv);
    
    // Setup window: double buffering for smooth animation, RGB color mode
    glutInitDisplayMode(GLUT_DOUBLE|GLUT_RGB);
    glutInitWindowSize(WIN_W,WIN_H);
    glutInitWindowPosition(80,40);
    glutCreateWindow("2D Air Defense Simulation System | [S] Spawn [P] Pause [ESC] Quit");

    // Set clear color to dark night sky
    glClearColor(0.01f,0.04f,0.16f,1.0f);

    // Setup 2D orthographic projection (no perspective)
    glMatrixMode(GL_PROJECTION); 
    glLoadIdentity();
    gluOrtho2D(0,WIN_W,0,WIN_H);               // Map screen coordinates directly
    glMatrixMode(GL_MODELVIEW);  
    glLoadIdentity();

    // Initialize game objects, background, and state
    initAll();

    // Register GLUT callback functions
    glutDisplayFunc(display);                  // Render callback
    glutReshapeFunc(reshape);                  // Window resize callback
    glutKeyboardFunc(keyboard);                // Keyboard input callback
    glutTimerFunc(16,update,0);                // Game update loop (60 FPS)

    // Start main event loop
    glutMainLoop();
    return 0;
}