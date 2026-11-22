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

void makeMove(uint8_t fx, uint8_t fy, uint8_t tx, uint8_t ty);
void updateGameState(void);
uint32_t cx(uint32_t n);

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
uint8_t legalmap[GRID_H][GRID_W];

uint8_t selectx = 0;
uint8_t selecty = 0;

bool isselected = false;
uint8_t selectedx = 0;
uint8_t selectedy = 0;

bool turn = 0; // 0 for white, 1 for black
bool playercolor = 0;
bool ai = false;

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
    blit(x, y, w, h, data, 0, 0, w, BLIT_1BPP | BLIT_TRANSPARENT, 0, 0);
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
// Piece values: positive for white, negative for black.
const int piece_values[13] = {
    0, -100, -320, -330, -900, -20000, -500,
    100, 320, 330, 900, 20000, 500};


uint8_t difficulty = 3;

typedef struct
{
    uint8_t b[64];
    uint8_t castle;
    int8_t epX, epY;
    uint8_t turn;
} ChessPos;

typedef struct
{
    uint32_t nodes;
    uint32_t limit;
    bool aborted;
} SearchCtx;

static const uint32_t node_limits[10] = {
    300, 700, 1500, 2500, 5000,
    9000, 16000, 30000, 50000, 90000};

static const uint8_t max_depths[10] = {
    1, 2, 2, 3, 3, 3, 4, 4, 5, 5};

#define POS_AT(p, x, y) (((uint8_t *)(p))[(y) * 8 + (x)])

static void positionInit(ChessPos *p)
{
    memcpy(p->b, board, 64);
    p->castle = castling;
    p->epX = epX;
    p->epY = epY;
    p->turn = turn;
}

static bool positionClearPath(uint8_t *b,
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
        if (b[y * 8 + x] != EMPTY)
            return false;
        x += sx;
        y += sy;
    }
    return true;
}

static bool positionAttacked(uint8_t *b,
                             uint8_t x, uint8_t y,
                             uint8_t byColor)
{
    int pawnY = byColor ? y - 1 : y + 1;
    int px;

    for (px = (int)x - 1; px <= (int)x + 1; px += 2)
    {
        if (px >= 0 && px < 8 && pawnY >= 0 && pawnY < 8)
        {
            uint8_t piece = b[pawnY * 8 + px];
            if (piece == (byColor ? BPAWN : WPAWN))
                return true;
        }
    }

    const int knightX[8] = {1, 2, 2, 1, -1, -2, -2, -1};
    const int knightY[8] = {-2, -1, 1, 2, 2, 1, -1, -2};

    for (int i = 0; i < 8; i++)
    {
        px = x + knightX[i];
        int py = y + knightY[i];
        if (px >= 0 && px < 8 && py >= 0 && py < 8)
        {
            uint8_t piece = b[py * 8 + px];
            if (piece == (byColor ? BKNIGHT : WKNIGHT))
                return true;
        }
    }

    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++)
        {
            if (!dx && !dy)
                continue;
            px = x + dx;
            int py = y + dy;
            if (px >= 0 && px < 8 && py >= 0 && py < 8)
            {
                uint8_t piece = b[py * 8 + px];
                if (piece == (byColor ? BKING : WKING))
                    return true;
            }
        }

    const int dirX[8] = {1, -1, 0, 0, 1, 1, -1, -1};
    const int dirY[8] = {0, 0, 1, -1, 1, -1, 1, -1};

    for (int d = 0; d < 8; d++)
    {
        px = x + dirX[d];
        int py = y + dirY[d];

        while (px >= 0 && px < 8 && py >= 0 && py < 8)
        {
            uint8_t piece = b[py * 8 + px];
            if (piece != EMPTY)
            {
                if (d < 4)
                {
                    if (piece == (byColor ? BROOK : WROOK) ||
                        piece == (byColor ? BQUEEN : WQUEEN))
                        return true;
                }
                else
                {
                    if (piece == (byColor ? BBISHOP : WBISHOP) ||
                        piece == (byColor ? BQUEEN : WQUEEN))
                        return true;
                }
                break;
            }
            px += dirX[d];
            py += dirY[d];
        }
    }

    return false;
}

