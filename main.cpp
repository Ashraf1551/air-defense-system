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

// ─── Data Structures ─────────────────────────────────────────
// Enemy drone that flies across the screen from right to left
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

// ─── Globals ─────────────────────────────────────────────────

// Radar subsystem
float gRadarAngle   = 0.0f;
const float RADAR_X = 100.0f;                      // Radar center X position (top-left)
const float RADAR_Y = 580.0f;                      // Radar center Y position (top-left)
const float RADAR_R = 88.0f;                       // Radar display radius on screen
const float RADAR_DETECT_RANGE = 420.0f;           // World-space detection radius

// Game entity arrays (pool allocation for efficiency)
Drone      gDrones    [MAX_DRONES];                // Enemy drones
Missile    gMissiles  [MAX_MISSILES];              // Guided missiles
Explosion  gExplosions[MAX_EXPLOSIONS];            // Explosion effects
Particle   gParticles [MAX_PARTICLES];             // Individual particles
Vehicle    gVehicles  [5];                         // Road traffic (cars/motorcycles)
Cloud      gClouds    [20];                        // Parallax cloud layer (expanded)

// Game state
int  gScore          = 0;                          // Player's current score
int  gSpawnTimer     = 0;                          // Counter for drone spawning
int  gSpawnInterval  = 200;                        // Frames between auto-spawns
bool gPaused         = false;                      // Pause toggle
int  gFrameCounter   = 0;                          // Master frame counter for animations

// Procedural generation data
float gStarX[200], gStarY[200];                    // Random star positions
float gStreetLights[20][2];                        // Street light coordinates

// ─── Forward Declarations ────────────────────────────────────
// Placeholder functions to be implemented in later commits
static void drawBackground();
static void drawCitySkyline();
static void drawDefenseVehicle();
static void drawRadar();
static void drawDrone(float x, float y, bool detected);
static void drawMissileProj(float x, float y, float tx, float ty);
static void drawExplosion(const Explosion& e);
static void drawParticles();
static void drawVehicles();
static void drawHUD();

static void updateClouds();
static void updateParticles();
static void display();
static void reshape(int w, int h);
static void keyboard(unsigned char key, int, int);
static void update(int);
static void initAll();

// ─── Helpers ─────────────────────────────────────────────────
// Calculate Euclidean distance between two points
static inline float fDist(float x1, float y1, float x2, float y2) {
    float dx = x2 - x1, dy = y2 - y1;
    return sqrtf(dx * dx + dy * dy);
}

// Set 3-component RGB color
static void col3(float r, float g, float b) { glColor3f(r, g, b); }

// Set 4-component RGBA color with alpha (transparency)
static void col4(float r, float g, float b, float a) { glColor4f(r, g, b, a); }

static void drawFilledCircle(float cx, float cy, float r, int seg = 60) {
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);
    for (int i = 0; i <= seg; i++) {
        float a = 2.0f * PI * i / seg;
        glVertex2f(cx + r * cosf(a), cy + r * sinf(a));
    }
    glEnd();
}

static void drawCircleOutline(float cx, float cy, float r, int seg = 60) {
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < seg; i++) {
        float a = 2.0f * PI * i / seg;
        glVertex2f(cx + r * cosf(a), cy + r * sinf(a));
    }
    glEnd();
}

static void drawLine(float x1, float y1, float x2, float y2) {
    glBegin(GL_LINES);
    glVertex2f(x1, y1);
    glVertex2f(x2, y2);
    glEnd();
}

static void drawRect(float x, float y, float w, float h) {
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
}

static void drawText(float x, float y, const char* s, void* font = GLUT_BITMAP_HELVETICA_18) {
    glRasterPos2f(x, y);
    for (; *s; s++) glutBitmapCharacter(font, *s);
}
static void drawCloud(float cx, float cy, float scale) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    col4(200.0f / 255.0f, 232.0f / 255.0f, 245.0f / 255.0f, 0.65f);
    
    float radius = 18.0f * scale;
    drawFilledCircle(cx, cy, 0.07f * radius, 20);
    drawFilledCircle(cx - 0.06f * radius, cy, 0.07f * radius, 20);
    drawFilledCircle(cx + 0.06f * radius, cy, 0.07f * radius, 20);
    
    glDisable(GL_BLEND);
}

