#include "dialoggeometryimage.hpp"
#include "logger.hpp"
#include "ui_dialoggeometryimage.h"

static QString dblToString(double val) { return QString::number(val, 'g', 14); }

static QImage toIndexedImageNoAlpha(const QImage &image) {
  constexpr auto flagNoDither{Qt::AvoidDither | Qt::ThresholdDither |
                              Qt::ThresholdAlphaDither | Qt::NoOpaqueDetection};
  auto imgNoAlpha{image};
  if (image.hasAlphaChannel()) {
    SPDLOG_DEBUG("ignoring alpha channel");
    imgNoAlpha = image.convertToFormat(QImage::Format_RGB32, flagNoDither);
  }
  return imgNoAlpha.convertToFormat(QImage::Format_Indexed8, flagNoDither);
}

DialogGeometryImage::DialogGeometryImage(
    const QImage &image, double pixelWidth,
    const sme::model::ModelUnits &modelUnits, QWidget *parent)
    : QDialog(parent), ui{std::make_unique<Ui::DialogGeometryImage>()},
      originalImage(image), pixelModelUnits(pixelWidth), units(modelUnits) {
  coloredImage = toIndexedImageNoAlpha(originalImage);
  rescaledImage = coloredImage;
  colorTable = coloredImage.colorTable();
  ui->setupUi(this);
  ui->lblImage->setImage(rescaledImage);
  ui->btnApplyColours->setEnabled(false);
  for (auto *cmb : {ui->cmbUnitsWidth, ui->cmbUnitsHeight}) {
    for (const auto &u : units.getLengthUnits()) {
      cmb->addItem(u.name);
    }
    cmb->setCurrentIndex(units.getLengthIndex());
  }
  modelUnitSymbol = ui->cmbUnitsWidth->currentText();
  pixelLocalUnits = pixelModelUnits;
  ui->spinPixelsX->setValue(rescaledImage.width());
  ui->spinPixelsY->setValue(rescaledImage.height());
  updateColours();
  updatePixelSize();

  connect(ui->buttonBox, &QDialogButtonBox::accepted, this,
          &DialogGeometryImage::accept);
  connect(ui->buttonBox, &QDialogButtonBox::rejected, this,
          &DialogGeometryImage::reject);
  connect(ui->lblImage, &QLabelMouseTracker::mouseClicked, this,
          &DialogGeometryImage::lblImage_mouseClicked);
  connect(ui->txtImageWidth, &QLineEdit::editingFinished, this,
          &DialogGeometryImage::txtImageWidth_editingFinished);
  connect(ui->txtImageHeight, &QLineEdit::editingFinished, this,
          &DialogGeometryImage::txtImageHeight_editingFinished);
  connect(ui->cmbUnitsWidth, qOverload<int>(&QComboBox::activated), this,
          [this](int index) {
            ui->cmbUnitsHeight->setCurrentIndex(index);
            updatePixelSize();
          });
  connect(ui->cmbUnitsHeight, qOverload<int>(&QComboBox::activated), this,
          [this](int index) {
            ui->cmbUnitsWidth->setCurrentIndex(index);
            updatePixelSize();
          });
  connect(ui->spinPixelsX, qOverload<int>(&QSpinBox::valueChanged), this,
          &DialogGeometryImage::spinPixelsX_valueChanged);
  connect(ui->spinPixelsY, qOverload<int>(&QSpinBox::valueChanged), this,
          &DialogGeometryImage::spinPixelsY_valueChanged);
  connect(ui->btnResetPixels, &QPushButton::clicked, this,
          &DialogGeometryImage::btnResetPixels_clicked);
  connect(ui->btnSelectColours, &QPushButton::clicked, this,
          &DialogGeometryImage::btnSelectColours_clicked);
  connect(ui->btnApplyColours, &QPushButton::clicked, this,
          &DialogGeometryImage::btnApplyColours_clicked);
  connect(ui->btnResetColours, &QPushButton::clicked, this,
          &DialogGeometryImage::btnResetColours_clicked);

  ui->txtImageWidth->selectAll();
}

DialogGeometryImage::~DialogGeometryImage() = default;

double DialogGeometryImage::getPixelWidth() const { return pixelModelUnits; }

bool DialogGeometryImage::imageSizeAltered() const {
  return alteredSize;
}

bool DialogGeometryImage::imageColoursAltered() const {
  return alteredColours;
}

const QImage &DialogGeometryImage::getAlteredImage() const {
  return ui->lblImage->getImage();
}

void DialogGeometryImage::updatePixelSize() {
  // calculate size of image in local units
  double w{static_cast<double>(rescaledImage.width()) * pixelLocalUnits};
  ui->txtImageWidth->setText(dblToString(w));
  double h{static_cast<double>(rescaledImage.height()) * pixelLocalUnits};
  ui->txtImageHeight->setText(dblToString(h));
  // calculate pixel width in model units
  pixelModelUnits = sme::model::rescale(
      pixelLocalUnits,
      units.getLengthUnits().at(ui->cmbUnitsWidth->currentIndex()),
      units.getLength());
  ui->lblPixelSize->setText(
      QString("%1 %2").arg(pixelModelUnits).arg(modelUnitSymbol));
}