static bool positionKingInCheck(ChessPos *p, uint8_t color)
{
    uint8_t king = color ? BKING : WKING;
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++)
            if (POS_AT(p, x, y) == king)
                return positionAttacked(p->b, x, y, !color);
    return true;
}

static bool positionPseudo(ChessPos *p,
                           uint8_t fx, uint8_t fy,
                           uint8_t tx, uint8_t ty)
{
    if (fx >= 8 || fy >= 8 || tx >= 8 || ty >= 8)
        return false;

    uint8_t piece = POS_AT(p, fx, fy);
    uint8_t target = POS_AT(p, tx, ty);

    if (piece == EMPTY || (fx == tx && fy == ty))
        return false;
    if (PIECE_COLOR(piece) != p->turn)
        return false;
    if (target != EMPTY && PIECE_COLOR(target) == p->turn)
        return false;
    if (target == BKING || target == WKING)
        return false;

    int dx = (int)tx - fx;
    int dy = (int)ty - fy;
    int ax = abs(dx);
    int ay = abs(dy);

    if (piece == WPAWN || piece == BPAWN)
    {
        int dir = piece == WPAWN ? -1 : 1;
        int start = piece == WPAWN ? 6 : 1;

        if (dx == 0)
        {
            if (target != EMPTY)
                return false;
            if (dy == dir)
                return true;
            if (fy == start && dy == dir * 2 &&
                POS_AT(p, fx, fy + dir) == EMPTY)
                return true;
            return false;
        }

        return ax == 1 && dy == dir &&
               (target != EMPTY || (tx == p->epX && ty == p->epY));
    }

    if (piece == WKNIGHT || piece == BKNIGHT)
        return (ax == 1 && ay == 2) || (ax == 2 && ay == 1);

    if (piece == WKING || piece == BKING)
    {
        if (ax <= 1 && ay <= 1)
            return true;
        if (ay != 0 || ax != 2 || fx != 4)
            return false;

        bool black = piece == BKING;
        uint8_t row = black ? 0 : 7;
        if (fy != row || (tx != 2 && tx != 6))
            return false;

        uint8_t right;
        if (black)
            right = tx == 6 ? CASTLE_BK : CASTLE_BQ;
        else
            right = tx == 6 ? CASTLE_WK : CASTLE_WQ;

        uint8_t rook = black ? BROOK : WROOK;
        int rookX = tx == 6 ? 7 : 0;

        if (!(p->castle & right) || POS_AT(p, rookX, row) != rook)
            return false;

        int step = tx > fx ? 1 : -1;
        for (int x = fx + step; x != rookX; x += step)
            if (POS_AT(p, x, row) != EMPTY)
                return false;

        for (int x = fx;; x += step)
        {
            if (positionAttacked(p->b, x, fy, !black))
                return false;
            if (x == tx)
                break;
        }
        return true;
    }

    if (piece == WROOK || piece == BROOK)
    {
        if (dx != 0 && dy != 0)
            return false;
        return positionClearPath(p->b, fx, fy, tx, ty);
    }

    if (piece == WBISHOP || piece == BBISHOP)
    {
        if (ax != ay)
            return false;
        return positionClearPath(p->b, fx, fy, tx, ty);
    }

    if (piece == WQUEEN || piece == BQUEEN)
    {
        if (dx != 0 && dy != 0 && ax != ay)
            return false;
        return positionClearPath(p->b, fx, fy, tx, ty);
    }

    return false;
}

