#include <graphics.h> // Includes BGI graphics library for drawing
#include <conio.h> // Includes console IO for keyboard handling
#include <iostream> // Includes standard IO stream for debug
#include <vector> // Includes vector container for arrays
#include <string> // Includes string class for text
#include <windows.h> // Includes Windows API for async key states
#include <cmath> // Includes math functions like sin, cos
#include <fstream> // Includes file stream to check assets

using namespace std; // Standard namespace for convenience

#define C(r,g,b) (((b)<<16)|((g)<<8)|(r)) // Macro to convert RGB components to BGI compatible color format

/* ==============================================================================
 * INDEX OF ARCHITECTURE (AS REQUESTED)
 * ------------------------------------------------------------------------------
 * struct Player      : Stores player state and physics. (Line ~35)
 * struct Portal      : Teleportation entity. (Line ~50)
 * struct Platform    : Unified environment entity (Solid, Wind, Moving). (Line ~55)
 * main()             : Core double-buffered game loop. (Line ~100)
 * initLevel()        : Level 1-10 initialization and mechanic scaling. (Line ~200)
 * handleInput()      : Keyboard input handler. (Line ~450)
 * handleAnimation()  : Processes jumping arcs and animation frames. (Line ~480)
 * updatePlatforms()  : Advanced math equations for moving platforms. (Line ~510)
 * updatePhysics()    : Continuous collision, gravity, portal bounds. (Line ~560)
 * drawPlayer()       : Renders player with programmatic mirroring. (Line ~750)
 * drawMenu()         : Modernized UI with centered coordinates. (Line ~800)
 * ============================================================================== */

// ========================================== // Section header
// CONSTANTS & ENUMS                          // Section header
// ========================================== // Section header
const int SW = 1200; // Screen width in pixels
const int SH = 800; // Screen height in pixels
const float GRAVITY = 0.8f; // Global gravity force applied per frame
const float JUMP_FORCE = -15.0f; // Upward jump velocity
const float MOVE_SPEED = 7.0f; // Horizontal movement speed
const int PORTAL_COOLDOWN_MAX = 60; // Frames to prevent instant teleport looping

enum State { ST_MENU, ST_CHARSEL, ST_LEVELSEL, ST_PLAY, ST_WIN }; // Enum for game FSM states

enum PlatformType { // Enum for unified platform types replacing spaghetti code
    PT_NORMAL,      // Static solid platform
    PT_GOAL,        // Level completion trigger
    PT_SPIKE,       // Deadly hazard
    PT_MOVING,      // Linear interpolated moving platform
    PT_FAKE,        // Disappears after short delay when stood upon
    PT_ONEWAY,      // Passable from bottom, solid from top
    PT_TIMED_SPIKE, // Hazard that toggles on and off automatically
    PT_WIND         // Force field that modifies player X velocity
}; // Ends PlatformType enum

// ========================================== // Section header
// STRUCTURES                                 // Section header
// ========================================== // Section header
struct Player { // Player data structure
    float x, y; // Spatial coordinates
    float vx, vy; // Velocity vectors
    int w, h; // Hitbox dimensions
    bool onGround; // Grounded physics state flag
    int charType; // 0 = Dokja, 1 = Soyoung
    int frame; // Current animation frame integer
    string action; // Current action state string (e.g., JUMP, IDLE)
    bool facingLeft; // Flag used to trigger programmatic sprite mirroring
    int landingTimer; // Tracks frames for landing impact animation
}; // Ends Player struct

struct Portal { // Portal data structure
    int x, y, w, h; // Bounding box for portal entrance
    int color; // Visual particle color
    int targetIndex; // Points to another portal's index for chaining
}; // Ends Portal struct

struct Platform { // Unified environment data structure
    float x1, y1, x2, y2; // Bounding box coordinates
    PlatformType type; // Defines mechanic behavior
    float startX, startY; // Origin anchor for moving platforms
    float endX, endY; // Destination anchor for moving platforms
    float speed; // Multi-purpose float: move speed or wind strength
    bool isActive; // Flag for timed or fake mechanics
    int timer; // State frame counter
    int maxTimer; // State interval threshold
    int moveDir; // Interpolation direction (1 or -1)
}; // Ends Platform struct

// ========================================== // Section header
// GLOBAL VARIABLES                           // Section header
// ========================================== // Section header
State gState = ST_MENU; // Initializes state to main menu
int fc = 0; // Global frame counter for timers and visual fx

Player player; // Global player instance
vector<Portal> portals; // Dynamic array of portal chaining entities
vector<Platform> platforms; // Dynamic array of unified level geometry
int portalCooldown = 0; // Cooldown variable to prevent overlap bouncing
int currentLevel = 1; // Current progression index

int menuSel = 0; // Menu UI cursor index (0=Play, 1=LevelSelect, 2=Exit)
int charSel = 0; // Character UI cursor index
int levelSel = 1; // Level select UI index

bool wasSpaceHeld = false; // Input buffer tracking for jump
bool wasUp = false; // Input buffer tracking for UI
bool wasDown = false; // Input buffer tracking for UI
bool wasLeft = false; // Input buffer tracking for UI
bool wasRight = false; // Input buffer tracking for UI
bool wasEnterMenu = false; // Input buffer tracking for menu confirm
bool wasEnterChar = false; // Input buffer tracking for char confirm
bool wasEnterLevelSel = false; // Input buffer tracking for level select confirm

// ========================================== // Section header
// PROTOTYPES                                 // Section header
// ========================================== // Section header
void initPlayer(int level); // Resets player state
void initLevel(int level); // Builds level geometry
void wipeMemory(); // Clears memory buffers
void drawMenu(); // Renders main menu UI
void drawCharSel(); // Renders character select UI
void drawLevelSel(); // Renders level select UI
void drawBG(); // Renders procedural background
void drawPlatforms(); // Renders all active platforms
void drawPortals(); // Renders all linked portals
void drawPlayer(); // Renders player with mirroring
void drawHUD(); // Renders overlay UI
void drawWin(); // Renders completion screen
void handleInput(); // Consumes keyboard events
void handleAnimation(); // Updates sprite sequence based on physics
void updatePhysics(); // Processes collisions and gravity
void updatePlatforms(); // Processes moving and timed mechanics
bool checkCollision(float px, float py, int pw, int ph, const Platform& p); // AABB math vs platform
bool checkCollision(float px, float py, int pw, int ph, const Portal& p); // AABB math vs portal
void myFilledEllipse(int cx, int cy, int rx, int ry, int color); // Primitive drawing wrapper
string getCheckedSpritePath(int charType, string action, int frame); // Robust asset loader
bool fileExists(const string& name); // File validation helper

