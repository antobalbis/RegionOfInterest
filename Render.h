#include <vtkCamera.h>
#include <vtkColorTransferFunction.h>
#include <vtkVolume16Reader.h>
#include <vtkOpenGLGPUVolumeRayCastMapper.h>
#include <vtkNamedColors.h>
#include <vtkNew.h>
#include <vtkPiecewiseFunction.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>
#include <vtkStructuredPointsReader.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkVolume.h>
#include <vtkVolumeProperty.h>
#include <vtkExtractVOI.h>
#include <vtkCallbackCommand.h>
#include <vtkCommand.h>
#include <vtkRendererCollection.h>
#include <vtkVolumeCollection.h>
#include <vtkBoxWidget2.h>
#include <vtkBoxRepresentation.h>
#include <vtkTransform.h>
#include <vtkPolyData.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkOctreePointLocator.h>
#include <vtkImageData.h>
#include <vtkImageShrink3D.h>
#include <vtkImageReader2.h>
#include <vtkPolyDataMapper.h>
#include <vtkUnstructuredGrid.h>
#include <vtkThreshold.h>
#include <vtkClipVolume.h>
#include <vtkDICOMImageReader.h>
#include <vtkNrrdReader.h>

class Render{
  private:
    int factor;
    int dims[3];
    double spacing[3];
    vtkSmartPointer<vtkImageShrink3D> shrink;
    vtkSmartPointer<vtkImageShrink3D> shrink2;
    vtkSmartPointer<vtkVolumeProperty> volumeProperty;
    vtkSmartPointer<vtkOpenGLGPUVolumeRayCastMapper> volumeMapper;
    vtkSmartPointer<vtkVolume> volume;
    vtkSmartPointer<vtkOctreePointLocator> octree;
    vtkSmartPointer<vtkPolyData> octreeRepresentation;
    vtkSmartPointer<vtkPolyDataMapper> polyMapper;
    vtkSmartPointer<vtkImageData> original;
    vtkSmartPointer<vtkImageData> current;

    void readDataFromDir(char* path, int x_dim, int y_dim, int z_dim, int z_init);
    void readDataFromFile(char* path, int x_dim, int y_dim, int z_dim);
    void readDICOMImage(char* path, int x_dim, int y_dim, int z_dim, int z_init);
    void readNrrdImage(char* path);
    void deleteOutsideRegion();
    void fixImage();
    vtkSmartPointer<vtkExtractVOI> extractVOI(double bounds[6]);
    void extractCone();
    void extractSphere(double radius, double* center);

  public:
    Render();
    Render(char *argv, double spacing[3], int dims[3]);
	  void graphicPipeline();
	  void getRegionOfInterest(int bounds[6]);
    void fixDataBounds(double bounds[6]);
    void clipImage(double bounds[6]);
    void extractSelectedVOI(double bounds[6], bool localBounds);
    void extractFormedVOI(int type, double* bounds, double* center);
    void restart();
    vtkSmartPointer<vtkImageShrink3D> getImage();
    vtkSmartPointer<vtkOpenGLGPUVolumeRayCastMapper> getVolumeMapper();
    vtkSmartPointer<vtkVolume> getVolume();
    vtkSmartPointer<vtkPolyData> getOctreeRepresentation();
    vtkSmartPointer<vtkPolyDataMapper> getSurfaceMapper();
};
