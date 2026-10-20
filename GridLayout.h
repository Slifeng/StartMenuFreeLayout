#pragma once
#include <algorithm>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <vector>

namespace layout {
// XAML dimensions can differ by a small floating-point rounding error at a
// scaled display. Count every complete cell without reserving an extra gap.
inline int FitCells(double available,double cellSize) {
    if(!std::isfinite(available)||!std::isfinite(cellSize)||cellSize<=0)return 0;
    return std::max(0,int(std::floor((available+0.01)/cellSize)));
}
struct Cell {
    int column{},row{};
    bool operator==(Cell const&) const = default;
};
struct Grid {
    int columns{8},rows{7};
    double width{96},height{84};
    Cell Clamp(Cell c) const {return {std::clamp(c.column,0,columns-1),std::clamp(c.row,0,rows-1)};}
    bool Contains(Cell c) const {return c.column>=0&&c.column<columns&&c.row>=0&&c.row<rows;}
    Cell Snap(double x,double y) const {
        if(!std::isfinite(x)||!std::isfinite(y))return {};
        return Clamp({int(std::round(std::clamp(x,0.0,(columns-1)*width)/width)),int(std::round(std::clamp(y,0.0,(rows-1)*height)/height))});
    }
    double X(Cell c) const {return c.column*width;}
    double Y(Cell c) const {return c.row*height;}
    std::optional<Cell> NearestFree(Cell desired,std::vector<Cell> const& used) const {
        desired=Clamp(desired);std::optional<Cell> result;long long best=0;
        for(int r=0;r<rows;r++)for(int c=0;c<columns;c++) {
            Cell current{c,r};if(std::find(used.begin(),used.end(),current)!=used.end())continue;
            long long dx=c-desired.column,dy=r-desired.row,distance=dx*dx+dy*dy;
            if(!result||distance<best){result=current;best=distance;}
        }
        return result;
    }
};
struct Request {Cell desired;bool saved;};
inline std::vector<Cell> Resolve(Grid const& grid,std::vector<Request> const& requests) {
    if(requests.size()>static_cast<size_t>(grid.columns*grid.rows))throw std::runtime_error("Not enough cells");
    std::vector<Cell> result(requests.size()),used;std::vector<size_t> order;
    for(size_t i=0;i<requests.size();i++)order.push_back(i);
    std::stable_sort(order.begin(),order.end(),[&](auto a,auto b){return requests[a].saved>requests[b].saved;});
    for(auto i:order){auto cell=grid.NearestFree(requests[i].desired,used);if(!cell)throw std::runtime_error("No free cell");result[i]=*cell;used.push_back(*cell);}
    return result;
}
// Occupied destinations are handled by the native folder integration.
inline bool Move(Grid const& grid,std::vector<Cell>& cells,size_t index,Cell destination) {
    if(index>=cells.size()||!grid.Contains(destination))return false;
    for(size_t j=0;j<cells.size();j++)if(j!=index&&cells[j]==destination)return false;
    cells[index]=destination;return true;
}
}
