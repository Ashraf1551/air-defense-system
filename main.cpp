#include <GL/glut.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#define PI              3.14159265358979323846f
#define WIN_W           1200
#define WIN_H           700
#define MAX_DRONES      6
#define MAX_MISSILES    10
#define MAX_EXPLOSIONS  10
#define MAX_PARTICLES   80

// // ─── Data Structures ─────────────────────────────────────────
// // Enemy drone that flies across the screen from right to left
struct Drone {
    float x, y;              // Position in world space
    float speed;             // Horizontal movement speed (pixels per frame)
    bool  active;            // Whether drone is currently active
    bool  detected;          // Whether radar has detected this drone
    int   missileAssigned;   // Which missile index targets it (-1 = none)
};

struct Missile {
    float x, y;           // Current position of the missile in world space
    float speed;          // Velocity (pixels per frame) - moves toward target
    bool  active;         // Is this missile currently flying
    int   targetIdx;      // Index of the drone being targeted
};

struct Explosion {
    float x, y;           // Center position of explosion effect
    float radius;         // Current blast radius (expands over time)
    float maxRadius;      // Maximum radius before explosion ends (~38 units)
    int   frame;          // Frame counter for animation timing
    bool  active;         // Is explosion currently visible
};

struct Particle {
    float x, y;           // Position of individual particle
    float vx, vy;         // Velocity components for physics simulation
    float life;           // Fading factor (0..1, decreases each frame)
    float r, g, b;        // Color channels
    int   type;           // 0=spark (bright), 1=smoke (rises), 2=debris
    bool  active;         // Is particle currently rendered
};

struct Vehicle {
    float x, y;           // Car or motorcycle position on road
    float speed;          // Pixels per frame movement speed
    int   type;           // 0=car, 1=motorcycle (different models)
    float r, g, b;        // Vehicle body color
};

struct Cloud {
    float x, y;           // Position in sky
    float speed;          // Parallax scrolling speed (slower = further away)
    float scale;          // Visual size multiplier (0.5..1.5)
    bool  active;         // Is cloud visible
};

// // ─── Globals ─────────────────────────────────────────────────

// // Radar subsystem
float gRadarAngle   = 0.0f;
const float RADAR_X = 100.0f;                      // Radar center X position (top-left)
const float RADAR_Y = 580.0f;                      // Radar center Y position (top-left)
const float RADAR_R = 88.0f;                       // Radar display radius on screen
const float RADAR_DETECT_RANGE = 580.0f;           // World-space detection radius (increased)

// // Game entity arrays (pool allocation for efficiency)
Drone      gDrones    [MAX_DRONES];                // Enemy drones
Missile    gMissiles  [MAX_MISSILES];              // Guided missiles
Explosion  gExplosions[MAX_EXPLOSIONS];            // Explosion effects
Particle   gParticles [MAX_PARTICLES];             // Individual particles
Vehicle    gVehicles  [5];                         // Road traffic (cars/motorcycles)
Cloud      gClouds    [20];                        // Parallax cloud layer (expanded)

// Game state
int  gSpawnTimer     = 0;                          // Counter for drone spawning
int  gSpawnInterval  = 200;                        // Frames between auto-spawns
bool gPaused         = false;                      // Pause toggle
int  gFrameCounter   = 0;                          // Master frame counter for animations

// Procedural generation data
float gStarX[200], gStarY[200];                    // Random star positions
float gStreetLights[20][2];                        // Street light coordinates

// ─── Helpers ─────────────────────────────────────────────────
// Calculate Euclidean distance between two points
static inline float fDist(float x1,float y1,float x2,float y2) {
    float dx=x2-x1, dy=y2-y1;
    return sqrtf(dx*dx+dy*dy);
}

// Set 3-component RGB color
static void col3(float r,float g,float b){ glColor3f(r,g,b); }
// Set 4-component RGBA color with alpha (transparency)
static void col4(float r,float g,float b,float a){ glColor4f(r,g,b,a); }

// ─── Primitive Drawing ───────────────────────────────────────
// Renders a filled circle using triangle fan (fast and smooth)
static void drawFilledCircle(float cx,float cy,float r,int seg=60)
{
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx,cy);
    // Generate vertices around circle perimeter
    for(int i=0;i<=seg;i++){
        float a=2.0f*PI*i/seg;
        glVertex2f(cx+r*cosf(a),cy+r*sinf(a));
    }
    glEnd();
}

// Renders a circle outline (wireframe)
static void drawCircleOutline(float cx,float cy,float r,int seg=60)
{
    glBegin(GL_LINE_LOOP);
    for(int i=0;i<seg;i++){
        float a=2.0f*PI*i/seg;
        glVertex2f(cx+r*cosf(a),cy+r*sinf(a));
    }
    glEnd();
}

static void drawLine(float x1,float y1,float x2,float y2){
    glBegin(GL_LINES);
    glVertex2f(x1,y1); glVertex2f(x2,y2);
    glEnd();
}

// Draw a rectangle (axis-aligned quad)
static void drawRect(float x,float y,float w,float h){
    glBegin(GL_QUADS);
    glVertex2f(x,  y);   glVertex2f(x+w,y);
    glVertex2f(x+w,y+h); glVertex2f(x,  y+h);
    glEnd();
}

// Render text at screen position using bitmap font
static void drawText(float x,float y,const char* s,void* font=GLUT_BITMAP_HELVETICA_18){
    glRasterPos2f(x,y);
    for(;*s;s++) glutBitmapCharacter(font,*s);
}

