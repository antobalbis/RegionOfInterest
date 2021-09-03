#ifndef gui_H
#define gui_H

#include <QMainWindow>
#include <QPushButton>

#include "Render.h"
#include "openfile.h"

class gui;

namespace Ui { class gui; }

class gui : public QMainWindow
{
    Q_OBJECT

public:
    explicit gui(QWidget *parent = nullptr);
    virtual ~gui();

private slots:
    void handleButton();
    void openFile();
    void loadFile();
    void addFunctionValue();
    void removeFunctionValue();
private:
    vtkSmartPointer<vtkBoxWidget2> boxWidget;
    vtkSmartPointer<vtkImageShrink3D> shrink;
    vtkSmartPointer<vtkOpenGLGPUVolumeRayCastMapper> volumeMapper;
    Render render;
    OpenFile *op;
    bool automatic = true;
    vtkSmartPointer<vtkGenericOpenGLRenderWindow> renWin;
    vtkSmartPointer<vtkRenderer> renderer;
    Ui::gui *ui;
    bool isANumber(std::string text);
};
#endif // GUI_H
