#pragma once

#include "ptapro/services/DecoderService.h"
#include "ptapro/services/EncoderService.h"

#include <QImage>
#include <QMainWindow>

class QComboBox;
class QLabel;
class QPushButton;
class QTextEdit;

namespace ptapro {

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void generateCode();
    void openImageAndDecode();

private:
    void setupUi();
    void updatePreview(const QImage& image);
    void showMessage(const QString& message, bool success);

    EncoderService encoderService_;
    DecoderService decoderService_;
    QImage currentImage_;

    QTextEdit* payloadEdit_{nullptr};
    QComboBox* symbologyCombo_{nullptr};
    QLabel* previewLabel_{nullptr};
    QLabel* resultLabel_{nullptr};
    QPushButton* generateButton_{nullptr};
    QPushButton* decodeButton_{nullptr};
};

} // namespace ptapro
