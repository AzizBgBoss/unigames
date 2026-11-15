// Chess lol
#include "sprites.h"

#define GRID_SIZE 8
#define GRID_W 8
#define GRID_H 8

enum Piece
{
    EMPTY,

    BPAWN,
    BKNIGHT,
    BBISHOP,
    BQUEEN,
    BKING,
    BROOK,

    WPAWN,
    WKNIGHT,
    WBISHOP,
    WQUEEN,
    WKING,
    WROOK
};

#define PIECE_COLOR(piece) ((piece) >= WPAWN ? 0 : 1)

const uint8_t initial_board[GRID_H][GRID_W] = {
    {BROOK, BKNIGHT, BBISHOP, BQUEEN, BKING, BBISHOP, BKNIGHT, BROOK},
    {BPAWN, BPAWN, BPAWN, BPAWN, BPAWN, BPAWN, BPAWN, BPAWN},
    {EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY},
    {EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY},
    {EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY},
    {EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY},
    {WPAWN, WPAWN, WPAWN, WPAWN, WPAWN, WPAWN, WPAWN, WPAWN},
    {WROOK, WKNIGHT, WBISHOP, WQUEEN, WKING, WBISHOP, WKNIGHT, WROOK}};

uint8_t board[GRID_H][GRID_W];

uint8_t selectx = 0;
uint8_t selecty = 0;

bool isselected = false;
uint8_t selectedx = 0;
uint8_t selectedy = 0;

bool turn = 0; // 0 for white, 1 for black
bool playercolor = 0;

// Castling rights
#define CASTLE_BK 1
#define CASTLE_BQ 2
#define CASTLE_WK 4
#define CASTLE_WQ 8

uint8_t castling = 15;

// En passant target
int8_t epX = -1;
int8_t epY = -1;

// Game state
bool inCheck = false;
bool gameOver = false;
bool checkmate = false;
bool stalemate = false;

void bmp(uint8_t x, uint8_t y, uint8_t w, uint8_t h, const uint8_t *data)
{
    for (uint8_t j = 0; j < h; j++)
    {
        uint8_t row_byte = romread(data + j, 1);
        for (uint8_t i = 0; i < w; i++)
        {
            if (x + i < 64 && y + j < 64)
            {
                uint8_t bit = (row_byte >> (7 - i)) & 1;

                if (bit)
                {
                    setpixel(x + i, y + j, bit);
                }
            }
        }
    }
}

void bmpxor(uint8_t x, uint8_t y, uint8_t w, uint8_t h, const uint8_t *data)
{
    for (uint8_t j = 0; j < h; j++)
    {
        uint8_t row_byte = romread(data + j, 1);
        for (uint8_t i = 0; i < w; i++)
        {
            if (x + i < 64 && y + j < 64)
            {
                uint8_t bit = (row_byte >> (7 - i)) & 1;

                if (bit)
                {
                    setpixel(x + i, y + j, !getpixel(x + i, y + j));
                }
            }
        }
    }
}

bool clearPath(uint8_t b[GRID_H][GRID_W],
               uint8_t fx, uint8_t fy,
               uint8_t tx, uint8_t ty)
{
    int dx = (int)tx - fx;
    int dy = (int)ty - fy;

    int sx = (dx > 0) - (dx < 0);
    int sy = (dy > 0) - (dy < 0);

    int x = fx + sx;
    int y = fy + sy;

    while (x != tx || y != ty)
    {
        if (b[y][x] != EMPTY)
            return false;

        x += sx;
        y += sy;
    }

    return true;
}

