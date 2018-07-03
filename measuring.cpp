#include "measuring.h"
#include "ui_measuring.h"
#include "qcustomplot.h"
#include "persistence1d.hpp"
#include "spline.h"

#include <QPixmap>
#include <QString>
#include <QMouseEvent>
#include <QPainter>
#include <iostream>
#include <vector>
#include <QDebug>

#include <opencv2/opencv.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>


#define PI 3.14159

std::vector<cv::Point> *linePoints,         //point from mouse move
*smooth_linePoints;  //point that smoothed
std::vector<std::vector<cv::Point3i>> result_line;

cv::Point pre_A,pre_B,A,B,line_begin;

int MAX_KERNEL_LENGTH;


inline QImage  cvMatToQImage( const cv::Mat &inMat )
{
    switch ( inMat.type() )
    {
    // 8-bit, 4 channel
    case CV_8UC4:
    {
        QImage image( inMat.data,
                      inMat.cols, inMat.rows,
                      static_cast<int>(inMat.step),
                      QImage::Format_ARGB32 );

        return image;
    }

        // 8-bit, 3 channel
    case CV_8UC3:
    {
        QImage image( inMat.data,
                      inMat.cols, inMat.rows,
                      static_cast<int>(inMat.step),
                      QImage::Format_RGB888 );

        return image.rgbSwapped();
    }

        // 8-bit, 1 channel
    case CV_8UC1:
    {
        QImage image( inMat.data,
                      inMat.cols, inMat.rows,
                      static_cast<int>(inMat.step),
                      QImage::Format_Grayscale8 );
        static QVector<QRgb>  sColorTable;

        // only create our color table the first time
        if ( sColorTable.isEmpty() )
        {
            sColorTable.resize( 256 );

            for ( int i = 0; i < 256; ++i )
            {
                sColorTable[i] = qRgb( i, i, i );
            }
        }

        image.setColorTable( sColorTable );

        return image;
    }

    default:

        break;
    }

    return QImage();
}

inline QPixmap cvMatToQPixmap( const cv::Mat &inMat )
{
    return QPixmap::fromImage( cvMatToQImage( inMat ) );
}

inline cv::Mat QImageToCvMat( const QImage &inImage, bool inCloneImageData = true )
{
    switch ( inImage.format() )
    {
    // 8-bit, 4 channel
    case QImage::Format_ARGB32:
    case QImage::Format_ARGB32_Premultiplied:
    {
        cv::Mat  mat( inImage.height(), inImage.width(),
                      CV_8UC4,
                      const_cast<uchar*>(inImage.bits()),
                      static_cast<size_t>(inImage.bytesPerLine())
                      );

        return (inCloneImageData ? mat.clone() : mat);
    }

        // 8-bit, 3 channel
    case QImage::Format_RGB32:
    {
        if ( !inCloneImageData )
        {
            qWarning() << "ASM::QImageToCvMat() - Conversion requires cloning so we don't modify the original QImage data";
        }

        cv::Mat  mat( inImage.height(), inImage.width(),
                      CV_8UC4,
                      const_cast<uchar*>(inImage.bits()),
                      static_cast<size_t>(inImage.bytesPerLine())
                      );

        cv::Mat  matNoAlpha;

        cv::cvtColor( mat, matNoAlpha, cv::COLOR_BGRA2BGR );   // drop the all-white alpha channel

        return matNoAlpha;
    }

        // 8-bit, 3 channel
    case QImage::Format_RGB888:
    {
        if ( !inCloneImageData )
        {
            qWarning() << "ASM::QImageToCvMat() - Conversion requires cloning so we don't modify the original QImage data";
        }

        QImage   swapped = inImage.rgbSwapped();

        return cv::Mat( swapped.height(), swapped.width(),
                        CV_8UC3,
                        const_cast<uchar*>(swapped.bits()),
                        static_cast<size_t>(swapped.bytesPerLine())
                        ).clone();
    }

        // 8-bit, 1 channel
    case QImage::Format_Indexed8:
    {
        cv::Mat  mat( inImage.height(), inImage.width(),
                      CV_8UC1,
                      const_cast<uchar*>(inImage.bits()),
                      static_cast<size_t>(inImage.bytesPerLine())
                      );

        return (inCloneImageData ? mat.clone() : mat);
    }

    default:
        qWarning() << "ASM::QImageToCvMat() - QImage format not handled in switch:" << inImage.format();
        break;
    }

    return cv::Mat();
}

inline cv::Mat QPixmapToCvMat( const QPixmap &inPixmap, bool inCloneImageData = true )
{
    return QImageToCvMat( inPixmap.toImage(), inCloneImageData );
}

