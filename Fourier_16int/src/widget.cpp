#include "widget.h"
#include "ui_widget.h"
#include "defines.h"

#include <QSettings>
#include <QMessageBox>
#include <QDir>
#include <QFileDialog>
#ifdef QT_DEBUG
 #include <QDebug>
#endif
#include <cmath>
#include <fftw3.h>

/* Массив строк для вывода в infoLBL */
QStringList fileMessages{
    "Обрабатываю файл",
    "Обрабатываю файл.",
    "Обрабатываю файл..",
    "Обрабатываю файл...",
    "Обрабатываю файл....",
    "Обрабатываю файл....."
};

Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
{
    ui->setupUi(this);
    QSettings s;
    //inBuf = nullptr;


    int ind = 0;                                            // Индекс строки для работы таймера
    timer = new QTimer( this );
    connect(timer, &QTimer::timeout, this, [=]() mutable {  // Подключаем таймер к слоту
        ui->infoLBL->setText(fileMessages[ind]);
        ind = (ind + 1) % fileMessages.size();
    });


    ui->infoLBL->setText( fileMessages.at( 0 ) );
    QWidget::restoreGeometry( s.value( "geometry" ).toByteArray() );
    inFileName = s.value( "inFileName",  QDir::homePath() ).toString();
    ui->inFileLED->setText( inFileName );
    outFileName = s.value( "outFileName" ).toString();
    ui->outFileLED->setText( outFileName );

    bool extraSettingsChecked = s.value( "extraSettingsChecked", false ).toBool();
    ui->extraSettingsCHB->setChecked(   extraSettingsChecked    );
    ui->blockSizeLBL->setEnabled(       extraSettingsChecked    );
    ui->blockSizeCMB->setEnabled(       extraSettingsChecked    );
    ui->blockSizeHSL->setEnabled(       extraSettingsChecked    );
    ui->overlapLBL->setEnabled(         extraSettingsChecked    );
    ui->overlapSPB->setEnabled(         extraSettingsChecked    );
    ui->overlapHSL->setEnabled(         extraSettingsChecked    );


    qint32 blockSizeInd = s.value( "blockSize", DEFAULT_INDEX ).toInt();
    ui->blockSizeCMB->setCurrentIndex( blockSizeInd );
    qint32 overlap   = s.value( "overlap", DEFAULT_OVERLAP_SIZE ).toInt();
    ui->overlapSPB->setValue( overlap );
    qint32 samplingIn = s.value( "samplingIn").toInt();
    ui->samplingInSPB->setValue( samplingIn );

    qint32 samplingOut = s.value( "samplingOut").toInt();
    ui->samplingOutSPB->setValue( samplingOut );


    ui->infoLBL->setVisible( false );
}

Widget::~Widget()
{
    QSettings s;
    s.setValue( "geometry", QWidget::saveGeometry() );
    s.setValue( "inFileName",  ui->inFileLED->text() );
    s.setValue( "outFileName",  ui->outFileLED->text());
    s.setValue( "extraSettingsChecked", ui->extraSettingsCHB->isChecked() );
    s.setValue( "blockSize", ui->blockSizeCMB->currentIndex() );
    s.setValue( "overlap",   ui->overlapSPB->value() );
    s.setValue( "samplingIn", ui->samplingInSPB->value() );
    s.setValue( "samplingOut", ui->samplingOutSPB->value() );
    delete ui;
}

void Widget::on_extraSettingsCHB_stateChanged(int arg1)
{
    ui->blockSizeLBL->setEnabled( arg1 );
    ui->blockSizeCMB->setEnabled( arg1 );
    ui->blockSizeHSL->setEnabled( arg1 );
    ui->overlapLBL->setEnabled( arg1 );
    ui->overlapSPB->setEnabled( arg1 );
    ui->overlapHSL->setEnabled( arg1 );
    ui->blockSizeCMB->setCurrentIndex( DEFAULT_INDEX );
    ui->overlapSPB->setValue(   DEFAULT_OVERLAP_SIZE );
    if( arg1 )
        ui->extraSettingsCHB->setText("Дополнительные настройки");
    else
        ui->extraSettingsCHB->setText("Дополнительные настройки (Не применяются, если выключены)");
    //ui->optionsGRB->adjustSize();
}