static void drawClouds() {
    for (int i = 0; i < 20; i++) {
        if (gClouds[i].active) {
            drawCloud(gClouds[i].x, gClouds[i].y, gClouds[i].scale);
        }
    }
}

static void updateClouds() {
    for (int i = 0; i < 20; i++) {
        if (gClouds[i].active) {
            gClouds[i].x += gClouds[i].speed;
            if (gClouds[i].x > WIN_W + 150) gClouds[i].x = -150;
        }
    }
}

static void drawBackground() {
    glBegin(GL_QUADS);
    col3(0.01f, 0.04f, 0.16f);
    glVertex2f(0, WIN_H);
    glVertex2f(WIN_W, WIN_H);
    col3(0.04f, 0.09f, 0.26f);
    glVertex2f(WIN_W, 230);
    glVertex2f(0, 230);
    glEnd();

    col3(0.12f, 0.12f, 0.14f);
    drawRect(0, 0, WIN_W, 230);

    col3(0.20f, 0.20f, 0.23f);
    drawRect(0, 75, WIN_W, 130);

    col3(0.55f, 0.55f, 0.60f);
    drawRect(0, 73, WIN_W, 4);
    drawRect(0, 201, WIN_W, 4);

    col3(0.85f, 0.80f, 0.10f);
    glLineWidth(2.5f);
    for (int i = 0; i < WIN_W; i += 90) drawLine((float)i, 138, (float)(i + 55), 138);
    glLineWidth(1.0f);

    drawClouds();
}

// ========== CITY SKYLINE ==========
static void drawCitySkyline() {
    // Simple buildings
    float buildings[][6] = {
        {0, 55, 190, 0.07f, 0.11f, 0.26f},
        {50, 48, 155, 0.08f, 0.13f, 0.29f},
        {95, 65, 215, 0.06f, 0.10f, 0.24f},
    };
    
    for (auto& b : buildings) {
        col3(b[3], b[4], b[5]);
        drawRect(b[0], 230, b[1], b[2]);
    }
}

// ========== RADAR ==========
static void drawRadar() {
    float cx = RADAR_X, cy = RADAR_Y, r = RADAR_R;
    float sweepRad = gRadarAngle * PI / 180.0f;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    col4(0.00f, 0.06f, 0.00f, 0.95f);
    drawFilledCircle(cx, cy, r);

    glLineWidth(1.0f);
    for (int i = 1; i <= 4; i++) {
        col4(0.00f, 0.45f, 0.12f, 0.6f);
        drawCircleOutline(cx, cy, r * (float)i / 4.0f);
    }

    col4(0.00f, 0.80f, 0.20f, 0.3f);
    glLineWidth(4.0f);
    drawLine(cx, cy, cx + r * cosf(sweepRad), cy + r * sinf(sweepRad));
    
    col4(0.10f, 0.70f, 0.25f, 1.0f);
    glLineWidth(3.0f);
    drawCircleOutline(cx, cy, r);
    glLineWidth(1.0f);

    col4(0.60f, 1.00f, 0.70f, 0.5f);
    drawFilledCircle(cx, cy, 8);
    col4(0.30f, 1.00f, 0.50f, 1.0f);
    drawFilledCircle(cx, cy, 5);

    glDisable(GL_BLEND);
}
static void drawDrone(float x, float y, bool detected) {
    float bankAngle = sinf(gFrameCounter * 0.05f) * 8.0f;
    
    glPushMatrix();
    glTranslatef(x, y, 0);
    glRotatef(bankAngle, 0, 0, 1);

    if (detected) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        float pulse = 0.5f + 0.3f * sinf(gFrameCounter * 0.1f);
        col4(1.0f, 0.3f, 0.0f, pulse * 0.25f);
        drawFilledCircle(0, 0, 30);
        glDisable(GL_BLEND);
    }

    col3(0.48f, 0.50f, 0.55f);
    glBegin(GL_POLYGON);
    glVertex2f(28, 0);
    glVertex2f(15, 6);
    glVertex2f(-28, 5);
    glVertex2f(-28, -5);
    glVertex2f(15, -6);
    glEnd();

    col3(0.20f, 0.65f, 0.82f);
    drawFilledCircle(18, 0, 7);

    col3(1.0f, 0.0f, 0.0f);
    drawFilledCircle(-15, 24, 2.5f);
    col3(0.0f, 1.0f, 0.0f);
    drawFilledCircle(-15, -24, 2.5f);

    glPopMatrix();
}