int** cvMatToArrays(const cv::Mat &img){
    int row = img.rows;
    int col = img.cols;
    int** arr=0;
    arr=new int*[row];

    for (int x = 0;x < row; x++){
        arr[x]=new int[col];
        for (int y = 0; y < col; y++){
            if(img.at<uchar>(x,y) == 255)
                arr[x][y] = 1;
            else
                arr[x][y] = 0;

            //arr[x][y] = img.at<uchar>(x,y);
        }
    }
    return arr;
}


measuring::measuring(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::measuring)
{
    ui->setupUi(this);

    ui->circle_setting->setEnabled(false);
    ui->circle_offset_radL->setText(QString::number(ui->circle_offsetDeg->minimum()));
    ui->circle_offset_numL->setText(QString::number(ui->circle_offsetNum->minimum()));

    ui->amplitude_value->setText(QString::number(ui->amplitudeSlider->minimum()));
    ui->smooth_value->setText(QString::number(ui->smoothSlider->minimum()));
    ui->offset_numL->setText(QString::number(ui->offsetNum->minimum()));
    ui->offset_disL->setText(QString::number(ui->offsetVal->minimum()));


    MAX_KERNEL_LENGTH = ui->smoothSlider->minimum();
    mousePressed = false;

}

void measuring::mousePressEvent(QMouseEvent *event){

    if(!image.empty()){

        tmp_pix = mPix;

        int dis_x = ( ui->imgShow->width() - tmp_pix.width() ) / 2;
        int dis_y = ( ui->imgShow->height() - tmp_pix.height() ) / 2;
        int x = event->pos().x() - ui->imgShow->pos().x() - dis_x;
        int y = event->pos().y() - ui->imgShow->pos().y() - dis_y;

        if( x < 0 || y < 0 ||
                x > tmp_pix.width() ||
                y > tmp_pix.height())
        {}
        else
        {
            ui->imgShow->setPixmap(tmp_pix);
            ui->imgShow->setAlignment(Qt::AlignCenter);

            line_begin.x=x;
            line_begin.y=y;
            mousePressed = true;
        }
    }
}

void measuring::mouseMoveEvent(QMouseEvent *event){

    if(!image.empty() && mousePressed){
        ui->imgShow->setPixmap(mPix);
        ui->imgShow->setAlignment(Qt::AlignCenter);
        linePix = mPix;

        int dis_x = ( ui->imgShow->width() - ui->imgShow->pixmap()->width() ) / 2;
        int dis_y = ( ui->imgShow->height() - ui->imgShow->pixmap()->height() ) / 2;
        int x = event->pos().x() - ui->imgShow->pos().x() - dis_x;
        int y = event->pos().y() - ui->imgShow->pos().y() - dis_y;

        if( x < 0 || y < 0 ||
                x > ui->imgShow->pixmap()->width() ||
                y > ui->imgShow->pixmap()->height() )
        {
            line_begin.x = NULL;
            line_begin.y = NULL;
            mousePressed = false;
        }
        else
        {
            mLine.setLine(line_begin.x,line_begin.y,x,y);

            QPainter *paint = new QPainter(&linePix);
            paint->setPen(QColor(255,34,255,255));
            paint->drawLine(mLine);
            delete paint;
            ui->imgShow->setPixmap(linePix);
            ui->imgShow->setAlignment(Qt::AlignCenter);
        }
    }
}