/*  Функция интерполяции для моно сигналов  */
qint16 interpolate( long double curSampTime, double oldSampValArr[], double DCT[], qint32 numOfSamp ){

    const long double N = static_cast<long double>(numOfSamp);
    const long double t = curSampTime;
    long double sum = DCT[0];        // уже содержит половинку
    for( int k=1; k<N; ++k )
        sum += DCT[k] * cosl(M_PI*(t+0.5L)*k/N);
    sum /= N;
    sum *= INT16_MAX;
    return static_cast<qint16>(sum);

}
void Widget::on_inFilePBN_clicked()
{
    // Открываем диалог выбора файла
    QString s = QFileDialog::getOpenFileName(this, "Выберите файл", inFileName);

    if(!s.isEmpty()){  // Если файл был выбран
        inFileName = s;
        #ifdef QT_DEBUG
            qDebug() << "Input file has been selected " << s;
        #endif
        ui->inFileLED->setText(s);

        QFileInfo fileInfo( inFileName );
        QString fileExt = fileInfo.suffix();
        if( fileExt != "wav")
            return;
        QFile inFile( inFileName );
        if( !inFile.open( QIODevice::ReadOnly ) )
            return;
        /* Если работаем с wav файлом, то считываем его частоту дискретизации */
        inFile.seek( WAV_SAMP_RATE_OFFSET );
        quint32 sampRate;
        qint32 r = inFile.read((char*)&sampRate,sizeof(sampRate));
        if( r != 4 )
            return;
        ui->samplingInSPB->setValue(sampRate);
        inFile.close();
    }
}
void Widget::on_outFilePBN_clicked()
{
    // Открываем диалог выбора файла
    QString s = QFileDialog::getSaveFileName(this, "Выберите файл",outFileName,"WAV файл (*.wav) ;; PCM файл (*.pcm);; Все файлы (*)");

    if(!s.isEmpty()){  // Если файл был выбран
        outFileName = s;
        #ifdef QT_DEBUG
            qDebug() << "Output file has been selected " << s;
        #endif
        ui->outFileLED->setText(s);
    }
}


