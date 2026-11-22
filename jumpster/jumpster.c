#include "sprites.h"
#include "tiles.h"
#include "maps.h"

#define PLAYER_WIDTH 16
#define PLAYER_HEIGHT 16

#define TILE_AT(x, y) (map[(y) * MAP_WIDTH + (x)])

uint8_t map[MAP_HEIGHT * MAP_WIDTH];

uint8_t player_x = 0;
uint8_t player_y = 0;
bool player_touching_ground = false;
bool player_walking = false;

int8_t player_x_velocity = 0;
int8_t player_y_velocity = 0;

uint16_t offset_x;
uint16_t offset_y;

void bmp(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t *data)
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

uint8_t main()
{

    srand(gettime());
    printf("jumpster\n");

    if (isflagsupported(FLAG_GRAPHICS,
                        GFX_MONO_128X64))
    {
        flag(FLAG_GRAPHICS, GFX_MONO_128X64);
        printf("Starting graphics...\n");
    }
    else
    {
        printf("128x64@1 graphics unsupported, exiting.\n");
        return 1;
    }

    if (isflagsupported(1,
                        1))
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

    while (true)
    {
        uint8_t keys = getkey();

        if (keys & KEY_RIGHT)
            player_x_velocity = 1;
        if (keys & KEY_LEFT)
            player_x_velocity = -1;
        
        if (keys & KEY_A)
            player_y_velocity = -3;

        if (TILE_AT((player_x + PLAYER_HEIGHT / 2) / TILE_SIZE, (player_y + PLAYER_HEIGHT) / TILE_SIZE) != EMPTY && player_y_velocity > 0)
            player_y_velocity = 0;

        player_x += player_x_velocity;
        player_y += player_y_velocity;
        
        if (player_x_velocity > 0)
            player_x_velocity--;
        else if (player_x_velocity < 0)
            player_x_velocity++;
        
        player_y_velocity++;

        offset_x = clamp(player_x + PLAYER_WIDTH / 2 - 128 / 2, 0, MAP_WIDTH * TILE_SIZE - 128 / 2);
        offset_y = clamp(player_y + PLAYER_HEIGHT / 2 - 64 / 2, 0, MAP_HEIGHT * TILE_SIZE - 64 / 2);

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

        bmp(player_x - offset_x, player_y - offset_y, PLAYER_WIDTH, PLAYER_HEIGHT, player_bmp + (gettime() / 100 % 4) * 16 * 2);

        refreshscreen();

        sleep(16);
    }

    return 0;
}