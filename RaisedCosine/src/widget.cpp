#include "widget.h"
#include "ui_widget.h"
#include "qcustomplot.h"
#include <QSettings>
#include <QSplitter>
#include <cmath>
#include <QDebug>
#include "tinyspline.h"
#include <complex.h>
#include <vector>
#include <QFileDialog>
#include "fft.h"

void spline_interp_c(const double *h, int N, int factor, double samplingInterval,
                     double *x_interp, double *h_interp)
{
    int M = (N - 1) * factor + 1;

    /* 1) Создаём B‑сплайн степени 3 с N контрольными точками, clamped */
    tsBSpline spline;
    tsStatus status;
    ts_bspline_new((size_t)N,      /* num_control_points */
                   1,               /* dimension = 1 (скалярный сплайн) */
                   3,               /* degree = 3 (кубический) */
                   TS_CLAMPED,      /* тип узлов (натуральный, «clamped») */
                   &spline,         /* выходной объект сплайна */
                   &status);        /* статус создания */
    if( status.code !=  TS_SUCCESS)
        return;


    /* 2) Заполняем массив контрольных точек (линейно, h[0], h[1], …) */
    /*    tinyspline требует float‑чисел (ts_real), но double тоже сойдёт */
    tsReal *ctrl = (tsReal*)malloc(sizeof(tsReal) * N * 1);
    for (int i = 0; i < N; ++i)
        ctrl[i] = (tsReal)h[i];
    ts_bspline_set_control_points(&spline, ctrl, &status);
    if( status.code !=  TS_SUCCESS)
        return;
    /* 3) Для каждой новой точки вычисляем параметр u ∈ [0..1] и eval */
    for (int i = 0; i < M; ++i) {
        tsReal u = (tsReal)i / (M - 1);
        tsDeBoorNet net = ts_deboornet_init();
        ts_bspline_eval(&spline, u, &net, &status);
        if (status.code != TS_SUCCESS) {
            fprintf(stderr, "Ошибка ts_bspline_eval[%d]: %s\n", i, status.message);
            ts_deboornet_free(&net);
            break;
        }
        /* Берём указатель на результат (масштаб dim=1 → единственное число) */
        const tsReal *res = ts_deboornet_result_ptr(&net);
        h_interp[i] = (double)res[0];
        x_interp[i] = (double)u * (N - 1) * samplingInterval;
        ts_deboornet_free(&net);
    }

    /* 4) Чистим за собой */
    free(ctrl);
    ts_bspline_free(&spline);
}
Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
{
    ui->setupUi(this);
    QSettings s;

    double rolloffFactor = s.value("rolloffFactor").toDouble();
    ui->rolloffDSPB->setValue( rolloffFactor );
    qint32 samplingFreq = s.value("samplingFreq").toInt();
    ui->samplingSPB->setValue( samplingFreq );
    qint32 numOfTaps = s.value("numOfTaps").toInt();
    ui->numOfTapsSPB->setValue( numOfTaps );
    qint32 symbolRate = s.value("symbolRate").toInt();
    ui->symbolRateSPB->setValue( symbolRate );
    QWidget::restoreGeometry(s.value("geometry").toByteArray());

    h = nullptr;
    numOfTaps = 0;
    impulseCPT = new QCustomPlot( this );
    afrCPT     = new QCustomPlot( this );
    phaseCPT   = new QCustomPlot( this );
    groupCPT   = new QCustomPlot( this );

    QSplitter* verticalSPL = new QSplitter( Qt::Vertical, this );
    QSplitter* upSPL = new QSplitter( this );
    QSplitter* downSPL = new QSplitter( this );

    verticalSPL->addWidget( upSPL );
    verticalSPL->addWidget( downSPL );

    upSPL->addWidget( impulseCPT );
    upSPL->addWidget(  afrCPT );
    downSPL->addWidget( phaseCPT );
    downSPL->addWidget( groupCPT );

    ui->verticalLayout_2->insertWidget( 1, verticalSPL );


}

Widget::~Widget()
{
    QSettings s;
    s.setValue("symbolRate", ui->symbolRateSPB->value() );
    s.setValue("rolloffFactor", ui->rolloffDSPB->value() );
    s.setValue("samplingFreq", ui->samplingSPB->value() );
    s.setValue("numOfTaps", ui->numOfTapsSPB->value() );
    s.setValue("geometry", QWidget::saveGeometry() );
    if( h != nullptr )
        delete[] h;
    delete ui;
}