void measuring::mouseReleaseEvent(QMouseEvent *event){

    if(!image.empty() && mousePressed){
        ui->imgShow->setPixmap(cvMatToQPixmap(image));
        ui->imgShow->setAlignment(Qt::AlignCenter);
        linePix = mPix;

        int dis_x = ( ui->imgShow->width() - ui->imgShow->pixmap()->width() ) / 2;
        int dis_y = ( ui->imgShow->height() - ui->imgShow->pixmap()->height() ) / 2;
        int x = event->pos().x() - ui->imgShow->pos().x() - dis_x;
        int y = event->pos().y() - ui->imgShow->pos().y() - dis_y;

        if( x < 0 || y < 0 ||
                x > ui->imgShow->pixmap()->width() ||
                y > ui->imgShow->pixmap()->height() )
        {}
        else
        {
            QPainter *paint = new QPainter(&linePix);
            paint->setPen(QColor(0,255,255,255));

            mLine.setLine(line_begin.x,line_begin.y,x,y);
            paint->drawLine(mLine);
            paint->setBrush(QBrush(Qt::red));     //--line header-|
            paint->drawEllipse(QPoint(x,y),3,3);    //--------------|

            pre_A.x=mLine.x1();
            pre_A.y=mLine.y1();
            pre_B.x=mLine.x2();
            pre_B.y=mLine.y2();
            ui->x1->setText(QString::number(mLine.x1()));
            ui->y1->setText(QString::number(mLine.y1()));
            ui->x2->setText(QString::number(mLine.x2()));
            ui->y2->setText(QString::number(mLine.y2()));

            ui->showGraph->setEnabled(true);

            delete paint;
            ui->imgShow->setPixmap(linePix);
            ui->imgShow->setAlignment(Qt::AlignCenter);
            // }
            /*else if(operation == "circle"){ //more calculate in Circle operation
                cv::Point tmp;
                tmp.x = line_begin.x - (x-line_begin.x);
                tmp.y = line_begin.y - (y-line_begin.y);

                if( tmp.x < 0 || tmp.y < 0 ||
                    tmp.x > ui->imgShow->pixmap()->width() ||
                    tmp.y > ui->imgShow->pixmap()->height() )
                {delete paint;}
                else {
                    line_begin.x = tmp.x;
                    line_begin.y = tmp.y;

                    mLine.setLine(line_begin.x,line_begin.y,x,y);
                    paint->drawLine(mLine);
                    paint->setBrush(QBrush(Qt::red));                             //--line header-|
                    paint->drawEllipse(QPoint(x,y),3,3);                          //--------------|
                    paint->setBrush(QBrush(Qt::yellow));                          //--line tail-|
                    paint->drawEllipse(QPoint(line_begin.x,line_begin.y),3,3);    //------------|

                    pre_A.x=mLine.x1();
                    pre_A.y=mLine.y1();
                    pre_B.x=mLine.x2();
                    pre_B.y=mLine.y2();
                    ui->x1->setText(QString::number(mLine.x1()));
                    ui->y1->setText(QString::number(mLine.y1()));
                    ui->x2->setText(QString::number(mLine.x2()));
                    ui->y2->setText(QString::number(mLine.y2()));

                    ui->showGraph->setEnabled(true);

                    delete paint;
                    ui->imgShow->setPixmap(linePix);
                    ui->imgShow->setAlignment(Qt::AlignCenter);
                }
            }*/

        }
    }

    mousePressed =false;

}

measuring::~measuring()
{
    delete ui;
}

void measuring::on_showImg_clicked()
{
    //QString path = "E://5.jpg";//ui->imgPath->text();
    QString path = QFileDialog::getOpenFileName(this,tr("Open File"),"E://ProjectPictures");
    if(!path.isEmpty()){
        /// UI:control ///
        ui->showGraph->setEnabled(false);
        ui->smoothSlider->setEnabled(false);
        ui->amplitudeSlider->setEnabled(false);
        ui->offsetVal->setEnabled(false);
        ui->offsetNum->setEnabled(false);

        ui->Operation->setEnabled(true);
        //////////////////

        image = cv::imread(path.toStdString(), 0);

        mPix = cvMatToQPixmap(image);

        ui->imgShow->setPixmap(mPix);
        ui->imgShow->setAlignment(Qt::AlignCenter);
    }

}

void measuring::on_showGraph_clicked()
{
    A=pre_A;
    B=pre_B;

    cv::LineIterator it(image, A, B, 8 ,false);//'true' is left to right ,not order || 'false' A point to B point
    linePoints = new std::vector<cv::Point>(it.count);
    smooth_linePoints = new std::vector<cv::Point>(it.count);

    std::vector<double> axisX;
    std::vector<double> axisY;

    for(int i = 0; i < it.count; i++, ++it)
    {
        linePoints->at(i) = it.pos();
        smooth_linePoints->at(i) = it.pos();
    }

    for(int i =0;i<linePoints->size();i++){
        axisX.push_back(i);                                     // point index
        axisY.push_back(image.at<uchar>(linePoints->at(i)));    // pixel color
        //qDebug() << re_linePoints->at(i).x << re_linePoints->at(i).y;
    }

    ui->customPlot->addGraph();
    ui->customPlot->graph(0)->setData(QVector<double>::fromStdVector(axisX),QVector<double>::fromStdVector(axisY));
    ui->customPlot->xAxis->setLabel("Index of each point");
    ui->customPlot->yAxis->setLabel("Pixel Color");
    ui->customPlot->xAxis->setRange(0,linePoints->size()-1);
    ui->customPlot->yAxis->setRange(0,255);             //color 0-255
    ui->customPlot->replot();

    ui->customPlot->addGraph();
    ui->customPlot->graph(1)->setPen(QPen(QColor(255, 100, 0)));
    ui->customPlot->graph(1)->setLineStyle(QCPGraph::lsNone);
    ui->customPlot->graph(1)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, 10));

    /// UI:control ///
    ui->smoothSlider->setEnabled(true);
    ui->amplitudeSlider->setEnabled(true);
    ui->smoothSlider->setValue(ui->smoothSlider->minimum());
    ui->amplitudeSlider->setValue(ui->amplitudeSlider->minimum());
    ui->offsetVal->setValue(ui->offsetVal->minimum());
    ui->offsetNum->setValue(ui->offsetNum->minimum());
    /////////////////

}

