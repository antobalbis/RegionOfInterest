# Resumen del desarrollo

## Lectura de la imagen

La idea es mantener en la memoria del sistema un elemento vtkImageData con el volumen original y otro vtkImageData con el volumen en el estado actual.

Lo primero es usar un lector que va a depender del formato de la imagen, y luego pasamos la imagen a un objeto del tipo vtkShrinkImage3D, al que le indicamos un factor de 1 para no perder resolucińo en la imagen (luego se implementará una forma de calcular el factor en función de la memoria que ocupe para que entre en la VRAM).

Para generar la estructura OCTREE usamos un objeto del tipo vtkOctreePointLocator al que le pasamos la imagen para generar la indexación del espacio y se indicará el nivel 4 como el máximo nivel del octree.

```
octree = vtkSmartPointer<vtkOctreePointLocator>::New();
  octree->SetDataSet(shrink->GetOutput());
  octree->SetMaxLevel(4);
  octree->BuildLocator();
  octreeRepresentation = vtkSmartPointer<vtkPolyData>::New();
  octree->GenerateRepresentation(4, octreeRepresentation);
```

Luego usamos vtkContourFilter para extraer la isosuperficie del volumen y vtkPolyDataNormals para generar las normales. Esta isosuperficie se va a usar para mostrar siempre la visualziación completa del objeto.

```
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

```


## Extracción de región de interés

La extracción de una ROI cúbica o esférica se lleva a cabo de forma diferente, lo primero en ambos casos es usar vtkExtractVOI para enviar a GPU solamente el área seleccionada (en esta primera versión), para poder hacer esto primero es necesario traducir las coordenadas de mundo en índices locales del volumen, asegurándonos que no se extralimiten las dimensiones del volumen. Para esto contamos con el método TransformPhysicalPointToContinuousIndex de la clase vtkImageData.

```
void Render::extractSelectedVOI(double bounds[6], bool localBounds){
  double *_bounds = new double[6];
  if(!localBounds){
    _bounds = getLocalBounds(bounds);

    std::cout << bounds[0] << " " << bounds[1] << " " << bounds[2] << " " << bounds[3] << " " << bounds[4] << " " << bounds[5] << endl;
  }else{
    _bounds[0] = bounds[0];
    _bounds[1] = bounds[1];
    _bounds[2] = bounds[2];
    _bounds[3] = bounds[3];
    _bounds[4] = bounds[4];
    _bounds[5] = bounds[5];
  }

  vtkSmartPointer<vtkExtractVOI> voi = extractVOI(_bounds, current);
  current = voi->GetOutput();
  volumeMapper->SetInputConnection(voi->GetOutputPort());
}
```
Lo primero que hacemos para extraer la región de interés en el código anterior es traducir las coordenadas a índices locales cuando sea necesario, luego se extrae la ROI y se actualiza el estado actual del volumen (*current*) como la ROI.

```
vtkSmartPointer<vtkExtractVOI> Render::extractVOI(double bounds[6], vtkSmartPointer<vtkImageData> dataSet){
  vtkSmartPointer<vtkExtractVOI> voi = vtkSmartPointer<vtkExtractVOI>::New();
  voi->SetInputData(dataSet);
  voi->SetVOI((int) bounds[0], (int) bounds[1], (int) bounds[2], (int) bounds[3], (int) bounds[4], (int) bounds[5]);
  voi->Update();

  return voi;
}
```
El código anterior muestra como se extrae la ROI cúbica.

Para extraer una ROI esférica se realiza lo siguiente. Primero definimos un objeto del tipo vtkSphere a partir del centro y el radio.

```
void Render::extractSphere(double radius, double *center, double *bounds){
  vtkSmartPointer<vtkSphere> sphere = vtkSmartPointer<vtkSphere>::New();
  sphere->SetCenter(center);
  sphere->SetRadius(radius);

  doExtraction(sphere, bounds);
}
```

Luego se extrae una ROI con las dimensiones establecidas, luego se calcula la intersección de la esfera con el volumen y se usa la salida como máscara usando vtkPolyDataToImageStencil y el objeto vtkImageStencil para aplicar la máscara. Por último, se envía la salida de este objeto a GPU.