bool attacked(uint8_t b[GRID_H][GRID_W],
              uint8_t x, uint8_t y, uint8_t color)
{
    // Pawn attacks
    int py = (color == 1) ? y - 1 : y + 1;

    for (int px = (int)x - 1; px <= (int)x + 1; px += 2)
    {
        if (px >= 0 && px < 8 && py >= 0 && py < 8)
        {
            uint8_t p = b[py][px];

            if (p == (color ? BPAWN : WPAWN))
                return true;
        }
    }

    // Knight attacks
    const int nx[] = {1, 2, 2, 1, -1, -2, -2, -1};
    const int ny[] = {-2, -1, 1, 2, 2, 1, -1, -2};

    for (int i = 0; i < 8; i++)
    {
        int px = x + nx[i];
        int py2 = y + ny[i];

        if (px >= 0 && px < 8 && py2 >= 0 && py2 < 8)
        {
            uint8_t p = b[py2][px];

            if (p == (color ? BKNIGHT : WKNIGHT))
                return true;
        }
    }

    // King attacks
    for (int dy = -1; dy <= 1; dy++)
    {
        for (int dx = -1; dx <= 1; dx++)
        {
            if (!dx && !dy)
                continue;

            int px = x + dx;
            int py2 = y + dy;

            if (px >= 0 && px < 8 && py2 >= 0 && py2 < 8)
            {
                if (b[py2][px] == (color ? BKING : WKING))
                    return true;
            }
        }
    }

    // Rook and queen attacks
    const int rx[] = {1, -1, 0, 0};
    const int ry[] = {0, 0, 1, -1};

    for (int d = 0; d < 4; d++)
    {
        int px = x + rx[d];
        int py2 = y + ry[d];

        while (px >= 0 && px < 8 && py2 >= 0 && py2 < 8)
        {
            uint8_t p = b[py2][px];

            if (p != EMPTY)
            {
                if (p == (color ? BROOK : WROOK) ||
                    p == (color ? BQUEEN : WQUEEN))
                    return true;

                break;
            }

            px += rx[d];
            py2 += ry[d];
        }
    }

    // Bishop and queen attacks
    const int bx[] = {1, 1, -1, -1};
    const int by[] = {1, -1, 1, -1};

    for (int d = 0; d < 4; d++)
    {
        int px = x + bx[d];
        int py2 = y + by[d];

        while (px >= 0 && px < 8 && py2 >= 0 && py2 < 8)
        {
            uint8_t p = b[py2][px];

            if (p != EMPTY)
            {
                if (p == (color ? BBISHOP : WBISHOP) ||
                    p == (color ? BQUEEN : WQUEEN))
                    return true;

                break;
            }

            px += bx[d];
            py2 += by[d];
        }
    }

    return false;
}

bool kingInCheck(uint8_t b[GRID_H][GRID_W], uint8_t color)
{
    uint8_t king = color ? BKING : WKING;

    for (int y = 0; y < 8; y++)
    {
        for (int x = 0; x < 8; x++)
        {
            if (b[y][x] == king)
                return attacked(b, x, y, !color);
        }
    }

    return true;
}