void measuring::on_smoothSlider_valueChanged(int value)
{
    /// UI:control ///
    if(ui->offsetVal->value() != ui->offsetVal->minimum())
        ui->offsetVal->setValue(ui->offsetVal->minimum());
    else
        ui->offsetNum->setValue(0);

    ui->offsetNum->setEnabled(true);
    ui->offsetVal->setEnabled(true);
    //////////////////

    if(value%2!=0 && value!=1){ //Gaussian Smooth
        MAX_KERNEL_LENGTH = value;

        for ( int i = 1; i < MAX_KERNEL_LENGTH; i = i + 2 ){
            cv::GaussianBlur(image,blur_img,cv::Size(i,i),0,0);
        }

        std::vector<double> axisX,axisY;
        for(int i =0;i<linePoints->size();i++){
            axisX.push_back(i);
            axisY.push_back(blur_img.at<uchar>(linePoints->at(i)));
            smooth_linePoints->at(i).x = i;
            smooth_linePoints->at(i).y = blur_img.at<uchar>(linePoints->at(i));

            //qDebug() << re_linePoints->at(i).x << re_linePoints->at(i).y;
        }

        //ui->customPlot->graph(0)->setData(QVector<double>::fromStdVector(axisX),QVector<double>::fromStdVector(axisY));
        ui->customPlot->graph(0)->setData(QVector<double>::fromStdVector(axisX),QVector<double>::fromStdVector(axisY));
        ui->customPlot->replot();

        findPeak(axisY,axisX,axisY,ui->amplitudeSlider->value());

        ui->customPlot->graph(1)->setData(QVector<double>::fromStdVector(axisX),QVector<double>::fromStdVector(axisY));
        ui->customPlot->replot();

        std::vector<double> smoothX,smoothY;
        for(int i =0;i<smooth_linePoints->size();i++){
            smoothX.push_back(smooth_linePoints->at(i).x);
            smoothY.push_back(smooth_linePoints->at(i).y);
        }

        std::vector<cv::Point3i> ffPoints = ffSlope(smoothX,smoothY,ui->amplitudeSlider->value()); //ffSlope.x is *INDEX* for linePoints(from user)  ,ffSlope.y is PixColor
        //for(int i =0;i<ffPoints.size();i++) qDebug() << ffPoints.at(i).x<< ffPoints.at(i).y;

        pointPix = linePix;
        QPainter *paint = new QPainter(&pointPix);
        paint->setPen(QColor(0,0,255,255));
        for(int i=0 ;i<ffPoints.size() ;i++){
            mLine.setLine(linePoints->at(ffPoints.at(i).x).x-4,
                             linePoints->at(ffPoints.at(i).x).y-4,
                             linePoints->at(ffPoints.at(i).x).x+4,
                             linePoints->at(ffPoints.at(i).x).y+4);
            paint->drawLine(mLine);
            mLine.setLine(linePoints->at(ffPoints.at(i).x).x+4,
                              linePoints->at(ffPoints.at(i).x).y-4,
                              linePoints->at(ffPoints.at(i).x).x-4,
                              linePoints->at(ffPoints.at(i).x).y+4);
            paint->drawLine(mLine);
        }
        delete paint;
        ui->imgShow->setPixmap(pointPix);
        ui->imgShow->setAlignment(Qt::AlignCenter);

    }
}

void measuring::on_amplitudeSlider_valueChanged(int value)
{
    /// UI:control ///
    if(ui->offsetVal->value() != ui->offsetVal->minimum())
        ui->offsetVal->setValue(ui->offsetVal->minimum());
    else
        ui->offsetNum->setValue(0);

    ui->offsetNum->setEnabled(true);
    ui->offsetVal->setEnabled(true);
    //////////////////

    std::vector<double> smoothX,smoothY;
    for(int i =0;i<smooth_linePoints->size();i++){
        smoothX.push_back(smooth_linePoints->at(i).x);
        smoothY.push_back(smooth_linePoints->at(i).y);
    }

    std::vector<double> axisX,axisY;
    findPeak(smoothY,axisX,axisY,value);

    ui->customPlot->graph(1)->setData(QVector<double>::fromStdVector(axisX),QVector<double>::fromStdVector(axisY));
    ui->customPlot->replot();

    std::vector<cv::Point3i> ffPoints = ffSlope(smoothX,smoothY,value);

    pointPix = linePix;
    QPainter *paint = new QPainter(&pointPix);
    paint->setPen(QColor(0,0,255,255));
    for(int i=0 ;i<ffPoints.size() ;i++){
        mLine.setLine(linePoints->at(ffPoints.at(i).x).x-4,
                         linePoints->at(ffPoints.at(i).x).y-4,
                         linePoints->at(ffPoints.at(i).x).x+4,
                         linePoints->at(ffPoints.at(i).x).y+4);
        paint->drawLine(mLine);
        mLine.setLine(linePoints->at(ffPoints.at(i).x).x+4,
                          linePoints->at(ffPoints.at(i).x).y-4,
                          linePoints->at(ffPoints.at(i).x).x-4,
                          linePoints->at(ffPoints.at(i).x).y+4);
        paint->drawLine(mLine);
    }
    delete paint;
    ui->imgShow->setPixmap(pointPix);
    ui->imgShow->setAlignment(Qt::AlignCenter);
}