// ========================================== // Section header
// MAIN LOOP                                  // Section header
// ========================================== // Section header
int main() { // Application entry point
    initwindow(SW, SH, "PORTAL MINI - MULTI LEVEL", 60, 40, false); // Initializes fixed-size BGI window
    
    currentLevel = 1; // Start at level 1
    initLevel(currentLevel); // Load geometry
    initPlayer(currentLevel); // Load player state
    
    while(true) { // Infinite game loop
        setactivepage(1 - getvisualpage()); // Double-buffering: draw to hidden page
        cleardevice(); // Wipe hidden page clean
        fc++; // Increment master frame counter
        
        switch(gState) { // FSM router
            case ST_MENU: // Main menu state
                drawMenu(); // Draw menu elements
                break; // Exit case
            case ST_CHARSEL: // Character select state
                drawCharSel(); // Draw select elements
                break; // Exit case
            case ST_LEVELSEL: // Level select state
                drawLevelSel(); // Draw level select grid
                break; // Exit case
            case ST_PLAY: // Gameplay state
                handleInput(); // Read inputs
                handleAnimation(); // Update sprite logic
                updatePlatforms(); // Move level pieces
                updatePhysics(); // Apply velocities and check bounds
                drawBG(); // Draw background
                drawPlatforms(); // Draw geometry
                drawPortals(); // Draw teleporters
                drawPlayer(); // Draw character
                drawHUD(); // Draw overlay
                break; // Exit case
            case ST_WIN: // Victory state
                drawWin(); // Draw victory UI
                break; // Exit case
        } // End switch
        
        setvisualpage(1 - getvisualpage()); // Double-buffering: flip hidden page to screen
        delay(16); // Thread sleep to maintain ~60 FPS
        
        if(gState == ST_MENU && (GetAsyncKeyState(VK_ESCAPE) & 0x8000)) break; // Exit game from menu via ESC
        if((gState == ST_PLAY || gState == ST_CHARSEL || gState == ST_LEVELSEL) && (GetAsyncKeyState(VK_ESCAPE) & 0x8000)) { // Pause to menu
            gState = ST_MENU; // Switch state
            Sleep(200); // Debounce delay
        } // Ends if
    } // Ends while
    closegraph(); // Closes BGI window
    return 0; // Exits program safely
} // Ends main function

// ========================================== // Section header
// UTILITY FUNCTIONS                          // Section header
// ========================================== // Section header
void myFilledEllipse(int cx, int cy, int rx, int ry, int color) { // Draws a solid filled ellipse
    setcolor(color); // Set border color
    for(int dy=-ry; dy<=ry; dy++){ // Scanline fill algorithm
        double f = 1.0-(double)dy*dy/((double)ry*ry); // Calculate bounds
        if(f<0) continue; // Skip out of bounds
        int hw=(int)(rx*sqrt(f)); // Compute horizontal width
        line(cx-hw, cy+dy, cx+hw, cy+dy); // Draw horizontal line segment
    } // Ends for
} // Ends function

bool fileExists(const string& name) { // Helper to verify file exists
    ifstream f(name.c_str()); // Attempt to open stream
    return f.good(); // Return true if valid
} // Ends function

string getCheckedSpritePath(int charType, string action, int frame) { // Builds and validates asset path
    string path; // Variable for final string
    if (action == "IDLE") { // If idle
        if (charType == 0) path = "kim dokja idol.jpg"; // Path for Dokja idle
        else path = "han soyoung IDOL.jpg"; // Path for Soyoung idle
    } else { // If moving or jumping
        string name = (charType == 0) ? "kim_dokja" : "han_soyoung"; // Set name prefix
        path = name + "_" + action + "_frame" + to_string(frame) + ".jpg"; // Concatenate full path
    } // Ends if
    
    if (!fileExists(path)) { // Validate existence
        if (charType == 0) return "kim dokja idol.jpg"; // Fallback Dokja
        else return "han soyoung IDOL.jpg"; // Fallback Soyoung
    } // Ends if
    return path; // Return safe path
} // Ends function

bool checkCollision(float px, float py, int pw, int ph, const Platform& p) { // AABB logic against platforms
    return (px < p.x2 && px + pw > p.x1 && py < p.y2 && py + ph > p.y1); // Fast bounding box overlap math
} // Ends function

bool checkCollision(float px, float py, int pw, int ph, const Portal& p) { // AABB logic against portals
    return (px < p.x + p.w && px + pw > p.x && py < p.y + p.h && py + ph > p.y); // Fast bounding box overlap math
} // Ends function

// ========================================== // Section header
// INITIALIZATION                             // Section header
// ========================================== // Section header
void addPlat(float x1, float y1, float x2, float y2, PlatformType type = PT_NORMAL, float sx = 0, float sy = 0, float ex = 0, float ey = 0, float spd = 0, int maxT = 0) { // Helper to insert platforms
    Platform p; // Instantiate struct
    p.x1 = x1; p.y1 = y1; p.x2 = x2; p.y2 = y2; // Apply bounds
    p.type = type; // Apply mechanic type
    p.startX = sx; p.startY = sy; p.endX = ex; p.endY = ey; // Apply anchors
    p.speed = spd; // Apply speed variable
    p.isActive = true; // Set active by default
    p.timer = 0; // Reset logic timer
    p.maxTimer = maxT; // Set max limit
    p.moveDir = 1; // Start moving forward
    platforms.push_back(p); // Commit to vector
} // Ends function

void addPortal(int x, int y, int w, int h, int color, int target) { // Helper to insert portals
    Portal p; // Instantiate struct
    p.x = x; p.y = y; p.w = w; p.h = h; // Apply bounds
    p.color = color; // Apply VFX color
    p.targetIndex = target; // Set chain destination
    portals.push_back(p); // Commit to vector
} // Ends function

void initPlayer(int level) { // Resets player position and variables
    switch(level) { // Check level ID
        case 1: player.x = 50; player.y = 600; break; // Level 1 spawn
        case 2: player.x = 1100; player.y = 550; break; // Level 2 spawn
        case 3: player.x = 50; player.y = 50; break; // Level 3 spawn
        case 4: player.x = 50; player.y = 550; break; // Level 4 spawn
        case 5: player.x = 50; player.y = 150; break; // Level 5 spawn
        case 6: player.x = 50; player.y = 550; break; // Level 6 spawn
        case 7: player.x = 50; player.y = 350; break; // Level 7 spawn
        case 8: player.x = 50; player.y = 650; break; // Level 8 spawn
        case 9: player.x = 50; player.y = 550; break; // Level 9 spawn
        case 10: player.x = 50; player.y = 550; break; // Level 10 spawn
        default: player.x = 50; player.y = 550; break; // Failsafe spawn
    } // Ends switch
    
    player.vx = 0; player.vy = 0; // Reset inertia
    player.w = 40; player.h = 60; // Assign hitbox sizes
    player.onGround = false; // Player spawns airborne safely
    player.frame = 1; // Reset animation frame
    player.action = "IDLE"; // Reset action string
    player.facingLeft = false; // Face right by default
    player.landingTimer = 0; // Clear landing queue
    portalCooldown = 0; // Refresh portal use
} // Ends function

