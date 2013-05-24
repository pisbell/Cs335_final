// Designates teams for ships/weapons, to prevent friendly fire.
#define TEAM_EMPIRE (-1)
#define TEAM_REBELS 1

// Bitmask values for arrow key inputs.
#define INPUT_UP 0x1
#define INPUT_DOWN 0x2
#define INPUT_LEFT 0x4
#define INPUT_RIGHT 0x8

// Indexes/IDs for ship types.
#define SHIP_FIGHTER     0
#define SHIP_BOMBER      1
#define SHIP_INTERCEPTER 2
#define SHIP_OPRESSOR    3
#define SHIP_AWING       4
#define SHIP_XWING       5
#define SHIP_COUNT       6

// Indexes/IDs for difficulty levels.
#define DIFFICULTY_EASY   0
#define DIFFICULTY_MEDIUM 1
#define DIFFICULTY_HARD   2
#define DIFFICULTY_COUNT  3

// Stats for different ships at different difficulties.
int statsHealth[SHIP_COUNT][DIFFICULTY_COUNT] = { 
    { 100, 200, 300},
    {  50,  75, 150},
    { 200, 300, 400},
    { 300, 400, 500},
    {1000, 800, 650},
    { 800, 750, 600}
};
int statsShields[SHIP_COUNT][DIFFICULTY_COUNT] = {
    { 100, 200, 300},
    { 150, 225, 350},
    {  50,  75, 150},
    { 100, 200, 300},
    {1000, 700, 500},
    { 750, 500, 350}
};
int statsDamage[SHIP_COUNT][DIFFICULTY_COUNT] = {
    { 100, 175, 225},
    { 150, 250, 300},
    {   2,   6,   9},
    { 400, 500, 600},
    {  75,  75,  75},
    { 100, 100, 100}
};
int statsShotfreq[SHIP_COUNT][DIFFICULTY_COUNT] = {
    {  40,  25,  40},
    {  25,  15,  25},
    {  40,  70,  95},
    {   5,  10,  15},
    {   2,   2,   2},
    {   5,   5,   5}
};
int statsSpeed[SHIP_COUNT][DIFFICULTY_COUNT] = {
    {  15,  25,  40},
    {   5,  15,  25},
    {  40,  70,  95},
    {   5,  10,  15},
    {   2,   2,   2},
    {   5,   5,   5}
};
int statsSpeed[SHIP_COUNT][DIFFICULTY_COUNT] = {
    {  15,  25,  40},
    {   5,  15,  25},
    {  40,  70,  95},
    {   5,  10,  15},
    {   2,   2,   2},
    {   5,   5,   5}
};

int statsScore[SHIP_COUNT][DIFFICULTY_COUNT] = {
    { 100, 100, 100},
    { 200, 200, 200},
    { 250, 250, 250},
    { 350, 350, 350},
    {-250,-200,-100},
    {-200,-150,-100}
};

float statsEdgeLength[SHIP_COUNT] = {
	75.0,
	75.0,
	75.0,
	75.0,
	75.0,
	75.0
};
float statsHitboxRadius[SHIP_COUNT] = {
	40.0,
	40.0,
	40.0,
	40.0,
	40.0,
	40.0
};

// Screen settings
int xres = 800;
int yres = 600;
int pad = 500; // For empty space on sides of screen
int halfpad = 250;
