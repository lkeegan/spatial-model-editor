// QLabelMouseTracker
//  - a modified QLabel
//  - displays (and rescales without interpolation) an image,
//  - tracks the mouse location in terms of the pixels of the original image
//  - provides the colour of the last pixel that was clicked on
//  - emits a signal when the user clicks the mouse, along with the colour of
//  the pixel that was clicked on

#pragma once

#include <QLabel>
#include <QMouseEvent>

class QLabelMouseTracker : public QLabel {
  Q_OBJECT
 public:
  explicit QLabelMouseTracker(QWidget *parent = nullptr);
  // QImage used for pixel location and colour
  void setImage(const QImage &img);
  const QImage &getImage() const;
  // colour of pixel at last mouse click position
  const QRgb &getColour() const;

 signals:
  // user clicks somewhere on the image
  void mouseClicked(QRgb col);

 protected:
  void mouseMoveEvent(QMouseEvent *ev) override;
  void mousePressEvent(QMouseEvent *ev) override;
  void resizeEvent(QResizeEvent *ev) override;

 private:
  // (x,y) location of current pixel
  void setCurrentPixel(QMouseEvent *ev);
  QPoint currentPixel;
  QRgb colour;
  QImage image;
  // Pixmap used to display scaled version of image
  QPixmap pixmap = QPixmap(1, 1);
};
