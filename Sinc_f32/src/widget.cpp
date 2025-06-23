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

/* Массив строк для вывода в infoLBL */
QStringList fileMessages{
    "Обрабатываю файл",
    "Обрабатываю файл.",
    "Обрабатываю файл..",
    "Обрабатываю файл...",
    "Обрабатываю файл....",
    "Обрабатываю файл....."
};
/* Я думаю, очевидно, что это*/
FORCEINLINE long double sinc( long double x ){
    return ( x == 0.0L ? 1.0L : std::sin( M_PI * x ) / ( M_PI * x ) );
}

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
    ui->blockSizeSPB->setEnabled(       extraSettingsChecked    );
    ui->blockSizeHSL->setEnabled(       extraSettingsChecked    );
    ui->overlapLBL->setEnabled(         extraSettingsChecked    );
    ui->overlapSPB->setEnabled(         extraSettingsChecked    );
    ui->overlapHSL->setEnabled(         extraSettingsChecked    );


    qint32 blockSize = s.value( "blockSize", DEFAULT_BLOCK_SIZE ).toInt();
    ui->blockSizeSPB->setValue( blockSize );
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
    s.setValue( "blockSize", ui->blockSizeSPB->value() );
    s.setValue( "overlap",   ui->overlapSPB->value() );
    s.setValue( "samplingIn", ui->samplingInSPB->value() );
    s.setValue( "samplingOut", ui->samplingOutSPB->value() );
    delete ui;
}

void Widget::on_extraSettingsCHB_stateChanged(int arg1)
{
    ui->blockSizeLBL->setEnabled( arg1 );
    ui->blockSizeSPB->setEnabled( arg1 );
    ui->blockSizeHSL->setEnabled( arg1 );
    ui->overlapLBL->setEnabled( arg1 );
    ui->overlapSPB->setEnabled( arg1 );
    ui->overlapHSL->setEnabled( arg1 );
    ui->blockSizeSPB->setValue( DEFAULT_BLOCK_SIZE );
    ui->overlapSPB->setValue(   DEFAULT_OVERLAP_SIZE );
    if( arg1 )
        ui->extraSettingsCHB->setText("Дополнительные настройки");
    else
        ui->extraSettingsCHB->setText("Дополнительные настройки (Не применяются, если выключены)");
    //ui->optionsGRB->adjustSize();
}

