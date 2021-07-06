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

class Render{
  private:
    int factor;
    int* dims;
    vtkSmartPointer<vtkImageShrink3D> shrink;
    vtkSmartPointer<vtkImageShrink3D> shrink2;
    vtkSmartPointer<vtkUnstructuredGrid> grid;
    vtkSmartPointer<vtkClipVolume> clipper;
    vtkSmartPointer<vtkVolumeProperty> volumeProperty;
    vtkSmartPointer<vtkOpenGLGPUVolumeRayCastMapper> volumeMapper;
    vtkSmartPointer<vtkVolume> volume;
    vtkSmartPointer<vtkOctreePointLocator> octree;
    vtkSmartPointer<vtkPolyData> octreeRepresentation;
    vtkSmartPointer<vtkPolyDataMapper> polyMapper;
    vtkSmartPointer<vtkImageData> original;

    void readDataFromDir(char* path, int x_dim, int y_dim, int z_dim, int z_init);
    void readDataFromFile(char* path, int x_dim, int y_dim, int z_dim);
    void deleteOutsideRegion();
    void fixImage();
    vtkSmartPointer<vtkExtractVOI> extractVOI(double bounds[6]);

  public:
    Render();
    Render(char *argv);
	  void graphicPipeline();
	  void getRegionOfInterest(int bounds[6]);
    void fixDataBounds(double bounds[6]);
    void clipImage(double bounds[6]);
    void extractSelectedVOI(double bounds[6], bool localBounds);
    vtkSmartPointer<vtkImageShrink3D> getImage();
    vtkSmartPointer<vtkOpenGLGPUVolumeRayCastMapper> getVolumeMapper();
    vtkSmartPointer<vtkVolume> getVolume();
    vtkSmartPointer<vtkPolyData> getOctreeRepresentation();
    vtkSmartPointer<vtkPolyDataMapper> getSurfaceMapper();
};
