#include <vtkContourFilter.h>
#include <vtkImageReader.h>
#include <vtkMetaImageReader.h>
#include <vtkPolyDataNormals.h>
#include <vtkGenericCell.h>
#include <vtkPointSource.h>
#include <vtkPointOccupancyFilter.h>
#include <vtkMaskPointsFilter.h>
#include <vtkPointSet.h>
#include <vtkImplicitFunctionToImageStencil.h>
#include <vtkImageDataToPointSet.h>
#include <vtkImageThreshold.h>
#include <vtkIdTypeArray.h>
#include <vtkBox.h>
#include <vtkThreshold.h>
#include <vtkPlanes.h>
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
#include <QDoubleValidator>
#include "Render.h"

Render::Render(){}
Render::Render(std::string path, double spacing[3], int dims[3], std::vector<double> intensities,
  std::vector<std::string> colores, std::vector<double> opacities, bool file){
  //create volume reader
  std::cout << path << endl;

  this->dims[0] = dims[0];
  this->dims[1] = dims[1];
  this->dims[2] = dims[2];
  this->spacing[0] = spacing[0];
  this->spacing[1] = spacing[1];
  this->spacing[2] = spacing[2];
  this->level = 0;
  this->factor = 1;

  if(file){
    if(path.find(".raw") != std::string::npos)
      readDataFromFile(path.c_str(), dims[0], dims[1], dims[2]);
    else if(path.find(".nrrd") != std::string::npos)
      readNrrdImage(path.c_str());
    else readDataFromHeader(path.c_str());
  }else readDataFromDir(path.c_str(), dims[0], dims[1], dims[2], 1);

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

  if(intensities.size() > 0){
    //Extract contour (surface representation) of the object withing the volume.
    vtkNew<vtkContourFilter> contourFilter;
    contourFilter->SetInputData(current->GetOutput());
    contourFilter->SetValue(0, intensities[0]);
    contourFilter->SetValue(1, intensities[1]);
    contourFilter->Update();

    //Define normals of the surface representation
    vtkNew<vtkPolyDataNormals> normals;
    normals->SetInputConnection(contourFilter->GetOutputPort());
    normals->SetFeatureAngle(60.0);
    normals->Update();

    polyMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    polyMapper->SetInputConnection(normals->GetOutputPort());
  }

  root = vtkSmartPointer<vtkOctreePointLocatorNode>::New();
  double *node_bounds = new double[6];
  original->GetBounds(node_bounds);
  //Añadimos pequeño margen
  node_bounds[0] = node_bounds[0] - 0.001;
  node_bounds[2] = node_bounds[2] - 0.001;
  node_bounds[4] = node_bounds[4] - 0.001;
  node_bounds[1] = node_bounds[1] + 0.001;
  node_bounds[3] = node_bounds[3] + 0.001;
  node_bounds[5] = node_bounds[5] + 0.001;

  root->SetBounds(node_bounds);
  node = root;
  selected[0] = root;
  selected[1] = root;

  createOctreeNodes(node, 0, 4);

  std::cout << "SENDING VOLUME TO MAPPER " << endl;
  //vtkSmartPointer<vtkImageData> image = vtkSmartPointer<vtkImageData>::New();
  volumeMapper = vtkSmartPointer<vtkOpenGLGPUVolumeRayCastMapper>::New();
  volumeMapper->SetInputConnection(current->GetOutputPort());
  volumeMapper->SetMaskTypeToBinary();

  std::cout << volumeMapper->GetInformation() << endl;

  volume = vtkSmartPointer<vtkVolume>::New();
  volume->SetMapper(volumeMapper);
  volume->SetProperty(volumeProperty);
}

//Method to read data from a number of files, we define the directory path and
//the volume's extent.
void Render::readDataFromDir(const char* path, int x_dim, int y_dim, int z_dim, int z_init){
  vtkNew<vtkVolume16Reader> reader;
  reader->SetDataDimensions (dims[0], dims[1]);
  reader->SetImageRange (1, dims[2]);
  reader->SetFilePrefix(path);
  //reader->SetFilePattern(".dmc");
  reader->SetDataSpacing (spacing[0], spacing[1], spacing[2]);
  reader->Update();

  original = reader->GetOutput();
  current = vtkSmartPointer<vtkImageShrink3D>::New();
  current->SetInputConnection(reader->GetOutputPort());
  int factor = original->GetActualMemorySize()/(512*1024) + 1;
  current->SetShrinkFactors(factor, factor, factor);
  current->Update();
}

