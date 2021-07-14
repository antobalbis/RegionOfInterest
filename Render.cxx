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
#include <vtkImplicitDataSet.h>
#include <vtkPolyDataToImageStencil.h>
#include <vtkImageStencil.h>
#include <vtkImageThresholdConnectivity.h>
#include <vtkScalarTree.h>
#include <vtkProbeFilter.h>
#include <vtkSphere.h>
#include <vtkCutter.h>
#include "Render.h"

Render::Render(){}
Render::Render(char *argv, double spacing[3], int dims[3]){
  //create volume reader
  std::cout << argv << endl;

  this->dims[0] = dims[0];
  this->dims[1] = dims[1];
  this->dims[2] = dims[2];
  this->spacing[0] = spacing[0];
  this->spacing[1] = spacing[1];
  this->spacing[2] = spacing[2];

  this->factor = 1;

  //readDataFromFile(argv, 64, 64, 93);
  readDataFromDir(argv, dims[0], dims[1], dims[2], 1);
  //readDICOMImage(argv, dims[0], dims[1], dims[2], 1);
  //readNrrdImage(argv);

  vtkNew<vtkNamedColors> colors;

  vtkNew<vtkPiecewiseFunction> opacityTransferFunction;
  opacityTransferFunction->AddPoint(499, 0);
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
  contourFilter->SetInputConnection(shrink->GetOutputPort());
  contourFilter->SetValue(0, 500);
  contourFilter->SetValue(1, 1150);
  contourFilter->Update();

  //Define normals of the surface representation
  vtkNew<vtkPolyDataNormals> normals;
  normals->SetInputConnection(contourFilter->GetOutputPort());
  normals->SetFeatureAngle(60.0);
  normals->Update();

  /*vtkNew<vtkImageThreshold> threshold;
  threshold->SetInputData(original);
  threshold->ThresholdByUpper(500);
  threshold->SetOutputScalarTypeToUnsignedChar();
  threshold->ReplaceInOn();
  threshold->SetInValue(255);
  threshold->SetOutValue(0);
  threshold->ReplaceOutOn();
  threshold->Update();
  */

  //We use the surface extracted to define an octree.
  octree = vtkSmartPointer<vtkOctreePointLocator>::New();
  octree->SetDataSet(normals->GetOutput());
  octree->SetMaxLevel(4);
  octree->BuildLocator();
  octreeRepresentation = vtkSmartPointer<vtkPolyData>::New();
  octree->GenerateRepresentation(4, octreeRepresentation);

  vtkSmartPointer<vtkExtractVOI> voi = fixImage();
  std::cout << "IT'S OK???" << endl;

  vtkNew<vtkPolyDataToImageStencil> stencilImage;
  stencilImage->SetInputData(octreeRepresentation);
  stencilImage->SetOutputOrigin(voi->GetOutput()->GetOrigin());
  stencilImage->SetOutputSpacing(voi->GetOutput()->GetSpacing());
  stencilImage->SetOutputWholeExtent(voi->GetOutput()->GetExtent());
  stencilImage->Update();

  vtkNew<vtkImageStencil> stencil;
  stencil->SetInputConnection(shrink->GetOutputPort());
  stencil->SetStencilConnection(stencilImage->GetOutputPort());
  stencil->SetBackgroundValue(0);
  stencil->Update();

  original = stencil->GetOutput();
  current = stencil->GetOutput();

  std::cout << "STENCIL: " << stencil->GetOutput()->GetActualMemorySize() << endl;
  std::cout << "VOLUME: " << shrink->GetOutput()->GetActualMemorySize() << endl;

  //vtkSmartPointer<vtkImageData> image = vtkSmartPointer<vtkImageData>::New();
  volumeMapper = vtkSmartPointer<vtkOpenGLGPUVolumeRayCastMapper>::New();
  //volumeMapper->SetMaskTypeToBinary();
  //volumeMapper->SetMaskInput(threshold->GetOutput());
  //volumeMapper->SetInputConnection(accumulator->GetOutputPort());
  //volumeMapper->SetInputConnection(shrink->GetOutputPort());
  volumeMapper->SetInputConnection(stencil->GetOutputPort());
  //volumeMapper->SetPartitions(10,10,10);
  //volumeMapper->SetInputData(image);

  polyMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
  polyMapper->SetInputConnection(normals->GetOutputPort());

  std::cout << volumeMapper->GetInformation() << endl;

  volume = vtkSmartPointer<vtkVolume>::New();
  volume->SetMapper(volumeMapper);
  volume->SetProperty(volumeProperty);
}

//Method to read data from a number of files, we define the directory path and
//the volume's extent.
void Render::readDataFromDir(char* path, int x_dim, int y_dim, int z_dim, int z_init){

  vtkNew<vtkVolume16Reader> reader;
  reader->SetDataDimensions (dims[0], dims[1]);
  reader->SetImageRange (1, dims[2]);
  reader->SetFilePrefix(path);
  reader->SetDataSpacing (spacing[0], spacing[1], spacing[2]);

  shrink = vtkSmartPointer<vtkImageShrink3D>::New();
  shrink->SetInputConnection(reader->GetOutputPort());
  shrink->SetShrinkFactors(1, 1, 1);
  shrink->Update();
}

