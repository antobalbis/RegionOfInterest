#ifndef gui_H
#define gui_H

#include <QMainWindow>
#include <QPushButton>

#include "Render.h"
#include "openfile.h"
#include <vtkBoxRepresentation.h>
#include <vtkCubeSource.h>

class gui;

namespace Ui { class gui; }

class gui : public QMainWindow
{
    Q_OBJECT

public:
    explicit gui(QWidget *parent = nullptr);
    virtual ~gui();
    void updateField(double *bounds);
private slots:
    void applyROI();
    void openFile();
    void loadFile();
    void addFunctionValue();
    void removeFunctionValue();

private:
    vtkSmartPointer<vtkBoxRepresentation> representation;
    vtkSmartPointer<vtkBoxWidget2> boxWidget;
    vtkSmartPointer<vtkOpenGLGPUVolumeRayCastMapper> volumeMapper;
    vtkSmartPointer<vtkCubeSource> cube;
    Render render;
    OpenFile *op;
    bool automatic = true;
    vtkSmartPointer<vtkGenericOpenGLRenderWindow> renWin;
    vtkSmartPointer<vtkRenderer> renderer;
    Ui::gui *ui;

    //bool isANumber(std::string text);
};
#endif // GUI_H