void Render::readDICOMImage(const char* path, int x_dim, int y_dim, int z_dim, int z_init){
  vtkNew<vtkDICOMImageReader> reader;
  reader->SetFileName(path);
  reader->Update();

  original = reader->GetOutput();
  current = vtkSmartPointer<vtkImageShrink3D>::New();
  current->SetInputConnection(reader->GetOutputPort());
  factor = original->GetActualMemorySize()/(512*1024) + 1;
  current->SetShrinkFactors(factor, factor, factor);
  current->Update();
}

//Read volume from file.
void Render::readDataFromFile(const char* path, int x_dim, int y_dim, int z_dim){
  vtkNew<vtkImageReader2> reader;
  reader->SetFileDimensionality(3);
  reader->SetDataExtent(0, dims[0], 0, dims[1], 0, dims[2]);
  reader->SetFileName(path);
  reader->SetDataSpacing (spacing[0], spacing[1], spacing[2]);
  reader->SetDataScalarTypeToUnsignedShort();
  reader->SetDataByteOrderToBigEndian();
  reader->Update();

  original = reader->GetOutput();
  current = vtkSmartPointer<vtkImageShrink3D>::New();
  current->SetInputConnection(reader->GetOutputPort());
  //factor = original->GetActualMemorySize()/(512*1024) + 1;
  factor = 1;
  current->SetShrinkFactors(factor, factor, factor);
  current->Update();
}

void Render::readDataFromHeader(const char* path){
  vtkNew<vtkMetaImageReader> reader;
  reader->SetFileName(path);
  reader->SetDataByteOrderToBigEndian();
  reader->Update();

  original = reader->GetOutput();

  this->dims[0] = original->GetExtent()[0];
  this->dims[1] = original->GetExtent()[1];
  this->dims[2] = original->GetExtent()[2];

  this->spacing[0] = original->GetSpacing()[0];
  this->spacing[1] = original->GetSpacing()[1];
  this->spacing[2] = original->GetSpacing()[2];

  current = vtkSmartPointer<vtkImageShrink3D>::New();
  current->SetInputConnection(reader->GetOutputPort());
  //factor = original->GetActualMemorySize()/(512*1024) + 1;
  factor = 1;
  current->SetShrinkFactors(factor, factor, factor);
  current->Update();
}

void Render::readNrrdImage(const char *path){
  vtkNew<vtkNrrdReader> reader;
  reader->SetFileName(path);
  reader->Update();

  original = reader->GetOutput();

  original = reader->GetOutput();
  current = vtkSmartPointer<vtkImageShrink3D>::New();
  current->SetInputConnection(reader->GetOutputPort());
  factor = original->GetActualMemorySize()/(512*1024) + 1;
  current->SetShrinkFactors(factor, factor, factor);
  current->Update();
}


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

    std::cout << bounds[0] << " " << bounds[1] << " " << bounds[2] << " " << bounds[3] << " " << bounds[4] << " " << bounds[5] << endl;


    vtkSmartPointer<vtkOctreePointLocatorNode> node_ = getOctreeBounds(bounds, root, 0);
    bool same = compareNodes(node_);
    std::cout << "LEVEL = " << level << endl;

    /*if(level == 0){
      double min_point[3] = {bounds[0], bounds[2], bounds[4]};
      double max_point[3] = {bounds[1], bounds[3], bounds[5]};

      vtkSmartPointer<vtkOctreePointLocatorNode> min_node = findNodeFromPoint(min_point);
      vtkSmartPointer<vtkOctreePointLocatorNode> max_node = findNodeFromPoint(max_point);

      if(min_node != selected[0] || max_node != selected[1]){
        std::cout << "cambiamos nodos" << endl;
        selected[0] = min_node;
        selected[1] = max_node;

        bounds_[0] = min_node->GetMinBounds()[0];
        bounds_[2] = min_node->GetMinBounds()[1];
        bounds_[4] = min_node->GetMinBounds()[2];
        bounds_[1] = max_node->GetMaxBounds()[0];
        bounds_[3] = max_node->GetMaxBounds()[1];
        bounds_[5] = max_node->GetMaxBounds()[2];

        bounds_ = getLocalBounds(bounds_);

        vtkSmartPointer<vtkExtractVOI> voi = extractVOI(bounds_, original);
        current->SetInputConnection(voi->GetOutputPort());
        current->Update();
        //volumeMapper->SetInputConnection(voi->GetOutputPort());
      }

    }else*/ if(!same){
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

      vtkSmartPointer<vtkExtractVOI> voi = extractVOI(bounds_, original);
      current->SetInputConnection(voi->GetOutputPort());
      current->Update();
      //volumeMapper->SetInputConnection(voi->GetOutputPort());
    }

    std::cout << bounds[0] << " " << bounds[1] << " " << bounds[2] << " " << bounds[3] << " " << bounds[4] << " " << bounds[5] << endl;

    vtkSmartPointer<vtkBox> box = vtkSmartPointer<vtkBox>::New();
    box->SetBounds(bounds);
    doExtraction(box, bounds, 0);

  }else{
    vtkSmartPointer<vtkExtractVOI> voi = extractVOI(bounds, original);
    current->SetInputConnection(voi->GetOutputPort());
    current->Update();
    //volumeMapper->SetInputConnection(voi->GetOutputPort());
  }
}