void measuring::on_offsetNum_valueChanged(int value)
{
    result_line.clear();

    double L = std::sqrt((A.x-B.x)*(A.x-B.x)+(A.y-B.y)*(A.y-B.y));

    double offsetPixels = ui->offsetVal->value();
    double re_offsetPixels;

    int valT=value;

    cv::Point startP,endP;

    offsetPix = pointPix;
    QPainter *paint = new QPainter(&offsetPix);

    QLine offsetLine;

    for(int n = 0 ; n<value*2+1 ; n++){// Create N line

        re_offsetPixels = offsetPixels*valT;
        startP.x=(A.x + re_offsetPixels * (B.y-A.y) / L);
        startP.y=(A.y + re_offsetPixels * (A.x-B.x) / L);
        endP.x=(B.x + re_offsetPixels * (B.y-A.y) / L);
        endP.y=(B.y + re_offsetPixels * (A.x-B.x) / L);
        if(valT!=0){    //protect Origin Line
            offsetLine.setLine(startP.x,startP.y,endP.x,endP.y);
            paint->setPen(QColor(255,0,255,255));
            paint->drawLine(offsetLine);
        }

        cv::LineIterator it(image, startP, endP, 8 ,false);//'true' is left to right ,not order
        std::vector<cv::Point> re_linePoints(it.count);
        for(int i = 0; i < it.count; i++, ++it)
        {
            re_linePoints.at(i) = it.pos();
        }

        //Gaussian Smooth

        for ( int i = 1; i < MAX_KERNEL_LENGTH; i = i + 2 ){
            cv::GaussianBlur(image,blur_img,cv::Size(i,i),0,0);
        }

        std::vector<double> axisX;
        std::vector<double> axisY;

        for(unsigned int i =0;i<re_linePoints.size();i++){
            axisX.push_back(i);
            axisY.push_back(blur_img.at<uchar>(re_linePoints.at(i)));
        }

        std::vector<cv::Point3i> ffPoints = ffSlope(axisX,axisY,ui->amplitudeSlider->value()); //ffSlope.x is *INDEX* for linePoints(from user)  ,ffSlope.y is PixColor

        std::vector<cv::Point3i> points_perOffset(ffPoints.size());

        for(unsigned int i=0 ;i<ffPoints.size() ;i++){
            points_perOffset.at(i).x= re_linePoints.at(ffPoints.at(i).x).x;
            points_perOffset.at(i).y= re_linePoints.at(ffPoints.at(i).x).y;
            points_perOffset.at(i).z= ffPoints.at(i).z;
            if(valT!=0){
                QLine mLine;
                paint->setPen(QColor(100,100,100,255));
                mLine.setLine(re_linePoints.at(ffPoints.at(i).x).x-4,
                              re_linePoints.at(ffPoints.at(i).x).y-4,
                              re_linePoints.at(ffPoints.at(i).x).x+4,
                              re_linePoints.at(ffPoints.at(i).x).y+4);
                paint->drawLine(mLine);
                mLine.setLine(re_linePoints.at(ffPoints.at(i).x).x+4,
                              re_linePoints.at(ffPoints.at(i).x).y-4,
                              re_linePoints.at(ffPoints.at(i).x).x-4,
                              re_linePoints.at(ffPoints.at(i).x).y+4);
                paint->drawLine(mLine);
            }
        }
        //
        result_line.push_back(points_perOffset);
        valT--;

    }
    delete paint;
    ui->imgShow->setPixmap(offsetPix);
    ui->imgShow->setAlignment(Qt::AlignCenter);
}

void measuring::on_offsetVal_valueChanged(int value)
{
    ui->offsetNum->setValue(0);
}