// ========== MISSILE ==========
static void drawMissileProj(float x, float y, float tx, float ty) {
    float angle = atan2f(ty - y, tx - x);
    glPushMatrix();
    glTranslatef(x, y, 0);
    glRotatef(angle * 180.0f / PI, 0, 0, 1);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    for (int i = 1; i <= 5; i++) {
        float alpha = 0.4f * (1.0f - i / 5.0f);
        col4(0.85f, 0.70f, 0.40f, alpha);
        drawFilledCircle(-8.0f * i, -0.5f, 3.0f + i * 0.5f, 16);
    }

    glDisable(GL_BLEND);

    col3(0.72f, 0.72f, 0.78f);
    drawRect(-13, -4, 28, 8);

    col3(0.90f, 0.18f, 0.10f);
    glBegin(GL_TRIANGLES);
    glVertex2f(15, 0);
    glVertex2f(5, -4);
    glVertex2f(5, 4);
    glEnd();

    glPopMatrix();
}

static void drawExplosion(const Explosion& e) {
    float t = e.radius / e.maxRadius;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    col4(1.0f, 0.20f, 0.00f, 1.0f - t * 0.6f);
    drawFilledCircle(e.x, e.y, e.radius);
    col4(1.0f, 0.60f, 0.00f, 1.0f - t * 0.7f);
    drawFilledCircle(e.x, e.y, e.radius * 0.68f);
    col4(1.0f, 0.95f, 0.40f, 1.0f - t * 0.8f);
    drawFilledCircle(e.x, e.y, e.radius * 0.40f);
    col4(1.0f, 1.00f, 1.00f, 1.0f - t * 0.9f);
    drawFilledCircle(e.x, e.y, e.radius * 0.18f);

    glDisable(GL_BLEND);
}

// ========== PARTICLES ==========
static void drawParticles() {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glPointSize(4.5f);
    glBegin(GL_POINTS);
    for (auto& p : gParticles) {
        if (p.active && p.type == 0) {
            col4(p.r, p.g, p.b, p.life);
            glVertex2f(p.x, p.y);
        }
    }
    glEnd();
    glPointSize(1.0f);
    glDisable(GL_BLEND);
}

static void updateParticles() {
    for (auto& p : gParticles) {
        if (!p.active) continue;
        p.x += p.vx;
        p.y += p.vy;
        p.vy -= 0.08f;
        p.life -= 0.025f;
        if (p.life <= 0) p.active = false;
    }
}

// ─── Display ─────────────────────────────────────────────────
// Render the complete scene (called once per frame)
static void display()
{
    glClear(GL_COLOR_BUFFER_BIT);
    // Placeholder - will be populated in commit 10
    drawCitySkyline();
    drawRadar();
    // ... other drawing functions
    glutSwapBuffers();
}

// ─── Reshape ─────────────────────────────────────────────────
// Handle window resizing - maintains 2D orthographic projection
static void reshape(int w, int h)
{
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, WIN_W, 0, WIN_H);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

// ─── Input ───────────────────────────────────────────────────
// Handle keyboard input for player controls
static void keyboard(unsigned char key, int, int)
{
    switch (key) {
        case 27:  exit(0); break;
        case 's': case 'S': break; // Placeholder
        case 'p': case 'P': gPaused = !gPaused; break;
    }
}

// ─── Update / Timer ──────────────────────────────────────────
// Main game logic update called every 16ms (~60 fps)
static void update(int)
{
    if (!gPaused) {
        gFrameCounter++;
        if (gFrameCounter > 10000) gFrameCounter = 0;
    }
    glutPostRedisplay();
    glutTimerFunc(16, update, 0);
}

// ─── Initialization ──────────────────────────────────────────
// Initialize all game systems and data structures
static void initAll()
{
    // Will be implemented in commit 9
}

// ─── Main ────────────────────────────────────────────────────
// Program entry point - sets up OpenGL context and starts main loop
int main(int argc, char** argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(WIN_W, WIN_H);
    glutInitWindowPosition(80, 40);
    glutCreateWindow("2D Air Defense | Commit: Setup | [S] Spawn [P] Pause [ESC] Quit");

    glClearColor(0.01f, 0.04f, 0.16f, 1.0f);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, WIN_W, 0, WIN_H);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    initAll();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutTimerFunc(16, update, 0);

    glutMainLoop();
    return 0;
}
