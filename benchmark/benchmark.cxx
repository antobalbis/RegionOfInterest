#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkNamedColors.h>
#include <vtkNew.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkSphereSource.h>
#include <vtkVersion.h>
#include <ctime>
#include <iostream>
#include <fstream>
#include "Render.h"

int main(int argc, char **argv){
  if(argc < 8){
    std::cout << "USAGE: ./benchmark <file_path> spacing[3] dimensions[3]";
    return -1;
  }

  std::ofstream fileA;
  fileA.open("../times/n_octree.txt");

  std::ofstream fileB;
  fileB.open("../times/y_octree.txt");

  vtkSmartPointer<vtkRenderer> renderer = vtkSmartPointer<vtkRenderer>::New();
  vtkSmartPointer<vtkRenderWindow> renWin = vtkSmartPointer<vtkRenderWindow>::New();
  vtkSmartPointer<vtkRenderWindowInteractor> renInt = vtkSmartPointer<vtkRenderWindowInteractor>::New();

  renWin->SetSize(600, 600);
  renWin->AddRenderer(renderer);
  renInt->SetRenderWindow(renWin);

  double spacing[3];
  int dimensions[3];

  std::cout << "READ DATA " << endl;

  spacing[0] = std::stod(argv[2]);
  spacing[1] = std::stod(argv[3]);
  spacing[2] = std::stod(argv[4]);

  dimensions[0] = std::atoi(argv[5]);
  dimensions[1] = std::atoi(argv[6]);
  dimensions[2] = std::atoi(argv[7]);

  std::cout << "EVERYTHING IS OK" << endl;

  Render render = Render(argv[1], spacing, dimensions);

  renderer->AddVolume(render.getVolume());
  renderer->GetActiveCamera()->Azimuth(45);
  renderer->GetActiveCamera()->Elevation(30);
  renderer->ResetCameraClippingRange();
  renderer->ResetCamera();

  renWin->Render();

  clock_t init;
  clock_t end;

  for(int k = 0; k < 10; k++){
    if(k != 0) render.restart();
    init = clock();

    for(int j = 0; j < 100; j++){
      for(int i = 1; i <= 10; i++){
        double bounds[6] = {0, 64/i-1, 0, 64/i-1, 0, 93/i-1};
        render.extractSelectedVOI(bounds, true);
        renWin->Render();
      }
    }

    end = clock();
    fileA << double(end - init)/CLOCKS_PER_SEC << "\n";

    render.restart();
    double *new_bounds = render.getImage()->GetBounds();

    init = clock();
    for(int j = 0; j < 100; j++){
      for(int i = 1; i <= 10; i++){
        double selected[6] = {new_bounds[0], new_bounds[1]/i, new_bounds[2], new_bounds[3]/i, new_bounds[4], new_bounds[5]/i};
        render.extractSelectedVOI(selected, false);
        renWin->Render();
      }
    }
    end = clock();
    fileB << double(end - init)/CLOCKS_PER_SEC << "\n";
  }

  fileA.close();
  fileB.close();

  return EXIT_SUCCESS;
}
