#define GRID_SIZE 6
#define GRID_W (64 / GRID_SIZE)
#define GRID_H (64 / GRID_SIZE - 2)

#define EASY 10
#define MEDIUM 15
#define HARD 20

#define GRID_BOMBS (GRID_W * GRID_H * EASY / 100)

#define BOMB 9
#define UNEXPLODED 10
#define FLAG 11
#define BFLAG 12

uint8_t grid[GRID_W][GRID_H];

uint8_t selectx = 0;
uint8_t selecty = 0;

bool lost;
bool first;

uint8_t flags = 0;
uint16_t tflags = 0;

uint32_t starttime;

void invertrect(uint8_t x, uint8_t y, uint8_t w, uint8_t h)
{
    for (uint8_t i = 0; i < w; i++)
        for (uint8_t j = 0; j < h; j++)
            setpixel(x + i, y + j, !getpixel(x + i, y + j));
}

uint8_t nearby(uint8_t x, uint8_t y)
{
    uint8_t bombs = 0;
    for (uint8_t i = 0; i < 3; i++)
    {
        for (uint8_t j = 0; j < 3; j++)
        {
            if ((i != 1 || j != 1) && x + i - 1 >= 0 && x + i - 1 < GRID_W && y + j - 1 >= 0 && y + j - 1 < GRID_H)
                if (grid[x + i - 1][y + j - 1] == BOMB || grid[x + i - 1][y + j - 1] == BFLAG)
                    bombs++;
        }
    }
    return bombs;
}

uint8_t stack[GRID_W * GRID_H];

void updategrid(uint8_t x, uint8_t y)
{
    uint8_t sp = 0;
    uint8_t n = nearby(x, y);
    grid[x][y] = n;
    if (n == 0)
    {
        stack[sp] = x * GRID_H + y;
        sp++;
    }
    while (sp > 0)
    {
        sp--;
        uint8_t cx = stack[sp] / GRID_H;
        uint8_t cy = stack[sp] % GRID_H;
        for (uint8_t i = 0; i < 3; i++)
            for (uint8_t j = 0; j < 3; j++)
            {
                if (cx + i >= 1 && cx + i - 1 < GRID_W && cy + j >= 1 && cy + j - 1 < GRID_H)
                {
                    uint8_t nx = cx + i - 1;
                    uint8_t ny = cy + j - 1;
                    if (grid[nx][ny] == UNEXPLODED)
                    {
                        n = nearby(nx, ny);
                        grid[nx][ny] = n;
                        if (n == 0)
                        {
                            stack[sp] = nx * GRID_H + ny;
                            sp++;
                        }
                    }
                }
            }
    }
}

void explode(uint8_t x, uint8_t y)
{
    if (lost || flags == GRID_BOMBS)
        return;
    if (grid[x][y] == BOMB)
    {
        if (first)
        {
            grid[x][y] = UNEXPLODED;
            { // move the bomb to a new location cuh
                uint8_t nx = rand(0, GRID_W);
                uint8_t ny = rand(0, GRID_H);
                while (grid[nx][ny] == BOMB || (nx == x && ny == y))
                {
                    nx = rand(0, GRID_W);
                    ny = rand(0, GRID_H);
                }
                grid[nx][ny] = BOMB;
            }
            grid[x][y] = nearby(x, y);
            updategrid(x, y);
            return;
        }
        lost = true;
        return;
    }
    if (grid[x][y] != UNEXPLODED)
        return;
    first = false;
    grid[x][y] = nearby(x, y);
    updategrid(x, y);
}

