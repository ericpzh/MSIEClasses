
#include "cs225/PNG.h"
#include "FloodFilledImage.h"
#include "Animation.h"

#include "imageTraversal/DFS.h"
#include "imageTraversal/BFS.h"

#include "colorPicker/RainbowColorPicker.h"
#include "colorPicker/GradientColorPicker.h"
#include "colorPicker/GridColorPicker.h"
#include "colorPicker/SolidColorPicker.h"
#include "colorPicker/MyColorPicker.h"
#include <iostream>
using namespace cs225;
using namespace std;

int main() {

  PNG png;
  png.readFromFile("tests/i.png");
  FloodFilledImage image(png);
  DFS dfs(png, Point(40, 40), 0.05);
  BFS bfs(png, Point(0, 0), 0.05);
  RainbowColorPicker rainbow(0.05);
  MyColorPicker my(0.2);
  image.addFloodFill( dfs, rainbow );
  image.addFloodFill( bfs, my );
  Animation animation = image.animate(1000);
  PNG lastFrame = animation.getFrame( animation.frameCount() - 1 );
  lastFrame.writeToFile("myFloodFill.png");
  animation.write("myFloodFill.gif");



  // @todo [Part 3]
  // - The code below assumes you have an Animation called `animation`
  // - The code provided below produces the `myFloodFill.png` file you must
  //   submit Part 3 of this assignment -- uncomment it when you're ready.

  /*
  PNG lastFrame = animation.getFrame( animation.frameCount() - 1 );
  lastFrame.writeToFile("myFloodFill.png");
  animation.write("myFloodFill.gif");
  */

  return 0;
}