void Widget::on_mainPBN_clicked()
{

    qint32 blockSize = ui->extraSettingsCHB->isChecked() ? ui->blockSizeCMB->currentText().toInt()    // Размер блока в сэмплах
                                                         : DEFAULT_BLOCK_SIZE;
    qint32 overlap   = ui->extraSettingsCHB->isChecked() ? ui->overlapSPB->value()      // Размер перекрытия в сэмплах
                                                       : DEFAULT_OVERLAP_SIZE;
    qint32 inBufSize = blockSize;                                                       // Размер входного буфера

    //inBuf     = ippsMalloc_32fc( inBufSize );                                           // Выделяем память под буферы
    // Выделяем входной и выходной массивы (реальные числа)
    inBufMono         = (qint16*)  malloc(      sizeof(qint16)  *     inBufSize );
    inBufMonoDouble   = (double*) fftw_malloc( sizeof(double) *     inBufSize );
    DCT               = (double*) fftw_malloc( sizeof(double) *     inBufSize );



    inBufStereo       = (qint16*)  malloc(      sizeof(qint16)  * 2  * inBufSize );
    inBufLeft         = (double*) fftw_malloc(      sizeof(double)     * inBufSize );
    inBufRight        = (double*) fftw_malloc(      sizeof(double)     * inBufSize );
    DCT_Left          = (double*) fftw_malloc( sizeof(double) *      inBufSize );
    DCT_Right         = (double*) fftw_malloc( sizeof(double) *      inBufSize );

    //float*  resStereo = (float*)  malloc(      sizeof(float)  * 2 * inBufSize );
    fftw_plan plan = fftw_plan_r2r_1d(
        inBufSize,
        inBufMonoDouble,
        DCT,
        FFTW_REDFT10,    // DCT-II
        FFTW_ESTIMATE
        );
    fftw_plan planLeft = fftw_plan_r2r_1d(
        inBufSize,
        inBufLeft,
        DCT_Left,
        FFTW_REDFT10,    // DCT-II
        FFTW_ESTIMATE
        );
    fftw_plan planRight = fftw_plan_r2r_1d(
        inBufSize,
        inBufRight,
        DCT_Right,
        FFTW_REDFT10,    // DCT-II
        FFTW_ESTIMATE
        );

    qint32 oldSampRate = ui->samplingInSPB->value();
    qint32 newSampRate = ui->samplingOutSPB->value();
    QMetaObject::invokeMethod( timer,"timeout", Qt::DirectConnection );                 // Вызываем срабатывание таймера

    QFile inFile( inFileName );
    QFileInfo fileInfo( inFileName );
    QString dirPath = fileInfo.absolutePath();
    QString baseName = fileInfo.baseName();
    QString inSuffix = fileInfo.completeSuffix();
    if( outFileName.isEmpty() ){                                                        // Если имя выходного файла пустое, то генерируем его
        if(dirPath.at(dirPath.length()-1) != '/')
            outFileName = QString("%1/%2_%3.wav")
                              .arg(dirPath)
                              .arg(baseName)
                              .arg(newSampRate);
        else
            outFileName = QString("%1%2_%3.wav")
                              .arg(dirPath)
                              .arg(baseName)
                              .arg(newSampRate);

    }
    qint32 numOfChannels = 2;                                                           // Число каналов по-умолчанию равно 2-м

    QFile outFile( outFileName );


    if( inFile.open( QIODevice::ReadOnly ) && outFile.open( QIODevice::WriteOnly ) ){
        ui->outFileLED->setText( outFileName );
        if( inSuffix == "wav"){                                                         // Если входной файл wav-формата, то считываем число каналов
            inFile.seek(WAV_CHANNELS_OFFSET);
            char channels[2];
            inFile.read(channels, 2);
            numOfChannels = static_cast<unsigned char>(channels[0]) |
                                           (static_cast<unsigned char>(channels[1]) << 8);
        }
        #ifdef QT_DEBUG
            qDebug() << "In file has been opened " << inFileName;
        #endif
        timer->start(500);
        ui->infoLBL->show();
        ui->inFilePBN->setEnabled(        false   );
        ui->outFilePBN->setEnabled(       false   );
        ui->outFileCancel->setEnabled(    false   );
        ui->samplingInSPB->setEnabled(    false   );
        ui->samplingOutSPB->setEnabled(   false   );
        ui->samplingInHSL->setEnabled(    false   );
        ui->samplingOutHSL->setEnabled(   false   );
        ui->extraSettingsCHB->setEnabled( false   );
        ui->blockSizeCMB->setEnabled(     false   );
        ui->blockSizeHSL->setEnabled(     false   );
        ui->mainPBN->setEnabled(          false   );
        ui->overlapSPB->setEnabled(       false   );
        ui->overlapHSL->setEnabled(       false   );

        qint32 sampCount;
        if( inSuffix == "wav")
            inFile.seek( WAV_HEADER_OFFSET );
        else
            inFile.seek(0);
        outFile.resize(0);
        QFileInfo fileInfo( outFileName );
        QString fileExt = fileInfo.suffix();
        WavHeader header = {};
        if( fileExt == "wav" ){
            header.chunkSize = 0;
            header.subchunk2Size = 0;
            header.numChannels = (quint16) numOfChannels;
            header.sampleRate = (quint32) newSampRate;
            header.blockAlign = (quint16) numOfChannels * 2;
            header.byteRate = header.sampleRate * header.blockAlign;
            outFile.write( (char*) &header, sizeof( WavHeader ) );
        }
        quint64 totalSamps = 0;
        // Создаём «план» на DCT-II (FFTW_REDFT10 — это DCT-II без нормировки)



        // Работаем в относительном времени
        long double ratio = (long double)oldSampRate/newSampRate;
        long double curSampTime = 0.0L;
        const int MAX_TIME = blockSize - overlap;
        const int NUM_OF_R_SAMPS = blockSize - 2 * overlap;
        if( numOfChannels == 1 ){
            sampCount = inFile.read((char*)inBufMono, inBufSize*sizeof(qint16)) / sizeof(qint16);
            for (int i = 0; i < sampCount; ++i) {
                inBufMonoDouble[i] = (double) inBufMono[i] / INT16_MAX;
            }
            fftw_execute(plan);
            DCT[0] *= 0.5L;
            while ( curSampTime < MAX_TIME ) {
                qint16 outSamp = interpolate( curSampTime, inBufMonoDouble, DCT, std::min( sampCount, inBufSize ) );
                outFile.write( (char*)&outSamp, sizeof(qint16) );
                totalSamps++;
                curSampTime += ratio;
            }
            ui->infoPRB->setValue( 100 * inFile.pos() / inFile.size() );
            qApp->processEvents();
            while(!inFile.atEnd()){
                inFile.seek( inFile.pos() - overlap * 2 * sizeof(qint16) );
                curSampTime -= ( NUM_OF_R_SAMPS );
                sampCount = inFile.read((char*)inBufMono, inBufSize*sizeof(qint16)) / sizeof(qint16);
                for (int i = 0; i < sampCount; ++i) {
                    inBufMonoDouble[i] = (double) inBufMono[i]/ INT16_MAX;
                }

                // Обработка последнего блока
                if (sampCount < inBufSize) {
                    for (int i = sampCount; i < inBufSize; ++i) {
                        inBufMonoDouble[i] = 0.0L;
                    }
                    fftw_execute(plan);
                    DCT[0] *= 0.5L;
                    while ( curSampTime < inBufSize ) {
                        qint16 outSamp = interpolate( curSampTime, inBufMonoDouble, DCT, std::min( sampCount, inBufSize ) );
                        outFile.write( (char*)&outSamp, sizeof(qint16) );
                        totalSamps++;
                        curSampTime += ratio;
                    }
                    ui->infoPRB->setValue( 100 * inFile.pos() / inFile.size() );
                    qApp->processEvents();
                    break;
                }
                fftw_execute(plan);
                DCT[0] *= 0.5L;
                while ( curSampTime < MAX_TIME ) {
                    qint16 outSamp =  interpolate( curSampTime, inBufMonoDouble, DCT, std::min( sampCount, inBufSize ) );
                    outFile.write( (char*)&outSamp, sizeof(qint16) );
                    totalSamps++;
                    curSampTime += ratio;
                }
                ui->infoPRB->setValue( 100 * inFile.pos() / inFile.size() );
                qApp->processEvents();
            }
        } else {
            sampCount = inFile.read((char*)inBufStereo, inBufSize*2*sizeof(qint16)) / sizeof(qint16);
            for (int i = 0; i < sampCount; i++) {
                i % 2 == 0 ? inBufLeft[i >> 1]       = (double) inBufStereo[i] / INT16_MAX
                           : inBufRight[ (i-1) >> 1] = (double) inBufStereo[i] / INT16_MAX;
            }
            fftw_execute(planLeft);
            fftw_execute(planRight);
            DCT_Left[0] *= 0.5L;
            DCT_Right[0] *= 0.5L;

            while ( curSampTime < MAX_TIME ) {
                qint16 outSampLeft =   interpolate( curSampTime, inBufLeft, DCT_Left, std::min( sampCount, inBufSize ) );
                outFile.write( (char*)&outSampLeft, sizeof(qint16) );
                qint16 outSampRight =  interpolate( curSampTime, inBufRight, DCT_Right, std::min( sampCount, inBufSize ) );
                outFile.write( (char*)&outSampRight, sizeof(qint16) );
                totalSamps += 2;
                curSampTime += ratio;
            }
            ui->infoPRB->setValue( 100 * inFile.pos() / inFile.size() );
            qApp->processEvents();
            while(!inFile.atEnd()){
                inFile.seek( inFile.pos() - overlap * 2 * 2 * sizeof(qint16) );
                curSampTime -= ( NUM_OF_R_SAMPS );
                sampCount = inFile.read((char*)inBufStereo, inBufSize*2*sizeof(qint16)) / sizeof(qint16);
                for (int i = 0; i < sampCount; i ++) {
                    i % 2 == 0 ? inBufLeft[i >> 1]       = (double) inBufStereo[i] / INT16_MAX
                               : inBufRight[ (i-1) >> 1] = (double) inBufStereo[i] / INT16_MAX;
                }

                if (sampCount < inBufSize) {
                    for (int i = sampCount; i < inBufSize*2; i ++) {
                        i % 2 == 0 ? inBufLeft[i >> 1]       = 0.0f
                                   : inBufRight[ (i-1) >> 1] = 0.0f;
                    }
                    fftw_execute(planLeft);
                    fftw_execute(planRight);
                    DCT_Left[0] *= 0.5L;
                    DCT_Right[0] *= 0.5L;
                    while ( curSampTime < inBufSize ) {
                        qint16 outSampLeft =  interpolate( curSampTime, inBufLeft, DCT_Left, std::min( sampCount, inBufSize ) );
                        outFile.write( (char*)&outSampLeft, sizeof(qint16) );
                        qint16 outSampRight =  interpolate( curSampTime, inBufRight, DCT_Right, std::min( sampCount, inBufSize ) );
                        outFile.write( (char*)&outSampRight, sizeof(qint16) );
                        totalSamps += 2;
                        curSampTime += ratio;
                    }
                    ui->infoPRB->setValue( 100 * inFile.pos() / inFile.size() );
                    qApp->processEvents();
                    break;
                }
                fftw_execute(planLeft);
                fftw_execute(planRight);
                DCT_Left[0] *= 0.5L;
                DCT_Right[0] *= 0.5L;
                while ( curSampTime < MAX_TIME ) {
                    qint16 outSampLeft =  interpolate( curSampTime, inBufLeft, DCT_Left, std::min( sampCount, inBufSize ) );
                    outFile.write( (char*)&outSampLeft, sizeof(qint16) );
                    qint16 outSampRight =  interpolate( curSampTime, inBufRight, DCT_Right, std::min( sampCount, inBufSize ) );
                    outFile.write( (char*)&outSampRight, sizeof(qint16) );
                    totalSamps += 2;
                    curSampTime += ratio;
                }
                ui->infoPRB->setValue( 100 * inFile.pos() / inFile.size() );
                qApp->processEvents();
            }
        }


        if( fileExt == "wav" ){
            quint32 subchunk2size = totalSamps  * 2;
            quint32 chunkSize = 36 + subchunk2size;
            header.chunkSize = chunkSize;
            header.subchunk2Size = subchunk2size;
            outFile.seek(0);
            outFile.write( (char*) &header, sizeof( WavHeader ) );
        }
    } else {
        if (!inFile.isOpen())
           QMessageBox::critical(this,"Ошибка","Ошибка открытия входного файла:\n" + inFile.errorString());
        else
           QMessageBox::critical(this,"Ошибка","Ошибка открытия выходного файла:\n" + outFile.errorString());
    }
    timer->stop();
    ui->infoLBL->setText(             "Файл успешно обработан");
    ui->inFilePBN->setEnabled(        true   );
    ui->outFilePBN->setEnabled(       true   );
    ui->outFileCancel->setEnabled(    true   );
    ui->samplingInSPB->setEnabled(    true   );
    ui->samplingOutSPB->setEnabled(   true   );
    ui->samplingInHSL->setEnabled(    true   );
    ui->samplingOutHSL->setEnabled(   true   );
    ui->extraSettingsCHB->setEnabled( true   );
    ui->mainPBN->setEnabled(          true   );
    ui->blockSizeCMB->setEnabled(     ui->extraSettingsCHB->isChecked()   );
    ui->blockSizeHSL->setEnabled(     ui->extraSettingsCHB->isChecked()   );
    ui->overlapSPB->setEnabled(       ui->extraSettingsCHB->isChecked()   );
    ui->overlapHSL->setEnabled(       ui->extraSettingsCHB->isChecked()   );

    inFile.close();
    outFile.close();

    free(              inBufMono       );
    fftw_free(         inBufMonoDouble );
    fftw_free(         DCT             );
    fftw_destroy_plan( plan            );



    free(              inBufStereo );
    fftw_free (        inBufLeft );
    fftw_free (        inBufRight );
    fftw_free (        DCT_Left );
    fftw_free (        DCT_Right );
    fftw_destroy_plan( planLeft);
    fftw_destroy_plan( planRight);

}
void Widget::on_outFileCancel_clicked()
{
    ui->outFileLED->setText("");
    outFileName = "";
}




void Widget::on_overlapSPB_valueChanged(int arg1)
{
    qint32 blockSize = ui->blockSizeCMB->currentText().toInt();
    if( arg1 >= blockSize )
        ui->overlapSPB->setValue( blockSize-1 );
}


void Widget::on_blockSizeHSL_valueChanged(int value)
{
    ui->blockSizeCMB->setCurrentIndex( value - ui->blockSizeHSL->minimum() );
}


void Widget::on_blockSizeCMB_currentTextChanged(const QString &arg1)
{
    qint32 blockSize = arg1.toInt();
    int p = 16;
    int power = 4;
    while( p != blockSize ){
        p <<= 1;
        power++;
    }

    ui->blockSizeHSL->setValue( power );


    qint32 MAX_OVERLAP = blockSize % 2 == 0 ? blockSize/2-1 : blockSize/2;
    ui->overlapHSL->setMaximum( MAX_OVERLAP );
    ui->overlapSPB->setMaximum( MAX_OVERLAP );
    qint32 overlap = ui->overlapSPB->value();
    if( overlap > MAX_OVERLAP )
        ui->overlapSPB->setValue( MAX_OVERLAP );

}