void wipeMemory() { // Hard clears all level vectors to prevent memory leaks over time
    platforms.clear(); // Free geometry memory
    platforms.shrink_to_fit(); // Force memory compaction reallocation
    portals.clear(); // Free portal memory
    portals.shrink_to_fit(); // Force memory compaction reallocation
} // Ends function

void initLevel(int level) { // Generates geometry for active level
    wipeMemory(); // Securely wipe previously used memory after each level
    
    // Boundary walls to prevent screen escape entirely
    addPlat(-50, 0, 0, SH, PT_NORMAL); // Left screen guard
    addPlat(SW, 0, SW+50, SH, PT_NORMAL); // Right screen guard
    addPlat(0, -50, SW, 0, PT_NORMAL); // Top ceiling guard
    
    switch(level) { // Route generation by level
        case 1: // Level 1: Training
            addPlat(0, 700, 250, 800); // Start ground
            addPlat(350, 600, 500, 620); // Step 1
            addPlat(600, 500, 750, 520); // Step 2
            addPlat(850, 400, 1150, 420); // Step 3
            addPlat(50, 250, 250, 270); // Return ledge
            addPlat(400, 180, 600, 200, PT_GOAL); // Goal
            addPlat(0, 780, 1200, 800, PT_SPIKE); // Floor spikes
            addPortal(950, 320, 60, 80, C(50, 150, 255), 1); // Portal IN
            addPortal(100, 170, 60, 80, C(255, 100, 50), 0); // Portal OUT
            break; // Ends case
            
        case 2: // Level 2: The Pit
            addPlat(1000, 650, 1200, 800); // Start ground
            addPlat(750, 550, 850, 570); // Step 1
            addPlat(500, 450, 600, 470); // Step 2
            addPlat(250, 350, 350, 370); // Step 3
            addPlat(50, 250, 150, 270); // Step 4
            addPlat(900, 150, 1100, 170); // Missing return step
            addPlat(450, 150, 650, 170, PT_GOAL); // Goal
            addPlat(700, 250, 800, 270); // Missing mid step
            addPlat(0, 780, 1000, 800, PT_SPIKE); // Floor spikes
            addPortal(70, 170, 60, 80, C(50, 150, 255), 1); // Portal IN
            addPortal(1000, 70, 60, 80, C(255, 100, 50), 0); // Portal OUT
            break; // Ends case
            
        case 3: // Level 3: The Gauntlet
            addPlat(0, 150, 100, 170); // Start ground
            addPlat(0, 250, 100, 800, PT_SPIKE); // Left spike wall
            addPlat(250, 250, 300, 800, PT_SPIKE); // Right spike wall
            addPlat(100, 350, 150, 370, PT_SPIKE); // Hazard
            addPlat(200, 500, 250, 520, PT_SPIKE); // Hazard
            addPlat(100, 650, 150, 670, PT_SPIKE); // Hazard
            addPlat(100, 750, 250, 770); // Safe bottom
            addPlat(900, 150, 1000, 170); // Missing return platform restored
            addPlat(1100, 150, 1200, 170, PT_GOAL); // Goal
            addPlat(0, 780, 1200, 800, PT_SPIKE); // Floor spikes
            addPortal(145, 670, 60, 80, C(50, 150, 255), 1); // Portal IN
            addPortal(920, 70, 60, 80, C(255, 100, 50), 0); // Portal OUT
            break; // Ends case
            
        case 4: // Level 4: The Ultimate Leap (Introduces Moving Platforms)
            addPlat(0, 650, 150, 670); // Start ground
            addPlat(250, 650, 400, 670, PT_MOVING, 250, 650, 250, 200, 3.0f); // Vertical elevator moving from Y=650 to 200
            addPlat(500, 200, 650, 220, PT_MOVING, 500, 200, 900, 200, 4.0f); // Horizontal ferry moving from X=500 to 900
            addPlat(1000, 200, 1150, 220, PT_GOAL); // Goal landing
            addPlat(0, 780, 1200, 800, PT_SPIKE); // Lethal lava floor
            break; // Ends case
            
        case 5: // Level 5: The Snake (Introduces Fake Platforms)
            addPlat(0, 250, 100, 270); // Start ground
            addPlat(200, 350, 300, 370, PT_FAKE, 0, 0, 0, 0, 0, 30); // Crumbles after 0.5 sec
            addPlat(400, 450, 500, 470, PT_FAKE, 0, 0, 0, 0, 0, 30); // Crumbles after 0.5 sec
            addPlat(600, 550, 700, 570, PT_FAKE, 0, 0, 0, 0, 0, 30); // Crumbles after 0.5 sec
            addPlat(800, 650, 900, 670); // Safe checkpoint
            addPlat(1050, 650, 1150, 670, PT_GOAL); // Goal
            addPlat(0, 780, 1200, 800, PT_SPIKE); // Spikes
            break; // Ends case
            
        case 6: // Level 6: The Long Jump (Introduces Wind Zones)
            addPlat(0, 650, 150, 670); // Start ground
            addPlat(150, 400, 450, 670, PT_WIND, 0, 0, 0, 0, 8.0f); // Fast wind pushing right
            addPlat(450, 650, 550, 670); // Safe island
            addPlat(550, 400, 900, 670, PT_WIND, 0, 0, 0, 0, 8.0f); // Fast wind pushing right
            addPlat(900, 650, 1050, 670, PT_GOAL); // Goal
            addPlat(0, 780, 1200, 800, PT_SPIKE); // Spikes
            break; // Ends case
            
        case 7: // Level 7: The Cage (Introduces Timed Spikes)
            addPlat(0, 450, 150, 470); // Start ground
            addPlat(150, 450, 1050, 470); // Continuous floor
            addPlat(300, 400, 400, 450, PT_TIMED_SPIKE, 0, 0, 0, 0, 0, 60); // Spike block 1
            addPlat(550, 400, 650, 450, PT_TIMED_SPIKE, 0, 0, 0, 0, 0, 60); // Spike block 2
            addPlat(800, 400, 900, 450, PT_TIMED_SPIKE, 0, 0, 0, 0, 0, 60); // Spike block 3
            addPlat(0, 250, 1150, 270); // Low ceiling (Cage) so you MUST time it
            addPlat(1050, 450, 1150, 470, PT_GOAL); // Goal
            addPlat(0, 780, 1200, 800, PT_SPIKE); // Spikes
            break; // Ends case
            
        case 8: // Level 8: The Tower (Wall Jumps)
            addPlat(0, 750, 150, 770); // Start ground
            addPlat(300, 630, 450, 650, PT_NORMAL); // Solid layer 1
            addPlat(300, 510, 450, 530, PT_NORMAL); // Solid layer 2
            addPlat(300, 390, 450, 410, PT_NORMAL); // Solid layer 3
            addPlat(300, 270, 450, 290, PT_NORMAL); // Solid layer 4
            addPlat(600, 270, 750, 290, PT_GOAL); // Goal ledge
            addPlat(0, 780, 1200, 800, PT_SPIKE); // Spikes
            break; // Ends case
            
        case 9: // Level 9: Precision (Portal Chaining System)
            addPlat(0, 650, 150, 670); // Start ground
            addPortal(100, 570, 60, 80, C(50, 150, 255), 1); // P0 targets P1
            addPlat(400, 150, 550, 170); // Floating high
            addPortal(450, 50, 60, 80, C(255, 100, 50), 2); // P1 targets P2
            addPlat(400, 650, 550, 670); // Floating low
            addPortal(450, 550, 60, 80, C(50, 255, 150), 3); // P2 targets P3
            addPlat(800, 350, 950, 370); // Pre-goal
            addPortal(850, 250, 60, 80, C(200, 50, 255), 4); // P3 targets P4
            addPlat(1050, 650, 1200, 670, PT_GOAL); // Goal ground
            addPortal(1100, 550, 60, 80, C(255, 255, 255), 0); // P4 connects back, forming a chain
            addPlat(0, 780, 1200, 800, PT_SPIKE); // Spikes
            break; // Ends case
            
        case 10: // Level 10: The Final Exam (Combined Mechanics)
            addPlat(0, 650, 100, 670); // Start ground
            addPlat(150, 650, 250, 670, PT_FAKE, 0, 0, 0, 0, 0, 20); // Fast falling
            addPlat(300, 550, 400, 570, PT_MOVING, 300, 550, 300, 200, 4.0f); // Fast elevator (Lowered to ensure jump is possible)
            addPlat(450, 200, 550, 220, PT_ONEWAY); // Catching net
            addPlat(600, 200, 800, 400, PT_WIND, 0, 0, 0, 0, 8.0f); // Push zone
            addPlat(850, 400, 950, 420); // SOLID Landing pad for the portal
            addPlat(850, 360, 900, 400, PT_TIMED_SPIKE, 0, 0, 0, 0, 0, 40); // Fast trap ON the landing pad
            addPortal(900, 300, 60, 80, C(50, 150, 255), 1); // Final P0
            addPlat(1050, 150, 1150, 170, PT_GOAL); // Goal
            addPortal(1080, 50, 60, 80, C(255, 100, 50), 0); // Final P1
            addPlat(0, 780, 1200, 800, PT_SPIKE); // Spikes
            break; // Ends case
    } // Ends switch
} // Ends function