```
void Render::doExtraction(vtkSmartPointer<vtkImplicitFunction> function, double *bounds){
  vtkSmartPointer<vtkExtractVOI> voi = extractVOI(bounds, current);

  vtkNew<vtkCutter> clipVolume;
  clipVolume->SetCutFunction(function);
  clipVolume->SetInputConnection(voi->GetOutputPort());
  clipVolume->Update();

  vtkNew<vtkPolyDataToImageStencil> stencilImage;
  stencilImage->SetInputData(clipVolume->GetOutput());
  stencilImage->SetOutputOrigin(voi->GetOutput()->GetOrigin());
  stencilImage->SetOutputSpacing(voi->GetOutput()->GetSpacing());
  stencilImage->SetOutputWholeExtent(voi->GetOutput()->GetExtent());
  stencilImage->Update();

  vtkNew<vtkImageStencil> stencil;
  stencil->SetInputData(current);
  stencil->SetStencilConnection(stencilImage->GetOutputPort());
  stencil->SetBackgroundValue(0);
  stencil->Update();

  current = stencil->GetOutput();
  volumeMapper->SetInputConnection(stencil->GetOutputPort());
}
```

## Rendering

Para el renderizado se usa un mapper de visualización directa de volúmenes del tipo vtkOpenGLGPUVolumeRayCastMapper, Que se añade a un objeto del tipo vtkVolume junto con las funciones de transferencia de color y opacidad (vtkColorTransferFunction y vtkPiecewiseFunction) en un objeto vtkVolumeProperty.

Para el cauce de renderizado se crea el renderer al que se le añaden un objeto vtkActor con la isosuperficie del volumen y el vtkVolume creado anteriormente. Luego se crea la ventana de renderizado como un objeto vtkGenericOpenGLRenderWindow al que se le añade el renderer.

## Interacción

Para la interacción contamos con una interfaz de usuario y con controles de teclado. Para la creación de la interfaz de usuario se ha usado QT. Para que se pueda llevar a cabo la visualización de la ventana de renderizado de vtk usando QT se usa un objeto qvtkWidget al que le pasamos el *renderer* y el *render window*.

En la interacción mediante interfaz de usuario tenemos una serie de campos para introducir el comienzo y el fin de las dimensiones para la extracción de una ROI cúbica y un botón para hacerla efectiva.

En la interacción mediante controles de teclado tenemos un objeto del tipo vtkBoxWidget2 para seleccionar el área que queremos visualizar. Este objeto se usa a través de una clase que hereda de vtkCommand. Lo que nos interesa de este objeto a la hora de extraer una ROI son sus *bounds*, es decir las coordenadas de inicio y fin en cada eje.

```
class vtkMyCallback : public vtkCommand
{
  public:
    static vtkMyCallback* New(){
      return new vtkMyCallback;
    }

    double* object_bounds = new double[6];
    double* l_bounds = new double[6];
    bool init = false;

    virtual void Execute(vtkObject* caller, unsigned long, void*){
      vtkSmartPointer<vtkBoxWidget2> widget = dynamic_cast<vtkBoxWidget2*>(caller);
      double* bounds = dynamic_cast<vtkBoxRepresentation*>(widget->GetRepresentation())->GetBounds();

      for(int i = 0; i < 6; i++) l_bounds[i] = bounds[i];

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
```
El código anterior muestra la clase que controla la caja para seleccionar la ROI.

Una vez seleccionada la ROI se aplica al pulsar la tecla "j" si queremos una ROI cúbica o "s" si queremos una ROI esférica. Para la lectura de teclado implementamos una clase que hereda de vtkInteractorStyleTrackballCamera. Es esta clase la encargada de llamar al método correspondiente para la extracción de la ROI. Lo que hace es leer los límites del vtkBoxWidget2 y solicitar la extracción de la ROI. Si la ROI es cúbica simplemente se pasarán los límites y si es esférica se calcula el radio de la esfera como el lado más pequeño del cubo partido por 2.

```
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

        if(bounds[1] > bounds[0] && bounds[3] > bounds[2] && bounds[5] > bounds[4]){
          render.extractSelectedVOI(bounds, false);
        }
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
```

El código anterior muestra la clase que captura las teclas pulsadas, además cuando se pulsa la tecla "r" se reinicia el estado de la visualización al estado inicial.

A continuación se muestra una captura con el estado actual del proyecto.
![Captura del estado del proyecto](https://github.com/antobalbis/TFM-RegionOfInterest/blob/master/images/capturaGUI.png)