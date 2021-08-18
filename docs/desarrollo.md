# Resumen del desarrollo

En el estado actual del desarrollo tenemos dos clases: Render y GUI. La clase Render es la que se encarga de la lectura y la manipulación del volumen, mientras que la clase gui se encarga del cauce de renderizado y de gestionar la interfaz de usuario.

## Lectura de la imagen

La idea es mantener en la memoria del sistema un elemento vtkImageData con el volumen original y otro vtkImageData con el volumen en el estado actual.

Lo primero es usar un lector que va a depender del formato de la imagen, y luego pasamos la imagen a un objeto del tipo vtkShrinkImage3D, al que le indicamos un factor de 1 para no perder resolucińo en la imagen (luego se implementará una forma de calcular el factor en función de la memoria que ocupe para que entre en la VRAM).

Para generar la estructura OCTREE partimos de un objeto vtkOctreePointLocatorNode, que será el nodo raíz del árbol, luego, creamos los 8 nodos hijos y vamos haciendo eso para todos los nodos hasta llegar al nivel 4 del octree.


```
void Render::createOctreeNodes(vtkSmartPointer<vtkOctreePointLocatorNode> node, int level, int maxLevel){
  if(level < maxLevel){
    node->CreateChildNodes();
    level++;

    double *bounds = node->GetMinBounds();

    for(int i = 0; i < 8; i++){
      createOctreeNodes(node->GetChild(i), level, maxLevel);
    }
  }
}
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
vtkSmartPointer<vtkExtractVOI> Render::extractVOI(double bounds[6], vtkSmartPointer<vtkImageData> dataSet){
  vtkSmartPointer<vtkExtractVOI> voi = vtkSmartPointer<vtkExtractVOI>::New();
  voi->SetInputData(dataSet);
  voi->SetVOI((int) bounds[0], (int) bounds[1], (int) bounds[2], (int) bounds[3], (int) bounds[4], (int) bounds[5]);
  voi->Update();

  return voi;
}
```

Una vez que tenemos los índices locales extraemos la región de interés usando vtkExtractVOI. El problema de esta solución es que cada vez que queremos extraer una ROI tenemos que eliminar y cargar en memoria el área seleccionada, por tanto se proponer una nueva solución.

### Extracción de ROI V2

En esta nueva solución usamos un octree para dividir el espacio y seleccionar el ocatante más pequeño que contenga el área seleccionada.

```
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

    if(!same){
      std::cout << "IS NOT THE SAME" << endl;
      bounds_[0] = node_->GetMinBounds()[0];
      bounds_[2] = node_->GetMinBounds()[1];
      bounds_[4] = node_->GetMinBounds()[2];
      bounds_[1] = node_->GetMaxBounds()[0];
      bounds_[3] = node_->GetMaxBounds()[1];
      bounds_[5] = node_->GetMaxBounds()[2];

      bounds_ = getLocalBounds(bounds_);

      vtkSmartPointer<vtkExtractVOI> voi = extractVOI(bounds_, original);
      current = voi->GetOutput();
      volumeMapper->SetInputConnection(voi->GetOutputPort());
    }

    vtkSmartPointer<vtkBox> box = vtkSmartPointer<vtkBox>::New();
    box->SetBounds(bounds);
    doExtraction(box, bounds, 0);

  }
}
```
Lo primero que hacemos para extraer la región de interés en el código anterior es ajustar las coordenadas de la región seleccionada a las coordenadas límites del objeto, de forma que la región seleccionada quede dentro de los límites del modelo.

Lo siguiente que hacemos es buscar el octante más pequeño del octree que engloba la región seleccionada y, si el octante no es el que teníamos previamente cargado en GPU, cargamos en GPU la región del modelo que está definida por los límites del nodo.

Por último enviamos al mapper la región definida por la caja que usamos para seleccionar el área de interés para cortar la imagen que queda fuera del área.

Para seleccionar el octante mínimo que contenga toda la región seleccionada se ha implementado un método recursivo que comprueba si la reggíon seleccionada se encuentra dentro de los límites del octante. Si es así se llama a los hijos del octante si los tiene, si el octante engloba a la región y no tiene ningún hijo que la engloba devuelve ese octante. En caso de que el octante no contenga a la región devuelve un puntero nulo.


```
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
      if(level == 4){
        return node;
      }else{
        for(int i = 0; i < 8; i++){
          if (getOctreeBounds(bounds, node->GetChild(i), level + 1) != nullptr){
            return node->GetChild(i);
          }
        }
        return node;
      }
    }else{
      return nullptr;
    }
}
```

El código anterior muestra como se extrae la ROI cúbica que va a ser cargada en GPU. El problema que encontramos con este método es que puede darse el caso de que se selecciona una región pequeña pero que el único nodo que la englobe por completo sea el nodo raíz, y por tanto no estemos reduciendo el tamaño del objeto que vamos a enviar a GPU.

### Extracción de ROI V3

Para esta nueva versión se propone una modificación sobre el caso anterior en la que si el nodo elegido del octree es el nodo raíz lo que se va a hacer es calcular una región usando los nodos de nivel 2.

```
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

}
```
Como vemos en el código anterior lo que hacemos es extraer el nodo usando como punto las coordenadas x, y, z mínimas y mñaximas de la región seleccionada, buscar los nodos en los que se encuentran y coger la x,y,z mínimas del primer nodo y las x, y, z máximas del segundo y usar eso como boundaries para la extracción de la región de interés.

### Extracción de una región esférica

Para extraer una ROI esférica, además de seleccionar y cargar el octante mínimo del octree de la misma forma que en el caso anterior, se realiza lo siguiente. Primero definimos un objeto del tipo vtkSphere a partir del centro y el radio.

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