// ========================================== // Section header
// INPUT & LOGIC SYSTEMS                      // Section header
// ========================================== // Section header
void handleInput() { // Consumes user keypresses
    float targetVx = 0.0f; // Stores desired velocity to allow smoothing later
    if (GetAsyncKeyState('A') & 0x8000) { // If A is pressed
        targetVx = -MOVE_SPEED; // Move left
        player.facingLeft = true; // Flag for sprite mirror engine
    } // Ends if
    if (GetAsyncKeyState('D') & 0x8000) { // If D is pressed
        targetVx = MOVE_SPEED; // Move right
        player.facingLeft = false; // Flag for normal sprite engine
    } // Ends if
    player.vx = targetVx; // Overwrite current velocity instantly
    
    bool spaceHeld = GetAsyncKeyState(VK_SPACE) & 0x8000; // Check jump input
    if (spaceHeld && !wasSpaceHeld && player.onGround) { // If jumping off solid ground
        player.vy = JUMP_FORCE; // Apply explosive upward force
        player.onGround = false; // Remove grounded tag
        player.action = "JUMP"; // Force action string
        player.frame = 1; // Animation Rule 1: Immediately show jump start frame
    } // Ends if
    wasSpaceHeld = spaceHeld; // Cache spacebar for buffer logic
} // Ends function

void handleAnimation() { // Determines visual representation of physics state
    if (player.onGround) { // Walking or idle state
        if (player.landingTimer > 0) { // Rule 5: Show landing frame briefly
            player.action = "JUMP"; // Abuse jump string for impact frame
            player.frame = 3; // Force frame 3
            player.landingTimer--; // Bleed timer
        } else if (player.vx != 0) { // If walking
            player.action = player.facingLeft ? "LEFT" : "RIGHT"; // Use explicit directional string
            if (fc % 5 == 0) { // Tick every 5 frames
                player.frame++; // Next frame
                if (player.frame > 3) player.frame = 1; // Cycle loop
            } // Ends if
        } else { // Idle standing
            player.action = "IDLE"; // Set string
            if (fc % 5 == 0) { // Keep ticking so it feels responsive when resuming walk
                player.frame++; // Next frame
                if (player.frame > 3) player.frame = 1; // Cycle loop
            } // Ends if
        } // Ends if
    } else { // Airborne state
        player.action = "JUMP"; // Set string
        if (player.vy == JUMP_FORCE) { // Rule 1 verification: Just jumped
            player.frame = 1; // Ensure start frame
        } else if (player.vy < 0) { // Rule 3: Rising through apex
            player.frame = 2; // Mid jump stretch
        } else { // Rule 4: Falling to ground
            player.frame = 3; // Fall impact brace
        } // Ends if
    } // Ends if
} // Ends function

void updatePlatforms() { // Simulates complex math for geometry
    for (size_t i = 0; i < platforms.size(); i++) { // Loop geometries
        if (platforms[i].type == PT_MOVING) { // Linear interpolator check
            float dx = platforms[i].endX - platforms[i].startX; // Range X
            float dy = platforms[i].endY - platforms[i].startY; // Range Y
            float dist = sqrt(dx*dx + dy*dy); // Hypotenuse length
            if (dist == 0) continue; // Safe div0 check
            
            platforms[i].x1 += (dx / dist) * platforms[i].speed * platforms[i].moveDir; // Shift rect X1
            platforms[i].x2 += (dx / dist) * platforms[i].speed * platforms[i].moveDir; // Shift rect X2
            platforms[i].y1 += (dy / dist) * platforms[i].speed * platforms[i].moveDir; // Shift rect Y1
            platforms[i].y2 += (dy / dist) * platforms[i].speed * platforms[i].moveDir; // Shift rect Y2
            
            float curX = platforms[i].x1 - platforms[i].startX; // Current offset X
            float curY = platforms[i].y1 - platforms[i].startY; // Current offset Y
            float curDist = sqrt(curX*curX + curY*curY); // Distance from start
            float endDist = sqrt(pow(platforms[i].x1 - platforms[i].endX, 2) + pow(platforms[i].y1 - platforms[i].endY, 2)); // Distance from end
            
            if (platforms[i].moveDir == 1 && curDist >= dist) { // Reached end
                platforms[i].moveDir = -1; // Reflect velocity
                float w = platforms[i].x2 - platforms[i].x1; float h = platforms[i].y2 - platforms[i].y1; // Cache size
                platforms[i].x1 = platforms[i].endX; platforms[i].y1 = platforms[i].endY; // Hard lock
                platforms[i].x2 = platforms[i].x1 + w; platforms[i].y2 = platforms[i].y1 + h; // Preserve size
            } else if (platforms[i].moveDir == -1 && endDist >= dist) { // Returned to start
                platforms[i].moveDir = 1; // Reflect velocity
                float w = platforms[i].x2 - platforms[i].x1; float h = platforms[i].y2 - platforms[i].y1; // Cache size
                platforms[i].x1 = platforms[i].startX; platforms[i].y1 = platforms[i].startY; // Hard lock
                platforms[i].x2 = platforms[i].x1 + w; platforms[i].y2 = platforms[i].y1 + h; // Preserve size
            } // Ends if
        } else if (platforms[i].type == PT_FAKE) { // Breakable logic
            if (checkCollision(player.x, player.y + 1, player.w, player.h, platforms[i])) { // Stand check
                platforms[i].timer++; // Burn fuse
                if (platforms[i].timer > platforms[i].maxTimer) { // Fuse blown
                    platforms[i].isActive = false; // Disable collision
                } // Ends if
            } else { // Not standing
                if (!platforms[i].isActive) { // Broken state
                    platforms[i].timer--; // Recharge
                    if (platforms[i].timer <= 0) platforms[i].isActive = true; // Reform
                } else { // Intact state
                    platforms[i].timer = 0; // Cool off fuse
                } // Ends if
            } // Ends if
        } else if (platforms[i].type == PT_TIMED_SPIKE) { // Oscillator logic
            platforms[i].timer++; // Tick clock
            if (platforms[i].timer > platforms[i].maxTimer) { // Tick limit
                platforms[i].isActive = !platforms[i].isActive; // Flip bool
                platforms[i].timer = 0; // Rewind clock
            } // Ends if
        } // Ends if
    } // Ends for
} // Ends function

