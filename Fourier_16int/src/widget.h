#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QTimer>
#include <complex>

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

    void on_blockSizeHSL_valueChanged(int value);

    void on_blockSizeCMB_currentTextChanged(const QString &arg1);

private:
    Ui::Widget *ui;

    qint16  *inBufMono;
    double  *inBufMonoDouble;
    double  *DCT;

    qint16  *inBufStereo;
    double  *inBufLeft;
    double  *inBufRight;
    qint16  *outBufLeft;
    double  *DCT_Left;
    qint16  *outBufRight;
    double  *DCT_Right;
    QTimer  *timer;
    QString inFileName;
    QString outFileName;
};
#endif // WIDGET_H
