#define TILE_SIZE 8
enum Tile
{
    EMPTY = 0,
    GROUND,
};

unsigned char ground_bmp[TILE_SIZE * TILE_SIZE] = {
    0b11111111,
    0b10000001,
    0b10000001,
    0b10000001,
    0b10000001,
    0b10000001,
    0b10000001,
    0b11111111,
};