/*  Функция интерполяции для стерео сигналов    */
//Ipp32fc interpolate( long double curSampTime, qint32 oldSampRate, const long double* oldSampTimeArr, const Ipp32fc* oldSampValArr, qint32 numOfSamp )
//{
//    long double outSampRe = 0.0L;
//    long double outSampIm = 0.0L;
//    for ( qint32 i = 0; i < numOfSamp; i++)
//    {
//        long double  w = sinc( M_PI * oldSampRate * ( curSampTime - oldSampTimeArr[i] ) );
//        outSampRe += oldSampValArr[i].re * w;
//        outSampIm += oldSampValArr[i].im * w;
//    }
//    return {(float)outSampRe, (float)outSampIm };
//}
/*  Функция интерполяции для моно сигналов  */
long double interpolate( long double curSampTime, float oldSampValArr[], qint32 numOfSamp ){
    long double outSamp = 0.0L;
    for ( qint32 i = 0; i < numOfSamp; i++)
    {
        long double  w = sinc(  curSampTime - i  );
        outSamp += oldSampValArr[i] * w;
    }
    return outSamp;
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

    qint32 blockSize = ui->extraSettingsCHB->isChecked() ? ui->blockSizeSPB->value()    // Размер блока в сэмплах
                                                         : DEFAULT_BLOCK_SIZE;
    qint32 overlap   = ui->extraSettingsCHB->isChecked() ? ui->overlapSPB->value()      // Размер перекрытия в сэмплах
                                                       : DEFAULT_OVERLAP_SIZE;
    qint32 inBufSize = blockSize;                                                       // Размер входного буфера

    //inBuf     = ippsMalloc_32fc( inBufSize );                                           // Выделяем память под буферы
    inBufMono = (float*) malloc(inBufSize*sizeof(float));

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
        ui->blockSizeSPB->setEnabled(     false   );
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
            header.byteRate = header.sampleRate * header.blockAlign;
            outFile.write( (char*) &header, sizeof( WavHeader ) );
        }
        quint64 totalSamps = 0;
        sampCount = inFile.read((char*)inBufMono, inBufSize*sizeof(float)) / sizeof(float);
        // Работаем в относительном времени
        long double ratio = (long double)oldSampRate/newSampRate;
        long double curSampTime = 0.0L;
        const int MAX_TIME = blockSize - overlap;
        const int NUM_OF_R_SAMPS = blockSize - 2 * overlap;
        while ( curSampTime < MAX_TIME ) {
            float outSamp = (float) interpolate( curSampTime, inBufMono, std::min( sampCount, inBufSize ) );
            outFile.write( (char*)&outSamp, sizeof(float) );
            totalSamps++;
            curSampTime += ratio;
        }
        ui->infoPRB->setValue( 100 * inFile.pos() / inFile.size() );
        qApp->processEvents();
        while(!inFile.atEnd()){
            inFile.seek( inFile.pos() - overlap * 2 * sizeof(float) );
            curSampTime -= ( NUM_OF_R_SAMPS );
            sampCount = inFile.read((char*)inBufMono, inBufSize*sizeof(float)) / sizeof(float);

            // Обработка последнего блока
            if (sampCount < inBufSize) {
                while ( curSampTime < blockSize - 1 ) {
                    float outSamp = (float) interpolate( curSampTime, inBufMono, std::min( sampCount, inBufSize ) );
                    outFile.write( (char*)&outSamp, sizeof(float) );
                    totalSamps++;
                    curSampTime += ratio;
                }
                ui->infoPRB->setValue( 100 * inFile.pos() / inFile.size() );
                qApp->processEvents();
                break;
            }
            while ( curSampTime < MAX_TIME ) {
                float outSamp = (float) interpolate( curSampTime, inBufMono, std::min( sampCount, inBufSize ) );
                outFile.write( (char*)&outSamp, sizeof(float) );
                totalSamps++;
                curSampTime += ratio;
            }
            ui->infoPRB->setValue( 100 * inFile.pos() / inFile.size() );
            qApp->processEvents();
        }


        if( fileExt == "wav" ){
            quint32 subchunk2size = totalSamps * numOfChannels * 4;
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
    ui->infoLBL->setText("Файл успешно обработан");
    ui->inFilePBN->setEnabled(        true   );
    ui->outFilePBN->setEnabled(       true   );
    ui->outFileCancel->setEnabled(    true   );
    ui->samplingInSPB->setEnabled(    true   );
    ui->samplingOutSPB->setEnabled(   true   );
    ui->samplingInHSL->setEnabled(    true   );
    ui->samplingOutHSL->setEnabled(   true   );
    ui->extraSettingsCHB->setEnabled( true   );
    ui->mainPBN->setEnabled(          true   );
    ui->blockSizeSPB->setEnabled(     ui->extraSettingsCHB->isChecked()   );
    ui->blockSizeHSL->setEnabled(     ui->extraSettingsCHB->isChecked()   );
    ui->overlapSPB->setEnabled(       ui->extraSettingsCHB->isChecked()   );
    ui->overlapHSL->setEnabled(       ui->extraSettingsCHB->isChecked()   );

    inFile.close();
    outFile.close();

    free(inBufMono);
}




void Widget::on_outFileCancel_clicked()
{
    ui->outFileLED->setText("");
    outFileName = "";
}




void Widget::on_overlapSPB_valueChanged(int arg1)
{
    qint32 blockSize = ui->blockSizeSPB->value();
    if( arg1 >= blockSize )
        ui->overlapSPB->setValue( blockSize-1 );
}


void Widget::on_blockSizeSPB_valueChanged(int arg1)
{
    qint32 MAX_OVERLAP = arg1 % 2 == 0 ? arg1/2-1 : arg1/2;
    ui->overlapHSL->setMaximum( MAX_OVERLAP );
    ui->overlapSPB->setMaximum( MAX_OVERLAP );
    qint32 overlap = ui->overlapSPB->value();
    if( overlap > MAX_OVERLAP )
        ui->overlapSPB->setValue( MAX_OVERLAP );
}

