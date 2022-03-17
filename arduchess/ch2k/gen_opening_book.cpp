#include <fstream>
#include <PGNGameCollection.h>
#include <stdio.h>

#include <map>
#include <vector>
#include <string>

#include "ch2k.hpp"

#define MAX_DEPTH 6
#define MIN_COUNT 13
#define MIN_CHILDREN 1

//#define MAX_DEPTH 2
//#define MIN_COUNT 400
//#define MIN_CHILDREN 1

struct mnode
{
    std::map<std::string, mnode> children;
    pgn::Ply ply;
    ch2k::move mv;
    int mv_n;
    int count;
    mnode() : count(0) {}
};

static int traverse(mnode const& n, int indent, int min_count, int depth)
{
    if(depth == 0) return 0;
    int r = 0;
    int nc = 0;
    for(auto const& c : n.children)
        if(c.second.count >= min_count) ++nc;
    if(nc < MIN_CHILDREN) return r;
    for(auto const& c : n.children)
    {
        if(c.second.count < min_count) continue;
        ++r;
        printf("%3d%*c%s (%s)\n",
            c.second.mv_n,
            indent, ' ',
            c.first.c_str(),
            c.second.mv.uci_str().c_str());
        r += traverse(c.second, indent + 2, min_count, depth - 1);
    }
    return r;
}

static void parse_san(ch2k::game& g, mnode& n, int depth)
{
    int i, nmvs = g.gen_moves(&g.mvs_[0]);
    if(nmvs > 64)
        __debugbreak();
    for(i = 0; i < nmvs; ++i)
    {
        ch2k::move m = g.mvs_[i];
        auto fr = n.ply.fromSquare();
        if(m.is_castle())
        {
            if(!n.ply.isLongCastle() && !n.ply.isShortCastle()) continue;
            if(n.ply.isLongCastle() && m.to().col() > m.fr().col()) continue;
            if(n.ply.isShortCastle() && m.to().col() < m.fr().col()) continue;
        }
        else if(n.ply.piece().letter() != toupper(g.square_to_piece(m.fr()).type().uci_char())) continue;
        else if(n.ply.toSquare().row() != '8' - m.to().row()) continue;
        else if(n.ply.toSquare().col() != 'a' + m.to().col()) continue;
        else if(fr.row() && fr.row() != '8' - m.fr().row()) continue;
        else if(fr.col() && fr.col() != 'a' + m.fr().col()) continue;
        
        n.mv = m;
        n.mv_n = i;
        break;
    }
    if(i >= nmvs)
        __debugbreak();
    if(depth >= 12) return;
    g.do_move(n.mv);
    for(auto& kv : n.children)
        parse_san(g, kv.second, depth + 1);
    g.undo_move(n.mv);
}

static void output(mnode const& n, std::vector<uint8_t>& v, bool islast, int min_count, int depth)
{
    if(depth == 0) return;
    if(n.count < MIN_COUNT) return;
    uint8_t d = n.mv_n;
    int num_children = 0;
    mnode const* last = nullptr;
    for(auto const& kv : n.children)
        if(kv.second.count >= min_count) ++num_children, last = &kv.second;
    if(num_children < MIN_CHILDREN) num_children = 0;
    if(depth == 1) num_children = 0;
    if(num_children > 0) d |= 0x80;
    if(islast) d |= 0x40;
    v.push_back(d);
    if(num_children == 0) return;
    for(auto const& kv : n.children)
    {
        if(kv.second.count < min_count) continue;
        output(kv.second, v, &kv.second == last, min_count, depth - 1);
    }
}

int main()
{
    printf("Reading %s ...\n", PGNFILE);
    pgn::GameCollection games;
    {
        std::ifstream f(PGNFILE);
        f >> games;
    }
    printf("Games: %d\n", (int)games.size());

    mnode root;
    int nmoves = 0;

    printf("Constructing tree...\n");
    for(auto const& game : games)
    {
        mnode* n = &root;
        for(auto const& move : game.moves())
        {
            if(!move.white().valid()) break;
            n = &n->children[move.white().str()];
            n->ply = move.white();
            n->count++;
            nmoves++;
            if(!move.black().valid()) break;
            n = &n->children[move.black().str()];
            n->ply = move.black();
            n->count++;
            nmoves++;
        }
    }
    printf("Moves: %d\n", nmoves);
    printf("Parsing SAN...\n");
    {
        ch2k::game g;
        g.new_game();
        for(auto& kv : root.children)
            parse_san(g, kv.second, 0);
    }
    int n = traverse(root, 2, MIN_COUNT, MAX_DEPTH);
    printf("Book size: %d\n", n);

    std::vector<uint8_t> v;
    {
        mnode const* last = nullptr;
        for(auto const& kv : root.children)
            if(kv.second.count >= MIN_COUNT) last = &kv.second;
        for(auto& kv : root.children)
            output(kv.second, v, &kv.second == last, MIN_COUNT, MAX_DEPTH);
    }
    FILE* f = fopen("book.txt", "w");
    fprintf(f, "// parameters: MAX_DEPTH=%d MIN_COUNT=%d MIN_CHILDREN=%d\n",
        MAX_DEPTH, MIN_COUNT, MIN_CHILDREN);
    fprintf(f, "static constexpr u8 const OPENING_BOOK_MAX_DEPTH = %d;\n", MAX_DEPTH);
    fprintf(f, "static u8 const OPENING_BOOK[%d] PROGMEM =\n{\n    ", (int)v.size());
    for(size_t i = 0; i < v.size(); ++i)
    {
        fprintf(f, "%3d,%s", v[i], i % 8 == 7 ? "\n    " : " ");
    }
    fprintf(f, "\n};");
    fclose(f);
    printf("Book bytes: %d\n", (int)v.size());
}