void updatePhysics() { // Continuous sweeping collision engine
    player.vy += GRAVITY; // Integrate acceleration into velocity
    
    // Wind force integration before position update
    for (size_t i = 0; i < platforms.size(); i++) { // Loop all areas
        if (platforms[i].type == PT_WIND && checkCollision(player.x, player.y, player.w, player.h, platforms[i])) { // Overlap check
            player.vx += platforms[i].speed; // Push player
        } // Ends if
    } // Ends for
    
    player.x += player.vx; // Shift spatial X position
    
    // Clamp to prevent screen overflow natively
    if (player.x < 0) { player.x = 0; player.vx = 0; } // Block left edge
    if (player.x + player.w > SW) { player.x = SW - player.w; player.vx = 0; } // Block right edge
    
    // Discrete X-Axis correction
    for (size_t i = 0; i < platforms.size(); i++) { // Check solid intersections
        if (!platforms[i].isActive) continue; // Ignore phantoms
        if (platforms[i].type == PT_ONEWAY || platforms[i].type == PT_WIND || platforms[i].type == PT_GOAL) continue; // Ignore ghosts
        
        if (checkCollision(player.x, player.y, player.w, player.h, platforms[i])) { // Box overlap
            if (platforms[i].type == PT_SPIKE || platforms[i].type == PT_TIMED_SPIKE) { // Check lethal
                initPlayer(currentLevel); return; // Kill and halt frame
            } // Ends if
            if (player.vx > 0) player.x = platforms[i].x1 - player.w; // Expel leftwards
            else if (player.vx < 0) player.x = platforms[i].x2; // Expel rightwards
            player.vx = 0; // Neutralize inertia
        } // Ends if
    } // Ends for
    
    float oldY = player.y; // Cache pre-movement height
    player.y += player.vy; // Shift spatial Y position
    bool wasOnGround = player.onGround; // Cache previous grounding
    player.onGround = false; // Revoke grounding
    Platform* standPlat = nullptr; // Pointer tracking for elevators
    
    // Discrete Y-Axis correction
    for (size_t i = 0; i < platforms.size(); i++) { // Check solid intersections
        if (!platforms[i].isActive || platforms[i].type == PT_WIND) continue; // Ignore ghosts
        
        if (checkCollision(player.x, player.y, player.w, player.h, platforms[i])) { // Box overlap
            if (platforms[i].type == PT_GOAL) { // Level win trigger
                gState = ST_WIN; return; // Transition FSM
            } // Ends if
            if (platforms[i].type == PT_SPIKE || platforms[i].type == PT_TIMED_SPIKE) { // Check lethal
                initPlayer(currentLevel); return; // Kill and halt frame
            } // Ends if
            
            if (platforms[i].type == PT_ONEWAY) { // Specialty pass-through math
                if (player.vy > 0 && oldY + player.h <= platforms[i].y1 + 10) { // Ensure fell from above lip
                    player.y = platforms[i].y1 - player.h; // Rest on lip
                    player.vy = 0; player.onGround = true; // Nullify fall
                    standPlat = &platforms[i]; // Bind elevator
                } // Ends if
            } else { // Standard rigid block
                if (player.vy > 0) { // Downward impact
                    player.y = platforms[i].y1 - player.h; // Rest on floor
                    player.vy = 0; player.onGround = true; // Nullify fall
                    standPlat = &platforms[i]; // Bind elevator
                } else if (player.vy < 0) { // Upward impact
                    player.y = platforms[i].y2; // Bonk ceiling
                    player.vy = 0; // Nullify rise
                } // Ends if
            } // Ends if
        } // Ends if
    } // Ends for
    
    // Relative movement transfer (Riding elevators)
    if (standPlat && standPlat->type == PT_MOVING) { // If standing on mover
        float dx = standPlat->endX - standPlat->startX; // Width
        float dy = standPlat->endY - standPlat->startY; // Height
        float dist = sqrt(dx*dx + dy*dy); // Scale
        if (dist > 0) { // Sanity check
            player.x += dx * standPlat->speed * standPlat->moveDir / dist; // Push player X
            player.y += dy * standPlat->speed * standPlat->moveDir / dist; // Push player Y
        } // Ends if
    } // Ends if
    
    // State event triggers
    if (player.onGround && !wasOnGround) { // Frame-perfect landing detection
        player.landingTimer = 5; // Trigger animation squish
    } // Ends if
    if (player.y > SH + 100) { // Out of bounds death plane
        initPlayer(currentLevel); return; // Kill and halt
    } // Ends if
    
    // Portal topological engine
    if (portalCooldown > 0) portalCooldown--; // Process cooldown tick
    else { // Portals armed
        for (size_t i = 0; i < portals.size(); i++) { // Sweep all portals
            if (checkCollision(player.x, player.y, player.w, player.h, portals[i])) { // Check entry
                int dest = portals[i].targetIndex; // Read map index
                if (dest >= 0 && dest < portals.size()) { // Validate array index
                    player.x = portals[dest].x + (portals[dest].w / 2) - (player.w / 2); // Map X mathematically
                    player.y = portals[dest].y + portals[dest].h - player.h; // Map Y mathematically
                    portalCooldown = PORTAL_COOLDOWN_MAX; // Lock topology
                    break; // Execute jump instantly
                } // Ends if
            } // Ends if
        } // Ends for
    } // Ends if
} // Ends function