static void positionApply(ChessPos *p,
                          uint8_t fx, uint8_t fy,
                          uint8_t tx, uint8_t ty)
{
    uint8_t piece = POS_AT(p, fx, fy);
    uint8_t captured = POS_AT(p, tx, ty);
    bool black = piece <= BROOK;

    if (piece == BKING)
        p->castle &= ~(CASTLE_BK | CASTLE_BQ);
    if (piece == WKING)
        p->castle &= ~(CASTLE_WK | CASTLE_WQ);

    if (piece == BROOK)
    {
        if (fx == 0 && fy == 0)
            p->castle &= ~CASTLE_BQ;
        if (fx == 7 && fy == 0)
            p->castle &= ~CASTLE_BK;
    }
    if (piece == WROOK)
    {
        if (fx == 0 && fy == 7)
            p->castle &= ~CASTLE_WQ;
        if (fx == 7 && fy == 7)
            p->castle &= ~CASTLE_WK;
    }
    if (captured == BROOK)
    {
        if (tx == 0 && ty == 0)
            p->castle &= ~CASTLE_BQ;
        if (tx == 7 && ty == 0)
            p->castle &= ~CASTLE_BK;
    }
    if (captured == WROOK)
    {
        if (tx == 0 && ty == 7)
            p->castle &= ~CASTLE_WQ;
        if (tx == 7 && ty == 7)
            p->castle &= ~CASTLE_WK;
    }

    if ((piece == WPAWN || piece == BPAWN) &&
        tx == p->epX && ty == p->epY &&
        fx != tx && captured == EMPTY)
        POS_AT(p, tx, ty + (black ? -1 : 1)) = EMPTY;

    if ((piece == WKING || piece == BKING) && abs((int)tx - fx) == 2)
    {
        if (tx == 6)
        {
            POS_AT(p, 5, fy) = POS_AT(p, 7, fy);
            POS_AT(p, 7, fy) = EMPTY;
        }
        else
        {
            POS_AT(p, 3, fy) = POS_AT(p, 0, fy);
            POS_AT(p, 0, fy) = EMPTY;
        }
    }

    POS_AT(p, tx, ty) = piece;
    POS_AT(p, fx, fy) = EMPTY;

    if (piece == WPAWN && ty == 0)
        POS_AT(p, tx, ty) = WQUEEN;
    if (piece == BPAWN && ty == 7)
        POS_AT(p, tx, ty) = BQUEEN;

    p->epX = -1;
    p->epY = -1;

    if ((piece == WPAWN || piece == BPAWN) && abs((int)ty - fy) == 2)
    {
        p->epX = tx;
        p->epY = (fy + ty) / 2;
    }

    p->turn = !p->turn;
}

static bool positionLegal(ChessPos *p,
                          uint8_t fx, uint8_t fy,
                          uint8_t tx, uint8_t ty)
{
    if (!positionPseudo(p, fx, fy, tx, ty))
        return false;

    uint8_t color = p->turn;
    ChessPos tmp;
    memcpy(&tmp, p, sizeof(ChessPos));
    positionApply(&tmp, fx, fy, tx, ty);
    return !positionKingInCheck(&tmp, color);
}

static int evaluatePosition(ChessPos *p)
{
    int score = 0;

    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++)
        {
            uint8_t piece = POS_AT(p, x, y);
            if (piece == EMPTY)
                continue;

            int value = piece_values[piece];

            if (piece == WPAWN)
                value += (6 - y) * 9;
            else if (piece == BPAWN)
                value -= (y - 1) * 9;

            int center = 6 - (abs(3 - x) + abs(3 - y));
            if (center > 0)
            {
                int bonus = center * 3;
                if (piece >= WKNIGHT && piece <= WQUEEN)
                    value += bonus;
                else if (piece >= BKNIGHT && piece <= BQUEEN)
                    value -= bonus;
            }

            score += value;
        }

    return score;
}

