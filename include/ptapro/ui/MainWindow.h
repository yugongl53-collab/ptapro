#pragma once

#include "ptapro/services/DecoderService.h"
#include "ptapro/services/EncoderService.h"

#include <QElapsedTimer>
#include <QImage>
#include <QMainWindow>
#include <QVector>

class QComboBox;
class QCheckBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;

#if defined(PTAPRO_HAS_QT_MULTIMEDIA)
class QCamera;
class QMediaCaptureSession;
class QVideoFrame;
class QVideoSink;
#endif

namespace ptapro {

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void generateCode();
    void openImageAndDecode();
    void toggleCameraRecognition();
    void chooseLogoImage();
    void clearLogoImage();
    void saveGeneratedImage();
    void copySelectedDecodedPayload();
    void openSelectedDecodedUrl();
    void markGeneratedPreviewStale();
    void refreshGeneratedPreview();
    void updateGeneratorOptionState();
    void updateDecodedActionState();
#if defined(PTAPRO_HAS_QT_MULTIMEDIA)
    void processCameraFrame(const QVideoFrame& frame);
#endif

private:
    void setupUi();
    EncodeRequest buildEncodeRequest() const;
    void updatePreview(const QImage& image);
    void decodeAndDisplayImage(const QImage& image);
    void updateDecodedResults(const DecodeResult& result);
    void clearDecodedResults(const QString& message);
    QImage renderDecodeOverlay(const QImage& image, const QVector<DecodedSymbol>& symbols) const;
    const DecodedSymbol* selectedDecodedSymbol() const;
    void stopCameraRecognition();
    void showMessage(const QString& message, bool success);

    EncoderService encoderService_;
    DecoderService decoderService_;
    QImage currentImage_;
    QImage generatedImage_;
    QImage logoImage_;
    EncodeRequest generatedRequest_;
    QVector<DecodedSymbol> decodedSymbols_;
    QVector<DecodedSymbol> cameraSymbols_;
    QElapsedTimer cameraDecodeClock_;
    bool cameraActive_{false};
    bool generatedPreviewVisible_{false};

    QLineEdit* payloadEdit_{nullptr};
    QComboBox* symbologyCombo_{nullptr};
    QComboBox* errorCorrectionCombo_{nullptr};
    QSpinBox* marginSpin_{nullptr};
    QSpinBox* sizeSpin_{nullptr};
    QCheckBox* readableTextCheck_{nullptr};
    QLabel* previewLabel_{nullptr};
    QLabel* resultLabel_{nullptr};
    QLabel* logoStatusLabel_{nullptr};
    QListWidget* resultList_{nullptr};
    QPushButton* generateButton_{nullptr};
    QPushButton* decodeButton_{nullptr};
    QPushButton* cameraButton_{nullptr};
    QPushButton* chooseLogoButton_{nullptr};
    QPushButton* clearLogoButton_{nullptr};
    QPushButton* saveButton_{nullptr};
    QPushButton* copyButton_{nullptr};
    QPushButton* openUrlButton_{nullptr};

#if defined(PTAPRO_HAS_QT_MULTIMEDIA)
    QCamera* camera_{nullptr};
    QMediaCaptureSession* cameraSession_{nullptr};
    QVideoSink* cameraSink_{nullptr};
#endif
};

} // namespace ptapro