// ========================================== // Section header
// RENDER PIPELINE                            // Section header
// ========================================== // Section header
void drawBG() { // Procedural starfield
    srand(12345); // Deterministic seed for static layout
    int twinkle = (sin(fc * 0.05f) + 1.0f) * 127; // Global luminance wave
    for(int i=0; i<150; i++) { // Cast 150 points
        int sx = rand() % SW; // Fixed X
        int sy = rand() % SH; // Fixed Y
        int brightness = 150 + (rand() % 105) + (twinkle / 4); // Combine static and wave
        if (brightness > 255) brightness = 255; // Clamp white
        putpixel(sx, sy, C(brightness, brightness, brightness)); // Print to buffer
    } // Ends for
    srand(fc); // Restore true randomness
} // Ends function

void drawPlatforms() { // Paints active level geometry
    for (size_t i = 0; i < platforms.size(); i++) { // Loop primitives
        if (!platforms[i].isActive) continue; // Do not draw broken fakes
        
        Platform p = platforms[i]; // Fetch copy
        int px1 = (int)p.x1, py1 = (int)p.y1, px2 = (int)p.x2, py2 = (int)p.y2; // Cast floats safely
        
        switch (p.type) { // Multi-branch render style
            case PT_SPIKE: // Lava / Hazard
                setfillstyle(SOLID_FILL, C(150, 0, 0)); // Dark red
                bar(px1, py1, px2, py2); // Core
                setcolor(C(255, 50, 50)); line(px1, py1, px2, py1); // Edge glow
                setcolor(C(255,100,100)); // Needles
                for(int x=px1; x<px2-10; x+=15) { // Triangle loop
                    line(x, py1, x+7, py1-15); // Up slope
                    line(x+7, py1-15, x+15, py1); // Down slope
                } // Ends for
                break; // Exit case
            case PT_TIMED_SPIKE: // Toggle Hazard
                setfillstyle(HATCH_FILL, C(200, 100, 0)); // Orange industrial
                bar(px1, py1, px2, py2); // Core
                setcolor(C(255, 100, 0)); // Spike tips
                for(int x=px1; x<px2-10; x+=15) { // Triangle loop
                    line(x, py1, x+7, py1-15); // Up slope
                    line(x+7, py1-15, x+15, py1); // Down slope
                } // Ends for
                break; // Exit case
            case PT_GOAL: // Win trigger
                setfillstyle(SOLID_FILL, C(0, 200, 0)); // Green
                bar(px1, py1, px2, py2); // Core
                break; // Exit case
            case PT_WIND: // Push zone
                setcolor(C(100, 200, 255)); // Cyan
                for(int y=py1+10; y<py2; y+=20) { // Striping
                    int wx = px1 + ((fc * (int)p.speed * 2) % (px2 - px1)); // Scroll math
                    line(wx, y, wx + 20, y); // Dash
                } // Ends for
                break; // Exit case
            case PT_FAKE: // Destructible
                setfillstyle(XHATCH_FILL, C(80, 80, 90)); // Crumbly grid
                bar(px1, py1, px2, py2); // Core
                break; // Exit case
            case PT_ONEWAY: // Platforming
                setfillstyle(SOLID_FILL, C(60, 60, 70)); // Dimmed
                bar(px1, py1, px2, py1 + 10); // Thin top ledge only
                break; // Exit case
            case PT_MOVING: // Elevator
                setfillstyle(SOLID_FILL, C(100, 100, 150)); // Bright blue-gray
                bar(px1, py1, px2, py2); // Core
                break; // Exit case
            default: // PT_NORMAL Static
                setfillstyle(SOLID_FILL, C(40, 40, 45)); // Gray
                bar(px1, py1, px2, py2); // Core
                setcolor(C(100, 100, 110)); line(px1, py1, px2, py1); // Top light
                setcolor(C(20, 20, 25)); line(px1, py2, px2, py2); // Bottom shadow
                break; // Exit case
        } // Ends switch
    } // Ends for
} // Ends function

void drawPortals() { // Trignometric visualizer for topological bounds
    for (size_t pIdx = 0; pIdx < portals.size(); pIdx++) { // Iterate nodes
        Portal p = portals[pIdx]; // Fetch struct
        int rx = p.w/2; // Center offset X
        int ry = p.h/2; // Center offset Y
        int cx = p.x + rx; // Pivot X
        int cy = p.y + ry; // Pivot Y
        
        for(int i=0; i<5; i++) { // Onion skin ring generator
            int r = (p.color & 0xFF) / (i+1); // Bitshift Red falloff
            int g = ((p.color >> 8) & 0xFF) / (i+1); // Bitshift Green falloff
            int b = ((p.color >> 16) & 0xFF) / (i+1); // Bitshift Blue falloff
            setcolor(C(r, g, b)); // Set ring hex
            ellipse(cx, cy, 0, 360, rx + i*2, ry + i*2); // Print ring
        } // Ends for
        
        myFilledEllipse(cx, cy, rx, ry, C(10, 10, 10)); // Void core
        
        for(int i=0; i<3; i++) { // Orbital moonlet generator
            float angle = fc * 0.1f + i * 2.09f; // Euler angular rotation
            int px = cx + cos(angle) * (rx+5); // Cosine projection X
            int py = cy + sin(angle) * (ry+5); // Sine projection Y
            myFilledEllipse(px, py, 3, 3, p.color); // Print moonlet
        } // Ends for
        
        setcolor(C(255,255,255)); // White ink
        settextstyle(SANS_SERIF_FONT, HORIZ_DIR, 1); // Small UI font
        string label = "P" + to_string(pIdx); // Build node ID string
        outtextxy(cx - textwidth((char*)label.c_str())/2, p.y - 20, (char*)label.c_str()); // Draw centered label
    } // Ends for
} // Ends function

void drawPlayer() { // Rasterizer wrapper with mirroring feature
    string path = getCheckedSpritePath(player.charType, player.action, player.frame); // Resolve safe hard drive path
    
    int drawW = player.w; // Box W
    int drawH = player.h; // Box H
    int drawX = (int)player.x; // Float to Int Cast
    int drawY = (int)player.y; // Float to Int Cast
    
    if (player.action == "IDLE") { // Expand box for idle padded sprites
        drawW = player.w + 16; // Add padding
        drawH = player.h + 24; // Add padding
        drawX = (int)player.x - 8; // Adjust origin
        drawY = (int)player.y - 24; // Adjust origin
    } // Ends if
    
    // Draw the image normally first
    readimagefile((char*)path.c_str(), drawX, drawY, drawX + drawW, drawY + drawH); // Standard raster write
    
    // Manual pixel flipping for programmatic mirroring (resolves WinBGIm negative width bugs)
    if (player.facingLeft && player.action != "LEFT") { // If facing left and no native mirrored sprite exists
        for (int y = 0; y < drawH; y++) { // Loop vertically
            for (int x = 0; x < drawW / 2; x++) { // Loop horizontally to midpoint
                int px1 = drawX + x; // Left side pixel X
                int px2 = drawX + drawW - 1 - x; // Right side pixel X
                int py = drawY + y; // Current Y
                
                int color1 = getpixel(px1, py); // Read left pixel
                int color2 = getpixel(px2, py); // Read right pixel
                
                putpixel(px1, py, color2); // Swap left
                putpixel(px2, py, color1); // Swap right
            } // Ends inner for
        } // Ends outer for
    } // Ends if
} // Ends function