void DialogGeometryImage::updateColours() {
  auto n{static_cast<int>(colorTable.size())};
  QImage colourTableImage(1, 1, QImage::Format_Indexed8);
  colourTableImage.fill(qRgb(0, 0, 0));
  if (n > 0) {
    int w{std::max(n, ui->lblColours->width())};
    int h{std::max(1, ui->lblColours->height())};
    int pixelsPerColour{w / n};
    colourTableImage = QImage(w, h, QImage::Format_Indexed8);
    colourTableImage.setColorTable(colorTable);
    for (int x = 0; x < w; ++x) {
      auto colourIndex{
          static_cast<QRgb>(std::clamp(x / pixelsPerColour, 0, n - 1))};
      for (int y = 0; y < h; ++y) {
        colourTableImage.setPixel(x, y, colourIndex);
      }
    }
  }
  if (n == 1) {
    ui->lblLabelColours->setText("1 colour:");
  } else {
    ui->lblLabelColours->setText(QString("%1 colours:").arg(n));
  }
  ui->lblColours->setPixmap(QPixmap::fromImage(colourTableImage));
}

void DialogGeometryImage::enableWidgets(bool enable) {
  ui->txtImageWidth->setEnabled(enable);
  ui->cmbUnitsWidth->setEnabled(enable);
  ui->txtImageHeight->setEnabled(enable);
  ui->cmbUnitsHeight->setEnabled(enable);
  ui->spinPixelsX->setEnabled(enable);
  ui->spinPixelsY->setEnabled(enable);
  ui->btnResetPixels->setEnabled(enable);
  ui->btnSelectColours->setEnabled(enable);
}

void DialogGeometryImage::lblImage_mouseClicked(QRgb col,
                                                [[maybe_unused]] QPoint point) {
  if (!selectingColours) {
    return;
  }
  if (colorTable.contains(col)) {
    return;
  }
  colorTable.push_back(col);
  ui->btnApplyColours->setEnabled(true);
  updateColours();
}

void DialogGeometryImage::txtImageWidth_editingFinished() {
  double w{ui->txtImageWidth->text().toDouble()};
  pixelLocalUnits = w / static_cast<double>(rescaledImage.width());
  updatePixelSize();
}

void DialogGeometryImage::txtImageHeight_editingFinished() {
  double h{ui->txtImageHeight->text().toDouble()};
  pixelLocalUnits = h / static_cast<double>(rescaledImage.height());
  updatePixelSize();
}

void DialogGeometryImage::spinPixelsX_valueChanged(int value) {
  if (value <= 0) {
    return;
  }
  if (value != coloredImage.width()) {
    alteredSize = true;
  }
  rescaledImage = coloredImage.scaledToWidth(value, Qt::FastTransformation);
  ui->lblImage->setImage(rescaledImage);
  ui->spinPixelsY->blockSignals(true);
  ui->spinPixelsY->setValue(rescaledImage.height());
  ui->spinPixelsY->blockSignals(false);
  txtImageWidth_editingFinished();
}

void DialogGeometryImage::spinPixelsY_valueChanged(int value) {
  if (value <= 0) {
    return;
  }
  if (value != coloredImage.height()) {
    alteredSize = true;
  }
  alteredSize = true;
  rescaledImage = coloredImage.scaledToHeight(value, Qt::FastTransformation);
  ui->lblImage->setImage(rescaledImage);
  ui->spinPixelsX->blockSignals(true);
  ui->spinPixelsX->setValue(rescaledImage.width());
  ui->spinPixelsX->blockSignals(false);
  txtImageHeight_editingFinished();
}

void DialogGeometryImage::btnResetPixels_clicked() {
  alteredSize = false;
  rescaledImage = coloredImage;
  ui->lblImage->setImage(rescaledImage);
  ui->spinPixelsX->setValue(rescaledImage.width());
  ui->spinPixelsY->setValue(rescaledImage.height());
  updatePixelSize();
}

static int distance(QRgb a, QRgb b) {
  int dr{qRed(a) - qRed(b)};
  int dg{qGreen(a) - qGreen(b)};
  int db{qBlue(a) - qBlue(b)};
  return dr * dr + dg * dg + db * db;
}

static void reduceImageToTheseColours(QImage &image,
                                      const QVector<QRgb> &colorTable) {
  // map each index in image colorTable to index of nearest color in colorTable
  auto nOld{static_cast<int>(image.colorTable().size())};
  QVector<int> newIndex(nOld, 0);
  for (int iOld = 0; iOld < nOld; ++iOld) {
    int dist{std::numeric_limits<int>::max()};
    for (int iNew = 0; iNew < colorTable.size(); ++iNew) {
      if (auto d{distance(image.color(iOld), colorTable.at(iNew))}; d < dist) {
        dist = d;
        newIndex[iOld] = iNew;
      }
    }
  }
  // set each pixel to the corresponding new index
  for (int x = 0; x < image.width(); ++x) {
    for (int y = 0; y < image.height(); ++y) {
      int iOld{static_cast<int>(image.pixelIndex(x, y))};
      image.setPixel(x, y, static_cast<QRgb>(newIndex[iOld]));
    }
  }
  // update image colorTable to the new one
  image.setColorTable(colorTable);
}

void DialogGeometryImage::btnSelectColours_clicked() {
  selectingColours = true;
  colorTable.clear();
  updateColours();
  enableWidgets(false);
}

void DialogGeometryImage::btnApplyColours_clicked() {
  alteredColours = true;
  selectingColours = false;
  reduceImageToTheseColours(coloredImage, colorTable);
  enableWidgets(true);
  ui->btnApplyColours->setEnabled(false);
  spinPixelsX_valueChanged(ui->spinPixelsX->value());
}

void DialogGeometryImage::btnResetColours_clicked() {
  alteredColours = false;
  selectingColours = false;
  enableWidgets(true);
  coloredImage = toIndexedImageNoAlpha(originalImage);
  colorTable = coloredImage.colorTable();
  updateColours();
  spinPixelsX_valueChanged(ui->spinPixelsX->value());
}
