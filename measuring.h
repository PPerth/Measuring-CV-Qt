#ifndef MEASURING_H
#define MEASURING_H

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

    ~measuring();

private:
    Ui::measuring *ui;

    QPainter painter;
    QLine mLine;
    QPixmap mPix;

protected:
    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void paintEvent(QPaintEvent *event);

private slots:
    void on_showImg_clicked();
};



#endif // MEASURING_H
