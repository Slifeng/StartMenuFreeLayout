#include "SpatialNavigation.h"
#include <cassert>
#include <iostream>
#include <algorithm>
using navigation::Direction;
int main() {
    layout::Grid grid{8,7,96,84};
    // Deliberately scrambled model order: direction must follow cells instead.
    std::vector<layout::Cell> cells{{7,6},{3,5},{0,2},{3,0},{6,2},{3,2},{3,4}};
    auto original=cells;
    auto next=[&](Direction d){auto result=navigation::Candidates(grid,{3,2},d,cells);assert(!result.empty());return cells[result.front()];};
    assert((next(Direction::Down)==layout::Cell{3,4}));
    assert((next(Direction::Up)==layout::Cell{3,0}));
    assert((next(Direction::Left)==layout::Cell{0,2}));
    assert((next(Direction::Right)==layout::Cell{6,2}));
    // Aligned candidate wins over an adjacent diagonal; skip several empty rows.
    std::vector<layout::Cell> sparse{{4,3},{3,6},{2,3}};
    auto choices=navigation::Candidates(grid,{3,2},Direction::Down,sparse);
    assert(choices.front()==1);
    sparse.erase(sparse.begin()+1);
    assert(navigation::Candidates(grid,{3,2},Direction::Down,sparse).front()==1); // stable left tie
    std::reverse(sparse.begin(),sparse.end());
    assert(navigation::Candidates(grid,{3,2},Direction::Down,sparse).front()==0);
    assert(navigation::Candidates(grid,{0,0},Direction::Up,cells).empty());
    assert(navigation::Candidates(grid,{0,0},Direction::Left,cells).empty());
    assert(navigation::Candidates(grid,{7,6},Direction::Down,cells).empty());
    assert(navigation::Candidates(grid,{7,6},Direction::Right,cells).empty());
    assert(navigation::Candidates(grid,{-1,0},Direction::Right,cells).empty());
    assert(navigation::Candidates(grid,{0,0},Direction::Down,{{0,0},{0,-1},{0,7},{8,1}}).empty());
    assert(navigation::Candidates(grid,{0,0},Direction::Down,{}).empty());
    assert(cells==original); // navigation never moves pins or fills holes
    // Every returned candidate is strictly ahead, across all cells/directions.
    for(int r=0;r<7;++r)for(int c=0;c<8;++c)for(auto d:{Direction::Left,Direction::Right,Direction::Up,Direction::Down}) {
        for(auto i:navigation::Candidates(grid,{c,r},d,cells)) {
            auto target=cells[i];
            assert(d==Direction::Left?target.column<c:d==Direction::Right?target.column>c:d==Direction::Up?target.row<r:target.row>r);
        }
    }
    std::cout<<"PASS: visual-cell navigation, four directions, gaps, diagonal fallback, stable ties, boundaries, no layout mutation\n";
}