bool pseudoLegal(uint8_t b[GRID_H][GRID_W],
                 uint8_t fx, uint8_t fy,
                 uint8_t tx, uint8_t ty)
{
    if (fx >= 8 || fy >= 8 || tx >= 8 || ty >= 8)
        return false;

    uint8_t piece = b[fy][fx];
    uint8_t target = b[ty][tx];

    if (piece == EMPTY || (fx == tx && fy == ty))
        return false;

    if (target != EMPTY &&
        PIECE_COLOR(target) == PIECE_COLOR(piece))
        return false;

    // Never capture a king
    if (target == BKING || target == WKING)
        return false;

    int dx = (int)tx - fx;
    int dy = (int)ty - fy;
    int ax = abs(dx);
    int ay = abs(dy);

    // Pawns
    if (piece == BPAWN || piece == WPAWN)
    {
        int dir = piece == BPAWN ? 1 : -1;
        int start = piece == BPAWN ? 1 : 6;

        if (dx == 0)
        {
            if (target != EMPTY)
                return false;

            if (dy == dir)
                return true;

            if (fy == start && dy == dir * 2 &&
                b[fy + dir][fx] == EMPTY)
                return true;

            return false;
        }

        // Normal capture
        if (ax == 1 && dy == dir && target != EMPTY)
            return true;

        // En passant
        if (ax == 1 && dy == dir &&
            tx == epX && ty == epY)
            return true;

        return false;
    }

    // Knights
    if (piece == BKNIGHT || piece == WKNIGHT)
        return (ax == 1 && ay == 2) ||
               (ax == 2 && ay == 1);

    // Kings
    if (piece == BKING || piece == WKING)
    {
        if (ax <= 1 && ay <= 1)
            return true;

        // Castling
        if (dy != 0 || ax != 2)
            return false;

        uint8_t row = piece == BKING ? 0 : 7;
        if (fy != row || fx != 4 || tx != 2 && tx != 6)
            return false;

        bool black = piece == BKING;

        uint8_t right = black ? (tx == 6 ? CASTLE_BK : CASTLE_BQ) : (tx == 6 ? CASTLE_WK : CASTLE_WQ);

        if (!(castling & right))
            return false;

        uint8_t rook = black ? BROOK : WROOK;
        int rookX = tx == 6 ? 7 : 0;

        if (b[row][rookX] != rook)
            return false;

        int step = tx > fx ? 1 : -1;

        for (int x = fx + step; x != rookX; x += step)
        {
            if (b[row][x] != EMPTY)
                return false;
        }

        // Cannot castle out of, through, or into check
        if (kingInCheck(b, black))
            return false;

        if (attacked(b, fx + step, fy, black))
            return false;

        if (attacked(b, tx, ty, black))
            return false;

        return true;
    }

    // Rooks
    if (piece == BROOK || piece == WROOK)
    {
        if (dx != 0 && dy != 0)
            return false;

        return clearPath(b, fx, fy, tx, ty);
    }

    // Bishops
    if (piece == BBISHOP || piece == WBISHOP)
    {
        if (ax != ay)
            return false;

        return clearPath(b, fx, fy, tx, ty);
    }

    // Queens
    if (piece == BQUEEN || piece == WQUEEN)
    {
        if (dx != 0 && dy != 0 && ax != ay)
            return false;

        return clearPath(b, fx, fy, tx, ty);
    }

    return false;
}

void simulateMove(uint8_t b[GRID_H][GRID_W],
                  uint8_t fx, uint8_t fy,
                  uint8_t tx, uint8_t ty)
{
    uint8_t piece = b[fy][fx];

    // En passant capture
    if ((piece == BPAWN || piece == WPAWN) &&
        tx == epX && ty == epY &&
        fx != tx && b[ty][tx] == EMPTY)
    {
        b[ty - (piece == BPAWN ? 1 : -1)][tx] = EMPTY;
    }

    // Castling rook movement
    if ((piece == BKING || piece == WKING) &&
        abs((int)tx - fx) == 2)
    {
        if (tx == 6)
        {
            b[fy][5] = b[fy][7];
            b[fy][7] = EMPTY;
        }
        else
        {
            b[fy][3] = b[fy][0];
            b[fy][0] = EMPTY;
        }
    }

    b[ty][tx] = piece;
    b[fy][fx] = EMPTY;

    // Automatic promotion to queen
    if (piece == BPAWN && ty == 7)
        b[ty][tx] = BQUEEN;

    if (piece == WPAWN && ty == 0)
        b[ty][tx] = WQUEEN;
}

bool legal(uint8_t b[GRID_H][GRID_W],
           uint8_t fx, uint8_t fy,
           uint8_t tx, uint8_t ty)
{
    if (!pseudoLegal(b, fx, fy, tx, ty))
        return false;

    uint8_t temp[GRID_H][GRID_W];
    memcpy(temp, b, sizeof(temp));

    uint8_t color = PIECE_COLOR(b[fy][fx]);

    simulateMove(temp, fx, fy, tx, ty);

    return !kingInCheck(temp, color);
}

