#ifndef MEASURING_H
#define MEASURING_H

#include "qcustomplot.h"

#include <QGuiApplication>
#include <QDialog>
#include <QtGui>
#include <QtCore>
#include <QPixmap>
#include <QImage>
#include <QLine>
#include <QLabel>

#include <QPainter>
#include <QWidget>


#include <opencv2/opencv.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>

namespace Ui {
class measuring;
}

class measuring : public QDialog
{
    Q_OBJECT

public:
    explicit measuring(QWidget *parent = 0);

    bool mousePressed;
    bool drawStarted;
    QString operation; // "linear" and "circle"

    ~measuring();

private:
    Ui::measuring *ui;

    cv::Mat image,blur_img; //blur_img : use in Guassian Smooth
    QLine mLine;
    QPixmap mPix,linePix,offsetPix,pointPix,tmp_pix;

protected:
    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);

    std::vector<double> linspace(double a, double b, int n) ;
    std::vector<cv::Point3i> ffSlope(std::vector<double> smoothX,std::vector<double> smoothY,int lengthAmpi);
    void findPeak(std::vector<double> &smoothY,std::vector<double> &outputX,std::vector<double> &outputY,int distanceAmpi);

private slots:
    void on_showImg_clicked();
    void on_showGraph_clicked();
    void on_smoothSlider_valueChanged(int value);
    void on_amplitudeSlider_valueChanged(int value);
    void on_offsetVal_valueChanged(int value);
    void on_offsetNum_valueChanged(int value);
    void on_resultLine_clicked();
    void on_line_operation_toggled(bool checked);
    void on_circle_operation_toggled(bool checked);
    void on_circle_offsetDeg_valueChanged(int value);
    void on_circle_offsetNum_valueChanged(int value);
    void on_resultCircle_clicked();
};



#endif // MEASURING_H