void measuring::on_resultLine_clicked()
{
    qDebug()<<result_line.size();

    for(int i = 0; i< result_line.size(); i++){
        qDebug("result_lin[%d] = %d",i,result_line.at(i).size());
    }

    for(int i = 0; i< result_line.size(); i++){
        for(int j = 0; j< result_line.at(i).size(); j++)
            qDebug("result_lin[%d][%d] : x=%d y=%d z=%d",i,j,result_line.at(i).at(j).x,result_line.at(i).at(j).y,result_line.at(i).at(j).z);
    }

    QPixmap resultPix = offsetPix;
    QPainter *paint = new QPainter(&resultPix);
    QLine outputLine;

    paint->setPen(QPen(QColor(50,100,200,255),5));

    int slopeType=2; // 1 = up slope, 2 = down slope

    std::vector<cv::Point> interest_line; //Just one line interested
    cv::Vec4i re_interest_line;             //Point(x,y) use vec4:(vx, vy, x0, y0) , Point(x,y,z) use vec6:(vx, vy, vz, x0, y0, z0)
    cv::Point tmp;

    if(result_line.size() >= 3 ){
        for(int i=0 ; i<result_line.size() ; i++){
            if(result_line.at(i).size() && result_line.at(i).at(result_line.at(i).size()-1).z == slopeType){ //each offset line select only last point
                tmp.x=result_line.at(i).at(result_line.at(i).size()-1).x;
                tmp.y=result_line.at(i).at(result_line.at(i).size()-1).y;
                interest_line.push_back(tmp);
            }
        }

        if(interest_line.size()>1){ //more than 1 point to create line
            cv::fitLine(interest_line,re_interest_line,cv::DIST_L2, 0, 0.01, 0.01);

            if(re_interest_line[0]){    //re_interest_line[0] == 1 ; it is vertical line
                unsigned int changeX = (interest_line.at(0).x-interest_line.at(interest_line.size()-1).x)/2;
                outputLine.setLine(re_interest_line[2]+changeX,
                        re_interest_line[3],
                        re_interest_line[2]-changeX,
                        re_interest_line[3]);
            }
            else if(re_interest_line[1]){    //re_interest_line[1] == 1 ; it is horizental line
                unsigned int changeY = (interest_line.at(0).y-interest_line.at(interest_line.size()-1).y)/2;
                outputLine.setLine(re_interest_line[2],
                        re_interest_line[3]+changeY,
                        re_interest_line[2],
                        re_interest_line[3]-changeY);
            }
            //qDebug("A(%d,%d) B(%d,%d)",interest_line.at(0).x,interest_line.at(0).y,interest_line.at(interest_line.size()-1).x,interest_line.at(interest_line.size()-1).y);
            //qDebug()<<re_interest_line[2]<<re_interest_line[3]<<" "<<re_interest_line[2]+(re_interest_line[0]*10)<<re_interest_line[3]+(re_interest_line[1]*10);
            paint->drawLine(outputLine);
        }
    }

    delete paint;
    ui->imgShow->setPixmap(resultPix);
    ui->imgShow->setAlignment(Qt::AlignCenter);
}

void measuring::findPeak(std::vector<double> &smoothY,std::vector<double> &outputX,std::vector<double> &outputY,int distanceAmpi){
    //qDebug() << "Gooooooooooooooooooooooood";
    std::vector<float> dataY(smoothY.size());

    for(int i =0;i<smoothY.size();i++){
        dataY[i]=smoothY[i];
    }

    outputX.clear();
    outputY.clear();

    p1d::Persistence1D p;
    p.RunPersistence(dataY);

    std::vector< p1d::TPairedExtrema > Extrema;
    p.GetPairedExtrema(Extrema, distanceAmpi);

    if(Extrema.size())
        for(std::vector< p1d::TPairedExtrema >::iterator it2 = Extrema.begin(); it2 != Extrema.end(); it2++){
            outputX.push_back((*it2).MaxIndex);
            outputX.push_back((*it2).MinIndex);
        }

    if(outputX.size()<1){
        int dataMaximum=0;
        int GetGlobalMaximum;
        for(int i =0;i<dataY.size();i++)
        {
            if(dataY[i]>dataMaximum){
                dataMaximum = dataY[i];
                GetGlobalMaximum = i;
            }
        }
        if(dataMaximum-p.GetGlobalMinimumValue() > distanceAmpi)
            outputX.push_back(GetGlobalMaximum);
    }
    outputX.push_back(p.GetGlobalMinimumIndex());

    std::sort(outputX.begin(),outputX.end());

    for(int i=0 ;i<outputX.size() ;i++){
        outputY.push_back(dataY[outputX[i]]);
        //axisY.push_back(dataY[(*it2).MinIndex]);
    }
    //axisY.push_back(dataY[p.GetGlobalMinimumIndex()]);
}

std::vector<double> measuring::linspace(double a, double b, int n) {
    std::vector<double> array;
    double step = (b-a) / (n-1);

    while(a <= b) {
        array.push_back(a);
        a += step;           // could recode to better handle rounding errors
    }
    return array;
}

