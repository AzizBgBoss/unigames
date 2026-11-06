#define GRID_SIZE 6
#define GRID_W (64 / GRID_SIZE)
#define GRID_H (64 / GRID_SIZE)
#define GRID_BOMBS 4

#define BOMB 1

uint8_t grid [GRID_W][GRID_H];

uint8_t selectx = 0;
uint8_t selecty = 0;

uint8_t main() {
    srand(gettime());
    printf("geemu\n");

    if (isflagsupported(FLAG_GRAPHICS, GFX_MONO_64X64)) {
        flag(FLAG_GRAPHICS, GFX_MONO_64X64);
        printf("Starting graphics...\n");
    }
    else
    {
        printf("64x64@1 graphics unsupported, exiting.\n");
        return 1;
    }

    if (isflagsupported(1, 1)) {
        flag(1, 1);
        printf("Starting keyboard...\n");
    }
    else
    {
        printf("NES keyboard unsupported, exiting.\n");
        return 1;
    }

    for (uint8_t i = 0; i < GRID_W; i++) {
        for (uint8_t j = 0; j < GRID_H; j++) {
            grid[i][j] = 0;
        }
    }

    for (uint8_t i = 0; i < GRID_BOMBS; i++) {
        uint8_t x = rand(0, GRID_W);
        uint8_t y = rand(0, GRID_H);

        while (grid[x][y] == BOMB) {
            x = rand(0, GRID_W);
            y = rand(0, GRID_H);
        }

        grid[x][y] = BOMB;
    }

    while (1) {
        
        uint8_t keys = getkey();
        
        if (keys & KEY_UP)
            selecty = (selecty - 1) % GRID_H;
        if (keys & KEY_DOWN)
            selecty = (selecty + 1) % GRID_H;
            
        if (keys & KEY_LEFT)
            selectx = (selectx - 1) % GRID_W;
        if (keys & KEY_RIGHT)
            selectx = (selectx + 1) % GRID_W;
        
        for (uint8_t i = 0; i < GRID_W; i++) {
            for (uint8_t j = 0; j < GRID_H; j++) {
                drawrect(i * GRID_SIZE - 1, j * GRID_SIZE - 1, GRID_SIZE + 1, GRID_SIZE + 1, 1);
                if (i == selectx && j == selecty)
                    fillrect(i * GRID_SIZE - 1, j * GRID_SIZE - 1, GRID_SIZE + 1, GRID_SIZE + 1, 1);
            }
        }
        refreshscreen();
        sleep(16);
    }

    return 0;
}