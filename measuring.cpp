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

   // If inPixmap exists for the lifetime of the resulting cv::Mat, pass false to inCloneImageData to share inPixmap's data
   // with the cv::Mat directly
   //    NOTE: Format_RGB888 is an exception since we need to use a local QImage and thus must clone the data regardless
   inline cv::Mat QPixmapToCvMat( const QPixmap &inPixmap, bool inCloneImageData = true )
   {
      return QImageToCvMat( inPixmap.toImage(), inCloneImageData );
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

//    QPainter tempPainter(&mPix);
//    tempPainter.drawLine(mLine);
//    cv::imshow("Painter",QPixmapToCvMat(mPix));

}

void measuring::paintEvent(QPaintEvent *event)
{

    painter.begin(this);
    painter.drawPixmap(ui->imgNormal->x(),ui->imgNormal->y(),mPix);
    //When the mouse is pressed
        if(mousePressed){
            // we are taking QPixmap reference again and again
            //on mouse move and drawing a line again and again
            //hence the painter view has a feeling of dynamic drawing

            mPix = pPix;
            painter.drawPixmap(ui->imgNormal->x(),ui->imgNormal->y(),mPix);
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
            //cv::imshow("Painter",QPixmapToCvMat(mPix));
            painter.drawPixmap(ui->imgNormal->x(),ui->imgNormal->y(),mPix);

        }

        painter.end();


}


measuring::~measuring()
{
    delete ui;
}

void measuring::on_showImg_clicked()
{
    QString path = "E://1.jpg";//ui->imgPath->text();
    image = cv::imread(path.toStdString(), 1);
    mPix = cvMatToQPixmap(image);
    pPix = cvMatToQPixmap(image);
    cv::Canny(image, dst, 50, 200, 3);

    ui->imgNormal->setFixedHeight(cvMatToQPixmap(image).height());
    ui->imgNormal->setFixedWidth(cvMatToQPixmap(image).width());

    cv::imshow("canny",dst);

    ///////canny to array for check pixel/////////
    cv::Canny(image, dst, 50, 200, 3);
    int row = dst.rows;
    int col = dst.cols;

    //printf("%d --- %d\n",col,row);

    int output[row][col];
    int i=0;

    for (int x = 0;x < col; x++){
            for (int y = 0; y < row; y++){
                if(dst.at<uchar>(x,y) == 255)
                    output[x][y] = 1;
                else
                    output[x][y] = 0;

                i++;
            }
    }

    /*for (int x = 0;x < col; x++){
            for (int y = 0; y < row; y++){
                printf("%d",output[x][y]);
            }
            printf("\n");
    }*/
    //printf("\n\n%d\n\n",i);
    //////////////////////////////////////////////

}

void measuring::on_showmPix_clicked()
{
    cv::imshow("Painter",QPixmapToCvMat(mPix));

}
