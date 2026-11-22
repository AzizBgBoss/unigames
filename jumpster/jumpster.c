#include "sprites.h"
#include "maps.h"

#define PLAYER_WIDTH 16
#define PLAYER_HEIGHT 16

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

    while (true)
    {
        uint8_t keys = getkey();

        bmp(0, 0, 16, 16, player_bmp + (gettime() / 100 % 4) * 16 * 2);

        refreshscreen();
    }

    return 0;
}