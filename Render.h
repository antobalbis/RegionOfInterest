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
#include <vtkOctreePointLocatorNode.h>
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
    int maxsize = 500;
    int factor;
    int level;
    int dims[3];
    double spacing[3];

    vtkSmartPointer<vtkImageShrink3D> shrink;
    vtkSmartPointer<vtkVolumeProperty> volumeProperty;
    vtkSmartPointer<vtkOpenGLGPUVolumeRayCastMapper> volumeMapper;
    vtkSmartPointer<vtkVolume> volume;
    vtkSmartPointer<vtkOctreePointLocator> octree;
    vtkSmartPointer<vtkPolyData> octreeRepresentation;
    vtkSmartPointer<vtkPolyDataMapper> polyMapper;
    vtkSmartPointer<vtkImageData> original;
    vtkSmartPointer<vtkImageData> current;
    vtkSmartPointer<vtkOctreePointLocatorNode> root;
    vtkSmartPointer<vtkOctreePointLocatorNode> node;


    void readDataFromDir(char* path, int x_dim, int y_dim, int z_dim, int z_init);
    void readDataFromFile(char* path, int x_dim, int y_dim, int z_dim);
    void readDICOMImage(char* path, int x_dim, int y_dim, int z_dim, int z_init);
    void readNrrdImage(char* path);
    void deleteOutsideRegion();
    vtkSmartPointer<vtkExtractVOI> fixImage();
    vtkSmartPointer<vtkImageShrink3D> extractVOI(double bounds[6], vtkSmartPointer<vtkImageData> dataSet);
    void extractCone();
    void extractSphere(double radius, double *center, double* bounds);
    void extractTransformedBox(double *bounds, double *center, vtkSmartPointer<vtkAbstractTransform> transform);
    void doExtraction(vtkSmartPointer<vtkImplicitFunction> function, double *bounds, int type);
    double *getLocalBounds(double *bounds);
    vtkSmartPointer<vtkOctreePointLocatorNode> getOctreeBounds(double *bounds, vtkSmartPointer<vtkOctreePointLocatorNode> node, int level);
    void createOctreeNodes(vtkSmartPointer<vtkOctreePointLocatorNode> node, int level, int maxLevel);
    void computeNodeBounds(int index, vtkSmartPointer<vtkOctreePointLocatorNode> node, double* parentBounds);
    bool compareNodes(vtkSmartPointer<vtkOctreePointLocatorNode> _node);
    vtkSmartPointer<vtkOctreePointLocatorNode> findNodeFromPoint(double point[3]);

  public:
    Render();
    Render(char *argv, double spacing[3], int dims[3]);
	  void graphicPipeline();
	  void getRegionOfInterest(int bounds[6]);
    void fixDataBounds(double bounds[6]);
    void clipImage(double bounds[6]);
    void extractSelectedVOI(double bounds[6], bool localBounds);
    void extractFormedVOI(int type, double *bounds, double *center, double radius, vtkSmartPointer<vtkAbstractTransform> transform);
    void restart();
    void cropImageFromPlane();
    vtkSmartPointer<vtkImageData> getImage();
    vtkSmartPointer<vtkOpenGLGPUVolumeRayCastMapper> getVolumeMapper();
    vtkSmartPointer<vtkVolume> getVolume();
    vtkSmartPointer<vtkPolyData> getOctreeRepresentation();
    vtkSmartPointer<vtkPolyDataMapper> getSurfaceMapper();
};