// ─── Background ──────────────────────────────────────────────
// Draw a single fluffy cloud made of overlapping circles (improved pattern)
static void drawCloud(float cx, float cy, float scale)
{
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    col4(200.0f/255.0f, 232.0f/255.0f, 245.0f/255.0f, 0.65f);  // Light blue cloud color
    
    // Improved cloud shape using overlapping circles
    // Base coordinates normalized then scaled for screen position
    float radius = 18.0f * scale;  // Cloud puff radius
    
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

// Render all active clouds in the scene
static void drawClouds()
{
    for(int i = 0; i < 20; i++){
        if(gClouds[i].active){
            drawCloud(gClouds[i].x, gClouds[i].y, gClouds[i].scale);
        }
    }
}

// Update cloud positions each frame (parallax scrolling effect)
static void updateClouds()
{
    for(int i = 0; i < 20; i++){
        if(gClouds[i].active){
            gClouds[i].x += gClouds[i].speed;
            // Wrap around: when cloud exits right screen, reappear on left
            if(gClouds[i].x > WIN_W + 150) gClouds[i].x = -150;
        }
    }
}

// Draw all 20 street lights along the road with glow effects
static void drawStreetLights()
{
    col3(0.15f, 0.15f, 0.18f);
    for(int i = 0; i < 20; i++){
        float sx = gStreetLights[i][0];
        float sy = gStreetLights[i][1];
        
        // Light pole structure
        drawRect(sx - 2, sy, 4, 32);
        
        // Light fixture head
        col3(0.30f, 0.30f, 0.35f);
        drawFilledCircle(sx, sy + 32, 8);
        
        // Glow effect (multiple transparent halos around bulb)
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        col4(1.00f, 0.95f, 0.70f, 0.15f);  // Warm yellow glow
        drawFilledCircle(sx, sy + 32, 16);
        col4(1.00f, 0.95f, 0.70f, 0.08f);  // Outer dim halo
        drawFilledCircle(sx, sy + 32, 24);
        glDisable(GL_BLEND);
        
        // Light bulb
        col3(0.15f, 0.15f, 0.18f);
    }
}

// ─── Background ──────────────────────────────────────────────

static void drawBackground()
{
    // Night sky gradient
    glBegin(GL_QUADS);
    col3(0.01f,0.04f,0.16f); glVertex2f(0,WIN_H); glVertex2f(WIN_W,WIN_H);
    col3(0.04f,0.09f,0.26f); glVertex2f(WIN_W,230); glVertex2f(0,230);
    glEnd();

    // Ground
    col3(0.12f,0.12f,0.14f); drawRect(0,0,WIN_W,230);

    // Road surface with gradient effect
    col3(0.18f,0.18f,0.20f); drawRect(0,75,WIN_W,65);
    col3(0.22f,0.22f,0.25f); drawRect(0,140,WIN_W,65);

    // Road edges (white)
    col3(0.70f,0.70f,0.75f); drawRect(0,73,WIN_W,4);
    col3(0.70f,0.70f,0.75f); drawRect(0,201,WIN_W,4);

    // Lane divider - dashed yellow line (center)
    col3(0.90f,0.85f,0.15f);
    glLineWidth(2.0f);
    for(int i=0;i<WIN_W;i+=80) drawLine((float)i,138,(float)(i+50),138);
    
    // Side lane markings (subtle)
    col3(0.60f,0.58f,0.10f);
    glLineWidth(1.0f);
    for(int i=0;i<WIN_W;i+=100) {
        drawLine((float)i,105,(float)(i+40),105);
        drawLine((float)i,171,(float)(i+40),171);
    }
    glLineWidth(1.0f);

    // Stars
    glPointSize(2.0f);
    glBegin(GL_POINTS);
    for(int i=0;i<200;i++){
        float brt=0.5f+0.5f*(i%3)/2.0f;
        col3(brt,brt,brt);
        glVertex2f(gStarX[i],gStarY[i]);
    }
    glEnd();
    glPointSize(1.0f);

    // Moon with enhanced design
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    
    // Atmospheric glow halo (outer)
    col4(0.98f,0.95f,0.80f,0.08f); drawFilledCircle(1110,630,58);
    col4(0.98f,0.95f,0.80f,0.04f); drawFilledCircle(1110,630,70);
    
    // Main moon surface with gradient
    col3(0.93f,0.93f,0.78f); drawFilledCircle(1110,630,42);
    
    // Surface highlights (bright areas)
    col4(0.98f,0.98f,0.88f,0.6f); drawFilledCircle(1095,640,12);
    col4(0.95f,0.95f,0.85f,0.4f); drawFilledCircle(1115,620,8);
    col4(0.96f,0.96f,0.86f,0.35f); drawFilledCircle(1105,615,6);
    
    // Surface craters (dark spots for lunar texture)
    col4(0.75f,0.75f,0.68f,0.7f); drawFilledCircle(1120,635,5);
    col4(0.78f,0.78f,0.70f,0.6f); drawFilledCircle(1100,625,4);
    col4(0.76f,0.76f,0.69f,0.5f); drawFilledCircle(1115,610,3);
    col4(0.80f,0.80f,0.72f,0.55f); drawFilledCircle(1095,615,3.5f);
    col4(0.77f,0.77f,0.70f,0.6f); drawFilledCircle(1125,620,2.5f);
    col4(0.79f,0.79f,0.71f,0.5f); drawFilledCircle(1105,642,3);
    col4(0.75f,0.75f,0.67f,0.65f); drawFilledCircle(1118,642,2);
    
    // Crater shadows for depth
    col4(0.65f,0.65f,0.58f,0.8f); drawFilledCircle(1120,636,2);
    col4(0.68f,0.68f,0.60f,0.7f); drawFilledCircle(1100,626,1.5f);
    
    // Crescent shadow (night side of moon)
    col4(0.10f,0.14f,0.34f,0.9f); drawFilledCircle(1128,637,37);
    
    // Subtle shadow gradient on lit side
    col4(0.85f,0.85f,0.75f,0.2f); drawFilledCircle(1120,630,20);
    
    glDisable(GL_BLEND);
    
    // Clouds
    drawClouds();
}

// ─── City Skyline ────────────────────────────────────────────

static void drawWindow(float x,float y,float w,float h,bool lit)
{
    if(lit){ col3(0.95f,0.85f,0.40f); }
    else   { col3(0.04f,0.07f,0.18f); }
    drawRect(x,y,w,h);
}

static void drawBuilding(float x,float baseY,float w,float h,float dr,float dg,float db)
{
    col3(dr,dg,db); drawRect(x,baseY,w,h);
    // Windows grid
    int seed=(int)(x*31+h*7);
    for(float wy=baseY+8; wy<baseY+h-8; wy+=20){
        for(float wx=x+5; wx<x+w-10; wx+=16){
            bool lit=(((int)(wx+wy+seed))%3!=0);
            drawWindow(wx,wy,9,12,lit);
        }
    }
    // Antenna on tall buildings
    if(h>160){
        col3(dr,dg,db);
        drawRect(x+w/2-2,baseY+h,4,20);
        col3(1.0f,0.2f,0.2f);
        drawFilledCircle(x+w/2,baseY+h+22,4);
    }
}

static void drawCitySkyline()
{
    // Building layout: {x, w, h, r, g, b}
    float B[][6]={
        // Left cluster
        {  0,  55,190, 0.07f,0.11f,0.26f},
        { 50,  48,155, 0.08f,0.13f,0.29f},
        { 95,  65,215, 0.06f,0.10f,0.24f},
        {155,  42,175, 0.09f,0.13f,0.28f},
        {193,  58,200, 0.07f,0.11f,0.26f},
        {248,  38,145, 0.10f,0.14f,0.30f},
        {284,  75,235, 0.06f,0.10f,0.23f},
        {358,  48,168, 0.08f,0.12f,0.27f},
        // Right cluster
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

// ─── Defense Vehicle ─────────────────────────────────────────

static void drawDefenseVehicle()
{
    const float BX=65.0f, BY=200.0f;

    // === TRACKS - Improved with track segments ===
    // Track guide rail/torsion bar
    col3(0.12f,0.12f,0.12f); drawRect(BX-12,BY,220,5);
    col3(0.12f,0.12f,0.12f); drawRect(BX-12,BY+35,220,5);
    
    // Track segments (cleats)
    col3(0.16f,0.16f,0.16f);
    for(int i=0;i<9;i++){
        drawRect(BX+2+i*21.0f,BY+8,18,3);
        drawRect(BX+2+i*21.0f,BY+28,18,3);
    }
    
    // Road wheels (main suspension wheels)
    for(int i=0;i<8;i++){
        col3(0.22f,0.22f,0.22f); drawFilledCircle(BX+5+i*25.0f,BY+15,13);
        col3(0.10f,0.10f,0.10f); drawFilledCircle(BX+5+i*25.0f,BY+15, 8);
        col3(0.18f,0.18f,0.18f); drawFilledCircle(BX+5+i*25.0f,BY+15, 4);
    }
    
    // Support rollers (small upper wheels)
    col3(0.20f,0.20f,0.20f);
    for(int i=1;i<7;i++) drawFilledCircle(BX+5+i*25.0f,BY-3,6);

    // === Lower Hull - Armored base ===
    // Hull bottom (sloped sides for armor)
    col3(0.16f,0.28f,0.10f); drawRect(BX-2,BY+26,200,12);
    
    // Main hull sides (thicker armor)
    col3(0.18f,0.31f,0.11f); drawRect(BX-5,BY+38,205,32);
    
    // Side panel details
    col3(0.22f,0.36f,0.14f);
    drawRect(BX+5,BY+40,185,8);
    drawRect(BX+5,BY+58,185,8);
    
    // Hull vents/louvers
    col3(0.12f,0.20f,0.08f);
    for(int i=0;i<6;i++){
        drawRect(BX+15+i*28.0f,BY+32,6,4);
        drawRect(BX+15+i*28.0f,BY+62,6,4);
    }

    // === Upper Turret Hull ===
    col3(0.20f,0.35f,0.14f); drawRect(BX+15,BY+70,165,42);
    
    // Turret front slope (glacis)
    glBegin(GL_QUADS);
    col3(0.19f,0.33f,0.13f);
    glVertex2f(BX+15,BY+70); glVertex2f(BX+32,BY+112);
    glVertex2f(BX+80,BY+112); glVertex2f(BX+80,BY+70);
    glEnd();
    
    // Turret top with slight curve appearance
    col3(0.22f,0.37f,0.15f);
    drawRect(BX+20,BY+108,155,8);
    
    // Commander hatch
    col3(0.14f,0.24f,0.10f);
    drawRect(BX+130,BY+110,12,10);
    col3(0.25f,0.40f,0.18f);
    drawRect(BX+131,BY+111,10,8);
    
    // Periscope mount
    col3(0.18f,0.30f,0.12f); drawRect(BX+155,BY+108,6,8);
    col3(0.10f,0.16f,0.06f); drawFilledCircle(BX+158,BY+114,2);
    
    // Rivets along hull (realistic fasteners)
    col3(0.28f,0.42f,0.18f);
    for(int i=0;i<6;i++){
        drawFilledCircle(BX+30+i*28.0f,BY+75,2);
        drawFilledCircle(BX+30+i*28.0f,BY+105,2);
    }

    // === RADAR SYSTEM - Enhanced ===
    // Mast support structure
    col3(0.24f,0.40f,0.16f); drawRect(BX+22,BY+106,8,38);
    col3(0.18f,0.30f,0.12f);
    drawRect(BX+20,BY+140,12,2);
    drawRect(BX+20,BY+144,12,2);
    
    // Mast reinforcement bands
    col3(0.20f,0.34f,0.14f);
    for(int i=0;i<4;i++) drawRect(BX+20,BY+112+i*8.0f,12,1);
    
    // Dish mounting bracket

    
    // Dish bowl (parabolic reflector)
    // === RADAR DISH (Enhanced Parabolic Reflector - Rotating Semicircle) ===
    // Calculate rotation angle based on frame counter
    float dishRotation = gFrameCounter * 2.2f;  // Continuous rotation
    
    glPushMatrix();
    glTranslatef(BX+25, BY+150, 0);
    glRotatef(dishRotation, 0, 0, 1);  // Rotate around center
    glTranslatef(-(BX+25), -(BY+150), 0);
    
    // Outer dish rim shadow (3D depth effect)
    col3(0.35f,0.42f,0.24f);
    glBegin(GL_TRIANGLE_STRIP);
    for(int i=0;i<=32;i++){
        float a=PI+PI*i/32.0f;  // Only semicircle (PI to 2*PI)
        glVertex2f(BX+25+34.0f*cosf(a), BY+150+22.0f*sinf(a));
        glVertex2f(BX+25+35.5f*cosf(a), BY+150+23.5f*sinf(a));
    }
    glEnd();
    
    // Main dish reflector surface (parabolic semicircle base)
    col3(0.52f,0.64f,0.36f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(BX+25,BY+150);
    for(int i=0;i<=24;i++){
        float a=PI+PI*i/24.0f;  // Only semicircle
        glVertex2f(BX+25+34.0f*cosf(a), BY+150+22.0f*sinf(a));
    }
    glEnd();
    
    // Dish reflector segments (metallic panels) - 12 segments for semicircle
    col3(0.58f,0.70f,0.40f);
    for(int i=0;i<12;i++){
        float a1=PI+PI*i/12.0f;
        float a2=PI+PI*(i+1)/12.0f;
        glBegin(GL_TRIANGLES);
        glVertex2f(BX+25,BY+150);
        glVertex2f(BX+25+34*cosf(a1), BY+150+22*sinf(a1));
        glVertex2f(BX+25+34*cosf(a2), BY+150+22*sinf(a2));
        glEnd();
        
        // Panel edge highlight (metallic reflection)
        col3(0.68f,0.78f,0.52f);
        glLineWidth(1.5f);
        glBegin(GL_LINES);
        glVertex2f(BX+25,BY+150);
        glVertex2f(BX+25+34*cosf(a1), BY+150+22*sinf(a1));
        glEnd();
        glLineWidth(1.0f);
        col3(0.58f,0.70f,0.40f);
    }
    
    // Inner concentric rings for reflector detail (semicircle only)
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
    
    // Dish rim structure (circular frame with beveled edge - semicircle)
    col3(0.65f,0.75f,0.48f);
    glLineWidth(3.5f);
    glBegin(GL_LINE_STRIP);
    for(int i=0;i<=32;i++){
        float a=PI+PI*i/32.0f;
        glVertex2f(BX+25+34.0f*cosf(a), BY+150+22.0f*sinf(a));
    }
    glEnd();
    
    // Connect the two ends of semicircle with straight lines
    glLineWidth(3.5f);
    glBegin(GL_LINES);
    glVertex2f(BX+25+34.0f*cosf(PI), BY+150+22.0f*sinf(PI));
    glVertex2f(BX+25, BY+150);
    glVertex2f(BX+25+34.0f*cosf(2*PI), BY+150+22.0f*sinf(2*PI));
    glVertex2f(BX+25, BY+150);
    glEnd();
    glLineWidth(1.0f);
    
    // Rim inner edge highlight (semicircle)
    col3(0.72f,0.82f,0.55f);
    glLineWidth(2.0f);
    glBegin(GL_LINE_STRIP);
    for(int i=0;i<=32;i++){
        float a=PI+PI*i/32.0f;
        glVertex2f(BX+25+33.0f*cosf(a), BY+150+21.0f*sinf(a));
    }
    glEnd();
    glLineWidth(1.0f);
    
    glPopMatrix();  // End rotation
    
    // Dish feed horn (center receiver) - Enhanced
    col3(0.25f, 0.35f, 0.15f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(BX+25,BY+150-8);
    for(int i=0;i<=16;i++){
        float a=2.0f*PI*i/16.0f;
        glVertex2f(BX+25+8.0f*cosf(a), BY+150+8.0f*sinf(a)-8);
    }
    glEnd();
    
    // Feed horn outer ring
    col3(0.35f, 0.50f, 0.25f);
    glLineWidth(2.0f);
    glBegin(GL_LINE_LOOP);
    for(int i=0;i<=16;i++){
        float a=2.0f*PI*i/16.0f;
        glVertex2f(BX+25+8.0f*cosf(a), BY+150+8.0f*sinf(a)-8);
    }
    glEnd();
    
    // Feed horn waveguide
    col3(0.58f,0.68f,0.40f);
    drawRect(BX+22,BY+142,6,8);
    
    // Feed horn entrance window (receiver)
    col3(0.68f, 0.78f, 0.52f);
    drawFilledCircle(BX+25,BY+150-6,4);
    col3(0.58f, 0.70f, 0.42f);
    drawFilledCircle(BX+25,BY+150-6,2);
    glLineWidth(1.0f);

    // === MISSILE LAUNCHER SYSTEM (3 tubes) ===
    // Launcher frame/cradle
    col3(0.20f,0.34f,0.14f);
    drawRect(BX+95,BY+80,48,35);
    
    for(int t=0;t<3;t++){
        glPushMatrix();
        glTranslatef(BX+119,BY+95,0);
        glRotatef(50.0f+t*10.0f,0,0,1);

        // Tube exterior (gun barrel)
        col3(0.22f,0.36f,0.16f); drawRect(0,-6,90,12);
        
        // Tube ribbing (thermal dissipation fins)
        col3(0.18f,0.30f,0.13f);
        for(int r=0;r<8;r++) drawRect(10+r*8.0f,-6.5f,1.5f,13);
        
        // Tube muzzle (end cap)
        col3(0.16f,0.27f,0.11f); drawRect(87,-7,5,14);
        col3(0.20f,0.33f,0.14f); drawRect(88,-6,3,12);
        
        // Muzzle brake
        col3(0.24f,0.38f,0.17f);
        glBegin(GL_TRIANGLES);
        glVertex2f(93,-8); glVertex2f(100,-10); glVertex2f(100,-6);
        glEnd();
        glBegin(GL_TRIANGLES);
        glVertex2f(93,8); glVertex2f(100,10); glVertex2f(100,6);
        glEnd();

        // Missile shell in tube
        col3(0.68f,0.68f,0.72f); drawRect(45,-5,38,10);
        
        // Missile warhead (pointed nose)
        col3(0.82f,0.12f,0.08f);
        glBegin(GL_TRIANGLES);
        glVertex2f(83,0); glVertex2f(72,-5); glVertex2f(72,5);
        glEnd();
        
        // Missile body bands (segmentation)
        col3(0.88f,0.85f,0.20f);
        drawRect(60,-5,4,10);
        drawRect(50,-5,4,10);
        
        // Tail fin assembly
        col3(0.48f,0.50f,0.56f);
        glBegin(GL_POLYGON);
        glVertex2f(45,0); glVertex2f(40,-6); glVertex2f(48,-10); glVertex2f(50,-8);
        glEnd();
        glBegin(GL_POLYGON);
        glVertex2f(45,0); glVertex2f(40,6); glVertex2f(48,10); glVertex2f(50,8);
        glEnd();

        glPopMatrix();
    }    col3(0.22f,0.36f,0.16f);
    glBegin(GL_QUADS);
    glVertex2f(BX+22,BY+144); glVertex2f(BX+28,BY+144);
    glVertex2f(BX+30,BY+148); glVertex2f(BX+20,BY+148);
    glEnd();
    
    // Launcher elevation mechanism
    col3(0.16f,0.26f,0.10f);
    drawRect(BX+90,BY+75,2,35);
    drawRect(BX+145,BY+75,2,35);

    // === VEHICLE DETAILS ===
    // Headlights/searchlight
    col3(0.28f,0.44f,0.20f); drawRect(BX+175,BY+50,8,8);
    col3(0.60f,0.75f,0.90f); drawFilledCircle(BX+179,BY+54,3);
    
    // Antenna (small)
    col3(0.20f,0.34f,0.14f); drawRect(BX+50,BY+114,2,12);
    col3(0.25f,0.40f,0.18f); drawFilledCircle(BX+51,BY+128,2);

    // Vehicle identification marking
    col3(0.88f,0.88f,0.22f);
    drawText(BX+110,BY+32,"ZAM-7",GLUT_BITMAP_HELVETICA_12);
}

// ─── Radar Display ───────────────────────────────────────────

static void drawRadar()
{
    float cx=RADAR_X, cy=RADAR_Y, r=RADAR_R;
    float sweepRad = gRadarAngle * PI/180.0f;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

    // Background with pulsing effect - lighter green
    float bgPulse = 0.95f + 0.05f * sinf(gFrameCounter * 0.05f);
    col4(0.02f,0.12f,0.02f,bgPulse); drawFilledCircle(cx,cy,r);
    
    // Subtle radial gradient shading (lighter outer ring)
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    col4(0.05f,0.18f,0.05f,0.15f);
    drawFilledCircle(cx,cy,r);

    // Concentric rings with enhanced glow - lighter color
    glLineWidth(1.0f);
    for(int i=1;i<=4;i++){
        col4(0.10f,0.55f,0.20f,0.65f);
        drawCircleOutline(cx,cy,r*(float)i/4.0f);
    }
    // Cross hairs
    col4(0.00f,0.42f,0.12f,0.6f);
    drawLine(cx-r,cy,cx+r,cy);
    drawLine(cx,cy-r,cx,cy+r);

    // Sweep trail (fan) - enhanced
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
    
    // Sweep line with glow
    col4(0.00f,0.80f,0.20f,0.3f);
    glLineWidth(4.0f);
    drawLine(cx,cy, cx+r*cosf(sweepRad), cy+r*sinf(sweepRad));
    col4(0.20f,1.00f,0.40f,1.0f);
    glLineWidth(2.0f);
    drawLine(cx,cy, cx+r*cosf(sweepRad), cy+r*sinf(sweepRad));
    glLineWidth(1.0f);

    // Drone blips on radar
    for(int i=0;i<MAX_DRONES;i++){
        if(gDrones[i].active && gDrones[i].detected){
            // Map world position to radar screen
            float wx=(gDrones[i].x - cx)/(RADAR_DETECT_RANGE)*r;
            float wy=(gDrones[i].y - cy)/(RADAR_DETECT_RANGE)*r;
            float bd=sqrtf(wx*wx+wy*wy);
            if(bd>r-5){ wx=wx/bd*(r-5); wy=wy/bd*(r-5); }
            // Pulsing blip
            float blipPulse = 0.5f + 0.5f * sinf(gFrameCounter * 0.15f);
            if(blipPulse > 0.3f){
                col4(1.0f,0.25f,0.05f,1.0f);
                drawFilledCircle(cx+wx,cy+wy,5);
                col4(1.0f,0.60f,0.30f,0.6f);
                drawFilledCircle(cx+wx,cy+wy,8);
            }
        }
    }

    // Radar outer ring
    col4(0.10f,0.70f,0.25f,1.0f);
    glLineWidth(3.0f);
    drawCircleOutline(cx,cy,r);
    glLineWidth(1.0f);

    // Centre dot with glow
    col4(0.60f,1.00f,0.70f,0.5f);
    drawFilledCircle(cx,cy,8);
    col4(0.30f,1.00f,0.50f,1.0f);
    drawFilledCircle(cx,cy,5);

    glDisable(GL_BLEND);
}

// ─── Drone ───────────────────────────────────────────────────
// Draw an enemy drone with animation effects
static void drawDrone(float x,float y,bool detected)
{
    // Calculate banking (tilting) angle based on frame counter for smooth flight animation
    float bankAngle = sinf(gFrameCounter * 0.05f) * 8.0f;
    
    glPushMatrix();
    glTranslatef(x, y, 0);
    glRotatef(bankAngle, 0, 0, 1);  // Apply banking rotation
    
    // Detection highlight ring with pulsing effect (shows it's been detected)
    if(detected){
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        float pulse = 0.5f + 0.3f * sinf(gFrameCounter * 0.1f);  // Pulsing transparency
        col4(1.0f,0.3f,0.0f,pulse*0.25f); drawFilledCircle(0,0,30);
        glDisable(GL_BLEND);
    }

    // Fuselage (main body)
    col3(0.48f,0.50f,0.55f);
    glBegin(GL_POLYGON);
    glVertex2f(28,0);
    glVertex2f(15,6);
    glVertex2f(-28,5);
    glVertex2f(-28,-5);
    glVertex2f(15,-6);
    glEnd();

    // Swept wings (upper and lower)
    col3(0.42f,0.44f,0.50f);
    glBegin(GL_TRIANGLES);
    // upper wing
    glVertex2f(5,2);   glVertex2f(-15,24); glVertex2f(-20,2);
    // lower wing
    glVertex2f(5,-2);   glVertex2f(-15,-24); glVertex2f(-20,-2);
    glEnd();

    // Tail fin (vertical)
    col3(0.38f,0.40f,0.46f);
    glBegin(GL_TRIANGLES);
    glVertex2f(-22,0);  glVertex2f(-34,12); glVertex2f(-22,5);
    glEnd();

    // Cockpit canopy
    col3(0.20f,0.65f,0.82f);
    drawFilledCircle(18,0,7);
    col3(0.55f,0.85f,0.95f);
    drawFilledCircle(20,2,3);

    // Engine exhaust glow with animation
    float glowPulse = 0.7f + 0.3f * sinf(gFrameCounter * 0.15f);
    col3(0.85f,0.45f,0.05f * glowPulse); drawFilledCircle(-28,0,5);
    col3(1.00f,0.85f,0.10f * glowPulse); drawFilledCircle(-28,0,2.5f);

    // Navigation lights (red port/left, green starboard/right)
    col3(1.0f,0.0f,0.0f); drawFilledCircle(-15,24,2.5f);
    col3(0.0f,1.0f,0.0f); drawFilledCircle(-15,-24,2.5f);
    
    glPopMatrix();
}

// ─── Missile ─────────────────────────────────────────────────
// Draw a guided missile with smoke trail effects
static void drawMissileProj(float x,float y,float tx,float ty)
{
    // Calculate rotation angle to point toward target
    float angle=atan2f(ty-y,tx-x);
    glPushMatrix();
    glTranslatef(x,y,0);
    glRotatef(angle*180.0f/PI,0,0,1);

    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

    // Layered smoke trail effect (progressively fading halos)
    for(int i = 1; i <= 5; i++){
        float alpha = 0.4f * (1.0f - i/5.0f);
        col4(0.85f,0.70f,0.40f,alpha);
        drawFilledCircle(-8.0f*i,-0.5f,3.0f + i*0.5f, 16);
    }

    // Hot exhaust plume (innermost flame)
    col4(0.70f,0.30f,0.00f,0.70f);
    glBegin(GL_TRIANGLES);
    glVertex2f(-14,-3); glVertex2f(-45,0); glVertex2f(-14,3);
    glEnd();
    col4(1.00f,0.65f,0.10f,0.50f);
    glBegin(GL_TRIANGLES);
    glVertex2f(-14,-2); glVertex2f(-30,0); glVertex2f(-14,2);
    glEnd();

    glDisable(GL_BLEND);

    // Body
    col3(0.72f,0.72f,0.78f); drawRect(-13,-4,28,8);

    // Nose cone glow (heat effect)
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    col4(1.0f, 0.30f, 0.10f, 0.4f);
    drawFilledCircle(15, 0, 5);
    glDisable(GL_BLEND);

    // Nose
    col3(0.90f,0.18f,0.10f);
    glBegin(GL_TRIANGLES);
    glVertex2f(15,0); glVertex2f(5,-4); glVertex2f(5,4);
    glEnd();

    // Fins
    col3(0.50f,0.52f,0.60f);
    glBegin(GL_TRIANGLES);
    glVertex2f(-10,0); glVertex2f(-18,-10); glVertex2f(-5,0);
    glEnd();
    glBegin(GL_TRIANGLES);
    glVertex2f(-10,0); glVertex2f(-18, 10); glVertex2f(-5,0);
    glEnd();

    // Band markings
    col3(0.85f,0.80f,0.10f);
    drawRect(0,-4,4,8);
    drawRect(-7,-4,4,8);

    glPopMatrix();
}

// ─── Explosion ───────────────────────────────────────────────

static void drawExplosion(const Explosion& e)
{
    float t=e.radius/e.maxRadius; // 0..1
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

    col4(1.0f,0.20f,0.00f,1.0f-t*0.6f); drawFilledCircle(e.x,e.y,e.radius);
    col4(1.0f,0.60f,0.00f,1.0f-t*0.7f); drawFilledCircle(e.x,e.y,e.radius*0.68f);
    col4(1.0f,0.95f,0.40f,1.0f-t*0.8f); drawFilledCircle(e.x,e.y,e.radius*0.40f);
    col4(1.0f,1.00f,1.00f,1.0f-t*0.9f); drawFilledCircle(e.x,e.y,e.radius*0.18f);

    glDisable(GL_BLEND);
}

// ─── Particles ───────────────────────────────────────────────
// Spawn particles (sparks, smoke, debris) at explosion location
static void spawnParticles(float x,float y)
{
    for(int i=0;i<MAX_PARTICLES;i++){
        if(!gParticles[i].active){
            float a=(float)(rand()%360)*PI/180.0f;
            float spd=1.5f+(rand()%40)/10.0f;
            int ptype = rand()%3; // 0=spark, 1=smoke, 2=debris
            float r = 1.0f, g = (rand()%10)/10.0f, b = 0.0f;
            
            // Customize particle properties based on type
            if(ptype == 1){ // smoke: gray, slow, drifts upward
                r = 0.8f; g = 0.8f; b = 0.8f;
                spd *= 0.6f;
            } else if(ptype == 2){ // debris: brown, fast ballistic
                r = 0.7f; g = 0.5f; b = 0.3f;
                spd *= 1.4f;
            }
            // else: spark remains bright yellow/orange, medium speed
            
            gParticles[i]={x,y,cosf(a)*spd,sinf(a)*spd,1.0f,r,g,b,ptype,true};
            static int cnt=0; if(++cnt>=12){ cnt=0; return; }  // Limit 12 particles per burst
        }
    }
}

// Render all active particles with appropriate visuals
static void drawParticles()
{
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    
    // Draw spark particles as bright points
    glPointSize(4.5f);
    glBegin(GL_POINTS);
    for(auto& p:gParticles){
        if(p.active && p.type == 0){
            col4(p.r,p.g,p.b,p.life);
            glVertex2f(p.x,p.y);
        }
    }
    glEnd();
    
    // Draw smoke particles as expanding circles (soft billowing effect)
    for(auto& p:gParticles){
        if(p.active && p.type == 1){
            col4(p.r,p.g,p.b,p.life*0.4f);
            drawFilledCircle(p.x,p.y,2.5f+p.life*2.0f,12);
        }
    }
    
    // Draw debris particles as solid points
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

// Update particle physics each frame
static void updateParticles()
{
    for(auto& p:gParticles){
        if(!p.active) continue;
        p.x+=p.vx; p.y+=p.vy;
        
        if(p.type == 1){ // smoke: rises and dissipates horizontally
            p.vy += 0.04f;
            p.vx *= 0.98f;  // Air resistance
        } else {
            p.vy-=0.08f; // gravity for sparks and debris
        }
        
        p.life-=0.025f;  // Fade out over time
        if(p.life<=0) p.active=false;
    }
}

// ─── Vehicles ────────────────────────────────────────────────

static void drawCar(float x,float y,float r,float g,float b)
{
    glPushMatrix();
    glTranslatef(x+41,y+26,0);  // Center of car
    
    // Undercarriage shadow
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    col4(0.0f,0.0f,0.0f,0.2f);
    glBegin(GL_POLYGON);
    glVertex2f(-40,-16); glVertex2f(40,-16);
    glVertex2f(40,-14); glVertex2f(-40,-14);
    glEnd();
    glDisable(GL_BLEND);
    
    // === MAIN CAR BODY (Improved proportions) ===
    // Body shell with better sedan profile
    col3(r,g,b);
    glBegin(GL_POLYGON);
    // Front bumper area (wider)
    glVertex2f(-38,0);
    glVertex2f(-36,8);
    // Hood taper
    glVertex2f(-20,10);
    glVertex2f(0,12);
    glVertex2f(20,10);
    // Roof line
    glVertex2f(36,8);
    glVertex2f(38,0);
    // Rear bumper
    glVertex2f(36,-6);
    glVertex2f(-36,-6);
    glEnd();
    
    // Side skirt (darker accent line)
    col3(r*0.65f,g*0.65f,b*0.65f);
    glBegin(GL_QUADS);
    glVertex2f(-36,-5); glVertex2f(36,-5);
    glVertex2f(36,-2); glVertex2f(-36,-2);
    glEnd();
    
    // === ROOF SECTION ===
    col3(r*0.75f,g*0.75f,b*0.75f);
    glBegin(GL_POLYGON);
    glVertex2f(-20,10);
    glVertex2f(-22,22);
    glVertex2f(22,22);
    glVertex2f(20,10);
    glEnd();
    
    // === WINDSHIELDS ===
    // Front windshield (angled)
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
    
    // Rear windshield
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
    
    // === SIDE MIRRORS ===
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
    
    // === BUMPERS ===
    // Front bumper detail
    col3(0.25f,0.25f,0.30f);
    glBegin(GL_QUADS);
    glVertex2f(-36,0); glVertex2f(36,0);
    glVertex2f(36,2); glVertex2f(-36,2);
    glEnd();
    
    // Rear bumper detail
    glBegin(GL_QUADS);
    glVertex2f(-36,-6); glVertex2f(36,-6);
    glVertex2f(36,-4); glVertex2f(-36,-4);
    glEnd();
    
    // === WHEELS ===
    // Front wheel
    glPushMatrix();
    glTranslatef(-22,-15,0);
    
    // Tire (black)
    col3(0.12f,0.12f,0.12f);
    drawFilledCircle(0,0,14);
    
    // Rim (alloy)
    col3(0.55f,0.55f,0.60f);
    drawFilledCircle(0,0,9);
    
    // Hubcap (chrome)
    col3(0.85f,0.85f,0.90f);
    drawFilledCircle(0,0,5);
    
    // Hubcap center dot
    col3(0.70f,0.70f,0.75f);
    drawFilledCircle(0,0,2);
    
    glPopMatrix();
    
    // Rear wheel
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
    
    // === HEADLIGHTS ===
    glPushMatrix();
    glTranslatef(-32,4,0);
    
    col3(1.00f,1.00f,0.70f);
    drawFilledCircle(0,0,5);
    
    col3(0.95f,0.90f,0.40f);
    drawFilledCircle(0,0,3);
    
    glPopMatrix();
    
    // === TAIL LIGHTS ===
    glPushMatrix();
    glTranslatef(32,-2,0);
    
    col3(0.90f,0.15f,0.15f);
    drawFilledCircle(0,0,4);
    
    col3(1.00f,0.40f,0.40f);
    drawFilledCircle(0,0,2);
    
    glPopMatrix();
    
    // === GRILLE ===
    col3(0.20f,0.20f,0.25f);
    for(int i=0; i<4; i++){
        glBegin(GL_LINES);
        glVertex2f(-28,2+i*2); glVertex2f(-20,2+i*2);
        glEnd();
    }
    
    // === LICENSE PLATE ===
    col3(0.95f,0.95f,0.95f);
    glBegin(GL_QUADS);
    glVertex2f(28,-8); glVertex2f(38,-8);
    glVertex2f(38,-4); glVertex2f(28,-4);
    glEnd();
    
    col3(0.15f,0.15f,0.15f);
    drawText(30,-6,"ABC",GLUT_BITMAP_HELVETICA_12);
    
    glPopMatrix();
}


static void drawMotorcycle(float x,float y)
{
    glPushMatrix();
    glTranslatef(x+28.0f, y+20.0f, 0.0f);

    // Ground shadow keeps the bike anchored to the road.
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    col4(0.0f,0.0f,0.0f,0.18f);
    drawRect(-25.0f,-16.0f,54.0f,3.0f);
    glDisable(GL_BLEND);

    // Wheels
    col3(0.10f,0.10f,0.12f);
    drawFilledCircle(-16.0f,-8.0f,11.0f);
    drawFilledCircle(18.0f,-8.0f,11.0f);
    col3(0.50f,0.52f,0.56f);
    drawFilledCircle(-16.0f,-8.0f,6.5f);
    drawFilledCircle(18.0f,-8.0f,6.5f);
    col3(0.76f,0.76f,0.80f);
    drawFilledCircle(-16.0f,-8.0f,2.5f);
    drawFilledCircle(18.0f,-8.0f,2.5f);

    // Swingarm and chassis tubes
    col3(0.20f,0.20f,0.24f);
    glLineWidth(3.0f);
    drawLine(-12.0f,-5.0f, 6.0f,-2.0f);
    drawLine( 6.0f,-2.0f,18.0f,-5.0f);
    drawLine(-12.0f,-5.0f,-2.0f, 8.0f);
    drawLine(-2.0f, 8.0f,12.0f,10.0f);
    drawLine(12.0f,10.0f,18.0f,-5.0f);
    glLineWidth(1.0f);

    // Engine block
    col3(0.25f,0.25f,0.30f);
    drawRect(-2.0f,-2.0f,10.0f,8.0f);
    col3(0.35f,0.35f,0.40f);
    drawRect(-1.0f,-1.0f,8.0f,3.0f);

    // Exhaust
    col3(0.42f,0.42f,0.46f);
    drawRect(8.0f,-3.0f,16.0f,2.8f);
    col3(0.58f,0.58f,0.62f);
    drawRect(23.0f,-3.2f,3.0f,3.2f);

    // Fuel tank and seat
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

    // Front fork and handlebar
    col3(0.45f,0.45f,0.50f);
    glLineWidth(2.5f);
    drawLine(14.0f,12.0f,20.0f,-1.0f);
    drawLine(15.5f,12.0f,21.5f,-1.0f);
    glLineWidth(1.0f);
    col3(0.28f,0.28f,0.32f);
    drawRect(10.0f,15.0f,9.0f,1.8f);

    // Headlight and beam
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

    // Rider torso and head
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

    // Helmet and visor
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

    // Arms to handlebar
    col3(0.76f,0.56f,0.36f);
    glLineWidth(3.0f);
    drawLine(1.0f,20.0f, 9.5f,16.0f);
    drawLine(5.0f,20.0f,13.0f,15.8f);
    glLineWidth(1.0f);

    // Rear light
    col3(0.90f,0.14f,0.14f);
    drawRect(-10.2f,10.8f,2.2f,1.8f);

    glPopMatrix();
}

static void drawVehicles()
{
    for(auto& v:gVehicles){
        if(v.type==0) drawCar(v.x,v.y,v.r,v.g,v.b);
        else          drawMotorcycle(v.x,v.y);
    }
}

// ─── HUD ─────────────────────────────────────────────────────

static void drawHUD()
{
    // Display pause overlay only
    if(gPaused){
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        col4(0,0,0,0.6f); drawRect(0,0,WIN_W,WIN_H);
        glDisable(GL_BLEND);
        col3(1.0f,1.0f,0.0f);
        drawText(530,370,"-- PAUSED --");
        drawText(460,320,"[P] Resume | [ESC] Quit",GLUT_BITMAP_HELVETICA_12);
    }
}

// ─── Spawn / Init ────────────────────────────────────────────
// Initialize all game systems and data structures
static void initAll()
{
    srand((unsigned)time(nullptr));  // Seed random number generator

    // Generate random star positions in sky
    for(int i=0;i<200;i++){
        gStarX[i]=(float)(rand()%WIN_W);
        gStarY[i]=230+(float)(rand()%(WIN_H-230));  // Only in sky area
    }

    // Initialize street light positions (two rows along road)
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

    // Deactivate all game entities initially
    for(auto& d:gDrones)    { d.active=false; d.detected=false; d.missileAssigned=-1; }
    for(auto& m:gMissiles)  { m.active=false; }
    for(auto& e:gExplosions){ e.active=false; }
    for(auto& p:gParticles) { p.active=false; }
    
    // Initialize cloud layer with parallax speeds (faster = closer to camera) and VERY LARGE sizes (4x bigger)
    float cloudData[][4] = {
        // Far layer (small, slow) - 4x scale
        {50,600,0.08f,1.2f},    // x, y, speed, scale
        {280,610,0.06f,1.4f},
        {520,590,0.07f,1.2f},
        {750,605,0.075f,1.28f},
        {950,615,0.065f,1.12f},
        {1100,600,0.07f,1.2f},
        // Middle layer (medium, moderate speed) - 4x scale
        {100,520,0.15f,2.8f},
        {350,550,0.12f,3.2f},
        {600,510,0.14f,3.0f},
        {880,535,0.13f,3.12f},
        {1150,520,0.16f,2.88f},
        // Close layer (large, fast) - 4x scale
        {150,450,0.35f,5.6f},
        {400,480,0.32f,6.0f},
        {750,430,0.38f,6.4f},
        {1000,470,0.34f,5.8f},
        // Extra large backdrop clouds - 4x scale
        {200,350,0.12f,8.8f},
        {700,380,0.1f,8.0f},
        {1050,360,0.11f,8.4f},
        {50,420,0.16f,5.2f},
        {900,520,0.18f,5.0f},
    };
    for(int i=0;i<20;i++){
        gClouds[i]={cloudData[i][0],cloudData[i][1],cloudData[i][2],cloudData[i][3],true};
    }

    // Initialize road vehicles at various positions and speeds
    float vd[][7]={
        {220,88,1.5f,0, 0.70f,0.10f,0.12f},   // x, y, speed, type, r, g, b
        {480,88,2.0f,1, 0.00f,0.00f,0.00f},
        {700,88,1.8f,0, 0.10f,0.22f,0.72f},
        {900,88,2.2f,0, 0.08f,0.55f,0.18f},
        {1060,88,1.6f,0,0.50f,0.10f,0.50f},
    };
    for(int i=0;i<5;i++){
        gVehicles[i]={vd[i][0],vd[i][1],vd[i][2],(int)vd[i][3],vd[i][4],vd[i][5],vd[i][6]};
    }
}

// Spawn a new enemy drone with random speed and altitude
static void spawnDrone()
{
    for(auto& d:gDrones){
        if(!d.active){
            d.x       = (float)(WIN_W+60);                          // Start off right edge
            d.y       = 420.0f+(rand()%180);                        // Random altitude - moved up higher
            d.speed   = 1.4f+(rand()%25)/10.0f;                    // Variable speed
            d.active  = true;
            d.detected= false;
            d.missileAssigned=-1;
            return;
        }
    }
}

// Launch a missile from defense vehicle targeting the specified drone
static void launchMissile(int droneIdx)
{
    for(int i=0;i<MAX_MISSILES;i++){
        if(!gMissiles[i].active){
            gMissiles[i].x        = 185.0f;              // Launch position (gun location)
            gMissiles[i].y        = 318.0f;
            gMissiles[i].speed    = 5.5f;
            gMissiles[i].active   = true;
            gMissiles[i].targetIdx= droneIdx;
            gDrones[droneIdx].missileAssigned=i;
            return;
        }
    }
}

// Create explosion at specified location with particles
static void addExplosion(float x,float y)
{
    for(auto& e:gExplosions){
        if(!e.active){
            e={x,y,4.0f,38.0f,0,true};  // Starting radius 4.0, max 38.0
            spawnParticles(x,y);         // Generate particle burst
            return;
        }
    }
}

// ─── Update / Timer ──────────────────────────────────────────
// Main game logic update called every 16ms (~60 fps)
static void update(int)
{
    if(!gPaused){

        // Increment frame counter for animations (used for sine-wave effects)
        gFrameCounter++;
        if(gFrameCounter > 10000) gFrameCounter = 0;  // Prevent overflow

        // Rotate radar sweep continuously
        gRadarAngle+=2.2f;
        if(gRadarAngle>=360.0f) gRadarAngle-=360.0f;

        // Update cloud positions (parallax scrolling)
        updateClouds();

        // Auto-spawn drones at increasing difficulty
        gSpawnTimer++;
        if(gSpawnTimer>=gSpawnInterval){
            spawnDrone();
            gSpawnTimer=0;
            // Slightly increase rate over time (more drones faster)
            if(gSpawnInterval>80) gSpawnInterval--;
        }

        // Update drone positions and detection
        for(int i=0;i<MAX_DRONES;i++){
            auto& d=gDrones[i];
            if(!d.active) continue;
            d.x-=d.speed;  // Move drone left across screen
            if(d.x<-80){ d.active=false; continue; }  // Remove when off-screen

            // Check if radar detects this drone
            if(!d.detected){
                float dist=fDist(d.x,d.y,RADAR_X,RADAR_Y);
                if(dist<RADAR_DETECT_RANGE){
                    d.detected=true;
                    launchMissile(i);  // Automatically launch missile
                }
            }
        }

        // Update missile positions and collision detection
        for(auto& m:gMissiles){
            if(!m.active) continue;
            int ti=m.targetIdx;
            if(ti<0||!gDrones[ti].active){ m.active=false; continue; }  // Invalid target

            float tx=gDrones[ti].x, ty=gDrones[ti].y;
            float d=fDist(m.x,m.y,tx,ty);
            if(d<18.0f){
                // HIT! Missile reached target
                addExplosion(tx,ty);
                gDrones[ti].active=false;
                m.active=false;
            } else {
                // Move missile toward target
                m.x+=(tx-m.x)/d*m.speed;
                m.y+=(ty-m.y)/d*m.speed;
            }
        }

        // Update explosion radius expansion
        for(auto& e:gExplosions){
            if(!e.active) continue;
            e.radius+=1.8f; e.frame++;
            if(e.radius>e.maxRadius) e.active=false;
        }

        // Update all particles physics and fade
        updateParticles();

        // Update vehicle positions on road
        for(auto& v:gVehicles){
            v.x+=v.speed;
            if(v.x>WIN_W+120) v.x=-120;  // Wrap around
        }
    }

    glutPostRedisplay();
    glutTimerFunc(16,update,0); // ~60 fps (16ms per frame)
}

// ─── Display ─────────────────────────────────────────────────
// Render the complete scene (called once per frame)
static void display()
{
    glClear(GL_COLOR_BUFFER_BIT);

    // Background and environment
    drawBackground();      // Sky, road, ground, stars, clouds, street lights
    drawCitySkyline();     // Buildings with lit windows
    drawVehicles();        // Cars and motorcycles on road
    drawDefenseVehicle();  // Player's anti-aircraft gun system
    drawRadar();           // Radar display with sweep and targets

    // Draw drones
    for(auto& d:gDrones)
        if(d.active) drawDrone(d.x,d.y,d.detected);

    // Draw active missiles
    for(auto& m:gMissiles){
        if(!m.active) continue;
        int ti=m.targetIdx;
        if(ti>=0&&gDrones[ti].active)
            drawMissileProj(m.x,m.y,gDrones[ti].x,gDrones[ti].y);
    }

    // Draw explosions from hits
    for(auto& e:gExplosions)
        if(e.active) drawExplosion(e);

    // Draw particles (sparks, smoke, debris)
    drawParticles();
    
    // Draw HUD overlay (pause status, system info, etc.)
    drawHUD();

    glutSwapBuffers();  // Double-buffer display
}

// ─── Reshape ─────────────────────────────────────────────────
// Handle window resizing - maintains 2D orthographic projection
static void reshape(int w,int h)
{
    glViewport(0,0,w,h);              // Update viewport to window size
    glMatrixMode(GL_PROJECTION); 
    glLoadIdentity();
    gluOrtho2D(0,WIN_W,0,WIN_H);      // Set up 2D coordinate system
    glMatrixMode(GL_MODELVIEW);  
    glLoadIdentity();
}

// ─── Input ───────────────────────────────────────────────────
// Handle keyboard input for player controls
static void keyboard(unsigned char key,int,int)
{
    switch(key){
        case 27:  exit(0); break;                    // ESC → Quit program
        case 's': case 'S': spawnDrone(); break;     // S → Trigger manual drone spawn (bypasses auto-spawn interval)
        case 'p': case 'P': gPaused=!gPaused; break; // P → Toggle pause
    }
}

// ─── Main ────────────────────────────────────────────────────
// Program entry point - sets up OpenGL context and starts main loop
int main(int argc,char** argv)
{
    // Initialize GLUT and display mode
    glutInit(&argc,argv);
    glutInitDisplayMode(GLUT_DOUBLE|GLUT_RGB);     // Double-buffered RGB rendering
    glutInitWindowSize(WIN_W,WIN_H);               // Set window resolution
    glutInitWindowPosition(80,40);                 // Set window position on desktop
    glutCreateWindow("2D Air Defense Simulation System  |  [S] Spawn  [P] Pause  [ESC] Quit");

    // Initialize OpenGL settings
    glClearColor(0.01f,0.04f,0.16f,1.0f);          // Dark blue background

    // Set up 2D orthographic projection
    glMatrixMode(GL_PROJECTION); 
    glLoadIdentity();
    gluOrtho2D(0,WIN_W,0,WIN_H);
    glMatrixMode(GL_MODELVIEW);  
    glLoadIdentity();

    // Initialize game data and systems
    initAll();

    // Register GLUT callback functions
    glutDisplayFunc(display);                      // Render callback
    glutReshapeFunc(reshape);                      // Resize callback
    glutKeyboardFunc(keyboard);                    // Input callback
    glutTimerFunc(16,update,0);                    // Start 16ms update timer

    // Enter main event loop (runs indefinitely until exit)
    glutMainLoop();
    return 0;
}
