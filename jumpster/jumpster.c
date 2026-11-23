#include "sprites.h"
#include "tiles.h"
#include "maps.h"
// #include "song.h"

#define TARGET_FPS 60
#define FRAME_TIME (1000 / TARGET_FPS)

// Fixed timestep: physics always advances in 1/60 s steps, however slow the
// device is. time_acc counts 1/3 ms units, so one step is 50 units (16.67 ms).
#define STEP_UNITS 50
#define MAX_STEPS 4 // on a very slow device the game runs in slow motion instead

uint32_t last_time;
uint32_t frame_time;
uint32_t time_acc = 0;

#define PLAYER_WIDTH 16
#define PLAYER_HEIGHT 16

// Hitbox inside the 16x16 sprite (inclusive pixel offsets)
#define HB_X0 2
#define HB_X1 13
#define HB_Y0 1
#define HB_Y1 15

#define TILE_AT(x, y) (map[(y) * MAP_WIDTH + (x)])

// 16.16 fixed point for positions and velocities, values are per physics step
#define FP_ONE 65536
#define FP_FRICTION 6554 // 0.1
#define JUMP_VELOCITY (5 * FP_ONE)
#define JUMP_CUT (3 * FP_ONE) // rising speed limit once A is released
#define MAX_FALL_SPEED (7 * FP_ONE)
#define COYOTE_STEPS 6      // can still jump this many steps after leaving a ledge
#define JUMP_BUFFER_STEPS 6 // a jump pressed this early before landing still counts

uint8_t map[MAP_HEIGHT * MAP_WIDTH];

int32_t player_x = 0;
int32_t player_y = 0;
bool player_touching_ground = false;
bool player_walking = false;
bool player_facing_left = false;

int32_t player_x_velocity = 0;
int32_t player_y_velocity = 0;

uint8_t coyote = 0;
uint8_t jump_buffer = 0;
uint8_t prev_keys = 0;

uint16_t offset_x;
uint16_t offset_y;

void bmp(int16_t x, int16_t y, uint8_t w, uint8_t h, uint8_t *data, uint8_t flags)
{
    blit(x, y, w, h, data, 0, 0, w, BLIT_1BPP | BLIT_TRANSPARENT | BLIT_RAM | flags, 0, 0);
}

int clamp(int v, int min, int max)
{
    if (v < min)
        return min;
    if (v > max)
        return max;
    return v;
}

// True if the player hitbox at pixel position (px, py) overlaps a solid tile.
// The left and right map edges are solid; above and below the map is open.
bool box_hits(int px, int py)
{
    int x0 = px + HB_X0;
    int x1 = px + HB_X1;
    int y0 = py + HB_Y0;
    int y1 = py + HB_Y1;

    if (x0 < 0 || x1 >= MAP_WIDTH * TILE_SIZE)
        return true;
    if (y1 < 0 || y0 >= MAP_HEIGHT * TILE_SIZE)
        return false;
    if (y0 < 0)
        y0 = 0;
    if (y1 >= MAP_HEIGHT * TILE_SIZE)
        y1 = MAP_HEIGHT * TILE_SIZE - 1;

    for (int ty = y0 / TILE_SIZE; ty <= y1 / TILE_SIZE; ty++)
    {
        for (int tx = x0 / TILE_SIZE; tx <= x1 / TILE_SIZE; tx++)
        {
            if (TILE_AT(tx, ty) != EMPTY)
                return true;
        }
    }
    return false;
}

bool player_has_floor(int px, int py)
{
    int x0 = px + HB_X0;
    int x1 = px + HB_X1;
    int y = py + HB_Y1 + 1;

    int ty = y / TILE_SIZE;

    for (int tx = x0 / TILE_SIZE; tx <= x1 / TILE_SIZE; tx++)
    {
        if (TILE_AT(tx, ty) != EMPTY)
            return true;
    }

    return false;
}

