#pragma once
#include "GridLayout.h"
#include <cstdint>
#include <tuple>

namespace navigation {
enum class Direction { Left, Right, Up, Down };

// Return candidate indexes in visual-cell order, never the native Items order.
// Prefer the same row/column across holes, then nearby cells in the forward
// half-plane. Edges do not wrap; equal scores have a stable geometric tie-break.
inline std::vector<size_t> Candidates(layout::Grid const& grid,layout::Cell source,
                                     Direction direction,std::vector<layout::Cell> const& cells) {
    using Rank=std::tuple<bool,int64_t,int64_t,int64_t,int,int>;
    std::vector<std::pair<Rank,size_t>> ranked;
    if(!grid.Contains(source))return {};
    for(size_t i=0;i<cells.size();++i) {
        auto c=cells[i];if(!grid.Contains(c))continue;
        int64_t dx=int64_t(c.column)-source.column,dy=int64_t(c.row)-source.row;
        int64_t forward{},cross{};
        switch(direction) {
        case Direction::Left:forward=-dx;cross=dy;break;
        case Direction::Right:forward=dx;cross=dy;break;
        case Direction::Up:forward=-dy;cross=dx;break;
        case Direction::Down:forward=dy;cross=dx;break;
        }
        if(forward<=0)continue;
        cross=std::abs(cross);
        ranked.push_back({{cross!=0,forward*forward+2*cross*cross,cross,forward,c.row,c.column},i});
    }
    std::stable_sort(ranked.begin(),ranked.end(),[](auto const& a,auto const& b){return a.first<b.first;});
    std::vector<size_t> result;for(auto const& entry:ranked)result.push_back(entry.second);
    return result;
}
}