void Render::readDICOMImage(char* path, int x_dim, int y_dim, int z_dim, int z_init){

  vtkNew<vtkDICOMImageReader> reader;
  reader->SetFileName(path);
  reader->Update();

  shrink = vtkSmartPointer<vtkImageShrink3D>::New();
  shrink->SetInputConnection(reader->GetOutputPort());
  shrink->SetShrinkFactors(1, 1, 1);
  shrink->Update();
}

//Read volume from file.
void Render::readDataFromFile(char* path, int x_dim, int y_dim, int z_dim){
  dims[0] = 511;
  dims[1] = 511;
  dims[2] = 511;

  vtkNew<vtkImageReader2> reader;
  reader->SetDataExtent(0, dims[0], 0, dims[1], 0, dims[2]);
  reader->SetFileName(path);
  reader->SetDataSpacing (spacing[0], spacing[1], spacing[2]);

  shrink = vtkSmartPointer<vtkImageShrink3D>::New();
  shrink->SetInputConnection(reader->GetOutputPort());
  shrink->SetShrinkFactors(1, 1, 1);
  shrink->Update();
}

void Render::readNrrdImage(char *path){
  vtkNew<vtkNrrdReader> reader;
  reader->SetFileName(path);

  shrink = vtkSmartPointer<vtkImageShrink3D>::New();
  shrink->SetInputConnection(reader->GetOutputPort());
  shrink->SetShrinkFactors(1, 1, 1);
  shrink->Update();
}

vtkSmartPointer<vtkExtractVOI> Render::fixImage(){
  octreeRepresentation->ComputeBounds();
  double *bounds = octreeRepresentation->GetBounds();
  double *bounds_ = shrink->GetOutput()->GetBounds();

  if(bounds[0] < bounds_[0]) bounds[0] = bounds_[0];
  if(bounds[1] > bounds_[1]) bounds[1] = bounds_[1];
  if(bounds[2] < bounds_[2]) bounds[2] = bounds_[2];
  if(bounds[3] > bounds_[3]) bounds[3] = bounds_[3];
  if(bounds[4] < bounds_[4]) bounds[4] = bounds_[4];
  if(bounds[5] > bounds_[5]) bounds[5] = bounds_[5];

  double minBounds[3] = {bounds[0], bounds[2], bounds[4]};
  double maxBounds[3] = {bounds[1], bounds[3], bounds[5]};
  double minPoint[3];
  double maxPoint[3];

  shrink->GetOutput()->TransformPhysicalPointToContinuousIndex(minBounds, minPoint);
  shrink->GetOutput()->TransformPhysicalPointToContinuousIndex(maxBounds, maxPoint);

  double _bounds[6] = {minPoint[0], maxPoint[0], minPoint[1], maxPoint[1], minPoint[2], maxPoint[2]};

  return extractVOI(_bounds, shrink->GetOutput());
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
  vtkSmartPointer<vtkExtractVOI> voi = extractVOI(bounds, current);
  current = voi->GetOutput();
  volumeMapper->SetInputConnection(voi->GetOutputPort());
}

vtkSmartPointer<vtkExtractVOI> Render::extractVOI(double bounds[6], vtkSmartPointer<vtkImageData> dataSet){
  vtkSmartPointer<vtkExtractVOI> voi = vtkSmartPointer<vtkExtractVOI>::New();
  voi->SetInputData(dataSet);
  voi->SetVOI((int) bounds[0], (int) bounds[1], (int) bounds[2], (int) bounds[3], (int) bounds[4], (int) bounds[5]);
  voi->Update();

  return voi;
}

/*void Render::deleteOutsideRegion(){
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
}*/

void Render::extractFormedVOI(int type, double* bounds, double* center){
  extractSphere(bounds[0], center);
}

void Render::extractSphere(double radius, double *center){
  vtkNew<vtkSphere> sphere;
  sphere->SetCenter(center);
  sphere->SetRadius(radius);

  vtkNew<vtkCutter> clipVolume;
  clipVolume->SetCutFunction(sphere);
  clipVolume->SetInputData(current);
  clipVolume->Update();

  vtkNew<vtkPolyDataToImageStencil> stencilImage;
  stencilImage->SetInputData(clipVolume->GetOutput());
  stencilImage->SetOutputOrigin(current->GetOrigin());
  stencilImage->SetOutputSpacing(current->GetSpacing());
  stencilImage->SetOutputWholeExtent(current->GetExtent());
  stencilImage->Update();

  vtkNew<vtkImageStencil> stencil;
  stencil->SetInputData(current);
  stencil->SetStencilConnection(stencilImage->GetOutputPort());
  stencil->SetBackgroundValue(0);
  stencil->Update();

  current = stencil->GetOutput();
  volumeMapper->SetInputConnection(stencil->GetOutputPort());
}

void Render::restart(){
  current = original;

  shrink->SetInputData(original);
  shrink->Update();

  volumeMapper->SetInputConnection(shrink->GetOutputPort());
}

vtkSmartPointer<vtkImageData> Render::getImage(){
  return current;
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