uint8_t main()
{
    srand(gettime());
    printf("geemu\n");

    if (isflagsupported(FLAG_GRAPHICS,
                        GFX_MONO_64X64))
    {
        flag(FLAG_GRAPHICS, GFX_MONO_64X64);
        printf("Starting graphics...\n");
    }
    else
    {
        printf("64x64@1 graphics unsupported, exiting.\n");
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

    starttime = gettime();
    flags = 0;
    tflags = 0;
    lost = false;
    first = true;

    for (uint8_t i = 0; i < GRID_W; i++)
    {
        for (uint8_t j = 0; j < GRID_H; j++)
        {
            grid[i][j] = UNEXPLODED;
        }
    }

    for (uint8_t i = 0; i < GRID_BOMBS; i++)
    {
        uint8_t x = rand(0, GRID_W);
        uint8_t y = rand(0, GRID_H);

        while (grid[x][y] == BOMB)
        {
            x = rand(0, GRID_W);
            y = rand(0, GRID_H);
        }

        grid[x][y] = BOMB;
    }

    while (1)
    {

        uint8_t keys = getkey();
        while (getkey())
            ;

        if (keys & KEY_START)
            goto start;

        if (keys & KEY_UP)
            selecty = (selecty + GRID_H - 1) % GRID_H;
        if (keys & KEY_DOWN)
            selecty = (selecty + 1) % GRID_H;

        if (keys & KEY_LEFT)
            selectx = (selectx + GRID_W - 1) % GRID_W;
        if (keys & KEY_RIGHT)
            selectx = (selectx + 1) % GRID_W;

        if (keys & KEY_A)
            if (grid[selectx][selecty] > 8)
                explode(selectx, selecty);
        if ((keys & KEY_B) && !lost && flags < GRID_BOMBS)
        {
            if (grid[selectx][selecty] == UNEXPLODED)
            {
                grid[selectx][selecty] = FLAG;
                tflags++;
            }
            else if (grid[selectx][selecty] == BOMB)
            {
                grid[selectx][selecty] = BFLAG;
                flags++;
                tflags++;
            }
            else if (grid[selectx][selecty] == FLAG)
            {
                grid[selectx][selecty] = UNEXPLODED;
                tflags--;
            }
            else if (grid[selectx][selecty] == BFLAG)
            {
                grid[selectx][selecty] = BOMB;
                flags--;
                tflags--;
            }
        }

        for (uint8_t i = 0; i < GRID_W; i++)
        {
            for (uint8_t j = 0; j < GRID_H; j++)
            {
                if (grid[i][j] > 8)
                {
                    drawrect(i * GRID_SIZE - 1, j * GRID_SIZE - 1, GRID_SIZE + 1, GRID_SIZE + 1, 1);
                    if (grid[i][j] == FLAG || grid[i][j] == BFLAG)
                        drawchar('F', i * GRID_SIZE + 1, j * GRID_SIZE, 1);
                    if (grid[i][j] == BOMB && lost)
                        drawchar('X', i * GRID_SIZE + 1, j * GRID_SIZE, 1);
                }
                else if (grid[i][j] >= 1)
                    drawchar('0' + grid[i][j], i * GRID_SIZE + 1, j * GRID_SIZE, 1);
                if (i == selectx && j == selecty)
                    invertrect(i * GRID_SIZE, j * GRID_SIZE, GRID_SIZE - 1, GRID_SIZE - 1);
            }
        }

        if (lost)
            drawstring("You lost! Press Start to restart", 0, 64 - 6 * 2, 1);
        else if (flags == GRID_BOMBS)
            drawstring("You won! Press Start to restart", 0, 64 - 6 * 2, 1);
        else
        {
            drawstring("Flags: ", 0, 64 - 6 * 2, 1);
            drawnum(tflags, 4 * 7, 64 - 6 * 2, 1);
            drawstring("/", 4 * 9, 64 - 6 * 2, 1);
            drawnum(GRID_BOMBS, 4 * 10, 64 - 6 * 2, 1);

            drawstring("Time: ", 0, 64 - 6, 1);
            drawnum((gettime64() - starttime) / 1000, 4 * 6, 64 - 6, 1);
        }

        refreshscreen();
        sleep(50);
    }

    return 0;
}