void makeMove(uint8_t fx, uint8_t fy,
              uint8_t tx, uint8_t ty)
{
    uint8_t piece = board[fy][fx];
    uint8_t captured = board[ty][tx];
    uint8_t color = PIECE_COLOR(piece);

    // Moving king removes both castling rights
    if (piece == BKING)
        castling &= ~(CASTLE_BK | CASTLE_BQ);

    if (piece == WKING)
        castling &= ~(CASTLE_WK | CASTLE_WQ);

    // Moving rooks removes corresponding rights
    if (piece == BROOK)
    {
        if (fx == 0 && fy == 0)
            castling &= ~CASTLE_BQ;
        if (fx == 7 && fy == 0)
            castling &= ~CASTLE_BK;
    }

    if (piece == WROOK)
    {
        if (fx == 0 && fy == 7)
            castling &= ~CASTLE_WQ;
        if (fx == 7 && fy == 7)
            castling &= ~CASTLE_WK;
    }

    // Capturing rooks removes their castling rights
    if (captured == BROOK)
    {
        if (tx == 0 && ty == 0)
            castling &= ~CASTLE_BQ;
        if (tx == 7 && ty == 0)
            castling &= ~CASTLE_BK;
    }

    if (captured == WROOK)
    {
        if (tx == 0 && ty == 7)
            castling &= ~CASTLE_WQ;
        if (tx == 7 && ty == 7)
            castling &= ~CASTLE_WK;
    }

    // Perform move using old en passant state
    simulateMove(board, fx, fy, tx, ty);

    // Clear old en passant opportunity
    epX = -1;
    epY = -1;

    // Create new en passant opportunity
    if ((piece == BPAWN || piece == WPAWN) &&
        abs((int)ty - fy) == 2)
    {
        epX = tx;
        epY = (fy + ty) / 2;
    }

    turn = !turn;
    isselected = false;
}

bool hasLegalMove(uint8_t color)
{
    for (uint8_t fy = 0; fy < 8; fy++)
    {
        for (uint8_t fx = 0; fx < 8; fx++)
        {
            if (board[fy][fx] == EMPTY ||
                PIECE_COLOR(board[fy][fx]) != color)
                continue;

            for (uint8_t ty = 0; ty < 8; ty++)
            {
                for (uint8_t tx = 0; tx < 8; tx++)
                {
                    if (legal(board, fx, fy, tx, ty))
                        return true;
                }
            }
        }
    }

    return false;
}

void updateGameState()
{
    inCheck = kingInCheck(board, turn);

    if (!hasLegalMove(turn))
    {
        gameOver = true;

        if (inCheck)
        {
            checkmate = true;
        }
        else
        {
            stalemate = true;
        }
    }
}

const uint8_t *spriteOf(uint8_t p)
{
    switch (p)
    {
    case BPAWN:
        return bpawn_bmp;
    case WPAWN:
        return wpawn_bmp;
    case BKNIGHT:
        return bknight_bmp;
    case WKNIGHT:
        return wknight_bmp;
    case BBISHOP:
        return bbishop_bmp;
    case WBISHOP:
        return wbishop_bmp;
    case BROOK:
        return brook_bmp;
    case WROOK:
        return wrook_bmp;
    case BQUEEN:
        return bqueen_bmp;
    case WQUEEN:
        return wqueen_bmp;
    case BKING:
        return bking_bmp;
    }
    return wking_bmp;
}

#define FONT_H 6
#define FONT_W 4

