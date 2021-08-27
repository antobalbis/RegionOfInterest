#ifndef OPENFILE_H
#define OPENFILE_H

#include <QDialog>

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

    explicit OpenFile(QWidget *parent = nullptr);
    virtual ~OpenFile();

private:
    Ui::OpenFile *ui;

private slots:
  void accept();
  void close();
  void selectfile();
  void selectdir();

};

#endif // OPENFILE_H