vtkSmartPointer<vtkExtractVOI> Render::extractVOI(double bounds[6], vtkSmartPointer<vtkImageData> dataSet){
  vtkSmartPointer<vtkExtractVOI> voi = vtkSmartPointer<vtkExtractVOI>::New();
  voi->SetInputData(dataSet);
  voi->SetVOI((int) bounds[0], (int) bounds[1], (int) bounds[2], (int) bounds[3], (int) bounds[4], (int) bounds[5]);
  voi->Update();

  return voi;
}

void Render::extractFormedVOI(int type, double* bounds, double* center, double radius, vtkSmartPointer<vtkPlanes> transform){
  extractSelectedVOI(bounds, false);
  if(type == 0) extractSphere(radius, center, getLocalBounds(bounds));
  else extractTransformedBox(bounds, center, transform);
}

void Render::extractSphere(double radius, double *center, double *bounds){
  vtkSmartPointer<vtkSphere> sphere = vtkSmartPointer<vtkSphere>::New();
  sphere->SetCenter(center);
  sphere->SetRadius(radius);

  std::cout << "center: " << center << " radius: " << radius << endl;

  doExtraction(sphere, bounds, 1);
}

void Render::extractTransformedBox(double *bounds, double *center, vtkSmartPointer<vtkPlanes> planes){
  doExtraction(planes, nullptr, 1);
}

void Render::doExtraction(vtkSmartPointer<vtkImplicitFunction> function, double *bounds, int type){
  if(type == 0){
    volumeMapper->SetCropping(true);
    volumeMapper->SetCroppingRegionPlanes(bounds);
  }else{
    vtkNew<vtkImplicitFunctionToImageStencil> stencilImage;
    stencilImage->SetInput(function);
    stencilImage->SetOutputOrigin(current->GetOutput()->GetOrigin());
    stencilImage->SetOutputSpacing(current->GetOutput()->GetSpacing());
    stencilImage->SetOutputWholeExtent(current->GetOutput()->GetExtent());
    stencilImage->Update();

    vtkNew<vtkImageStencil> stencil;
    stencil->SetInputData(current->GetOutput());
    stencil->SetStencilConnection(stencilImage->GetOutputPort());
    stencil->SetBackgroundValue(0);
    stencil->Update();

    volumeMapper->SetMaskInput(stencil->GetOutput());
  }

}

void Render::restart(){
  current->SetInputData(original);
  current->Update();

  node = root;
  selected[0] = root;
  selected[0] = root;

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

    //double *bounds = node->GetMinBounds();
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

void Render::addFunctionValue(double intensity, std::string color, double opacity){
  vtkNew<vtkNamedColors> colors;
  volumeProperty->GetScalarOpacity()->AddPoint(intensity, opacity);
  volumeProperty->GetRGBTransferFunction()->AddRGBPoint(intensity, colors->GetColor3d(color)[0],
                                                        colors->GetColor3d(color)[1],
                                                        colors->GetColor3d(color)[2]);

  volume->Update();
}

void Render::removeFunctionValue(double intensity){
    volumeProperty->GetScalarOpacity()->RemovePoint(intensity);
    volumeProperty->GetRGBTransferFunction()->RemovePoint(intensity);
    volume->Update();
}

void Render::refactor(int factor){
  current->SetShrinkFactors(factor, factor, factor);
  current->Update();
  //volumeMapper->SetInputConnection(shrink2->GetOutputPort());
}

vtkSmartPointer<vtkImageData> Render::getImage(){
  return current->GetOutput();
}

double *Render::getOriginalBounds(){
  return original->GetBounds();
}

vtkSmartPointer<vtkOpenGLGPUVolumeRayCastMapper> Render::getVolumeMapper(){
  return volumeMapper;
}

vtkSmartPointer<vtkVolume> Render::getVolume(){
  return volume;
}

vtkSmartPointer<vtkPolyDataMapper> Render::getSurfaceMapper(){
  return polyMapper;
}
