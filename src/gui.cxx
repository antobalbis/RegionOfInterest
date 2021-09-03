#include "gui.h"
#include "ui_gui.h"
#include <vector>
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
#include <QDoubleValidator>


class FpsObserver : public vtkCommand
{
  public:
    static FpsObserver* New(){
      return new FpsObserver;
    }
    int count = 0;
    Render render;

    virtual void Execute(vtkObject* caller, unsigned long, void*){
      vtkSmartPointer<vtkRenderer> renderer = dynamic_cast<vtkRenderer*>(caller);
      double fps = 1/renderer->GetLastRenderTimeInSeconds();
      std::cout << "FPS = "  << fps << endl;

      if(fps < 20){
        //REFACTOR
        count++;
        if(count == 5)render.refactor(20/fps + 1);
      }else if (fps > 60){
        //REFACTOR
        count = 0;
      }else{
        count = 0;
      }
    }

    void setRender(Render render){
      this->render = render;
    }
};

class vtkMyCallback : public vtkCommand
{
  public:
    static vtkMyCallback* New(){
      return new vtkMyCallback;
    }

    double* object_bounds = new double[6];
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
    vtkSmartPointer<vtkMyCallback> callback;
    vtkSmartPointer<vtkBoxWidget2> boxWidget;
    vtkSmartPointer<vtkGenericOpenGLRenderWindow> renWin;
    Render render;
    int num_cells;

    virtual void OnKeyPress(){
      vtkRenderWindowInteractor* interactor = this->Interactor;
      std::string key = interactor->GetKeySym();
      double* bounds = callback->GetBounds();

      if(key == "j"){
        std::cout << "SE HA PULSADO?? " << key << endl;

        render.extractSelectedVOI(bounds, false);

      }
      else if(key == "s"){
        double radius = {getMin((bounds[1] - bounds[0])/2, (bounds[3] - bounds[2])/2, (bounds[5] - bounds[4])/2)};
        std::cout << radius << endl;
        double center[3];
        center[0] = (bounds[1] + bounds[0])/2;
        center[1] = (bounds[3] + bounds[2])/2;
        center[2] = (bounds[5] + bounds[4])/2;
        render.extractFormedVOI(0, bounds, center, radius, nullptr);
      }
      else if(key == "r"){
        render.restart();
      }
      renWin->Render();
    }

    double getMin(double x, double y, double z){
      if(x < y && x < z) return x;
      else if(y < z) return y;
      else return z;
    }

    void SetBoxWidget(vtkSmartPointer<vtkBoxWidget2> boxWidget){
      this->boxWidget = boxWidget;
    }

    void SetCallback(vtkSmartPointer<vtkMyCallback> callback){
      this->callback = callback;
    }

    void SetRender(Render render){
      this->render = render;
    }

    void SetRenWin(vtkSmartPointer<vtkGenericOpenGLRenderWindow> renWin){
      this->renWin = renWin;
    }
};

gui::gui(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::gui)
{
    ui->setupUi(this);

    //render = Render("../headsq/quarter", spacing, dims);
    op = new OpenFile();

    vtkNew<vtkNamedColors> colors;

    renWin = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();

    this->ui->qvtkWidget->setRenderWindow(renWin);

    //vtkNew<vtkPolyDataMapper> octreeMapper;
    //octreeMapper->SetInputData(render.getOctreeRepresentation());

    /*vtkNew<vtkActor> octreeActor;
    octreeActor->SetMapper(octreeMapper);
    octreeActor->GetProperty()->SetInterpolationToFlat();
    octreeActor->GetProperty()->SetRepresentationToWireframe();
    octreeActor->GetProperty()->SetOpacity(0.4);
    octreeActor->GetProperty()->EdgeVisibilityOn();
    octreeActor->GetProperty()->SetColor(
      colors->GetColor4d("SpringGreen").GetData()
    );*/

    std::cout << "OK1" << endl;

    //renderer->AddActor(polyActor);
    //renderer->AddActor(octreeActor);

    std::cout << "OK2" << endl;

    renWin->SetSize(600, 600);
    renWin->SetWindowName("SimpleRayCast");

    std::cout << "OK3" << endl;

    //this->ui->qvtkWidget->renderWindow()->AddRenderer(renderer);

    connect(this->ui->pushButton, SIGNAL(released()), this, SLOT(handleButton()));
    connect(this->ui->openButton, SIGNAL(released()), this, SLOT(openFile()));
    connect(this->ui->addButton, SIGNAL(released()), this, SLOT(addFunctionValue()));
    connect(this->ui->deleteButton, SIGNAL(released()), this, SLOT(removeFunctionValue()));

    QValidator *doubleValidator = new QDoubleValidator(this);

    this->ui->intText->setValidator(doubleValidator);
    this->ui->opText->setValidator(doubleValidator);
    this->ui->iText->setValidator(doubleValidator);
    this->ui->jText->setValidator(doubleValidator);
    this->ui->kText->setValidator(doubleValidator);
    this->ui->lengthText->setValidator(doubleValidator);
    this->ui->widthText->setValidator(doubleValidator);
    this->ui->depthText->setValidator(doubleValidator);

    std::string colors_ = colors->GetColorNames();
    this->ui->colorsList->document()->setPlainText(QString::fromUtf8(colors_.c_str()));
}