std::vector<cv::Point3i> measuring::ffSlope(std::vector<double> smoothX,std::vector<double> smoothY,int lengthAmpi){

    std::vector<double> peakX , peakY;
    findPeak(smoothY,peakX,peakY,lengthAmpi);
    //qDebug() <<"peakX"<< peakX;
    //qDebug() <<"peakY"<< peakY;

    std::vector<cv::Point3i> ffPoints(peakX.size()-1);

    for(int t=0 ; t<peakX.size()-1 ; t++){
        std::vector<double> axisX, axisY;
        for(int i =0 ;i <= peakX[t+1]-peakX[t];i++){
            axisX.push_back(peakX[t]+i);
            axisY.push_back(smoothY[peakX[t]+i]);
        }
        //qDebug() <<"axisX"<< axisX.size();
        //qDebug() <<"axisy"<< axisY.size();

        std::vector<double> splitX = linspace(axisX[0],axisX[axisX.size()-1],axisX.size()*10);
        //qDebug() <<"splitX"<< splitX;
        tk::spline s;
        s.set_points(axisX,axisY);

        axisX.clear();
        axisY.clear();
        for(int i =0 ;i<splitX.size();i++){
            axisX.push_back(splitX[i]);
            axisY.push_back(s(splitX[i]));
            qDebug() << axisX[i]<<axisY[i];
        }

        double slope = 0.0;
        double ffPoint = 0.0;
        if(peakY[t+1] > peakY[t]){
            for(int i =0 ; i < axisX.size()-1 ; i++){
                slope = (axisY[i+1]-axisY[i]) / (axisX[i+1]-axisX[i]);
                //qDebug() << slope;
                if( slope > ffPoint ){
                    ffPoint=slope;
                    ffPoints.at(t).x = axisX[i];
                    ffPoints.at(t).y = axisY[i];
                }
            }
            ffPoints.at(t).z = 1; //increase slope
        }
        else if(peakY[t+1] < peakY[t]){
            for(int i =0 ; i < axisX.size()-1 ; i++){
                slope = (axisY[i+1]-axisY[i]) / (axisX[i+1]-axisX[i]);
                //qDebug() << slope;
                if( slope < ffPoint ){
                    ffPoint=slope;
                    ffPoints.at(t).x = axisX[i];
                    ffPoints.at(t).y = axisY[i];
                }
            }
            ffPoints.at(t).z = 2; //decrease slope
        }

    }


    return ffPoints;
}

void measuring::on_line_operation_toggled(bool checked)
{
    operation = "linear";
    ui->imgShow->setPixmap(mPix);
    ui->circle_operation->setChecked(false);
    ui->circle_setting->setEnabled(false);
    ui->line_setting->setEnabled(true);

}

void measuring::on_circle_operation_toggled(bool checked)
{
    operation = "circle";
    ui->imgShow->setPixmap(mPix);
    ui->line_operation->setChecked(false);
    ui->circle_setting->setEnabled(true);
    ui->line_setting->setEnabled(false);


}

void measuring::on_circle_offsetDeg_valueChanged(int value)
{
    if(360%value)
        ui->circle_offsetNum->setMaximum(360/value);
    else
        ui->circle_offsetNum->setMaximum((360/value)-1);
}

