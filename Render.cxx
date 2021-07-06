#include <vtkContourFilter.h>
#include <vtkPolyDataNormals.h>
#include <vtkGenericCell.h>
#include <vtkPointSource.h>
#include <vtkPointOccupancyFilter.h>
#include <vtkMaskPointsFilter.h>
#include <vtkPointSet.h>
#include <vtkImageDataToPointSet.h>
#include <vtkImageThreshold.h>
#include <vtkIdTypeArray.h>
#include <vtkBox.h>
#include <vtkThreshold.h>
#include <vtkProbeFilter.h>
#include <vtkDoubleArray.h>
#include <vtkPointData.h>
#include <vtkImageAppend.h>
#include "Render.h"

Render::Render(){}
Render::Render(char *argv){
  //create volume reader
  std::cout << argv << endl;

  dims = new int[3];
  factor = 1;
  //readDataFromFile(argv, 64, 64, 93);
  readDataFromDir(argv, 64, 64, 93, 1);

  vtkNew<vtkNamedColors> colors;

  vtkNew<vtkPiecewiseFunction> opacityTransferFunction;
  opacityTransferFunction->AddPoint(499, 0.0);
  opacityTransferFunction->AddPoint(500, 1.);
  opacityTransferFunction->AddPoint(1150, 1.);
  opacityTransferFunction->AddPoint(1900, 1.);

  // Create transfer mapping scalar value to color
  vtkNew<vtkColorTransferFunction> colorTransferFunction;
  colorTransferFunction->AddRGBPoint(1900.0, colors->GetColor3d("ivory").GetData()[0],
                                            colors->GetColor3d("ivory").GetData()[1],
                                            colors->GetColor3d("ivory").GetData()[2]);
  colorTransferFunction->AddRGBPoint(1150, colors->GetColor3d("lblue").GetData()[0],
                                            colors->GetColor3d("blue").GetData()[1],
                                            colors->GetColor3d("blue").GetData()[2]);
  colorTransferFunction->AddRGBPoint(500.0, colors->GetColor3d("flesh").GetData()[0],
                                            colors->GetColor3d("flesh").GetData()[1],
                                            colors->GetColor3d("flesh").GetData()[2]);

  //Create volume property and adding color and opaccity transfer functions.
  volumeProperty = vtkSmartPointer<vtkVolumeProperty>::New();
  volumeProperty->SetColor(colorTransferFunction);
  volumeProperty->SetScalarOpacity(opacityTransferFunction);
  volumeProperty->ShadeOn();
  volumeProperty->SetInterpolationTypeToLinear();

  //Extract contour (surface representation) of the object withing the volume.
  vtkNew<vtkContourFilter> contourFilter;
  contourFilter->SetInputData(original);
  contourFilter->SetValue(0, 500);
  contourFilter->SetValue(1, 1150);
  contourFilter->Update();

  //Define normals of the surface representation
  vtkNew<vtkPolyDataNormals> normals;
  normals->SetInputConnection(contourFilter->GetOutputPort());
  normals->SetFeatureAngle(60.0);
  normals->Update();

  //We use the surface extracted to define an octree.
  octree = vtkSmartPointer<vtkOctreePointLocator>::New();
  octree->SetDataSet(normals->GetOutput());
  octree->SetMaxLevel(4);
  octree->BuildLocator();
  octreeRepresentation = vtkSmartPointer<vtkPolyData>::New();
  octree->GenerateRepresentation(3, octreeRepresentation);

  //vtkSmartPointer<vtkImageData> image = vtkSmartPointer<vtkImageData>::New();
  volumeMapper = vtkSmartPointer<vtkOpenGLGPUVolumeRayCastMapper>::New();

  polyMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
  polyMapper->SetInputConnection(normals->GetOutputPort());

  //volumeMapper->SetInputConnection(accumulator->GetOutputPort());
  //volumeMapper->SetInputConnection(shrink->GetOutputPort());
  volumeMapper->SetInputConnection(shrink->GetOutputPort());
  //volumeMapper->SetInputData(image);

  std::cout << volumeMapper->GetInformation() << endl;

  volume = vtkSmartPointer<vtkVolume>::New();
  volume->SetMapper(volumeMapper);
  volume->SetProperty(volumeProperty);
}

//Method to read data from a number of files, we define the directory path and
//the volume's extent.
void Render::readDataFromDir(char* path, int x_dim, int y_dim, int z_dim, int z_init){

  dims[0] = x_dim-1;
  dims[1] = y_dim-1;
  dims[2] = z_dim-z_init;

  vtkNew<vtkVolume16Reader> reader;
  reader->SetDataDimensions (x_dim, y_dim);
  reader->SetImageRange (z_init, z_dim);
  reader->SetFilePrefix("../headsq/quarter");
  reader->SetDataSpacing (3.2, 3.2, 1.5);

  shrink = vtkSmartPointer<vtkImageShrink3D>::New();
  shrink->SetInputConnection(reader->GetOutputPort());
  shrink->SetShrinkFactors(1, 1, 1);
  shrink->Update();

  original = reader->GetOutput();
}

//Read volume from file.
void Render::readDataFromFile(char* path, int x_dim, int y_dim, int z_dim){
  dims[0] = 511;
  dims[1] = 511;
  dims[2] = 511;

  vtkNew<vtkImageReader2> reader;
  reader->SetDataExtent(0, 1024, 0, 1024, 0, 1024);
  reader->SetDataByteOrderToLittleEndian();
  reader->SetFileName(path);
  reader->SetDataSpacing (1., 1., 1.);

  shrink = vtkSmartPointer<vtkImageShrink3D>::New();
  shrink->SetInputConnection(reader->GetOutputPort());
  shrink->SetShrinkFactors(1, 1, 1);
  shrink->Update();

  original = reader->GetOutput();
}

