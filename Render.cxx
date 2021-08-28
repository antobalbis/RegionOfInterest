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
Render::Render(char *argv, double spacing[3], int dims[3], std::vector<double> intensities,
  std::vector<std::string> colores, std::vector<double> opacities){
  //create volume reader
  std::cout << argv << endl;

  this->dims[0] = dims[0];
  this->dims[1] = dims[1];
  this->dims[2] = dims[2];
  this->spacing[0] = spacing[0];
  this->spacing[1] = spacing[1];
  this->spacing[2] = spacing[2];
  this->level = 0;
  this->factor = 1;

  //readDataFromFile(argv, 64, 64, 93);
  readDataFromDir(argv, dims[0], dims[1], dims[2], 1);
  //readDICOMImage(argv, dims[0], dims[1], dims[2], 1);
  //readNrrdImage(argv);

  vtkNew<vtkNamedColors> colors;

  vtkNew<vtkPiecewiseFunction> opacityTransferFunction;
  /*opacityTransferFunction->AddPoint(499, 0);
  opacityTransferFunction->AddPoint(500, 1.);
  opacityTransferFunction->AddPoint(1150, 1.);
  opacityTransferFunction->AddPoint(1900, 1.);*/

  // Create transfer mapping scalar value to color
  vtkNew<vtkColorTransferFunction> colorTransferFunction;
  /*colorTransferFunction->AddRGBPoint(1900.0, colors->GetColor3d("ivory").GetData()[0],
                                            colors->GetColor3d("ivory").GetData()[1],
                                            colors->GetColor3d("ivory").GetData()[2]);
  colorTransferFunction->AddRGBPoint(1150, colors->GetColor3d("lblue").GetData()[0],
                                            colors->GetColor3d("blue").GetData()[1],
                                            colors->GetColor3d("blue").GetData()[2]);
  colorTransferFunction->AddRGBPoint(500.0, colors->GetColor3d("flesh").GetData()[0],
                                            colors->GetColor3d("flesh").GetData()[1],
                                            colors->GetColor3d("flesh").GetData()[2]);*/

  for(int i = 0; i < intensities.size(); i++){
    opacityTransferFunction->AddPoint(intensities.at(i), opacities.at(i));
    colorTransferFunction->AddRGBPoint(intensities.at(i), colors->GetColor3d(colores.at(i))[0],
                                                          colors->GetColor3d(colores.at(i))[1],
                                                          colors->GetColor3d(colores.at(i))[2]);
  }

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

  //We use the surface extracted to define an octree.
  octree = vtkSmartPointer<vtkOctreePointLocator>::New();
  octree->SetDataSet(shrink->GetOutput());
  octree->SetMaxLevel(4);
  octree->BuildLocator();
  octreeRepresentation = vtkSmartPointer<vtkPolyData>::New();
  octree->GenerateRepresentation(4, octreeRepresentation);

  double *octreeBounds = octree->GetBounds();
  root = vtkSmartPointer<vtkOctreePointLocatorNode>::New();
  root->SetBounds(octreeBounds);
  node = root;
  createOctreeNodes(node, 0, 4);

  //vtkSmartPointer<vtkExtractVOI> voi = fixImage();

  //original = shrink->GetOutput();
  //current = shrink->GetOutput();

  //vtkSmartPointer<vtkImageData> image = vtkSmartPointer<vtkImageData>::New();
  volumeMapper = vtkSmartPointer<vtkOpenGLGPUVolumeRayCastMapper>::New();
  volumeMapper->SetMaskTypeToBinary();
  volumeMapper->SetMaskInput(nullptr);
  //volumeMapper->SetMaskInput(threshold->GetOutput());
  //volumeMapper->SetInputConnection(accumulator->GetOutputPort());
  volumeMapper->SetInputData(current);
  //volumeMapper->SetInputConnection(voi->GetOutputPort());
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

  original = reader->GetOutput();

  /*std::cout << "SIZE = " << shrink->GetOutput()->GetActualMemorySize() << endl;
  factor = shrink->GetOutput()->GetActualMemorySize() / maxsize + 1;

  shrink->SetShrinkFactors(factor, factor, factor);
  shrink->Update();*/

  current = shrink->GetOutput();
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

/*vtkSmartPointer<vtkExtractVOI> Render::fixImage(){
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
}*/

void Render::extractSelectedVOI(double bounds[6], bool localBounds){
  double *_bounds = new double[6];
  double *bounds_ = new double[6];

  if(!localBounds){
    original->GetBounds(_bounds);
    if(bounds[0] < _bounds[0]) bounds[0] = _bounds[0];
    if(bounds[1] > _bounds[1]) bounds[1] = _bounds[1];
    if(bounds[2] < _bounds[2]) bounds[2] = _bounds[2];
    if(bounds[3] > _bounds[3]) bounds[3] = _bounds[3];
    if(bounds[4] < _bounds[4]) bounds[4] = _bounds[4];
    if(bounds[5] > _bounds[5]) bounds[5] = _bounds[5];

    vtkSmartPointer<vtkOctreePointLocatorNode> node_ = getOctreeBounds(bounds, root, 0);
    bool same = compareNodes(node_);
    std::cout << "LEVEL = " << level << endl;

    if(level == 0){
      double min_point[3] = {bounds[0], bounds[2], bounds[4]};
      double max_point[3] = {bounds[1], bounds[3], bounds[5]};

      vtkSmartPointer<vtkOctreePointLocatorNode> min_node = findNodeFromPoint(min_point);
      vtkSmartPointer<vtkOctreePointLocatorNode> max_node = findNodeFromPoint(max_point);

      bounds_[0] = min_node->GetMinBounds()[0];
      bounds_[2] = min_node->GetMinBounds()[1];
      bounds_[4] = min_node->GetMinBounds()[2];
      bounds_[1] = max_node->GetMaxBounds()[0];
      bounds_[3] = max_node->GetMaxBounds()[1];
      bounds_[5] = max_node->GetMaxBounds()[2];

      bounds_ = getLocalBounds(bounds_);

      vtkSmartPointer<vtkImageShrink3D> voi = extractVOI(bounds_, original);
      current = voi->GetOutput();
      volumeMapper->SetInputConnection(voi->GetOutputPort());

    }else if(!same){
      std::cout << "IS NOT THE SAME" << endl;
      bounds_[0] = node_->GetMinBounds()[0];
      bounds_[2] = node_->GetMinBounds()[1];
      bounds_[4] = node_->GetMinBounds()[2];
      bounds_[1] = node_->GetMaxBounds()[0];
      bounds_[3] = node_->GetMaxBounds()[1];
      bounds_[5] = node_->GetMaxBounds()[2];

      //std::cout << "OCTREE NODE BOUNDS: " << endl;
      std::cout << _bounds[0] << " " << _bounds[1] << " " << _bounds[2] << " " << _bounds[3] << " " << _bounds[4] << " " << _bounds[5] << endl;
      bounds_ = getLocalBounds(bounds_);

      vtkSmartPointer<vtkImageShrink3D> voi = extractVOI(bounds_, original);
      current = voi->GetOutput();
      volumeMapper->SetInputConnection(voi->GetOutputPort());
    }

    std::cout << bounds[0] << " " << bounds[1] << " " << bounds[2] << " " << bounds[3] << " " << bounds[4] << " " << bounds[5] << endl;

    vtkSmartPointer<vtkBox> box = vtkSmartPointer<vtkBox>::New();
    box->SetBounds(bounds);
    doExtraction(box, bounds, 0);

  }else{
    vtkSmartPointer<vtkImageShrink3D> voi = extractVOI(bounds, original);
    current = voi->GetOutput();
    volumeMapper->SetInputConnection(voi->GetOutputPort());
  }
}

vtkSmartPointer<vtkImageShrink3D> Render::extractVOI(double bounds[6], vtkSmartPointer<vtkImageData> dataSet){
  vtkSmartPointer<vtkExtractVOI> voi = vtkSmartPointer<vtkExtractVOI>::New();
  voi->SetInputData(dataSet);
  voi->SetVOI((int) bounds[0], (int) bounds[1], (int) bounds[2], (int) bounds[3], (int) bounds[4], (int) bounds[5]);
  voi->Update();

  factor = voi->GetOutput()->GetActualMemorySize() / maxsize + 1;
  vtkSmartPointer<vtkImageShrink3D> shrink3d = vtkSmartPointer<vtkImageShrink3D>::New();
  shrink3d->SetInputConnection(voi->GetOutputPort());
  shrink3d->SetShrinkFactors(factor, factor, factor);

  return shrink3d;
}

void Render::extractFormedVOI(int type, double* bounds, double* center, double radius, vtkSmartPointer<vtkAbstractTransform> transform){
  double* _bounds = new double[6];

  _bounds[0] = bounds[0];
  _bounds[1] = bounds[1];
  _bounds[2] = bounds[2];
  _bounds[3] = bounds[3];
  _bounds[4] = bounds[4];
  _bounds[5] = bounds[5];

  //extractSelectedVOI(_bounds, false);
  extractSphere(radius, center, getLocalBounds(bounds));
}

void Render::extractSphere(double radius, double *center, double *bounds){
  vtkSmartPointer<vtkSphere> sphere = vtkSmartPointer<vtkSphere>::New();
  sphere->SetCenter(center);
  sphere->SetRadius(radius);

  doExtraction(sphere, bounds, 1);
}

void Render::extractTransformedBox(double *bounds, double *center, vtkSmartPointer<vtkAbstractTransform> transform){

}

void Render::doExtraction(vtkSmartPointer<vtkImplicitFunction> function, double *bounds, int type){
  if(type == 0){
    volumeMapper->SetMaskInput(nullptr);
    volumeMapper->SetCropping(true);
    volumeMapper->SetCroppingRegionPlanes(bounds);
  }else{
    vtkNew<vtkCutter> clipVolume;
    clipVolume->SetCutFunction(function);
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

    //volumeMapper->SetCropping(false);
    //current = stencil->GetOutput();
    volumeMapper->SetInputData(stencil->GetOutput());
  }

}

void Render::restart(){
  current = original;
  node = root;

  shrink->SetInputData(original);
  shrink->Update();

  volumeMapper->SetInputConnection(shrink->GetOutputPort());
  volumeMapper->SetCropping(false);
  volumeMapper->SetMaskInput(nullptr);
}

double *Render::getLocalBounds(double *bounds){
  double *_bounds = new double[6];
  double *bounds_local = original->GetBounds();

  if(bounds[0] < bounds_local[0]) bounds[0] = bounds_local[0];
  if(bounds[2] < bounds_local[2]) bounds[2] = bounds_local[2];
  if(bounds[4] < bounds_local[4]) bounds[4] = bounds_local[4];
  if(bounds[1] > bounds_local[1]) bounds[1] = bounds_local[1];
  if(bounds[3] > bounds_local[3]) bounds[3] = bounds_local[3];
  if(bounds[5] > bounds_local[5]) bounds[5] = bounds_local[5];

  double minBounds[3] = {bounds[0], bounds[2], bounds[4]};
  double maxBounds[3] = {bounds[1], bounds[3], bounds[5]};
  double minPoint[3];
  double maxPoint[3];

  original->TransformPhysicalPointToContinuousIndex(minBounds, minPoint);
  original->TransformPhysicalPointToContinuousIndex(maxBounds, maxPoint);

  _bounds[0] = minPoint[0];
  _bounds[1] = maxPoint[0];
  _bounds[2] = minPoint[1];
  _bounds[3] = maxPoint[1];
  _bounds[4] = minPoint[2];
  _bounds[5] = maxPoint[2];

  return _bounds;
}

vtkSmartPointer<vtkOctreePointLocatorNode> Render::getOctreeBounds(double *bounds, vtkSmartPointer<vtkOctreePointLocatorNode> node, int level){
    double points[2][3];
    double *_bounds;
    bool inside = true;

    points[0][0] = bounds[0];
    points[0][1] = bounds[2];
    points[0][2] = bounds[4];
    points[1][0] = bounds[1];
    points[1][1] = bounds[3];
    points[1][2] = bounds[5];

    inside = node->ContainsPoint(points[0][0], points[0][1], points[0][2], 0) &&
      node->ContainsPoint(points[1][0], points[1][1], points[1][2], 0);

    if(inside){
      std::cout << "IS INSIDE" << endl;
      if(level == 4){
        this->level = level;
        return node;
      }else{
        for(int i = 0; i < 8; i++){
          if (getOctreeBounds(bounds, node->GetChild(i), level + 1) != nullptr){
            return node->GetChild(i);
          }
        }
        this->level = level;
        return node;
      }
    }else{
      return nullptr;
    }

}

void Render::createOctreeNodes(vtkSmartPointer<vtkOctreePointLocatorNode> node, int level, int maxLevel){
  if(level < maxLevel){
    node->CreateChildNodes();
    level++;

    double *bounds = node->GetMinBounds();
    //std::cout << bounds[0] << " " << bounds[1] << " " << bounds[2];

    for(int i = 0; i < 8; i++){
      createOctreeNodes(node->GetChild(i), level, maxLevel);
    }
  }
}

vtkSmartPointer<vtkOctreePointLocatorNode> Render::findNodeFromPoint(double point[3]){
  vtkSmartPointer<vtkOctreePointLocatorNode> node_;
  for(int i = 0; i < 8; i++){
    for(int j = 0; j < 8; j++){
      node_ = root->GetChild(i)->GetChild(j);
      if(node_->ContainsPoint(point[0], point[1], point[2], 0)){
        return node_;
      }
    }
  }
  return root;
}

bool Render::compareNodes(vtkSmartPointer<vtkOctreePointLocatorNode> node_){
  if(node_ == node){
    return true;
  }else{
    node = node_;
    return false;
  }
}

void Render::cropImageFromPlane(){

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