// ========================================== // Section header
// USER INTERFACE OVERLAYS                    // Section header
// ========================================== // Section header
void drawHUD() { // Static upper bar
    setfillstyle(SOLID_FILL, C(20, 20, 20)); // Charcoal
    bar(0, 0, SW, 34); // Bar background
    
    setcolor(C(255,255,255)); // White
    settextstyle(SANS_SERIF_FONT, HORIZ_DIR, 1); // UI Font
    
    string name = (player.charType == 0) ? "KIM DOKJA" : "HAN SOYOUNG"; // Resolve display name
    string levelStr = "Level " + to_string(currentLevel) + " - " + name; // Concat metadata
    outtextxy(10, 10, (char*)levelStr.c_str()); // Print top left
    
    string controls = "[A/D] Move  [SPACE] Jump"; // Input text
    outtextxy(SW/2 - textwidth((char*)controls.c_str())/2, 10, (char*)controls.c_str()); // Print centered perfectly
    
    string escTxt = "[ESC] Menu"; // Esc text
    outtextxy(SW - textwidth((char*)escTxt.c_str()) - 20, 10, (char*)escTxt.c_str()); // Print top right aligned
} // Ends function

void drawMenu() { // Title screen loop state
    drawBG(); // Draw stars
    
    setcolor(C(255, 255, 255)); // White
    settextstyle(SANS_SERIF_FONT, HORIZ_DIR, 8); // Huge font
    string title = "PORTAL MINI"; // Title str
    outtextxy(SW/2 - textwidth((char*)title.c_str())/2, 200, (char*)title.c_str()); // Center absolute mathematically
    
    settextstyle(SANS_SERIF_FONT, HORIZ_DIR, 4); // Medium font
    string playStr = "PLAY GAME"; // Option str
    if (menuSel == 0) setcolor(C(255, 255, 0)); else setcolor(C(150, 150, 150)); // Selection tint
    outtextxy(SW/2 - textwidth((char*)playStr.c_str())/2, 400, (char*)playStr.c_str()); // Center absolute
    
    string lvlStr = "LEVEL SELECT"; // Option str
    if (menuSel == 1) setcolor(C(255, 255, 0)); else setcolor(C(150, 150, 150)); // Selection tint
    outtextxy(SW/2 - textwidth((char*)lvlStr.c_str())/2, 470, (char*)lvlStr.c_str()); // Center absolute
    
    string exitStr = "EXIT"; // Option str
    if (menuSel == 2) setcolor(C(255, 255, 0)); else setcolor(C(150, 150, 150)); // Selection tint
    outtextxy(SW/2 - textwidth((char*)exitStr.c_str())/2, 540, (char*)exitStr.c_str()); // Center absolute
    
    // Draw modern floating cursor matching selected item
    int curY = menuSel == 0 ? 420 : (menuSel == 1 ? 490 : 560); // Determine Y lock
    int curX = SW/2 - 180; // Determine X lock
    myFilledEllipse(curX + cos(fc * 0.1f) * 10, curY, 10, 10, C(0, 200, 255)); // Waving cursor orb
    myFilledEllipse(SW - curX - cos(fc * 0.1f) * 10, curY, 10, 10, C(0, 200, 255)); // Waving right cursor orb
    
    bool up = GetAsyncKeyState(VK_UP) & 0x8000; // Poll buffer
    bool down = GetAsyncKeyState(VK_DOWN) & 0x8000; // Poll buffer
    bool enter = GetAsyncKeyState(VK_RETURN) & 0x8000; // Poll buffer
    
    if (up && !wasUp) { menuSel--; if (menuSel < 0) menuSel = 2; } // Shift selector
    if (down && !wasDown) { menuSel++; if (menuSel > 2) menuSel = 0; } // Shift selector
    if (enter && !wasEnterMenu) { // Commit action
        if (menuSel == 0) { currentLevel = 1; initLevel(1); initPlayer(1); gState = ST_CHARSEL; } // Advance to char screen
        else if (menuSel == 1) { gState = ST_LEVELSEL; wasEnterLevelSel = true; } // Block input bleed
        else exit(0); // Exit desktop
    } // Ends if
    if (GetAsyncKeyState('1') & 0x8000) { currentLevel = 1; initLevel(1); initPlayer(1); gState = ST_CHARSEL; } // Hotkey advance
    if (GetAsyncKeyState('2') & 0x8000) exit(0); // Hotkey advance
    
    wasUp = up; wasDown = down; wasEnterMenu = enter; // Shift history buffer
} // Ends function

void drawLevelSel() { // Pick level screen loop state
    drawBG(); // Draw stars
    
    setcolor(C(255,255,255)); // White
    settextstyle(SANS_SERIF_FONT, HORIZ_DIR, 5); // Big font
    string title = "SELECT LEVEL"; // Title str
    outtextxy(SW/2 - textwidth((char*)title.c_str())/2, 50, (char*)title.c_str()); // Centered
    
    int startX = 130; // Grid offset X
    int startY = 150; // Grid offset Y
    int gapX = 190; // Spacing X
    int gapY = 220; // Spacing Y
    
    for (int i = 1; i <= 10; i++) { // Render 10 grids
        int col = (i - 1) % 5; // Column 0-4
        int row = (i - 1) / 5; // Row 0-1
        int bx = startX + col * gapX; // Box X
        int by = startY + row * gapY; // Box Y
        int bw = 160; // Box W
        int bh = 110; // Box H
        
        if (levelSel == i) setcolor(C(255, 255, 0)); else setcolor(C(100, 100, 100)); // Border color
        for(int j=0; j<3; j++) rectangle(bx-j, by-j, bx+bw+j, by+bh+j); // Thick border
        
        string thumbPath = "d:/gmes/all_sprites/level" + to_string(i) + ".jpg"; // Generate path
        if (fileExists(thumbPath)) { // Check hard drive
            readimagefile((char*)thumbPath.c_str(), bx, by, bx+bw, by+bh); // Render level photo on thumbnail
        } else { // Fallback
            setfillstyle(SOLID_FILL, C(40, 40, 40)); // Dark grey
            bar(bx, by, bx+bw, by+bh); // Draw placeholder
        } // Ends if
        
        setcolor(levelSel == i ? C(255,255,0) : C(150,150,150)); // Text tint
        settextstyle(SANS_SERIF_FONT, HORIZ_DIR, 2); // Med font
        string lName = "LEVEL " + to_string(i); // Label
        outtextxy(bx + bw/2 - textwidth((char*)lName.c_str())/2, by + bh + 15, (char*)lName.c_str()); // Centered under box
    } // Ends for
    
    setcolor(C(200,200,200)); // Light grey
    settextstyle(SANS_SERIF_FONT, HORIZ_DIR, 2); // Small font
    string esc = "ESC to Main Menu"; // Str
    outtextxy(SW/2 - textwidth((char*)esc.c_str())/2, 700, (char*)esc.c_str()); // Center absolute
    
    bool left = GetAsyncKeyState(VK_LEFT) & 0x8000; // Poll
    bool right = GetAsyncKeyState(VK_RIGHT) & 0x8000; // Poll
    bool up = GetAsyncKeyState(VK_UP) & 0x8000; // Poll
    bool down = GetAsyncKeyState(VK_DOWN) & 0x8000; // Poll
    bool enter = GetAsyncKeyState(VK_RETURN) & 0x8000; // Poll
    
    if (left && !wasLeft) { levelSel--; if (levelSel < 1) levelSel = 10; } // Shift
    if (right && !wasRight) { levelSel++; if (levelSel > 10) levelSel = 1; } // Shift
    if (up && !wasUp) { levelSel -= 5; if (levelSel < 1) levelSel += 10; } // Shift
    if (down && !wasDown) { levelSel += 5; if (levelSel > 10) levelSel -= 10; } // Shift
    
    if (enter && !wasEnterLevelSel) { // Commit
        currentLevel = levelSel; // Set to selected
        initLevel(currentLevel); // Load stage
        initPlayer(currentLevel); // Reset bounds
        gState = ST_CHARSEL; // Route to char selection next!
    } // Ends if
    
    wasLeft = left; wasRight = right; wasUp = up; wasDown = down; wasEnterLevelSel = enter; // Shift history
} // Ends function

