#pragma once

#include "ptapro/services/DecoderService.h"
#include "ptapro/services/EncoderService.h"

#include <QImage>
#include <QMainWindow>

class QComboBox;
class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTimer;

namespace ptapro {

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void generateCode();
    void openImageAndDecode();
    void chooseLogoImage();
    void clearLogoImage();
    void saveGeneratedImage();
    void schedulePreviewUpdate();
    void refreshGeneratedPreview();
    void updateGeneratorOptionState();

private:
    void setupUi();
    EncodeRequest buildEncodeRequest() const;
    void updatePreview(const QImage& image);
    void showMessage(const QString& message, bool success);

    EncoderService encoderService_;
    DecoderService decoderService_;
    QImage currentImage_;
    QImage generatedImage_;
    QImage logoImage_;
    EncodeRequest generatedRequest_;

    QLineEdit* payloadEdit_{nullptr};
    QComboBox* symbologyCombo_{nullptr};
    QComboBox* errorCorrectionCombo_{nullptr};
    QSpinBox* marginSpin_{nullptr};
    QSpinBox* sizeSpin_{nullptr};
    QCheckBox* readableTextCheck_{nullptr};
    QLabel* previewLabel_{nullptr};
    QLabel* resultLabel_{nullptr};
    QLabel* logoStatusLabel_{nullptr};
    QPushButton* generateButton_{nullptr};
    QPushButton* decodeButton_{nullptr};
    QPushButton* chooseLogoButton_{nullptr};
    QPushButton* clearLogoButton_{nullptr};
    QPushButton* saveButton_{nullptr};
    QTimer* previewTimer_{nullptr};
};

} // namespace ptapro