static bool tacticalMove(ChessPos *p,
                         uint8_t fx, uint8_t fy,
                         uint8_t tx, uint8_t ty)
{
    uint8_t piece = POS_AT(p, fx, fy);
    return POS_AT(p, tx, ty) != EMPTY ||
           (piece == WPAWN && ty == 0) ||
           (piece == BPAWN && ty == 7) ||
           ((piece == WPAWN || piece == BPAWN) &&
            tx == p->epX && ty == p->epY);
}

// The compiler inlines calls and cannot recurse, so the search is unrolled into fixed levels.
static int searchChess0(ChessPos *p, int depth,
                        int alpha, int beta,
                        SearchCtx *ctx, int ply)
{
    if (ctx->nodes >= ctx->limit)
    {
        ctx->aborted = true;
        return evaluatePosition(p);
    }
    ctx->nodes++;
    return evaluatePosition(p);
}

static int searchChess1(ChessPos *p, int depth,
                        int alpha, int beta,
                        SearchCtx *ctx, int ply)
{
    if (ctx->nodes >= ctx->limit)
    {
        ctx->aborted = true;
        return evaluatePosition(p);
    }
    ctx->nodes++;

    if (depth <= 0)
        return evaluatePosition(p);

    bool maximizing = p->turn == 0;
    bool found = false;
    int best = maximizing ? -32000 : 32000;

    for (int pass = 0; pass < 2; pass++)
    {
        for (int fy = 0; fy < 8; fy++)
            for (int fx = 0; fx < 8; fx++)
            {
                uint8_t piece = POS_AT(p, fx, fy);
                if (piece == EMPTY || PIECE_COLOR(piece) != p->turn)
                    continue;

                for (int ty = 0; ty < 8; ty++)
                    for (int tx = 0; tx < 8; tx++)
                    {
                        if (!positionLegal(p, fx, fy, tx, ty))
                            continue;

                        bool tactical = tacticalMove(p, fx, fy, tx, ty);
                        if (tactical != (pass == 0))
                            continue;

                        found = true;

                        ChessPos child;
                        memcpy(&child, p, sizeof(ChessPos));
                        positionApply(&child, fx, fy, tx, ty);

                        int score = searchChess0(&child, depth - 1,
                                                 alpha, beta, ctx, ply + 1);
                        if (ctx->aborted)
                            return best;

                        if (maximizing)
                        {
                            if (score > best)
                                best = score;
                            if (best > alpha)
                                alpha = best;
                        }
                        else
                        {
                            if (score < best)
                                best = score;
                            if (best < beta)
                                beta = best;
                        }

                        if (alpha >= beta)
                            return best;
                    }
            }
    }

    if (!found)
    {
        if (positionKingInCheck(p, p->turn))
            return maximizing ? -30000 + ply : 30000 - ply;
        return 0;
    }

    return best;
}

static int searchChess2(ChessPos *p, int depth,
                        int alpha, int beta,
                        SearchCtx *ctx, int ply)
{
    if (ctx->nodes >= ctx->limit)
    {
        ctx->aborted = true;
        return evaluatePosition(p);
    }
    ctx->nodes++;

    if (depth <= 0)
        return evaluatePosition(p);

    bool maximizing = p->turn == 0;
    bool found = false;
    int best = maximizing ? -32000 : 32000;

    for (int pass = 0; pass < 2; pass++)
    {
        for (int fy = 0; fy < 8; fy++)
            for (int fx = 0; fx < 8; fx++)
            {
                uint8_t piece = POS_AT(p, fx, fy);
                if (piece == EMPTY || PIECE_COLOR(piece) != p->turn)
                    continue;

                for (int ty = 0; ty < 8; ty++)
                    for (int tx = 0; tx < 8; tx++)
                    {
                        if (!positionLegal(p, fx, fy, tx, ty))
                            continue;

                        bool tactical = tacticalMove(p, fx, fy, tx, ty);
                        if (tactical != (pass == 0))
                            continue;

                        found = true;

                        ChessPos child;
                        memcpy(&child, p, sizeof(ChessPos));
                        positionApply(&child, fx, fy, tx, ty);

                        int score = searchChess1(&child, depth - 1,
                                                 alpha, beta, ctx, ply + 1);
                        if (ctx->aborted)
                            return best;

                        if (maximizing)
                        {
                            if (score > best)
                                best = score;
                            if (best > alpha)
                                alpha = best;
                        }
                        else
                        {
                            if (score < best)
                                best = score;
                            if (best < beta)
                                beta = best;
                        }

                        if (alpha >= beta)
                            return best;
                    }
            }
    }

    if (!found)
    {
        if (positionKingInCheck(p, p->turn))
            return maximizing ? -30000 + ply : 30000 - ply;
        return 0;
    }

    return best;
}

