#include "openfile.h"
#include "ui_openfile.h"

#include <QDoubleValidator>
#include <QIntValidator>
#include <iostream>
#include <vtkNamedColors.h>
#include <vtkNew.h>

OpenFile::OpenFile(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::OpenFile)
{
    ui->setupUi(this);

    path = new char[1024];

    QValidator *sv = new QDoubleValidator(0, 10000, 10, this);
    this->ui->xSpace->setValidator(sv);
    this->ui->ySpace->setValidator(sv);
    this->ui->zSpace->setValidator(sv);

    QValidator *sd = new QIntValidator(0, 100000, this);
    this->ui->xDim->setValidator(sd);
    this->ui->yDim->setValidator(sd);
    this->ui->zDim->setValidator(sd);

    QValidator *civ = new QDoubleValidator(this);
    this->ui->intensity->setValidator(civ);
    this->ui->opacity->setValidator(civ);

    vtkNew<vtkNamedColors> colors;
    std::string colors_ = colors->GetColorNames();
    this->ui->colorslist->document()->setPlainText(QString::fromUtf8(colors_.c_str()));

    connect(this->ui->buttonBox->button(QDialogButtonBox::Ok), SIGNAL(released()), this, SLOT(accept()));
    connect(this->ui->buttonBox->button(QDialogButtonBox::Cancel), SIGNAL(released()), this, SLOT(close()));
    connect(this->ui->selectfile, SIGNAL(released()), this, SLOT(selectfile()));
    connect(this->ui->selectdir, SIGNAL(released()), this, SLOT(selectdir()));
    connect(this->ui->addic, SIGNAL(released()), this, SLOT(addIntensityColorOpacity()));
    connect(this->ui->delic, SIGNAL(released()), this, SLOT(removeIntensityColorOpacity()));
}

OpenFile::~OpenFile()
{
    delete ui;
}

void OpenFile::accept(){
  std::string spacingx = this->ui->xSpace->text().toUtf8().data();
  std::string spacingy = this->ui->ySpace->text().toUtf8().data();
  std::string spacingz = this->ui->zSpace->text().toUtf8().data();

  if(!spacingx.empty() && !spacingy.empty() && !spacingz.empty()){
    spacing[0] = std::stod(spacingx);
    spacing[1] = std::stod(spacingy);
    spacing[2] = std::stod(spacingz);
  }

  std::string dimx = this->ui->xDim->text().toUtf8().data();
  std::string dimy = this->ui->yDim->text().toUtf8().data();
  std::string dimz = this->ui->zDim->text().toUtf8().data();

  if(!dimx.empty() && !dimy.empty() && !dimz.empty()){
    dimensions[0] = std::stoi(dimx);
    dimensions[1] = std::stoi(dimy);
    dimensions[2] = std::stoi(dimz);
  }

  std::string file = "";
  std::string dir = "";

  //std::string file = this->ui->FileSelection->toPlainText().toUtf8().data();
  //std::string dir = this->ui->DirSelection->toPlainText().toUtf8().data();

  std::cout << "HASTA AQUI BIEN\n";

  if(!this->ui->FileSelection->toPlainText().isEmpty()){
    file = this->ui->FileSelection->toPlainText().toUtf8().data();
    isFile = true;
  }else if(!this->ui->DirSelection->toPlainText().isEmpty()){
    dir = this->ui->DirSelection->toPlainText().toUtf8().data();
    isFile = false;
  }

  std::cout << "GetValueFromTextFields\n";

  if(!file.empty()){
    path = strcpy(path, file.c_str());
  }else if(!dir.empty()){
    path = strcpy(path, dir.c_str());
  }

  std::cout << "STILL FINE\n";

  for(int i = 0; i < intensities.size(); i++){
    std::cout << intensities.at(i) << " " << colors.at(i) << " " << opacities.at(i) << std::endl;
  }

  this->hide();
}

void OpenFile::close(){
  this->hide();
}

void OpenFile::selectfile(){
  char fileName[1024];
  FILE *file = popen("zenity --file-selection", "r");

  if(file != NULL){
    if(fgets(fileName, 1024, file) != NULL){
      this->ui->FileSelection->setText(fileName);
    }else{
      this->ui->FileSelection->setText("");
    }
    pclose(file);
  }else{
    this->ui->FileSelection->setText("");
  }

}

void OpenFile::selectdir(){
  char fileName[1024];
  FILE *file = popen("zenity --file-selection --directory", "r");

  if(file != NULL){
    if(fgets(fileName, 1024, file) != NULL){
      this->ui->DirSelection->setText(fileName);
    }else{
      this->ui->DirSelection->setText("");
    }
    pclose(file);
  }else{
    this->ui->DirSelection->setText("");
  }
}

void OpenFile::addIntensityColorOpacity(){
  std::string intensity = this->ui->intensity->text().toUtf8().data();
  std::string color = this->ui->color->text().toUtf8().data();
  std::string opacity = this->ui->opacity->text().toUtf8().data();

  if(!intensity.empty() && !color.empty() && !opacity.empty()){
    intensities.push_back(std::stod(intensity));
    colors.push_back(color);
    opacities.push_back(std::stod(opacity));
  }
}

void OpenFile::removeIntensityColorOpacity(){
  if(intensities.size() != 0 && colors.size() != 0 && opacities.size() != 0){
    intensities.pop_back();
    colors.pop_back();
    opacities.pop_back();
  }
}
