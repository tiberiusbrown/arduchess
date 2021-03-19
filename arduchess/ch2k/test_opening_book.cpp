#include <stdio.h>

#include "ch2k.hpp"

int main()
{
    ch2k::game g;

    for(int n = 0; n < 20; ++n)
    {
        g.new_game();
        while(g.opening_index_ > 0)
        {
            g.ai_move();
            g.execute_move(g.best_);
            printf(" %5s [%4d]", g.best_.uci_str().c_str(), (int)g.opening_index_);
        }
        printf("\n");
    }


    return 0;
}