void Widget::on_computePBN_clicked()
{
    double symbolInterval   = 1.0/ui->symbolRateSPB->value();
    double rolloffFactor    = ui->rolloffDSPB->value();
    double samplingInterval = 1.0/ui->samplingSPB->value();
    qint32 numOfTaps        = ui->numOfTapsSPB->value();

    this->numOfTaps = numOfTaps;
    if( h != nullptr )
        delete[] h;
    h = new double[numOfTaps];
    computeFilterCoef(h, numOfTaps, rolloffFactor, symbolInterval, samplingInterval );
    double norm = 0.0;
    for (int i = 0; i < numOfTaps; ++i)
        norm += h[i];

    if( norm != 0.0 )
        for (int i = 0; i < numOfTaps; ++i)
            h[i] /= norm;


    drawImpulseResponse( numOfTaps, samplingInterval, 20 );

    std::vector<double> h_pad;
    for (int i = 0; i < numOfTaps; ++i) {
        h_pad.push_back(h[i]);
    }
    const int FFT_SIZE = 1<<12;
    while ( h_pad.size() < FFT_SIZE ) {
        h_pad.push_back(0.0);
    }
    std::vector<std::complex<double>> fft_res( FFT_SIZE );

    const char* error = nullptr;
    bool ok = simple_fft::FFT(h_pad, fft_res, FFT_SIZE, error);
    if (!ok) {
        qDebug() << "Ошибка FFT: " << error << '\n';
    }
    drawFreqResponse( fft_res, FFT_SIZE, (double)ui->samplingSPB->value() );
    drawPhase( fft_res, FFT_SIZE, (double)ui->samplingSPB->value() );
    drawGroupDelay( numOfTaps,FFT_SIZE,(double)ui->samplingSPB->value() );


}

void Widget::computeFilterCoef(double h[], const qint32 numOfTaps, const double rolloffFactor, const double symbolInterval, const double samplingInterval)
{
    const qint32 N = numOfTaps;
    // В оригинале T_delta = 1/Fs = samplingInterval
    const double T_delta = samplingInterval;

    // Для правильного сравнения с нулём и краевыми точками
    const double eps = 1e-20;
    // Предрасчитаем константы
    const double pi = M_PI;
    const double denomEdge = symbolInterval / (2.0 * rolloffFactor);

    for (qint32 x = 0; x < N; ++x) {
        double t;
        if (N % 2 != 0) {
            t = (x - N / 2) * T_delta;
        } else {
            t = (x - (N - 1) / 2.0) * T_delta;
        }

        // Теперь сами условия
        if (std::fabs(t) < eps) {
            h[x] = 1.0;
        }
        else if ( rolloffFactor != 0.0 && std::fabs(std::fabs(t) - denomEdge) < eps) {
            // t == ± Ts/(2*alpha)
            // В Python два одинаковых случая для + и −
            double arg = pi * t / symbolInterval;
            double sinc = std::sin(arg) / arg;
            h[x] = (pi / 4.0) * sinc;
        }
        else {
            // общий случай
            double arg1 = pi * t / symbolInterval;
            double arg2 = pi * rolloffFactor * t / symbolInterval;
            double sinc = std::sin(arg1) / arg1;
            double denom = 1.0 - std::pow(2.0 * rolloffFactor * t / symbolInterval, 2);
            h[x] = sinc * (std::cos(arg2) / denom);
        }
    }
}

void Widget::drawImpulseResponse(int numOfTaps, double samplingInterval, int factor)
{
    int M = (numOfTaps - 1) * factor + 1;
    double* h_interp = new double[M];
    double* x_interp = new double[M];

    spline_interp_c(h, numOfTaps, factor, samplingInterval, x_interp, h_interp);
    double coef = samplingInterval*1000*numOfTaps/M;
    QVector<double> qx(M), qy(M);
    for (int i = 0; i < M; ++i) {
        qx[i] = coef*i;
        qy[i] = h_interp[i];
    }

    // 2) Если график ещё не добавлен, добавим; иначе очистим старые данные
    if (impulseCPT->graphCount() == 0)
        impulseCPT->addGraph();
    // очищать не обязательно — setData перезапишет
    impulseCPT->graph(0)->setData(qx, qy);
    //impulseCPT->setAntialiasedElement( QCP::aePlottables );

    // 3) Настроим оси (чтобы увидеть все точки)
    impulseCPT->xAxis->setLabel("Время, мc");
    impulseCPT->yAxis->setLabel("Импульсная характеристика");
    impulseCPT->xAxis->setRange(0, M - 1);
    auto minmaxY = std::minmax_element(h_interp, h_interp+M);
    impulseCPT->xAxis->setRange(0, (M-1)*coef);
    impulseCPT->yAxis->setRange(*minmaxY.first, *minmaxY.second);

    impulseCPT->replot();

    delete[]h_interp;
    delete[]x_interp;
}

