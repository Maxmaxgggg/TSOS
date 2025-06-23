#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include "qcustomplot.h"
#include <vector>
#include <complex.h>

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
    void on_computePBN_clicked();

    void on_rolloffHSL_valueChanged(int value);

    void on_rolloffDSPB_valueChanged(double arg1);

    void on_savePBN_clicked();

    void on_samplingSPB_valueChanged(int arg1);

private:
    Ui::Widget *ui;
    QCustomPlot* impulseCPT;
    QCustomPlot* afrCPT;
    QCustomPlot* phaseCPT;
    QCustomPlot* groupCPT;

    void computeFilterCoef(double h[], const qint32 numOfTaps, const double rolloffFactor, const double symbolInterval, const double samplingInterval );

    void drawImpulseResponse(int numOfTaps, double samplingInterval, int factor = 10);
    void drawFreqResponse(std::vector<std::complex<double>>& fft, const int FFT_SIZE, double samplingFreq );
    void drawPhase( std::vector<std::complex<double>>& fft, const int FFT_SIZE, double samplingFreq );
    void drawGroupDelay( int numOfSamps, const int FFT_SIZE, double samplingFreq );
    double* h;
    int numOfTaps;
};
#endif // WIDGET_H