uint8_t main()
{

    srand(gettime());
    printf("chess\n");

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
    memcpy(board, initial_board, sizeof(board));

    isselected = false;
    turn = 0;
    playercolor = rand(0, 2);

    castling = 15;
    epX = -1;
    epY = -1;

    gameOver = false;
    checkmate = false;
    stalemate = false;

    updateGameState();

    while (true)
    {
        uint8_t keys = getkey();

        while (getkey())
            ;

        if (keys & KEY_START)
        {
            goto start;
        }

        if (keys & KEY_UP)
            selecty = (selecty + GRID_H - 1) % GRID_H;
        if (keys & KEY_DOWN)
            selecty = (selecty + 1) % GRID_H;
        if (keys & KEY_LEFT)
            selectx = (selectx + GRID_W - 1) % GRID_W;
        if (keys & KEY_RIGHT)
            selectx = (selectx + 1) % GRID_W;

        if (keys & KEY_A && !gameOver)
        {
            if (!isselected)
            {
                if (board[selecty][selectx] != EMPTY &&
                    PIECE_COLOR(board[selecty][selectx]) == turn)
                {
                    isselected = true;
                    selectedx = selectx;
                    selectedy = selecty;
                }
            }
            else
            {
                if (selectedx == selectx && selectedy == selecty)
                {
                    isselected = false;
                }
                else if (board[selecty][selectx] != EMPTY &&
                         PIECE_COLOR(board[selecty][selectx]) == turn)
                {
                    selectedx = selectx;
                    selectedy = selecty;
                }
                else if (legal(board, selectedx, selectedy,
                               selectx, selecty))
                {
                    makeMove(selectedx, selectedy, selectx, selecty);
                    updateGameState();
                }
                else
                {
                    isselected = false;
                }
            }
        }

        for (uint8_t j = 0; j < GRID_H; j++)
        {
            for (uint8_t i = 0; i < GRID_W; i++)
            {
                if (board[j][i] == EMPTY)
                    fillrect(i * GRID_SIZE, j * GRID_SIZE, GRID_SIZE, GRID_SIZE, i % 2 == j % 2);
                else
                    bmp(i * GRID_SIZE, j * GRID_SIZE, 8, 8, spriteOf(board[j][i]));

                if (selectx == i && selecty == j && gettime() % 1000 < 500)
                {
                    bmpxor(i * GRID_SIZE, j * GRID_SIZE, 8, 8, select_bmp);
                }

                if (isselected && selectedx == i && selectedy == j && gettime() % 500 < 250)
                {
                    bmpxor(i * GRID_SIZE, j * GRID_SIZE, 8, 8, select_bmp);
                }

                if (isselected && legal(board, selectedx, selectedy, i, j) && gettime() % 500 < 250)
                {
                    bmpxor(i * GRID_SIZE, j * GRID_SIZE, 8, 8, legal_bmp);
                }

            }
        }

        if (inCheck && gettime() % 500 < 250)
        {
            uint8_t king = turn ? BKING : WKING;

            for (uint8_t y = 0; y < 8; y++)
                for (uint8_t x = 0; x < 8; x++)
                    if (board[y][x] == king)
                        bmpxor(x * 8, y * 8, 8, 8, check_bmp);
        }

        if (gameOver)
        {
            if (checkmate)
            {
                fillrect(cx(11) - 1, 64 / 2 - FONT_H - 1, FONT_W * 11 + 1, FONT_H * 3 + 1, 0);
                drawstring("Checkmate!", cx(10), 64 / 2 - FONT_H, 1);
                if (turn == 0)
                    drawstring("Black wins!", cx(11), 64 / 2, 1);
                else
                    drawstring("White wins!", cx(11), 64 / 2, 1);
                drawstring("Press START", cx(11), 64 / 2 + FONT_H, 1);
            }
            else if (stalemate)
            {
                fillrect(cx(11) - 1, 64 / 2 - FONT_H - 1, FONT_W * 11 + 1, FONT_H * 2 + 1, 0);
                drawstring("Stalemate!", cx(10), 64 / 2 - FONT_H, 1);
                drawstring("Press START", cx(11), 64 / 2, 1);
            }
        }

        refreshscreen();
        sleep(50);
    }
}

uint32_t cx(uint32_t n)
{
    return (64 - n * 4) / 2;
}