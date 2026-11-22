#include "sprites.h"
#include "tiles.h"
#include "maps.h"

#define TARGET_FPS 60
#define FRAME_TIME (1000 / TARGET_FPS)

uint32_t last_time;
uint32_t frame_time;
float delta_time = 0;

#define PLAYER_WIDTH 16
#define PLAYER_HEIGHT 16

#define TILE_AT(x, y) (map[(y) * MAP_WIDTH + (x)])

uint8_t map[MAP_HEIGHT * MAP_WIDTH];

float player_x = 0;
float player_y = 0;
bool player_touching_ground = false;
bool player_walking = false;

float player_x_velocity = 0;
float player_y_velocity = 0;

uint16_t offset_x;
uint16_t offset_y;

void bmp(int16_t x, int16_t y, uint8_t w, uint8_t h, uint8_t *data)
{
    for (uint8_t i = 0; i < h; i++)
    {
        for (uint8_t j = 0; j < w / 8; j++)
        {
            uint8_t byte = data[i * (w / 8) + j];
            for (uint8_t k = 0; k < 8; k++)
            {
                if (byte & (1 << (7 - k)))
                {
                    setpixel(x + j * 8 + k, y + i, 1);
                }
            }
        }
    }
}

int clamp(int v, int min, int max)
{
    if (v < min)
        return min;
    if (v > max)
        return max;
    return v;
}

float clampf(float v, float min, float max)
{
    if (v < min)
        return min;
    if (v > max)
        return max;
    return v;
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

start:
    memcpy(map, level1, sizeof(map));

    last_time = gettime();
    frame_time = last_time;

    while (true)
    {
        uint32_t now = gettime();
        delta_time = (now - last_time) / (1000.0f / TARGET_FPS);
        last_time = now;

        uint8_t keys = getkey();

        if (keys & KEY_RIGHT)
            player_x_velocity = 1;
        if (keys & KEY_LEFT)
            player_x_velocity = -1;

        if ((keys & KEY_A) && player_touching_ground)
            player_y_velocity = -7;

        if (TILE_AT((int)(player_x + PLAYER_WIDTH / 2) / TILE_SIZE, (int)(player_y + PLAYER_HEIGHT - 1 + player_y_velocity * delta_time) / TILE_SIZE) != EMPTY && player_y_velocity > 0)
        {
            player_y_velocity = 0;
            player_touching_ground = true;
        }
        else
        {
            player_touching_ground = false;
        }

        player_x += player_x_velocity * delta_time;
        player_y += player_y_velocity * delta_time;

        if (player_x_velocity > 0)
            player_x_velocity -= 0.1f * delta_time;
        else if (player_x_velocity < 0)
            player_x_velocity += 0.1f * delta_time;

        if (player_x_velocity > -0.1f && player_x_velocity < 0.1f)
            player_x_velocity = 0;

        player_y_velocity = clampf(player_y_velocity + 1.0f * delta_time, -1000.0f, 7.0f);

        offset_x = clamp((int)(player_x + PLAYER_WIDTH / 2 - 128 / 2), 0, MAP_WIDTH * TILE_SIZE - 128);
        offset_y = clamp((int)(player_y + PLAYER_HEIGHT / 2 - 64 / 2), 0, MAP_HEIGHT * TILE_SIZE - 64);

        for (int x = player_x / TILE_SIZE - 128 / TILE_SIZE; x < (player_x / TILE_SIZE + 128 / TILE_SIZE); x++)
        {
            for (int y = player_y / TILE_SIZE - 64 / TILE_SIZE; y < (player_y / TILE_SIZE + 64 / TILE_SIZE); y++)
            {
                if (x < 0 || x >= MAP_WIDTH || y < 0 || y >= MAP_HEIGHT)
                    continue;

                switch (TILE_AT(x, y))
                {
                case GROUND:
                    bmp(x * TILE_SIZE - offset_x, y * TILE_SIZE - offset_y, TILE_SIZE, TILE_SIZE, ground_bmp);
                    break;
                }
            }
        }

        bmp(
            (int16_t)player_x - offset_x,
            (int16_t)player_y - offset_y,
            PLAYER_WIDTH,
            PLAYER_HEIGHT,
            player_bmp + ((gettime() / 100) % 4) * 16 * 2);

        refreshscreen();

        while (gettime() - frame_time < FRAME_TIME)
            ;

        frame_time += FRAME_TIME;

        if (gettime() - frame_time > FRAME_TIME * 5)
            frame_time = gettime();
    }

    return 0;
}