void Widget::drawFreqResponse(std::vector<std::complex<double> > &fft, const int FFT_SIZE, double samplingFreq)
{
    const int halfN  = FFT_SIZE/2 + 1; // включая Nyquist

    // Подготовим QVector для QCustomPlot
    QVector<double> freq(halfN), magnitude(halfN);

    // Формируем массив частот и амплитуд (в дБ)
    double maxMag = 0.0;
    for(int i = 0; i < halfN; ++i)
    {
        freq[i]      = (samplingFreq / FFT_SIZE) * i;
        double mag   = std::abs(fft[i]);
        magnitude[i] = 20.0 * std::log10( std::max(mag, 1e-20) );
        if(magnitude[i] > maxMag)
            maxMag = magnitude[i];
    }
    // 2) Нормировка: найдём максимум и вычтем его из всех значений
    //double maxMag = *std::max_element(magnitude.begin(), magnitude.end());
    for(int i = 0; i < halfN; ++i)
    {
        magnitude[i] -= maxMag;
    }
    // Очищаем предыдущие графики
    afrCPT->clearGraphs();
    afrCPT->addGraph();
    //afrCPT->setAntialiasedElement( QCP::aePlottables );
    afrCPT->graph(0)->setData(freq, magnitude);
    afrCPT->xAxis->setLabel("Частота, Гц");
    afrCPT->yAxis->setLabel("Амплитуда, дБ");
    afrCPT->yAxis->setRange(-100, 0);
    afrCPT->xAxis->setRange(0,samplingFreq/2);
    afrCPT->replot();
}

void Widget::drawPhase(std::vector<std::complex<double> > &fft, const int FFT_SIZE, double samplingFreq)
{
    const int halfN  = FFT_SIZE/2 + 1; // включая Nyquist

    // Подготовим QVector для QCustomPlot
    QVector<double> freq(halfN), phase(halfN);

    for(int i = 0; i < halfN; ++i)
    {
        freq[i]      = (samplingFreq / FFT_SIZE) * i;
        double ph   = std::arg( fft[i] );
        phase[i] = ph;
    }
    // Очищаем предыдущие графики
    phaseCPT->clearGraphs();
    phaseCPT->addGraph();
    phaseCPT->graph(0)->setData(freq, phase);
    phaseCPT->xAxis->setLabel("Частота, Гц");
    phaseCPT->yAxis->setLabel("Фаза, рад");
    phaseCPT->yAxis->setRange( -3.14, 3.14 );
    phaseCPT->xAxis->setRange( 0, samplingFreq/2);
    phaseCPT->replot();
}

void Widget::drawGroupDelay( int numOfSamps, const int FFT_SIZE, double samplingFreq )
{
    const int halfN  = FFT_SIZE/2 + 1; // включая Nyquist

    // Подготовим QVector для QCustomPlot
    QVector<double> freq(halfN), delay(halfN);

    double d = 1.0/samplingFreq*(numOfSamps-1)/2.0;
    for(int i = 0; i < halfN; ++i)
    {
        freq[i]  = (samplingFreq / FFT_SIZE) * i;
        delay[i] = d;
    }
    // Очищаем предыдущие графики
    groupCPT->clearGraphs();
    groupCPT->addGraph();
    groupCPT->graph(0)->setData(freq, delay);
    groupCPT->xAxis->setLabel("Частота, Гц");
    groupCPT->yAxis->setLabel("Групповая задержка, с");
    groupCPT->yAxis->setRange( d/2.0, d*2.0 );
    groupCPT->xAxis->setRange( 0, samplingFreq/2);
    groupCPT->replot();
}


void Widget::on_rolloffHSL_valueChanged(int value)
{

    // где-то в коде:
    double v = double(value) / ui->rolloffHSL->maximum();
    ui->rolloffDSPB->setValue( v );
}


void Widget::on_rolloffDSPB_valueChanged(double arg1)
{
    ui->rolloffHSL->setValue(int(arg1*ui->rolloffHSL->maximum()));

    int maxVal = int( double(ui->samplingSPB->value())/(1.0+arg1) );
    ui->symbolRateSPB->setMaximum( maxVal );
    ui->symbolRateHSL->setMaximum( maxVal );
    if(ui->symbolRateSPB->value() > maxVal )
        ui->symbolRateSPB->setValue(maxVal);

}