void measuring::on_circle_offsetNum_valueChanged(int value)
{
    result_line.clear();
    offsetPix=pointPix;

    // qDebug() << value;
    cv::Point pointOnCircle;
    int re_value = value; //for[ 360 % (degree from user) ] = 0 ,that mean the last offset line same as Origin line

    int radius = std::sqrt((B.x-A.x)*(B.x-A.x) + (B.y-A.y)*(B.y-A.y));

    //reorigin line from X-axis to any AB line
    int deltaY=B.y-A.y;
    int deltaX=B.x-A.x;
    double angleInDegrees = atan2(deltaY,deltaX)*180/PI; //result is 0 to -180 if it's Quadrant 1 to 2 ,180 to 0 if it's Quadrant 3 to 4
    double angleFromAB;   //set origin line with AB line same as X-axis
    if(angleInDegrees<0){
        angleInDegrees = -angleInDegrees;
        angleFromAB = 2*PI-(angleInDegrees*2*PI/360);
    } else {
        angleFromAB = angleInDegrees*2*PI/360;
    }
    //

    double slice = ui->circle_offsetDeg->value()*2*PI/360 ;


    QPainter *paint = new QPainter(&offsetPix);

    for(int n =0;n<re_value+1;n++){ //n=0 is Origin line

        double re_angleFromAB = angleFromAB;
        re_angleFromAB += slice*(n);
        if(re_angleFromAB>= 2*PI) // > 2PI is > 360 degree
            re_angleFromAB = re_angleFromAB-(2*PI);

        if(n!=0){
            pointOnCircle.x = (int)(cos(re_angleFromAB) * radius + A.x);
            pointOnCircle.y = (int)(sin(re_angleFromAB) * radius + A.y);

            paint->setPen(QColor(255,0,255,255));
            mLine.setLine(A.x,A.y,pointOnCircle.x,pointOnCircle.y);
            paint->drawLine(mLine);
            paint->setBrush(QBrush(Qt::green));
            paint->drawEllipse(QPoint(pointOnCircle.x,pointOnCircle.y),3,3);
        }
        else{
            pointOnCircle.x = B.x;
            pointOnCircle.y = B.y;
        }

        cv::LineIterator it(image, A, pointOnCircle, 8 ,false);//'true' is left to right ,not order
        std::vector<cv::Point> re_linePoints(it.count);
        for(int i = 0; i < it.count; i++, ++it)
        {
            re_linePoints.at(i) = it.pos();
        }

        //Gaussian Smooth

        for ( int i = 1; i < MAX_KERNEL_LENGTH; i = i + 2 ){
            cv::GaussianBlur(image,blur_img,cv::Size(i,i),0,0);
        }

        std::vector<double> axisX;
        std::vector<double> axisY;

        for(unsigned int i =0;i<re_linePoints.size();i++){
            axisX.push_back(i);
            axisY.push_back(blur_img.at<uchar>(re_linePoints.at(i)));
        }
        //

        std::vector<cv::Point3i> ffPoints = ffSlope(axisX,axisY,ui->amplitudeSlider->value()); //ffSlope.x is *INDEX* for linePoints(from user)  ,ffSlope.y is PixColor

        std::vector<cv::Point3i> points_perOffset(ffPoints.size());

        for(unsigned int i=0 ;i<ffPoints.size() ;i++){
            points_perOffset.at(i).x= re_linePoints.at(ffPoints.at(i).x).x;
            points_perOffset.at(i).y= re_linePoints.at(ffPoints.at(i).x).y;
            points_perOffset.at(i).z= ffPoints.at(i).z;

            if(n!=0){
                QLine mLine;
                paint->setPen(QColor(100,100,100,255));
                mLine.setLine(re_linePoints.at(ffPoints.at(i).x).x-4,
                              re_linePoints.at(ffPoints.at(i).x).y-4,
                              re_linePoints.at(ffPoints.at(i).x).x+4,
                              re_linePoints.at(ffPoints.at(i).x).y+4);
                paint->drawLine(mLine);
                mLine.setLine(re_linePoints.at(ffPoints.at(i).x).x+4,
                              re_linePoints.at(ffPoints.at(i).x).y-4,
                              re_linePoints.at(ffPoints.at(i).x).x-4,
                              re_linePoints.at(ffPoints.at(i).x).y+4);
                paint->drawLine(mLine);
            }

        }

        result_line.push_back(points_perOffset);


    }

    delete paint;

    ui->imgShow->setPixmap(offsetPix);
    ui->imgShow->setAlignment(Qt::AlignCenter);


    qDebug()<<"Have line ="<<result_line.size();
    for(int i =0;i<result_line.size();i++){
        for(int j =0;j<result_line.at(i).size();j++){
            qDebug("[%d][%d]  x = %d , y = %d , z = %d",i,j,result_line.at(i).at(j).x,result_line.at(i).at(j).y,result_line.at(i).at(j).z);
        }
    }



}

void measuring::on_resultCircle_clicked()
{
    /* QPixmap resultPix = offsetPix;
    QPainter *paint = new QPainter(&resultPix);
    paint->setPen(QPen(QColor(50,100,200,255),5));

    QLine outputCircle;



        delete paint;
            ui->imgShow->setPixmap(resultPix);
            ui->imgShow->setAlignment(Qt::AlignCenter);*/
    //init
    cv::Mat circle_output = QPixmapToCvMat(offsetPix);
    std::vector<cv::Point> point;

    int slopeType=2;

    if(result_line.size() >= 3 ){
        for(int i=0 ; i<result_line.size() ; i++){
            if(result_line.at(i).size() && result_line.at(i).at(result_line.at(i).size()-1).z == slopeType){ //each offset line select only last point
                point.push_back(cv::Point(result_line.at(i).at(result_line.at(i).size()-1).x,result_line.at(i).at(result_line.at(i).size()-1).y));
            }
        }

        if(point.size()>1){ //more than 1 point to create line
            cv::Point2f center;
            float rad;
            //function
            cv::minEnclosingCircle(point,center,rad);
            cv::circle(circle_output,center,rad,cv::Scalar(255,80,40),2,cv::LINE_8);
            QPixmap resultPix = cvMatToQPixmap(circle_output);
            ui->imgShow->setPixmap(resultPix);
            ui->imgShow->setAlignment(Qt::AlignCenter);
        }
    }
}
