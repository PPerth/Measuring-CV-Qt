#include "measuring.h"
#include "ui_measuring.h"

#include <QPixmap>
#include <QString>
#include <QMouseEvent>
#include <QPainter>


#include <opencv2/opencv.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>

cv::Mat image,dst,cdst,cdstP;

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

measuring::measuring(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::measuring)
{
    ui->setupUi(this);

    mousePressed = false;
    drawStarted = false;

}

void measuring::mousePressEvent(QMouseEvent *event){
    //Mouse is pressed for the first time
    mousePressed = true;

    mLine.setP1(event->pos()-ui->imgNormal->pos());
    mLine.setP2(event->pos()-ui->imgNormal->pos());
}

void measuring::mouseMoveEvent(QMouseEvent *event){
    //As mouse is moving set the second point again and again
    // and update continuously
    if(event->type() == QEvent::MouseMove){
        mLine.setP2(event->pos()-ui->imgNormal->pos());
    }
    //it calls the paintEven() function continuously
    update();
}

void measuring::mouseReleaseEvent(QMouseEvent *event){
    //When mouse is released update for the one last time
    mousePressed = false;
    update();

    ui->x1->setText(QString::number(mLine.x1()));
    ui->y1->setText(QString::number(mLine.y1()));
    ui->x2->setText(QString::number(mLine.x2()));
    ui->y2->setText(QString::number(mLine.y2()));
}

void measuring::paintEvent(QPaintEvent *event)
{

    painter.begin(this);
    painter.drawPixmap(0,0,mPix);
    //When the mouse is pressed
        if(mousePressed){
            // we are taking QPixmap reference again and again
            //on mouse move and drawing a line again and again
            //hence the painter view has a feeling of dynamic drawing

            //QLine preLine;
            painter.drawPixmap(0,0,mPix);
            painter.drawLine(mLine);

            drawStarted = true;
        }
        else if (drawStarted){
            // It created a QPainter object by taking  a reference
            // to the QPixmap object created earlier, then draws a line
            // using that object, then sets the earlier painter object
            // with the newly modified QPixmap object
            QPainter tempPainter(&mPix);
            tempPainter.drawLine(mLine);

            painter.drawPixmap(0,0,mPix);
        }

        painter.end();


}


measuring::~measuring()
{
    delete ui;
}

void measuring::on_showImg_clicked()
{
    QString path = "E://2.jpg";//ui->imgPath->text();
    image = cv::imread(path.toStdString(), 1);
    mPix = cvMatToQPixmap(image);

    cv::Canny(image, dst, 50, 200, 3);

    ui->imgNormal->setFixedHeight(cvMatToQPixmap(image).height());
    ui->imgNormal->setFixedWidth(cvMatToQPixmap(image).width());
    //ui->imgNormal->setPixmap(mPix);

    cv::imshow("canny",dst);
}