static int searchChess3(ChessPos *p, int depth,
                        int alpha, int beta,
                        SearchCtx *ctx, int ply)
{
    if (ctx->nodes >= ctx->limit)
    {
        ctx->aborted = true;
        return evaluatePosition(p);
    }
    ctx->nodes++;

    if (depth <= 0)
        return evaluatePosition(p);

    bool maximizing = p->turn == 0;
    bool found = false;
    int best = maximizing ? -32000 : 32000;

    for (int pass = 0; pass < 2; pass++)
    {
        for (int fy = 0; fy < 8; fy++)
            for (int fx = 0; fx < 8; fx++)
            {
                uint8_t piece = POS_AT(p, fx, fy);
                if (piece == EMPTY || PIECE_COLOR(piece) != p->turn)
                    continue;

                for (int ty = 0; ty < 8; ty++)
                    for (int tx = 0; tx < 8; tx++)
                    {
                        if (!positionLegal(p, fx, fy, tx, ty))
                            continue;

                        bool tactical = tacticalMove(p, fx, fy, tx, ty);
                        if (tactical != (pass == 0))
                            continue;

                        found = true;

                        ChessPos child;
                        memcpy(&child, p, sizeof(ChessPos));
                        positionApply(&child, fx, fy, tx, ty);

                        int score = searchChess2(&child, depth - 1,
                                                 alpha, beta, ctx, ply + 1);
                        if (ctx->aborted)
                            return best;

                        if (maximizing)
                        {
                            if (score > best)
                                best = score;
                            if (best > alpha)
                                alpha = best;
                        }
                        else
                        {
                            if (score < best)
                                best = score;
                            if (best < beta)
                                beta = best;
                        }

                        if (alpha >= beta)
                            return best;
                    }
            }
    }

    if (!found)
    {
        if (positionKingInCheck(p, p->turn))
            return maximizing ? -30000 + ply : 30000 - ply;
        return 0;
    }

    return best;
}

static int searchChess4(ChessPos *p, int depth,
                        int alpha, int beta,
                        SearchCtx *ctx, int ply)
{
    if (ctx->nodes >= ctx->limit)
    {
        ctx->aborted = true;
        return evaluatePosition(p);
    }
    ctx->nodes++;

    if (depth <= 0)
        return evaluatePosition(p);

    bool maximizing = p->turn == 0;
    bool found = false;
    int best = maximizing ? -32000 : 32000;

    for (int pass = 0; pass < 2; pass++)
    {
        for (int fy = 0; fy < 8; fy++)
            for (int fx = 0; fx < 8; fx++)
            {
                uint8_t piece = POS_AT(p, fx, fy);
                if (piece == EMPTY || PIECE_COLOR(piece) != p->turn)
                    continue;

                for (int ty = 0; ty < 8; ty++)
                    for (int tx = 0; tx < 8; tx++)
                    {
                        if (!positionLegal(p, fx, fy, tx, ty))
                            continue;

                        bool tactical = tacticalMove(p, fx, fy, tx, ty);
                        if (tactical != (pass == 0))
                            continue;

                        found = true;

                        ChessPos child;
                        memcpy(&child, p, sizeof(ChessPos));
                        positionApply(&child, fx, fy, tx, ty);

                        int score = searchChess3(&child, depth - 1,
                                                 alpha, beta, ctx, ply + 1);
                        if (ctx->aborted)
                            return best;

                        if (maximizing)
                        {
                            if (score > best)
                                best = score;
                            if (best > alpha)
                                alpha = best;
                        }
                        else
                        {
                            if (score < best)
                                best = score;
                            if (best < beta)
                                beta = best;
                        }

                        if (alpha >= beta)
                            return best;
                    }
            }
    }

    if (!found)
    {
        if (positionKingInCheck(p, p->turn))
            return maximizing ? -30000 + ply : 30000 - ply;
        return 0;
    }

    return best;
}

