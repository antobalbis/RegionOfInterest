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

void doBenchmark(Render render, double *bounds, int nTimes, char* path, bool version1, vtkSmartPointer<vtkRenderWindow> window){
  clock_t init;
  clock_t end;

  std::ofstream file;
  file.open(path);

  std::cout << bounds[0] << " " << bounds[1] << " " << bounds[2] << " " << bounds[3] << " " << bounds[4] << " " << bounds[5] << endl;

  for(int k = 0; k < 10; k++){
    if(k != 0) render.restart();
    init = clock();

    for(int i = 1; i < nTimes; i++){
      double bounds_[6] = {bounds[0], bounds[1]/(nTimes-(nTimes-i)), bounds[2], bounds[3]/(nTimes-(nTimes-i)), bounds[4], bounds[5]/(nTimes-(nTimes-i))};
      render.extractSelectedVOI(bounds_, version1);
      window->Render();
    }

    end = clock();
    file << double(end - init)/CLOCKS_PER_SEC << "\n";
  }
  file.close();
}

int main(int argc, char **argv){
  if(argc < 12){
    std::cout << "USAGE: ./benchmark <file_path> spacing[3] dimensions[3] nTimes isLocal output isFile";
    return -1;
  }

  vtkSmartPointer<vtkRenderer> renderer = vtkSmartPointer<vtkRenderer>::New();
  vtkSmartPointer<vtkRenderWindow> renWin = vtkSmartPointer<vtkRenderWindow>::New();
  vtkSmartPointer<vtkRenderWindowInteractor> renInt = vtkSmartPointer<vtkRenderWindowInteractor>::New();

  renWin->SetSize(600, 600);
  renWin->AddRenderer(renderer);
  renInt->SetRenderWindow(renWin);

  double spacing[3];
  int dimensions[3];
  double dims[6];
  std::cout << "READ DATA " << endl;

  spacing[0] = std::stod(argv[2]);
  spacing[1] = std::stod(argv[3]);
  spacing[2] = std::stod(argv[4]);

  dimensions[0] = std::stoi(argv[5]);
  dimensions[1] = std::stoi(argv[6]);
  dimensions[2] = std::stoi(argv[7]);

  bool isLocal = std::atoi(argv[9]) == 0;
  bool isFile = std::atoi(argv[11]) == 0;

  int nTimes = std::atoi(argv[8]);

  std::cout << "EVERYTHING IS OK" << endl;

  std::vector<double> in = {};
  std::vector<std::string> co = {};
  std::vector<double> op = {};

  Render render = Render(argv[1], spacing, dimensions, in, co, op, isFile);
  double *bounds = new double[6];
  if(isFile){
    dimensions[0] = render.getImage()->GetDimensions()[0];
    dimensions[1] = render.getImage()->GetDimensions()[1];
    dimensions[2] = render.getImage()->GetDimensions()[2];
  }

  if(isLocal){
    bounds[0] = 0;
    bounds[1] = dimensions[0] - 1.0;
    bounds[2] = 0;
    bounds[3] = dimensions[1] - 1.0;
    bounds[4] = 0;
    bounds[5] = dimensions[2] - 1.0;
    std::cout << bounds[1] << " " << bounds[3] << " " << bounds[5] << std::endl;
  }else{
    bounds = render.getOriginalBounds();
  }
  renderer->AddVolume(render.getVolume());
  renderer->GetActiveCamera()->Azimuth(45);
  renderer->GetActiveCamera()->Elevation(30);
  renderer->ResetCameraClippingRange();
  renderer->ResetCamera();

  renWin->Render();

  doBenchmark(render, bounds, nTimes, argv[10], isLocal, renWin);

  return EXIT_SUCCESS;
}