QString formatCoeff(double val) {
    // Спецслучай нуля
    if (val == 0.0) {
        return QString("0.%1E+00")
            .arg(QString(18, '0'));  // "0.000...E+00"
    }

    // Получаем строку вида "[-]d.dddddddddddddddddE±XX"
    char buf[64];
    std::sprintf(buf, "%.18E", val);
    QString s(buf);

    // Определяем знак
    bool negative = s.startsWith('-');
    if (negative)
        s.remove(0, 1);

    // Выделяем мантиссу и экспоненту
    int ePos   = s.indexOf('E');
    QString mant = s.left(ePos);           // "d.ddddddddddddddddd"
    int origExp = s.mid(ePos + 1).toInt(); // числа от формата E±XX

    // Сдвигаем десятичную точку на одну позицию влево:
    //   из "d.xxxxx..." -> "0.dxxxxx..."
    QChar firstDigit = mant[0];
    QString fracPart = mant.mid(2);        // 18 цифр после точки
    QString newMant = QString("0.") + firstDigit + fracPart;

    // Увеличиваем экспоненту на 1
    int newExp = origExp + 1;
    // Форматируем E±XX (всегда 2 цифры порядка)
    QString expStr = QString::asprintf("E%+03d", newExp);

    // Формируем итоговую строку
    QString out = newMant + expStr;

    // Для отрицательных убираем ведущий "0", оставляем "-.xxxxx..."
    if (negative) {
        out.remove(0, 1);
        out.prepend('-');
    }

    return out;
}
void Widget::on_savePBN_clicked()
{
    if( h == nullptr ){
        QPushButton* btn = ui->savePBN;
        QString originalText = "Сохранить фильтр";
        if(btn->text() == originalText ){
            btn->setText("Сначала рассчитайте фильтр");

            QTimer::singleShot(1500, btn, [btn, originalText]() {
                btn->setText(originalText);
            });
        }
        return;
    }
    const QString filter = "Файл коэффициентов фильтра (*.flt)";
    QSettings s;
    QString lastPath = s.value("lastSavePath", QDir::homePath()).toString();
    QString fileName = QFileDialog::getSaveFileName(
        this,
        tr("Сохранить фильтр как…"),      // заголовок окна
        lastPath,   // стартовая папка - из настроек
        filter,
        nullptr,
        QFileDialog::Options()
        );

    if ( fileName.isEmpty() )
        return;
    // Сохраняем путь, который выбрал пользователь (папку, а не файл)
    QFileInfo fi(fileName);
    s.setValue("lastSavePath", fi.absolutePath());
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, tr("Ошибка"), tr("Не удалось открыть файл для записи"));
        return;
    }

    QTextStream out(&file);
    out.setLocale(QLocale::C);

    // --- Заголовок ---
    out << "FILTER COEFFICIENT FILE\n";
    out << "FIR DESIGN          FLOATING POINT\n";
    double fs = (double)ui->samplingSPB->value();
    int exp10 = 0;
    exp10 = static_cast<int>(std::floor(std::log10(fs))) + 1;
    double mantissa = fs / std::pow(10.0, exp10);
    QString mantStr = QString::number(mantissa, 'f', 6);
    QString expStr = QString::asprintf("E%+03d", exp10);
    out << "SAMPLING FREQUENCY          "
        << mantStr
        << expStr
        << " HERTZ\n";
    // --- Сведения о количестве коэффициентов и битах квантования ---
    int numOfTaps = this->numOfTaps;
    out << QString("  %1                  /* number of taps in decimal     */\n")
               .arg(numOfTaps);
    out << QString("  %1                  /* number of taps in hexadecimal */\n")
               .arg(QString::number(numOfTaps, 16).toUpper());
    out << QString("  %1                  /* number of bits in quantized coefficients (dec) */\n")
               .arg(52);
    out << QString("  %1                  /* number of bits in quantized coefficients (hex) */\n")
               .arg(34);
    for (int i = 0; i < numOfTaps; ++i) {
        //QString idxStr = QString("%1").arg(i, 5, 10, QLatin1Char(' '));           // номер отсчёта, ширина 5
        QString coeff  = formatCoeff(h[i]);                                      // ваша функция из прошлого шага
        QString valStr = QString("%1").arg(coeff, 23, QLatin1Char(' '));         // коэффициент, ширина 23 (подстроить, если нужно)

        out << valStr << " "                 // коэффициент
            << QString("/* coefficient of tap %1 */\n")
                   .arg(i, 5, 10, QLatin1Char(' '));  // комментарий с тем же номером
    }

}


void Widget::on_samplingSPB_valueChanged( int arg1 )
{
    int maxVal = int( double(arg1)/(1.0+ui->rolloffDSPB->value()) );
    ui->symbolRateSPB->setMaximum( maxVal );
    ui->symbolRateHSL->setMaximum( maxVal );
    if(ui->symbolRateSPB->value() > maxVal )
        ui->symbolRateSPB->setValue(maxVal);
}

