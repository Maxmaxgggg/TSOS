#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QTimer>

QT_BEGIN_NAMESPACE
namespace Ui {
class Widget;
}
QT_END_NAMESPACE

class Widget : public QWidget
{
    Q_OBJECT

public:
    Widget(QWidget *parent = nullptr);
    ~Widget();

private slots:
    void on_extraSettingsCHB_stateChanged(int arg1);

    void on_inFilePBN_clicked();

    void on_mainPBN_clicked();

    void on_outFilePBN_clicked();

    void on_outFileCancel_clicked();

    void on_overlapSPB_valueChanged(int arg1);

    void on_blockSizeSPB_valueChanged(int arg1);

private:
    Ui::Widget *ui;

    float   *inBufMono;
    QTimer  *timer;
    QString inFileName;
    QString outFileName;
};
#endif // WIDGET_H
