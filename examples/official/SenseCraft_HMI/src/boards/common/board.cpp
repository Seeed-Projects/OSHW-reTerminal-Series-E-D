#include "boards/common/board.h"

Board& Board::GetInstance()
{
    static Board* instance = static_cast<Board*>(create_board());
    return *instance;
}