void Render::fixImage(){
  octreeRepresentation->ComputeBounds();
  double *bounds = octreeRepresentation->GetBounds();
  double minBounds[3] = {bounds[0], bounds[2], bounds[4]};
  double maxBounds[3] = {bounds[1] + bounds[0], bounds[3] + bounds[2], bounds[5] + bounds[4]};
  double minPoint[3];
  double maxPoint[3];

  shrink->GetOutput()->TransformPhysicalPointToContinuousIndex(minBounds, minPoint);
  shrink->GetOutput()->TransformPhysicalPointToContinuousIndex(maxBounds, maxPoint);

  vtkSmartPointer<vtkExtractVOI> voi = vtkSmartPointer<vtkExtractVOI>::New();
  voi->SetInputConnection(shrink->GetOutputPort());
  voi->SetVOI((int) minPoint[0], (int) (maxPoint[0] - minPoint[0] + 1), (int) minPoint[1], (int) (maxPoint[1] - minPoint[1] + 1), (int) minPoint[2], (int) (maxPoint[2] - minPoint[2]));
  voi->Update();

  shrink2->SetInputConnection(voi->GetOutputPort());
  shrink2->Update();
}

void Render::extractSelectedVOI(double bounds[6], bool localBounds){
  if(!localBounds){
    double minBounds[3] = {bounds[0], bounds[2], bounds[4]};
    double maxBounds[3] = {bounds[1], bounds[3], bounds[5]};
    double minPoint[3];
    double maxPoint[3];

    original->TransformPhysicalPointToContinuousIndex(minBounds, minPoint);
    original->TransformPhysicalPointToContinuousIndex(maxBounds, maxPoint);

    bounds[0] = minPoint[0];
    bounds[1] = maxPoint[0];
    bounds[2] = minPoint[1];
    bounds[3] = maxPoint[1];
    bounds[4] = minPoint[2];
    bounds[5] = maxPoint[2];

    std::cout << bounds[0] << " " << bounds[1] << " " << bounds[2] << " " << bounds[3] << " " << bounds[4] << " " << bounds[5] << endl;
  }
  volumeMapper->SetInputConnection(extractVOI(bounds)->GetOutputPort());
}

vtkSmartPointer<vtkExtractVOI> Render::extractVOI(double bounds[6]){
  vtkSmartPointer<vtkExtractVOI> voi = vtkSmartPointer<vtkExtractVOI>::New();
  voi->SetInputData(original);
  voi->SetVOI((int) bounds[0], (int) bounds[1], (int) bounds[2], (int) bounds[3], (int) bounds[4], (int) bounds[5]);
  voi->Update();

  return voi;
}

void Render::deleteOutsideRegion(){
  //std::cout << "Memory shrink prev: " << shrink2->GetOutput()->GetActualMemorySize() << endl;
  int nbuckets = octree->GetNumberOfLeafNodes();
  double bounds[6];
  vtkIdTypeArray* ids;

  std::cout << "NUM BUCKETS: " << nbuckets << endl;

  for(int i = 0; i < nbuckets; i++){
    int size = octree->GetPointsInRegion(i)->GetSize();
    if(size == 0){  //el octante está vacío
      octree->GetRegionDataBounds(i, bounds);
      fixDataBounds(bounds);
      clipImage(bounds);
    }
  }
}

void Render::fixDataBounds(double bounds[6]){
  double *l_bounds = shrink->GetOutput()->GetBounds();
  /*if(bounds[0] > bounds[1]){
    int aux = bounds[0];
    bounds[0] = bounds[1];
    bounds[1] = aux;
  }
  if(bounds[2] > bounds[3]){
    int aux = bounds[2];
    bounds[2] = bounds[3];
    bounds[3] = aux;
  }
  if(bounds[4] > bounds[5]){
    int aux = bounds[4];
    bounds[4] = bounds[5];
    bounds[5] = aux;
  }*/

  if(bounds[0] < l_bounds[0]) bounds[0] = l_bounds[0];
  if(bounds[1] < l_bounds[1]) bounds[1] = l_bounds[1];
  if(bounds[2] < l_bounds[2]) bounds[2] = l_bounds[2];
  if(bounds[3] > l_bounds[3]) bounds[3] = l_bounds[3];
  if(bounds[4] > l_bounds[4]) bounds[4] = l_bounds[4];
  if(bounds[5] > l_bounds[5]) bounds[5] = l_bounds[5];
}

void Render::clipImage(double bounds[6]){
  double minBound[3] = {bounds[0], bounds[2], bounds[4]};
  double maxBound[3] = {bounds[1], bounds[3], bounds[5]};
  double minPoint[3], maxPoint[3];

  shrink->GetOutput()->TransformPhysicalPointToContinuousIndex(minBound, minPoint);
  shrink->GetOutput()->TransformPhysicalPointToContinuousIndex(maxBound, maxPoint);
}


vtkSmartPointer<vtkImageShrink3D> Render::getImage(){
  return shrink2;
}

vtkSmartPointer<vtkOpenGLGPUVolumeRayCastMapper> Render::getVolumeMapper(){
  return volumeMapper;
}

vtkSmartPointer<vtkVolume> Render::getVolume(){
  return volume;
}

vtkSmartPointer<vtkPolyData> Render::getOctreeRepresentation(){
  return octreeRepresentation;
}

vtkSmartPointer<vtkPolyDataMapper> Render::getSurfaceMapper(){
  return polyMapper;
}
