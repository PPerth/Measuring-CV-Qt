#include "measuring.h"
#include "ui_measuring.h"
#include "qcustomplot.h"

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

cv::Mat image,Limg;
std::vector<cv::Point> *linePoints;
cv::Point A,B,line_begin;


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

int movingAvg(int *ptrArrNumbers, long *ptrSum, int pos, int len, int nextNum)
{
  //Subtract the oldest number from the prev sum, add the new number
  *ptrSum = *ptrSum - ptrArrNumbers[pos] + nextNum;
  //Assign the nextNum to the position in the array
  ptrArrNumbers[pos] = nextNum;
  //return the average
  return *ptrSum / len;
}

measuring::measuring(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::measuring)
{
    ui->setupUi(this);

    mousePressed = false;
//    drawStarted = false;

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
            y > tmp_pix.height() )
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



//Mouse is pressed for the first time
//    mousePressed = true;

//    mLine.setP1(event->pos()-ui->imgShow->pos());
//    mLine.setP2(event->pos()-ui->imgShow->pos());
}

void measuring::mouseMoveEvent(QMouseEvent *event){

    if(!image.empty() && mousePressed){
        ui->imgShow->setPixmap(mPix);
        ui->imgShow->setAlignment(Qt::AlignCenter);
        tmp_pix = mPix;

        int dis_x = ( ui->imgShow->width() - ui->imgShow->pixmap()->width() ) / 2;
        int dis_y = ( ui->imgShow->height() - ui->imgShow->pixmap()->height() ) / 2;
        int x = event->pos().x() - ui->imgShow->pos().x() - dis_x;
        int y = event->pos().y() - ui->imgShow->pos().y() - dis_y;

        if( x < 0 || y < 0 ||
            x > ui->imgShow->pixmap()->width() ||
            y > ui->imgShow->pixmap()->height() )
        {
            line_begin.x = 0;
            line_begin.y = 0;
        }
        else
        {
            mLine.setLine(line_begin.x,line_begin.y,x,y);

            QPainter *paint = new QPainter(&tmp_pix);
            paint->setPen(QColor(255,34,255,255));
            paint->drawLine(mLine);
            delete paint;
            ui->imgShow->setPixmap(tmp_pix);
            ui->imgShow->setAlignment(Qt::AlignCenter);
        }
    }

    //As mouse is moving set the second point again and again
    // and update continuously
//    if(event->type() == QEvent::MouseMove){
//        mLine.setP2(event->pos()-ui->imgShow->pos());
//    }
    //it calls the paintEven() function continuously
//    update();
}

void measuring::mouseReleaseEvent(QMouseEvent *event){

    if(!image.empty() && mousePressed){
        ui->imgShow->setPixmap(cvMatToQPixmap(image));
        ui->imgShow->setAlignment(Qt::AlignCenter);
        tmp_pix = cvMatToQPixmap(image);

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
            mLine.setLine(line_begin.x,line_begin.y,x,y);

            QPainter *paint = new QPainter(&tmp_pix);
            paint->setPen(QColor(255,34,255,255));
            paint->drawLine(mLine);
            delete paint;
            ui->imgShow->setPixmap(tmp_pix);
            ui->imgShow->setAlignment(Qt::AlignCenter);

            A.x=mLine.x1();
            A.y=mLine.y1();
            B.x=mLine.x2();
            B.y=mLine.y2();
            ui->x1->setText(QString::number(mLine.x1()));
            ui->y1->setText(QString::number(mLine.y1()));
            ui->x2->setText(QString::number(mLine.x2()));
            ui->y2->setText(QString::number(mLine.y2()));

            ui->showGraph->setEnabled(true);
        }
    }


    /*When mouse is released update for the one last time
//    mousePressed = false;
 //   update();


    //cv::Canny(image, Limg, 50, 200, 3);

    //cv::Mat1b mask(image.size(), uchar(0));
    //cv::Point A(mLine.x1(),mLine.y1());
    //cv::Point B(mLine.x2(),mLine.y2());

    //cv::line(mask, A, B, cv::Scalar(255),1);

    //cv::imshow("line",mask);
    //cv::imshow("canny",Limg);

    //int** cannyArr = cvMatToArrays(Limg);
    //int** maskArr = cvMatToArrays(mask);

    //////////////find point between canny and mask///////////////////
    int pointX[100],pointY[100];
    int numP=0;

    for(int x =0;x<Limg.rows;x++)
        for(int y=0;y<Limg.cols;y++){
            if(cannyArr[x][y]==maskArr[x][y] && maskArr[x][y]==1 ){
                pointX[numP] = x;
                pointY[numP] = y;
                numP++;
            }
        }

    for(int i=0;i<numP;i++){
        qDebug() <<"("<<pointX[i]<<","<<pointY[i]<<")";
    }*/
    ////////////////////////////////////////////////////////////////




}

