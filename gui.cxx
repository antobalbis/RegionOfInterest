#include "gui.h"
#include "ui_gui.h"

#include <vtkBorderWidget.h>
#include <vtkCommand.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkNamedColors.h>
#include <vtkNew.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkSphereSource.h>
#include <vtkVersion.h>
#include <vtkPolyDataNormals.h>
#include <vtkContourFilter.h>

class vtkMyCallback : public vtkCommand
{
  public:
    static vtkMyCallback* New(){
      return new vtkMyCallback;
    }

    double* object_bounds = new double[6];
    vtkSmartPointer<vtkOpenGLGPUVolumeRayCastMapper> mapper;
    double* l_bounds = new double[6];
    bool init = false;

    /*
    This method is call when we manipulate the box.
    this initialize the value of the box's bounds in relation with the object.
    */
    virtual void Execute(vtkObject* caller, unsigned long, void*){
      vtkSmartPointer<vtkBoxWidget2> widget = dynamic_cast<vtkBoxWidget2*>(caller);
      double* bounds = dynamic_cast<vtkBoxRepresentation*>(widget->GetRepresentation())->GetBounds();

      for(int i = 0; i < 6; i++) l_bounds[i] = bounds[i];

      std::cout << "BOX" << endl;
      std::cout << l_bounds[0] << " " << l_bounds[2] << " " << l_bounds[4] << endl;
      std::cout << l_bounds[1] << " " << l_bounds[3] << " " << l_bounds[5] << endl;
      std::cout << "OBJECT" << endl;
      std::cout << object_bounds[0] << " " << object_bounds[2] << " " << object_bounds[4] << endl;
      std::cout << object_bounds[1] << " " << object_bounds[3] << " " << object_bounds[5] << endl;

      if(l_bounds[0] < object_bounds[0]) l_bounds[0] = object_bounds[0];
      if(l_bounds[2] < object_bounds[2]) l_bounds[2] = object_bounds[2];
      if(l_bounds[4] < object_bounds[4]) l_bounds[4] = object_bounds[4];
      if(l_bounds[1] > object_bounds[1]) l_bounds[1] = object_bounds[1];
      if(l_bounds[3] > object_bounds[3]) l_bounds[3] = object_bounds[3];
      if(l_bounds[5] > object_bounds[5]) l_bounds[5] = object_bounds[5];

    }

    void initBounds(){
      l_bounds[0] = 0;
      l_bounds[1] = 0;
      l_bounds[2] = 0;
      l_bounds[3] = 0;
      l_bounds[4] = 0;
      l_bounds[5] = 0;
    }

    void setBounds(double* bounds){
        object_bounds[0] = bounds[0];
        object_bounds[1] = bounds[1];
        object_bounds[2] = bounds[2];
        object_bounds[3] = bounds[3];
        object_bounds[4] = bounds[4];
        object_bounds[5] = bounds[5];
    }

    double* GetBounds(){
      return l_bounds;
    }

};

class KeyPressInteractionStyle : public vtkInteractorStyleTrackballCamera
{
  public:
    static KeyPressInteractionStyle* New(){
      return new KeyPressInteractionStyle;
    }
    vtkTypeMacro(KeyPressInteractionStyle, vtkInteractorStyleTrackballCamera);
    vtkSmartPointer<vtkOpenGLGPUVolumeRayCastMapper> mapper;
    vtkSmartPointer<vtkImageShrink3D> shrink;
    vtkSmartPointer<vtkMyCallback> callback;
    vtkSmartPointer<vtkBoxWidget2> boxWidget;
    vtkSmartPointer<vtkPolyData> octreeRepresentation;
    int num_cells;

    virtual void OnKeyPress(){
      vtkRenderWindowInteractor* interactor = this->Interactor;
      std::string key = interactor->GetKeySym();

      if(key == "j"){
        std::cout << "SE HA PULSADO?? " << key << endl;
        double* bounds = callback->GetBounds();

        if(bounds[1] > bounds[0] && bounds[3] > bounds[2] && bounds[5] > bounds[4]){
          double minBounds[3] = {bounds[0], bounds[2], bounds[4]};
          double maxBounds[3] = {bounds[1], bounds[3], bounds[5]};
          double minPoint[3];
          double maxPoint[3];

          shrink->GetOutput()->TransformPhysicalPointToContinuousIndex(minBounds, minPoint);
          shrink->GetOutput()->TransformPhysicalPointToContinuousIndex(maxBounds, maxPoint);

          std::cout << minPoint[0] << " " << minPoint[1] << " " << minPoint[2] << endl;
          std::cout << maxPoint[0] << " " << maxPoint[1] << " " << maxPoint[2] << endl;

          vtkSmartPointer<vtkExtractVOI> voi = vtkSmartPointer<vtkExtractVOI>::New();
          voi->SetInputConnection(shrink->GetOutputPort());
          voi->SetVOI((int) minPoint[0], (int) maxPoint[0], (int) minPoint[1], (int) maxPoint[1], (int) minPoint[2], (int) maxPoint[2]);
          voi->Update();

          mapper->SetInputConnection(voi->GetOutputPort());

          //boxWidget->GetRepresentation()->SetPlaceFactor(1);
          //boxWidget->GetRepresentation()->PlaceWidget(mapper->GetBounds());
        }
      }
    }