void gui::handleButton(){
  double initI = std::stod(this->ui->iText->text().toUtf8().constData());
  double initJ = std::stod(this->ui->jText->text().toUtf8().constData());
  double initK = std::stod(this->ui->kText->text().toUtf8().constData());

  double length = std::stod(this->ui->lengthText->text().toUtf8().constData());
  double width = std::stod(this->ui->widthText->text().toUtf8().constData());
  double depth = std::stod(this->ui->depthText->text().toUtf8().constData());

  double* dims = render.getOriginalBounds();
  if(initI < dims[0]) initI = dims[0];
  if(initJ < dims[2]) initJ = dims[2];
  if(initK < dims[4]) initK = dims[4];
  if(length > dims[1]) length = dims[0];
  if(width > dims[3]) width = dims[3];
  if(depth > dims[5]) depth = dims[5];

  if(initI < length && initJ < width && initK < depth){
    double bounds[6] = {initI, length, initJ, width, initK, depth};
    render.extractSelectedVOI(bounds, false);
  }
  renWin->Render();
  std::cout << initI << " " << initJ << " " << initK << endl;
}

void gui::openFile(){
  op->exec();
  if(op->isOk()){
    loadFile();
    std::string values = "";
    for(int i = 0; i < op->getIntensities().size(); i++){
      values = values + "[" + std::to_string(op->getIntensities()[i]) + " " + op->getColors()[i] + " " + std::to_string(op->getOpacities()[i]) + "]\n";
    }
    this->ui->funcValues->document()->setPlainText(QString::fromUtf8(values.c_str()));
  }
}

bool gui::isANumber(std::string text){
  bool isNumber = !text.empty();
  int nComas = 0;

  std::string::iterator it = text.begin();
  for(; it != text.end() && isNumber; it++){
    isNumber = std::isdigit(*it) || *it == ',';
    if(*it == ',') nComas++;
  }

  if(nComas > 1) isNumber = false;

  if(isNumber) cout << "still a number" << endl;
  else cout << "not a number" << endl;
  return isNumber;
}

void gui::addFunctionValue(){
  std::string iText = this->ui->intText->text().toUtf8().data();
  std::string cText = this->ui->colorText->text().toUtf8().data();
  std::string oText = this->ui->opText->text().toUtf8().data();

  std::cout << "intensity: " << iText << " color: " << cText << " opacity: " << oText << endl;

  if(isANumber(iText) && isANumber(oText)){
    std::cout << "let's do some things" << endl;
    if(!iText.empty() && !cText.empty() && !oText.empty()){
      render.addFunctionValue(std::stod(iText), cText, std::stod(oText));
      renWin->Render();
    }
  }
  //std::string text = this->ui->funcValues->plainText().toUtf8().data();
  //text = text + "[" + iText + " " + cText + " " + oText + "]\n";
}

void gui::removeFunctionValue(){
  std::string iText = this->ui->intText->text().toUtf8().data();
  render.removeFunctionValue(std::stod(iText));
  renWin->Render();
}