/*void measuring::paintEvent(QPaintEvent *event)
//{
//    if(!mPix.isNull()){
//    painter.begin(this);
//    painter.drawPixmap(ui->imgShow->x(),ui->imgShow->y(),mPix);
//    //When the mouse is pressed
//        if(mousePressed){
//            // we are taking QPixmap reference again and again
//            //on mouse move and drawing a line again and again
//            //hence the painter view has a feeling of dynamic drawing

//           // mPix = pPix;
//            painter.drawPixmap(ui->imgShow->x(),ui->imgShow->y(),mPix);
//            painter.drawLine(mLine);

//            drawStarted = true;
//        }
//        else if (drawStarted){
//            // It created a QPainter object by taking  a reference
//            // to the QPixmap object created earlier, then draws a line
//            // using that object, then sets the earlier painter object
//            // with the newly modified QPixmap object

//            /*QPainter tempPainter(&mPix);
//            tempPainter.drawLine(mLine);
//            //cv::imshow("Painter",QPixmapToCvMat(mPix));
//            painter.drawPixmap(ui->imgShow->x(),ui->imgShow->y(),mPix);

//        }

//        painter.end();
//    }


//}*/


measuring::~measuring()
{
    delete ui;
}

void measuring::on_showImg_clicked()
{
    //QString path = "E://5.jpg";//ui->imgPath->text();
    QString path = QFileDialog::getOpenFileName(this,tr("Open File"),"E://ProjectPictures","JPEG(*.jpg);;PNG(*.png);;Bitmap(*.bmp)");
    image = cv::imread(path.toStdString(), 0);

    mPix = cvMatToQPixmap(image);
    //pPix = cvMatToQPixmap(image);
    //cv::Canny(image, dst, 50, 200, 3);

    //ui->imgShow->setFixedHeight(cvMatToQPixmap(image).height());
    //ui->imgShow->setFixedWidth(cvMatToQPixmap(image).width());
    ui->imgShow->setPixmap(mPix);
    ui->imgShow->setAlignment(Qt::AlignCenter);

    //cv::imshow("canny",dst);

}


void measuring::on_showGraph_clicked()
{
    cv::LineIterator it(image, A, B, 8 ,true);//'true' is left to right ,not order
    std::vector<cv::Vec3b> buf(it.count);
    //std::vector<cv::Point> Points(it.count);
    linePoints = new std::vector<cv::Point>(it.count);
    //buf.size()=it.count;

    QVector<double> axisY(linePoints->size());
    QVector<double> axisX(linePoints->size());

    for(int i = 0; i < it.count; i++, ++it)
    {
        buf[i] = (const cv::Vec3b)*it;
        linePoints->at(i) = it.pos();
     //   qDebug() << points[i].x<<points[i].y;
    }

    for(int i =0;i<linePoints->size();i++){
        axisX[i] = linePoints->at(i).x; // point.x
        axisY[i]=image.at<uchar>(linePoints->at(i));// pixel color
        qDebug() << axisX[i] << axisY[i];

    }

    ui->customPlot->addGraph();
    ui->customPlot->graph(0)->setData(axisX,axisY);
    ui->customPlot->xAxis->setLabel("X");
    ui->customPlot->yAxis->setLabel("Y");
    ui->customPlot->xAxis->setRange(A.x,B.x); // base on X
    ui->customPlot->yAxis->setRange(0,256); //color 0-255
    ui->customPlot->replot();

    ui->smoothSlider->setEnabled(true);

}

void measuring::on_smoothSlider_valueChanged(int value)
{
    /*std::vector<cv::Point> newCurve(linePoints->size());
    std::vector<double> axisX(linePoints->size());
    std::vector<double> axisY(linePoints->size());

    for(int i =0;i<linePoints->size();i++){
        newCurve[i].x=linePoints->at(i).x;
        newCurve[i].y=image.at<uchar>(linePoints->at(i));
    }

    int arrNumbers[value] = {0};

    int pos = 0;
    int newAvg[newCurve.size()];
    long sum = 0;
    int len = sizeof(arrNumbers) / sizeof(int);
    int count = newCurve.size();

    for(int i = 0; i < count; i++){
        newAvg[i] = movingAvg(arrNumbers, &sum, pos, len, newCurve[i].y);
        //qDebug() << "The new average is" << newAvg[i] <<i;
        pos++;
        if (pos >= len){
            pos = 0;
        }
    }

   for(int i =0;i<linePoints->size();i++){
        axisX[i] = newCurve[i].x;
        axisY[i] = newAvg[i];
   }*/

    if(value%2!=0 && value!=1){
        int MAX_KERNEL_LENGTH = value;
        cv::Mat blur_img;
        for ( int i = 1; i < MAX_KERNEL_LENGTH; i = i + 2 ){
            cv::GaussianBlur(image,blur_img,cv::Size(i,i),0,0);
        }
        QVector<double> axisX(linePoints->size());
        QVector<double> axisY(linePoints->size());

        for(int i =0;i<linePoints->size();i++){
            axisX[i] = linePoints->at(i).x;
            axisY[i] = blur_img.at<uchar>(linePoints->at(i));
        }

       //ui->customPlot->graph(0)->setData(QVector<double>::fromStdVector(axisX),QVector<double>::fromStdVector(axisY));
       ui->customPlot->graph(0)->setData(axisX,axisY);
       ui->customPlot->replot();
    }
}