// One fixed 1/60 s physics step
void physics_step(uint8_t keys)
{
    if (keys & KEY_RIGHT)
    {
        player_x_velocity = FP_ONE;
        player_facing_left = false;
    }
    if (keys & KEY_LEFT)
    {
        player_x_velocity = -FP_ONE;
        player_facing_left = true;
    }

    if (jump_buffer > 0)
    {
        if (coyote > 0)
        {
            player_y_velocity = -JUMP_VELOCITY;
            coyote = 0;
            jump_buffer = 0;
        }
        else
            jump_buffer--;
    }

    // releasing A early cuts the jump short
    if (!(keys & KEY_A) && player_y_velocity < -JUMP_CUT)
        player_y_velocity = -JUMP_CUT;

    // horizontal move, blocked by walls
    int32_t old_x = player_x;
    player_x += player_x_velocity;
    if (box_hits(player_x / FP_ONE, player_y / FP_ONE))
    {
        player_x = old_x;
        player_x_velocity = 0;
    }

    if (player_x_velocity > 0)
        player_x_velocity -= FP_FRICTION;
    else if (player_x_velocity < 0)
        player_x_velocity += FP_FRICTION;

    if (player_x_velocity > -FP_FRICTION && player_x_velocity < FP_FRICTION)
        player_x_velocity = 0;

    // vertical move: gravity, then land on floors and bump into ceilings
    player_y_velocity += 0.3f * FP_ONE;
    if (player_y_velocity > MAX_FALL_SPEED)
        player_y_velocity = MAX_FALL_SPEED;

    player_y += player_y_velocity;
    player_touching_ground = false;
    if (box_hits(player_x / FP_ONE, player_y / FP_ONE))
    {
        int py = player_y / FP_ONE;
        if (player_y_velocity > 0)
        {
            // feet end up on top of the tile row they went into
            player_y = ((py + HB_Y1) / TILE_SIZE * TILE_SIZE - 1 - HB_Y1) * FP_ONE;
            player_touching_ground = true;
        }
        else
        {
            // head ends up just below the tile row it hit
            player_y = ((py + HB_Y0) / TILE_SIZE * TILE_SIZE + TILE_SIZE - HB_Y0) * FP_ONE;
        }
        player_y_velocity = 0;
    }

    if (!player_touching_ground && player_y_velocity >= 0)
    {
        int px = player_x / FP_ONE;
        int py = player_y / FP_ONE;

        if (player_has_floor(px, py))
        {
            player_touching_ground = true;
            player_y_velocity = 0;
        }
    }

    if (player_touching_ground)
        coyote = COYOTE_STEPS;
    else if (coyote > 0)
        coyote--;

    player_walking = player_touching_ground && player_x_velocity != 0;

    // fell out of the world: respawn
    if (player_y / FP_ONE > MAP_HEIGHT * TILE_SIZE + 32)
    {
        player_x = 0;
        player_y = 0;
        player_x_velocity = 0;
        player_y_velocity = 0;
        coyote = 0;
        jump_buffer = 0;
    }
}

uint8_t main()
{
    srand(gettime());
    printf("jumpster\n");

    if (isflagsupported(FLAG_GRAPHICS, GFX_MONO_128X64))
    {
        flag(FLAG_GRAPHICS, GFX_MONO_128X64);
        printf("Starting graphics...\n");
    }
    else
    {
        printf("128x64@1 graphics unsupported, exiting.\n");
        return 1;
    }

    if (isflagsupported(1, 1))
    {
        flag(1, 1);
        printf("Starting keyboard...\n");
    }
    else
    {
        printf("NES keyboard unsupported, exiting.\n");
        return 1;
    }
/*
    if (isflagsupported(FLAG_AUDIO, AUDIO_ENGINE))
    {
        flag(FLAG_AUDIO, AUDIO_ENGINE);
        printf("Starting music...\n");
        audioload(0, 0, smb_audio_sfx, sizeof(smb_audio_sfx));
        audioload(1, 0, smb_audio_music, sizeof(smb_audio_music));
        music(0);
    }
    else
    {
        printf("SFX engine unsupported, skipping...\n");
    }
*/
start:
    memcpy(map, level1, sizeof(map));

    last_time = gettime();
    frame_time = last_time;
    time_acc = 0;

    while (true)
    {
        uint32_t now = gettime();
        time_acc += (now - last_time) * 3;
        last_time = now;

        uint8_t keys = getkey();

        if ((keys & KEY_A) && !(prev_keys & KEY_A))
            jump_buffer = JUMP_BUFFER_STEPS;
        prev_keys = keys;

        uint8_t steps = 0;
        while (time_acc >= STEP_UNITS && steps < MAX_STEPS)
        {
            physics_step(keys);
            time_acc -= STEP_UNITS;
            steps++;
        }
        if (time_acc >= STEP_UNITS)
            time_acc = 0; // too far behind, drop the rest instead of catching up

        offset_x = clamp(player_x / FP_ONE + PLAYER_WIDTH / 2 - 128 / 2 + (player_x_velocity * 20) / FP_ONE, // this doesnt look as smooth, gotta treat cam velocity seperately
                         0, MAP_WIDTH * TILE_SIZE - 128);
        offset_y = clamp(player_y / FP_ONE + PLAYER_HEIGHT / 2 - 64 / 2,
                         0, MAP_HEIGHT * TILE_SIZE - 64);

        tilemap(map, MAP_WIDTH, MAP_HEIGHT, tiles, 8, offset_x, offset_y, TILE_SIZE, TILE_SIZE,
                BLIT_1BPP | BLIT_TRANSPARENT | TILEMAP_MAP_RAM, 0, 0);

        uint8_t frame = 0;
        if (player_walking)
            frame = (gettime() / 100) % 4;

        bmp(
            (int16_t)(player_x / FP_ONE) - offset_x,
            (int16_t)(player_y / FP_ONE) - offset_y,
            PLAYER_WIDTH,
            PLAYER_HEIGHT,
            player_bmp + frame * 16 * 2,
            player_facing_left ? BLIT_FLIP_X : 0);

        refreshscreen();

        while (gettime() - frame_time < FRAME_TIME)
            ;

        frame_time += FRAME_TIME;

        if (gettime() - frame_time > FRAME_TIME * 5)
            frame_time = gettime();
    }

    return 0;
}