    void SetMapper(vtkSmartPointer<vtkOpenGLGPUVolumeRayCastMapper> mapper){
      this->mapper = mapper;
    }

    void SetBoxWidget(vtkSmartPointer<vtkBoxWidget2> boxWidget){
      this->boxWidget = boxWidget;
    }

    void SetShrink(vtkSmartPointer<vtkImageShrink3D> shrink){
      this->shrink = shrink;
    }

    void SetCallback(vtkSmartPointer<vtkMyCallback> callback){
      this->callback = callback;
    }
};


gui::gui(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::gui)
{
    ui->setupUi(this);

    Render render = Render("../flower/flower_uint.raw");
    volumeMapper = render.getVolumeMapper();
    shrink = render.getImage();

    vtkNew<vtkNamedColors> colors;
    vtkNew<vtkRenderer> ren1;

    vtkNew<vtkGenericOpenGLRenderWindow> renWin;
    renWin->AddRenderer(ren1);

    this->ui->qvtkWidget->setRenderWindow(renWin);

    vtkNew<KeyPressInteractionStyle> key;
    key->SetMapper(volumeMapper);
    key->SetShrink(shrink);
    key->SetCurrentRenderer(ren1);

    vtkNew<vtkPolyDataMapper> octreeMapper;
    octreeMapper->SetInputData(render.getOctreeRepresentation());

    vtkNew<vtkActor> octreeActor;
    octreeActor->SetMapper(octreeMapper);
    octreeActor->GetProperty()->SetInterpolationToFlat();
    octreeActor->GetProperty()->SetRepresentationToWireframe();
    octreeActor->GetProperty()->SetOpacity(0.4);
    octreeActor->GetProperty()->EdgeVisibilityOn();
    octreeActor->GetProperty()->SetColor(
      colors->GetColor4d("SpringGreen").GetData()
    );

    vtkNew<vtkActor> polyActor;
    polyActor->SetMapper(render.getSurfaceMapper());
    polyActor->GetProperty()->SetInterpolationToFlat();
    polyActor->GetProperty()->SetOpacity(0.2);
    polyActor->GetProperty()->SetColor(
      colors->GetColor4d("flesh").GetData()
    );

    ren1->AddVolume(render.getVolume());
    ren1->AddActor(polyActor);
    ren1->AddActor(octreeActor);
    ren1->SetBackground(colors->GetColor3d("Wheat").GetData());
    ren1->GetActiveCamera()->Azimuth(45);
    ren1->GetActiveCamera()->Elevation(30);
    ren1->ResetCameraClippingRange();
    ren1->ResetCamera();

    renWin->SetSize(600, 600);
    renWin->SetWindowName("SimpleRayCast");

    this->ui->qvtkWidget->renderWindow()->AddRenderer(ren1);

    vtkNew<vtkBoxWidget2> boxWidget;
    boxWidget->SetInteractor(this->ui->qvtkWidget->interactor());
    boxWidget->RotationEnabledOff();
    boxWidget->TranslationEnabledOn();
    boxWidget->GetRepresentation()->SetPlaceFactor(1);
    boxWidget->GetRepresentation()->PlaceWidget(render.getVolume()->GetBounds());

    vtkSmartPointer<vtkMyCallback> callback = vtkSmartPointer<vtkMyCallback>::New();
    callback->setBounds(render.getVolume()->GetBounds());
    callback->initBounds();
    key->SetCallback(callback);
    key->SetBoxWidget(boxWidget);
    boxWidget->AddObserver(vtkCommand::InteractionEvent, callback);
    //boxWidget->On();
    this->boxWidget = boxWidget;
    this->boxWidget->On();

    this->ui->qvtkWidget->interactor()->SetInteractorStyle(key);

    connect(this->ui->pushButton, SIGNAL(released()), this, SLOT(handleButton()));
}

void gui::handleButton(){
  double initI = std::stod(this->ui->iText->toPlainText().toUtf8().constData());
  double initJ = std::stod(this->ui->jText->toPlainText().toUtf8().constData());
  double initK = std::stod(this->ui->kText->toPlainText().toUtf8().constData());

  double length = std::stod(this->ui->lengthText->toPlainText().toUtf8().constData());
  double width = std::stod(this->ui->widthText->toPlainText().toUtf8().constData());
  double depth = std::stod(this->ui->depthText->toPlainText().toUtf8().constData());

  int* dims = shrink->GetOutput()->GetExtent();
  if(initI < dims[0]) initI = dims[0];
  if(initJ < dims[2]) initJ = dims[2];
  if(initK < dims[4]) initK = dims[4];
  if(length > dims[1]) length = dims[0];
  if(width > dims[3]) width = dims[3];
  if(depth > dims[5]) depth = dims[5];

  if(initI < length && initJ < width && initK < depth){
    vtkSmartPointer<vtkExtractVOI> voi = vtkSmartPointer<vtkExtractVOI>::New();
    voi->SetInputConnection(shrink->GetOutputPort());
    voi->SetVOI(initI, length, initJ, width, initK, depth);
    voi->Update();
    volumeMapper->SetInputConnection(voi->GetOutputPort());

    boxWidget->GetRepresentation()->SetPlaceFactor(1);
    boxWidget->GetRepresentation()->PlaceWidget(volumeMapper->GetBounds());

  }

  std::cout << initI << " " << initJ << " " << initK << endl;
}

gui::~gui()
{
    delete this->ui;
}