static int searchChess(ChessPos *p, int depth,
                       int alpha, int beta,
                       SearchCtx *ctx, int ply)
{
    if (depth >= 4)
        return searchChess4(p, depth, alpha, beta, ctx, ply);
    if (depth == 3)
        return searchChess3(p, depth, alpha, beta, ctx, ply);
    if (depth == 2)
        return searchChess2(p, depth, alpha, beta, ctx, ply);
    if (depth == 1)
        return searchChess1(p, depth, alpha, beta, ctx, ply);
    return searchChess0(p, depth, alpha, beta, ctx, ply);
}

// Keep the original public function signatures used by the game.
bool legal(uint8_t b[8][8],
           uint8_t fx, uint8_t fy,
           uint8_t tx, uint8_t ty)
{
    ChessPos p;
    memcpy(p.b, b, 64);
    p.castle = castling;
    p.epX = epX;
    p.epY = epY;
    p.turn = PIECE_COLOR(b[fy][fx]);
    return positionLegal(&p, fx, fy, tx, ty);
}

void simulateMove(uint8_t b[8][8],
                  uint8_t fx, uint8_t fy,
                  uint8_t tx, uint8_t ty)
{
    ChessPos p;
    memcpy(p.b, b, 64);
    p.castle = castling;
    p.epX = epX;
    p.epY = epY;
    p.turn = PIECE_COLOR(b[fy][fx]);
    positionApply(&p, fx, fy, tx, ty);
    memcpy(b, p.b, 64);
}

void makeAIMove(void)
{
    ChessPos root;
    positionInit(&root);

    uint8_t level = difficulty;
    if (level < 1)
        level = 1;
    if (level > 10)
        level = 10;

    SearchCtx ctx;
    ctx.nodes = 0;
    ctx.limit = node_limits[level - 1];
    ctx.aborted = false;

    int targetDepth = max_depths[level - 1];

    uint8_t bestFX = 0, bestFY = 0;
    uint8_t bestTX = 0, bestTY = 0;
    bool found = false;

    // Find a legal fallback before starting the bounded search.
    for (int fy = 0; fy < 8 && !found; fy++)
        for (int fx = 0; fx < 8 && !found; fx++)
            for (int ty = 0; ty < 8 && !found; ty++)
                for (int tx = 0; tx < 8; tx++)
                    if (positionLegal(&root, fx, fy, tx, ty))
                    {
                        bestFX = fx;
                        bestFY = fy;
                        bestTX = tx;
                        bestTY = ty;
                        found = true;
                        break;
                    }

    if (!found)
        return;

    for (int depth = 1; depth <= targetDepth; depth++)
    {
        int bestScore = root.turn == 0 ? -32000 : 32000;
        uint8_t fxBest = bestFX, fyBest = bestFY;
        uint8_t txBest = bestTX, tyBest = bestTY;
        bool completed = true;
        int alpha = -32000;
        int beta = 32000;

        for (int pass = 0; pass < 2 && completed; pass++)
            for (int fy = 0; fy < 8 && completed; fy++)
                for (int fx = 0; fx < 8 && completed; fx++)
                {
                    uint8_t piece = POS_AT(&root, fx, fy);
                    if (piece == EMPTY || PIECE_COLOR(piece) != root.turn)
                        continue;

                    for (int ty = 0; ty < 8 && completed; ty++)
                        for (int tx = 0; tx < 8; tx++)
                        {
                            if (!positionLegal(&root, fx, fy, tx, ty))
                                continue;

                            bool tactical = tacticalMove(&root, fx, fy, tx, ty);
                            if (tactical != (pass == 0))
                                continue;

                            ChessPos child;
                            memcpy(&child, &root, sizeof(ChessPos));
                            positionApply(&child, fx, fy, tx, ty);

                            int score = searchChess(&child, depth - 1,
                                                    alpha, beta, &ctx, 1);
                            if (ctx.aborted)
                            {
                                completed = false;
                                break;
                            }

                            if ((root.turn == 0 && score > bestScore) ||
                                (root.turn == 1 && score < bestScore))
                            {
                                bestScore = score;
                                fxBest = fx;
                                fyBest = fy;
                                txBest = tx;
                                tyBest = ty;
                            }

                            if (root.turn == 0)
                            {
                                if (bestScore > alpha)
                                    alpha = bestScore;
                            }
                            else
                            {
                                if (bestScore < beta)
                                    beta = bestScore;
                            }
                        }
                }

        if (!completed)
            break;

        bestFX = fxBest;
        bestFY = fyBest;
        bestTX = txBest;
        bestTY = tyBest;
    }

    makeMove(bestFX, bestFY, bestTX, bestTY);
    updateGameState();
}