void drawCharSel() { // Pick screen loop state
    drawBG(); // Draw stars
    
    setcolor(C(255,255,255)); // White
    settextstyle(SANS_SERIF_FONT, HORIZ_DIR, 5); // Big font
    string title = "SELECT CHARACTER"; // Title str
    outtextxy(SW/2 - textwidth((char*)title.c_str())/2, 50, (char*)title.c_str()); // Centered
    
    // Left Box
    if (charSel == 0) setcolor(C(255,255,0)); else setcolor(C(100,100,100)); // Border color
    for(int i=0; i<3; i++) rectangle(190-i, 140-i, 510+i, 560+i); // Thick border
    readimagefile((char*)"d:/gmes/all_sprites/kim dokja idol.jpg", 200, 150, 500, 550); // Raw image
    setcolor(charSel == 0 ? C(255,255,0) : C(150,150,150)); // Text tint
    settextstyle(SANS_SERIF_FONT, HORIZ_DIR, 3); // Med font
    string c1 = "KIM DOKJA [1]"; // Char str
    outtextxy(350 - textwidth((char*)c1.c_str())/2, 580, (char*)c1.c_str()); // Centered under box
    
    // Right Box
    if (charSel == 1) setcolor(C(255,255,0)); else setcolor(C(100,100,100)); // Border color
    for(int i=0; i<3; i++) rectangle(690-i, 140-i, 1010+i, 560+i); // Thick border
    readimagefile((char*)"d:/gmes/all_sprites/han soyoung IDOL.jpg", 700, 150, 1000, 550); // Raw image
    setcolor(charSel == 1 ? C(255,255,0) : C(150,150,150)); // Text tint
    string c2 = "HAN SOYOUNG [2]"; // Char str
    outtextxy(850 - textwidth((char*)c2.c_str())/2, 580, (char*)c2.c_str()); // Centered under box
    
    setcolor(C(200,200,200)); // Light grey
    settextstyle(SANS_SERIF_FONT, HORIZ_DIR, 2); // Small font
    string esc = "ESC to Main Menu"; // Str
    outtextxy(SW/2 - textwidth((char*)esc.c_str())/2, 700, (char*)esc.c_str()); // Center absolute
    
    bool left = GetAsyncKeyState(VK_LEFT) & 0x8000; // Poll
    bool right = GetAsyncKeyState(VK_RIGHT) & 0x8000; // Poll
    bool enter = GetAsyncKeyState(VK_RETURN) & 0x8000; // Poll
    
    if (left && !wasLeft) charSel = 0; // Shift left
    if (right && !wasRight) charSel = 1; // Shift right
    if (enter && !wasEnterChar) { // Commit
        player.charType = charSel; // Assign ID
        currentLevel = 1; // Hardcode starting block
        initLevel(currentLevel); // Load stage
        initPlayer(currentLevel); // Reset player bounds
        gState = ST_PLAY; // Change state to play mode
    } // Ends if
    if (GetAsyncKeyState('1') & 0x8000) { player.charType = 0; currentLevel = 1; initLevel(1); initPlayer(1); gState = ST_PLAY; } // Quickbind
    if (GetAsyncKeyState('2') & 0x8000) { player.charType = 1; currentLevel = 1; initLevel(1); initPlayer(1); gState = ST_PLAY; } // Quickbind
    
    wasLeft = left; wasRight = right; wasEnterChar = enter; // Shift history
} // Ends function

void drawWin() { // Completion screen
    drawBG(); // Stars
    setcolor(C(255, 255, 0)); // Yellow text
    settextstyle(SANS_SERIF_FONT, HORIZ_DIR, 6); // Huge font
    
    string msg = (currentLevel < 10) ? "LEVEL " + to_string(currentLevel) + " CLEAR!" : "GAME BEATEN! YOU WIN!"; // Dynamic msg
    outtextxy(SW/2 - textwidth((char*)msg.c_str())/2, 280, (char*)msg.c_str()); // Absolute center text
    
    settextstyle(SANS_SERIF_FONT, HORIZ_DIR, 3); // Med font
    setcolor(C(200, 200, 200)); // Grey
    string opts = (currentLevel < 10) ? "Press N for Next Level, M for Menu" : "Press R to Replay, M for Menu"; // Dynamic instructions
    outtextxy(SW/2 - textwidth((char*)opts.c_str())/2, 450, (char*)opts.c_str()); // Absolute center text
    
    if (currentLevel < 10 && (GetAsyncKeyState('N') & 0x8000)) { // Valid advance
        currentLevel++; // Inc index
        initLevel(currentLevel); // Load stage
        initPlayer(currentLevel); // Load player
        gState = ST_PLAY; // Shift state
        Sleep(200); // Debounce
    } // Ends if
    if (currentLevel == 10 && (GetAsyncKeyState('R') & 0x8000)) { // Valid reset
        currentLevel = 1; // Restart index
        initLevel(currentLevel); // Load stage
        initPlayer(currentLevel); // Load player
        gState = ST_PLAY; // Shift state
        Sleep(200); // Debounce
    } // Ends if
    if (GetAsyncKeyState('M') & 0x8000) { // Valid retreat
        currentLevel = 1; // Clean reset
        initLevel(currentLevel); // Purge memory
        initPlayer(currentLevel);  // Purge memory
        gState = ST_MENU; // Menu state
        Sleep(200); // Debounce
    } // Ends if
} // Ends function
