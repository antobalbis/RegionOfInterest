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
    char *path;
    double spacing[3];
    int dimensions[3];
    bool isFile = false;
    std::vector<double> intensities;
    std::vector<std::string> colors;
    std::vector<double> opacities;

    explicit OpenFile(QWidget *parent = nullptr);
    virtual ~OpenFile();

private:
    Ui::OpenFile *ui;

private slots:
  void accept();
  void close();
  void selectfile();
  void selectdir();
  void addIntensityColorOpacity();
  void removeIntensityColorOpacity();

};

#endif // OPENFILE_H
