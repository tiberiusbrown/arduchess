#include "ch2k.hpp"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <iterator>

ch2k::game g, g_bak;

static bool startswith(std::string const& str, char const* x)
{
    char const* p = &str[0];
    while(*x)
        if(*x++ != *p++) return false;
    return true;
}

static bool contains(std::string const& str, char const* x)
{
    return str.find(x, 0) != std::string::npos;
}

static void process_command(std::string const& cmd)
{
    if(cmd == "quit") exit(0);
    if(cmd == "isready")
        printf("readyok\n");
    else if(cmd == "uci")
    {
        printf("id name ArduChess\n");
        printf("id author brow1067\n");
        printf("uciok\n");
    }
    else if(cmd == "ucinewgame")
    {
        // nothing to do
    }
    else if(startswith(cmd, "setoption "))
    {
        // nothing to do
    }
    else if(startswith(cmd, "position"))
    {
        memset(&g, 0, sizeof(g));
        if(contains(cmd, "startpos"))
            g.new_game();
        else if(contains(cmd, "fen "))
            g.load_fen(cmd.c_str() + cmd.find("fen ", 0) + 4);
        else
            exit(-3);
        if(contains(cmd, "moves "))
        {
            std::stringstream moves_ss(cmd.substr(cmd.find("moves ") + 6));
            std::vector<std::string> moves{
                std::istream_iterator<std::string>{moves_ss},
                std::istream_iterator<std::string>{} };
            for(std::string const& m : moves)
            {
                int i, n = g.gen_moves(&g.mvs_[0]);
                for(i = 0; i < n; ++i)
                {
                    auto mv = g.mvs_[i];
                    if(m[0] != 'a' + mv.fr().col()) continue;
                    if(m[1] != '8' - mv.fr().row()) continue;
                    if(m[2] != 'a' + mv.to().col()) continue;
                    if(m[3] != '8' - mv.to().row()) continue;
                    if(!mv.is_promotion()) break;
                    if(m[4] != mv.promotion_piece_type().uci_char()) continue;
                    break;
                }
                if(i >= n)
                    exit(-2); // invalid move
                g.execute_move(g.mvs_[i]);
            }
        }
    }
    else if(startswith(cmd, "go"))
    {
        memcpy(&g_bak, &g, sizeof(g));
        g.max_depth_ = 8;
        g.max_nodes_ = 512;
        g.ai_move();
        printf("bestmove %s\n", g.best_.uci_str().c_str());
        memcpy(&g, &g_bak, sizeof(g));
    }
}

int main()
{
    srand((unsigned)time(0));
    g.new_game();
    for(;;)
    {
        std::string cmd;
        std::getline(std::cin, cmd);
        process_command(cmd);
    }
    return 0;
}