#if 0 // dead board-based duplicates of the ChessPos functions above
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
        for (int x = fx; x != tx + step; x += step)
            if (attacked(b, x, fy, !black))
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

#endif

bool kingInCheck(uint8_t b[GRID_H][GRID_W], uint8_t color)
{
    ChessPos p;
    memcpy(p.b, b, 64);
    return positionKingInCheck(&p, color);
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
    if (p == BPAWN)
        return bpawn_bmp;
    if (p == WPAWN)
        return wpawn_bmp;
    if (p == BKNIGHT)
        return bknight_bmp;
    if (p == WKNIGHT)
        return wknight_bmp;
    if (p == BBISHOP)
        return bbishop_bmp;
    if (p == WBISHOP)
        return wbishop_bmp;
    if (p == BROOK)
        return brook_bmp;
    if (p == WROOK)
        return wrook_bmp;
    if (p == BQUEEN)
        return bqueen_bmp;
    if (p == WQUEEN)
        return wqueen_bmp;
    if (p == BKING)
        return bking_bmp;
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

    inCheck = false;

    while (true)
    {
        uint8_t keys = getkey();

        while (getkey())
            ;

        if (keys & KEY_A)
        {
            ai = true;
            break;
        }

        if (keys & KEY_B)
        {
            ai = false;
            break;
        }

        if (keys & KEY_RIGHT)
        {
            difficulty = difficulty % 10 + 1;
        }

        if (keys & KEY_LEFT)
        {
            difficulty = (difficulty + 8) % 10 + 1;
        }

        drawstring("A: Play vs AI", 0, 0, 1);
        drawstring("Difficulty: ", 0, FONT_H, 1);
        drawnum(difficulty, 0, FONT_H * 2, 1);

        drawstring("B: 2 Players", 0, FONT_H * 4, 1);

        refreshscreen();
        sleep(100);
    }

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

        if (ai && turn != playercolor && !gameOver)
        {
            makeAIMove();
        }

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
                else if (legalmap[selecty][selectx])
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

                legalmap[j][i] = 0;
                if (isselected)
                    legalmap[j][i] = legal(board, selectedx, selectedy, i, j);

                if (legalmap[j][i] && gettime() % 500 < 250)
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
        sleep(10);
    }
}

uint32_t cx(uint32_t n)
{
    return (64 - n * 4) / 2;
}