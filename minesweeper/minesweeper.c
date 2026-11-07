#define GRID_SIZE 6
#define GRID_W (64 / GRID_SIZE)
#define GRID_H (64 / GRID_SIZE)
#define GRID_BOMBS 4

#define BOMB 9
#define UNEXPLODED 10

uint8_t grid [GRID_W][GRID_H];

uint8_t selectx = 0;
uint8_t selecty = 0;

uint8_t nearby(uint8_t x, uint8_t y) {
uint8_t bombs = 0;
for (uint8_t i = 0; i < 3; i++)
for (uint8_t j = 0; j < 3; j++) {
if ((i != 1 || j != 1) && x+i-1 >= 0 && x+i-1 < GRID_W && y+j-1 >= 0 && y+j-1 < GRID_H)
if (grid[x+i-1][y+j-1] == BOMB)
bombs++;
}
return bombs;
}

void updategrid() {
for (uint8_t x = 0; x < GRID_W; x++) {
for (uint8_t y = 0; y < GRID_H; y++) {

bool near = false;
for (uint8_t i = 0; i < 3; i++) {
for (uint8_t j = 0; j < 3; j++) {
if (x+i-1 >= 0 && x+i-1 < GRID_W && y+j-1 >= 0 && y+j-1 < GRID_H)
if (grid[x-i-1][y-j-1] < 9)
near = true;
}
}

if (near) {
for (uint8_t i = 0; i < 3; i++) {
for (uint8_t j = 0; j < 3; j++) {
if (x+i-1 >= 0 && x+i-1 < GRID_W && y+j-1 >= 0 && y+j-1 < GRID_H)
if (grid[x+i-1][y+j-1] < 9)
grid[x+i-1][y+j-1] = nearby(x-i-1, y-j-1);
}
}
}

}
}
}

void explode(uint8_t x, uint8_t y) {
grid[x][y] = nearby(x, y);
updategrid();
}

uint8_t main() {
srand(gettime());
printf("geemu\n");

if (isflagsupported(FLAG_GRAPHICS,
GFX_MONO_64X64)) {
flag(FLAG_GRAPHICS, GFX_MONO_64X64);
printf("Starting graphics...\n");
}
else
{
printf("64x64@1 graphics unsupported, exiting.\n");
return 1;
}

if (isflagsupported(1,
1)) {
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
grid[i][j] = UNEXPLODED;
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

if (keys & KEY_A)
if (grid[selectx][selecty] > 8)
explode(selectx, selecty);

for (uint8_t i = 0; i < GRID_W; i++) {
for (uint8_t j = 0; j < GRID_H; j++) {
if (grid[i][j] > 8)
drawrect(i * GRID_SIZE - 1, j * GRID_SIZE - 1, GRID_SIZE + 1, GRID_SIZE + 1, 1);
else if (grid[i][j] >= 1)
drawchar('0' + grid[i][j], i * GRID_SIZE + 1, j * GRID_SIZE, 1);
if (i == selectx && j == selecty)
fillrect(i * GRID_SIZE - 1, j * GRID_SIZE - 1, GRID_SIZE + 1, GRID_SIZE + 1, 1);
}
}
refreshscreen();
sleep(50);
}

return 0;
}