void gui::loadFile(){

  renderer = vtkSmartPointer<vtkRenderer>::New();
  renWin->AddRenderer(renderer);

  std::cout << "OK" << endl;
  vtkNew<vtkNamedColors> colors;

  double spacing[3];
  spacing[0] = op->getSpacing()[0];
  spacing[1] = op->getSpacing()[1];
  spacing[2] = op->getSpacing()[2];

  int dims[3];
  dims[0] = op->getDimensions()[0];
  dims[1] = op->getDimensions()[1];
  dims[2] = op->getDimensions()[2];

  std::cout << spacing[0] << " " << spacing[1] << " " << spacing[2] << endl;
  std::cout << dims[0] << " " << dims[1] << " " << dims[2] << endl;

  std::vector<double> intensities = op->getIntensities();
  std::vector<std::string> colores = op->getColors();
  std::vector<double> opacities = op->getOpacities();

  char* path = op->getPath();
  render = Render(path, spacing, dims, intensities, colores, opacities, op->isFile());

  double *bounds = render.getOriginalBounds();
  std::string path_(path);
  std::string string = "Path: " + path_ + " bounds: " + "minX: " + std::to_string(bounds[0]) +
                        " maxX: " + std::to_string(bounds[1]) + " minY: " + std::to_string(bounds[2]) +
                        " maxY: " + std::to_string(bounds[3]) + " minZ: " + std::to_string(bounds[4]) +
                        " maxZ: " + std::to_string(bounds[5]);

  this->ui->infoText->setText(string.c_str());

  vtkNew<vtkBoxWidget2> boxWidget;
  boxWidget->SetInteractor(this->ui->qvtkWidget->interactor());
  boxWidget->RotationEnabledOff();
  boxWidget->TranslationEnabledOn();
  boxWidget->GetRepresentation()->SetPlaceFactor(1);
  boxWidget->GetRepresentation()->PlaceWidget(render.getVolume()->GetBounds());

  vtkSmartPointer<vtkMyCallback> callback = vtkSmartPointer<vtkMyCallback>::New();
  callback->setBounds(render.getVolume()->GetBounds());
  callback->initBounds();

  vtkNew<KeyPressInteractionStyle> key;
  key->SetCallback(callback);
  key->SetBoxWidget(boxWidget);
  key->SetCurrentRenderer(renderer);
  key->SetRender(render);
  key->SetRenWin(renWin);

  this->ui->qvtkWidget->interactor()->SetInteractorStyle(key);
  boxWidget->AddObserver(vtkCommand::InteractionEvent, callback);
  //boxWidget->On();
  this->boxWidget = boxWidget;
  this->boxWidget->On();

  if(op->getIntensities().size() > 0){
    vtkNew<vtkActor> polyActor;
    polyActor->SetMapper(render.getSurfaceMapper());
    polyActor->GetProperty()->SetInterpolationToFlat();
    polyActor->GetProperty()->SetOpacity(0.2);
    polyActor->GetProperty()->SetColor(
      colors->GetColor4d("flesh").GetData()
    );
    renderer->AddActor(polyActor);
  }

  /*vtkNew<vtkPolyDataMapper> polyMapper;
  polyMapper->SetInputData(render.getOctreeRepresentation());

  vtkNew<vtkActor> octreeActor;
  octreeActor->SetMapper(polyMapper);
  octreeActor->GetProperty()->SetInterpolationToFlat();
  octreeActor->GetProperty()->SetOpacity(0.2);
  octreeActor->GetProperty()->SetColor(
    colors->GetColor4d("green").GetData()
  );*/

  vtkSmartPointer<FpsObserver> fps = vtkSmartPointer<FpsObserver>::New();
  renderer->AddObserver(vtkCommand::EndEvent, fps);
  renderer->AddVolume(render.getVolume());
  renderer->SetBackground(colors->GetColor3d("Wheat").GetData());
  renderer->GetActiveCamera()->Azimuth(45);
  renderer->GetActiveCamera()->Elevation(30);
  renderer->ResetCameraClippingRange();
  renderer->ResetCamera();
  renWin->Render();
}

gui::~gui()
{
    delete this->ui;
}
