#include "openfile.h"
#include "ui_openfile.h"

#include <QDoubleValidator>
#include <QIntValidator>
#include <iostream>

OpenFile::OpenFile(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::OpenFile)
{
    ui->setupUi(this);

    QValidator *sv = new QDoubleValidator(0, 10000, 10, this);
    this->ui->xSpace->setValidator(sv);
    this->ui->ySpace->setValidator(sv);
    this->ui->zSpace->setValidator(sv);

    QValidator *sd = new QIntValidator(0, 100000, this);
    this->ui->xDim->setValidator(sd);
    this->ui->yDim->setValidator(sd);
    this->ui->zDim->setValidator(sd);


    connect(this->ui->buttonBox->button(QDialogButtonBox::Ok), SIGNAL(released()), this, SLOT(accept()));
    connect(this->ui->buttonBox->button(QDialogButtonBox::Cancel), SIGNAL(released()), this, SLOT(close()));
    connect(this->ui->selectfile, SIGNAL(released()), this, SLOT(selectfile()));
    connect(this->ui->selectdir, SIGNAL(released()), this, SLOT(selectdir()));
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

  if(!this->ui->FileSelection->toPlainText().isEmpty()){
    path = this->ui->FileSelection->toPlainText().toUtf8().data();
  }else if(!this->ui->DirSelection->toPlainText().isEmpty()){
    path = this->ui->DirSelection->toPlainText().toUtf8().data();
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
