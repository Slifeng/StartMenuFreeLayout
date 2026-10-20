#include "GridLayout.h"
#include <cassert>
#include <iostream>
#include <set>
int main() {
    assert(layout::FitCells(736-64,84)==8); // the previously unreachable bottom row
    assert(layout::FitCells(672-0.0001,84)==8); // fractional DPI rounding
    assert(layout::FitCells(671,84)==7); // never admit a clipped row
    assert(layout::FitCells(660,84)==7);
    assert(layout::FitCells(0,84)==0);
    assert(layout::FitCells(672,0)==0);
    layout::Grid expanded{8,8,96,84};
    auto bottom=expanded.Snap(300,10000);
    assert((bottom==layout::Cell{3,7}));
    assert(expanded.Y(bottom)+84==736-64);
    layout::Grid g{8,7,96,84};
    assert((g.Snap(-100,-20)==layout::Cell{0,0}));
    assert((g.Snap(47,41)==layout::Cell{0,0}));
    assert((g.Snap(49,43)==layout::Cell{1,1}));
    assert((g.Snap(10000,10000)==layout::Cell{7,6}));
    auto last=g.Snap(10000,10000);
    assert(g.X(last)+96<=8*96 && g.Y(last)+84<=7*84);
    std::vector<layout::Cell> cells{{0,0},{3,0},{7,6}};
    auto original=cells;
    assert(layout::Move(g,cells,0,{5,4}));
    assert(cells[1]==original[1] && cells[2]==original[2]); // holes stay holes
    auto beforeOccupied=cells;
    assert(!layout::Move(g,cells,0,{7,6}));
    assert(cells==beforeOccupied); // overlap never swaps; folder API handles it
    assert(!layout::Move(g,cells,0,{8,6}));
    std::vector<layout::Request> requests;
    for(int i=0;i<26;i++)requests.push_back({{0,0},i%2==0});
    auto migrated=layout::Resolve(g,requests);
    std::set<int> unique;
    for(auto c:migrated){assert(g.Contains(c));assert(unique.insert(c.row*8+c.column).second);}
    assert((migrated[0]==layout::Cell{0,0}));
    auto filled=layout::Resolve(layout::Grid{2,2,96,84},{{{0,0},true},{{0,0},true},{{0,0},true},{{0,0},true}});
    assert(!layout::Grid(2,2,96,84).NearestFree({0,0},filled));
    std::cout<<"PASS: snapping, boundaries, last row, empty cells, no overlap swap, migration collisions, full board\n";
}
