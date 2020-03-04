/**
 * @file maptiles.cpp
 * Code for the maptiles function.
 */

#include <iostream>
#include <map>
#include <vector>
#include "maptiles.h"
#include "kdtree.h"
using namespace std;

MosaicCanvas* mapTiles(SourceImage const& theSource,
                       vector<TileImage> const& theTiles, int pixelsPerTile)
{
    /**
     * @todo Implement this function!
     */
    MosaicCanvas* ret = new MosaicCanvas(theSource.getRows(),theSource.getColumns());
    ret->setImage(theTiles.size(),theTiles);

    map<Point<3>, unsigned> map;
    vector<Point<3>> vet;
    for(unsigned i = 0; i < theTiles.size() ; i++){
      HSLAPixel pixel = theTiles[i].getAverageColor();
      Point<3> pt = Point<3>(pixel.h/360,pixel.s,pixel.l);
      vet.push_back(pt);
      map[pt] = i;
    }
    KDTree<3>* tree = new KDTree<3>(vet);
    for(int i = 0 ; i < theSource.getRows(); i++){
      for(int j = 0; j < theSource.getColumns(); j ++){
        HSLAPixel pixel = theSource.getRegionColor(i,j);
        Point<3> pt = Point<3>(pixel.h/360,pixel.s,pixel.l);
        Point<3> clostest = tree->findNearestNeighbor(pt);
        ret->setTile(i,j,map[clostest]);
      }
    }
    ret->resizeImage(pixelsPerTile);
    delete tree;
    return ret;
}
