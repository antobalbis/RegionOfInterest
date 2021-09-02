#ifndef OPENFILE_H
#define OPENFILE_H

#include <QDialog>
#include <vector>

namespace Ui {
class OpenFile;
}

class OpenFile : public QDialog
{
    Q_OBJECT

public:
    explicit OpenFile(QWidget *parent = nullptr);
    virtual ~OpenFile();

    char *getPath();
    double* getSpacing();
    int* getDimensions();
    std::vector<double> getIntensities();
    std::vector<std::string> getColors();
    std::vector<double> getOpacities();
    bool isOk();
    bool isFile();

private:
  char *path;
  double spacing[3];
  int dimensions[3];
  bool file = false;
  bool ok = false;
  std::vector<double> intensities;
  std::vector<std::string> colors;
  std::vector<double> opacities;

  Ui::OpenFile *ui;
  void reject() override;
private slots:
  void accept();
  void close();
  void selectfile();
  void selectdir();
  void addIntensityColorOpacity();
  void removeIntensityColorOpacity();

};

#endif // OPENFILE_H
