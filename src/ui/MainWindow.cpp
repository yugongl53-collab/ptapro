#include "ptapro/ui/MainWindow.h"

#include "ptapro/core/CodecTypes.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QColor>
#include <QComboBox>
#include <QDesktopServices>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIODevice>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QSpinBox>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>
#include <QtGlobal>

#if defined(PTAPRO_HAS_QT_MULTIMEDIA)
#include <QCamera>
#include <QCameraDevice>
#include <QMediaCaptureSession>
#include <QMediaDevices>
#include <QVideoFrame>
#include <QVideoSink>
#endif

namespace ptapro {
namespace {

constexpr int kCameraDecodeIntervalMs = 250;

QString appendSuffixIfMissing(const QString& filePath, const QString& selectedFilter)
{
    const QFileInfo fileInfo(filePath);
    if (!fileInfo.suffix().isEmpty()) {
        return filePath;
    }

    if (selectedFilter.contains(QStringLiteral("SVG"))) {
        return filePath + QStringLiteral(".svg");
    }
    if (selectedFilter.contains(QStringLiteral("JPG"))) {
        return filePath + QStringLiteral(".jpg");
    }
    if (selectedFilter.contains(QStringLiteral("BMP"))) {
        return filePath + QStringLiteral(".bmp");
    }

    return filePath + QStringLiteral(".png");
}

QByteArray imageFormatForPath(const QString& filePath)
{
    const QString suffix = QFileInfo(filePath).suffix().toLower();
    if (suffix == QStringLiteral("jpg") || suffix == QStringLiteral("jpeg")) {
        return "JPG";
    }
    if (suffix == QStringLiteral("bmp")) {
        return "BMP";
    }

    // 未识别扩展名时按 PNG 保存，保证无损且兼容性最好。
    return "PNG";
}

bool isSvgPath(const QString& filePath)
{
    return QFileInfo(filePath).suffix().compare(QStringLiteral("svg"), Qt::CaseInsensitive) == 0;
}

QString resultItemText(int index, const DecodedSymbol& symbol)
{
    return QStringLiteral("%1. [%2] %3")
        .arg(index + 1)
        .arg(symbol.formatName)
        .arg(symbol.payload);
}

bool isOpenableUrl(const QString& payload)
{
    const QUrl url = QUrl::fromUserInput(payload.trimmed());
    return url.isValid() && (url.scheme() == QStringLiteral("http") || url.scheme() == QStringLiteral("https"));
}

QRectF symbolBoundingRect(const DecodedSymbol& symbol)
{
    if (symbol.corners.isEmpty()) {
        return {};
    }

    qreal minX = symbol.corners.first().x();
    qreal maxX = minX;
    qreal minY = symbol.corners.first().y();
    qreal maxY = minY;
    for (const QPointF& point : symbol.corners) {
        minX = qMin(minX, point.x());
        maxX = qMax(maxX, point.x());
        minY = qMin(minY, point.y());
        maxY = qMax(maxY, point.y());
    }

    QRectF rect(QPointF(minX, minY), QPointF(maxX, maxY));
    if (rect.width() < 8 || rect.height() < 8) {
        // 一维码有时只返回近似扫描线，扩展成可见区域便于用户定位。
        rect.adjust(-10, -10, 10, 10);
    }
    return rect;
}

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setupUi();
}

MainWindow::~MainWindow()
{
    stopCameraRecognition();
}

void MainWindow::setupUi()
{
    setWindowTitle(QStringLiteral("二维码/条形码生成与识别工具"));

    auto* centralWidget = new QWidget(this);
    auto* rootLayout = new QGridLayout(centralWidget);
    rootLayout->setContentsMargins(16, 16, 16, 16);
    rootLayout->setHorizontalSpacing(16);
    rootLayout->setVerticalSpacing(12);

    auto* inputGroup = new QGroupBox(QStringLiteral("生成器"), centralWidget);
    auto* inputLayout = new QVBoxLayout(inputGroup);
    auto* formLayout = new QFormLayout();
    formLayout->setLabelAlignment(Qt::AlignRight);
    formLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    payloadEdit_ = new QLineEdit(inputGroup);
    payloadEdit_->setPlaceholderText(QStringLiteral("输入文本、URL、联系人信息或产品序列号"));
    payloadEdit_->setClearButtonEnabled(true);

    symbologyCombo_ = new QComboBox(inputGroup);
    symbologyCombo_->addItems(supportedSymbologyNames());

    errorCorrectionCombo_ = new QComboBox(inputGroup);
    errorCorrectionCombo_->addItems(supportedErrorCorrectionNames());
    errorCorrectionCombo_->setCurrentIndex(1);

    marginSpin_ = new QSpinBox(inputGroup);
    marginSpin_->setRange(0, 64);
    marginSpin_->setValue(4);
    marginSpin_->setSuffix(QStringLiteral(" px"));

    sizeSpin_ = new QSpinBox(inputGroup);
    sizeSpin_->setRange(200, 1200);
    sizeSpin_->setSingleStep(40);
    sizeSpin_->setValue(420);
    sizeSpin_->setSuffix(QStringLiteral(" px"));

    readableTextCheck_ = new QCheckBox(QStringLiteral("条码下方显示可读数字/文本"), inputGroup);
    readableTextCheck_->setChecked(true);

    chooseLogoButton_ = new QPushButton(QStringLiteral("选择 Logo"), inputGroup);
    clearLogoButton_ = new QPushButton(QStringLiteral("清除"), inputGroup);
    logoStatusLabel_ = new QLabel(QStringLiteral("未选择"), inputGroup);
    logoStatusLabel_->setWordWrap(true);

    auto* logoLayout = new QHBoxLayout();
    logoLayout->addWidget(chooseLogoButton_);
    logoLayout->addWidget(clearLogoButton_);
    logoLayout->addWidget(logoStatusLabel_, 1);

    generateButton_ = new QPushButton(QStringLiteral("立即生成"), inputGroup);
    saveButton_ = new QPushButton(QStringLiteral("保存图片"), inputGroup);
    saveButton_->setEnabled(false);

    auto* actionLayout = new QHBoxLayout();
    actionLayout->addWidget(generateButton_);
    actionLayout->addWidget(saveButton_);

    formLayout->addRow(QStringLiteral("内容"), payloadEdit_);
    formLayout->addRow(QStringLiteral("码制"), symbologyCombo_);
    formLayout->addRow(QStringLiteral("纠错级别"), errorCorrectionCombo_);
    formLayout->addRow(QStringLiteral("边距"), marginSpin_);
    formLayout->addRow(QStringLiteral("尺寸"), sizeSpin_);
    formLayout->addRow(QStringLiteral("可读文本"), readableTextCheck_);
    formLayout->addRow(QStringLiteral("Logo"), logoLayout);

    inputLayout->addLayout(formLayout);
    inputLayout->addLayout(actionLayout);
    inputLayout->addStretch();

    auto* previewGroup = new QGroupBox(QStringLiteral("识别器"), centralWidget);
    auto* previewLayout = new QVBoxLayout(previewGroup);

    previewLabel_ = new QLabel(QStringLiteral("输入内容生成预览，或打开图片进行识别"), previewGroup);
    previewLabel_->setAlignment(Qt::AlignCenter);
    previewLabel_->setMinimumSize(360, 360);
    previewLabel_->setFrameShape(QFrame::StyledPanel);
    previewLabel_->setScaledContents(false);

    decodeButton_ = new QPushButton(QStringLiteral("打开图片识别"), previewGroup);
    cameraButton_ = new QPushButton(QStringLiteral("启动摄像头识别"), previewGroup);

    auto* decodeActionLayout = new QHBoxLayout();
    decodeActionLayout->addWidget(decodeButton_);
    decodeActionLayout->addWidget(cameraButton_);

    resultList_ = new QListWidget(previewGroup);
    resultList_->setMinimumHeight(110);
    resultList_->setWordWrap(true);

    copyButton_ = new QPushButton(QStringLiteral("复制结果"), previewGroup);
    openUrlButton_ = new QPushButton(QStringLiteral("用浏览器打开"), previewGroup);
    copyButton_->setEnabled(false);
    openUrlButton_->setEnabled(false);

    auto* resultActionLayout = new QHBoxLayout();
    resultActionLayout->addWidget(copyButton_);
    resultActionLayout->addWidget(openUrlButton_);

    resultLabel_ = new QLabel(QStringLiteral("等待输入"), previewGroup);
    resultLabel_->setWordWrap(true);

    previewLayout->addWidget(previewLabel_, 1);
    previewLayout->addLayout(decodeActionLayout);
    previewLayout->addWidget(resultList_);
    previewLayout->addLayout(resultActionLayout);
    previewLayout->addWidget(resultLabel_);

    rootLayout->addWidget(inputGroup, 0, 0);
    rootLayout->addWidget(previewGroup, 0, 1);
    rootLayout->setColumnStretch(0, 1);
    rootLayout->setColumnStretch(1, 2);

    connect(payloadEdit_, &QLineEdit::textChanged, this, &MainWindow::markGeneratedPreviewStale);
    connect(symbologyCombo_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() {
        updateGeneratorOptionState();
        markGeneratedPreviewStale();
    });
    connect(errorCorrectionCombo_, qOverload<int>(&QComboBox::currentIndexChanged), this, &MainWindow::markGeneratedPreviewStale);
    connect(marginSpin_, qOverload<int>(&QSpinBox::valueChanged), this, &MainWindow::markGeneratedPreviewStale);
    connect(sizeSpin_, qOverload<int>(&QSpinBox::valueChanged), this, &MainWindow::markGeneratedPreviewStale);
    connect(readableTextCheck_, &QCheckBox::toggled, this, &MainWindow::markGeneratedPreviewStale);
    connect(generateButton_, &QPushButton::clicked, this, &MainWindow::generateCode);
    connect(saveButton_, &QPushButton::clicked, this, &MainWindow::saveGeneratedImage);
    connect(chooseLogoButton_, &QPushButton::clicked, this, &MainWindow::chooseLogoImage);
    connect(clearLogoButton_, &QPushButton::clicked, this, &MainWindow::clearLogoImage);
    connect(decodeButton_, &QPushButton::clicked, this, &MainWindow::openImageAndDecode);
    connect(cameraButton_, &QPushButton::clicked, this, &MainWindow::toggleCameraRecognition);
    connect(copyButton_, &QPushButton::clicked, this, &MainWindow::copySelectedDecodedPayload);
    connect(openUrlButton_, &QPushButton::clicked, this, &MainWindow::openSelectedDecodedUrl);
    connect(resultList_, &QListWidget::currentRowChanged, this, &MainWindow::updateDecodedActionState);

    updateGeneratorOptionState();
    setCentralWidget(centralWidget);
}

void MainWindow::generateCode()
{
    stopCameraRecognition();
    refreshGeneratedPreview();
}

void MainWindow::openImageAndDecode()
{
    stopCameraRecognition();

    const QString filePath = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("选择图片"),
        {},
        QStringLiteral("图片文件 (*.png *.jpg *.jpeg *.bmp);;所有文件 (*.*)")
    );

    if (filePath.isEmpty()) {
        return;
    }

    QImage image(filePath);
    if (image.isNull()) {
        showMessage(QStringLiteral("图片读取失败，请确认文件格式是否受支持。"), false);
        return;
    }

    decodeAndDisplayImage(image);
}

void MainWindow::toggleCameraRecognition()
{
#if defined(PTAPRO_HAS_QT_MULTIMEDIA)
    if (cameraActive_) {
        stopCameraRecognition();
        showMessage(QStringLiteral("已停止摄像头识别。"), true);
        return;
    }

    const QList<QCameraDevice> cameras = QMediaDevices::videoInputs();
    if (cameras.isEmpty()) {
        showMessage(QStringLiteral("未检测到可用摄像头。"), false);
        return;
    }

    clearDecodedResults(QStringLiteral("摄像头识别中"));
    cameraSymbols_.clear();
    cameraSession_ = new QMediaCaptureSession(this);
    cameraSink_ = new QVideoSink(this);
    camera_ = new QCamera(cameras.first(), this);

    // 摄像头帧进入 QVideoSink 后，在 UI 线程节流调用 ZXing，避免每帧都做高成本解码。
    cameraSession_->setCamera(camera_);
    cameraSession_->setVideoSink(cameraSink_);
    connect(cameraSink_, &QVideoSink::videoFrameChanged, this, &MainWindow::processCameraFrame);
    connect(camera_, &QCamera::errorOccurred, this, [this](QCamera::Error, const QString& errorString) {
        stopCameraRecognition();
        showMessage(errorString.isEmpty() ? QStringLiteral("摄像头启动失败。") : errorString, false);
    });

    cameraActive_ = true;
    cameraDecodeClock_.restart();
    cameraButton_->setText(QStringLiteral("停止摄像头识别"));
    showMessage(QStringLiteral("摄像头已启动：%1").arg(cameras.first().description()), true);
    camera_->start();
#else
    showMessage(QStringLiteral("当前构建未启用 QtMultimedia，无法使用摄像头识别。"), false);
#endif
}

void MainWindow::chooseLogoImage()
{
    const QString filePath = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("选择 Logo 图片"),
        {},
        QStringLiteral("图片文件 (*.png *.jpg *.jpeg *.bmp);;所有文件 (*.*)")
    );

    if (filePath.isEmpty()) {
        return;
    }

    QImage image(filePath);
    if (image.isNull()) {
        showMessage(QStringLiteral("Logo 图片读取失败，请确认文件格式是否受支持。"), false);
        return;
    }

    logoImage_ = image;
    logoStatusLabel_->setText(QFileInfo(filePath).fileName());
    updateGeneratorOptionState();
    markGeneratedPreviewStale();
}

void MainWindow::clearLogoImage()
{
    logoImage_ = {};
    logoStatusLabel_->setText(QStringLiteral("未选择"));
    updateGeneratorOptionState();
    markGeneratedPreviewStale();
}

void MainWindow::saveGeneratedImage()
{
    if (generatedImage_.isNull()) {
        showMessage(QStringLiteral("当前没有可保存的生成图。"), false);
        return;
    }

    QString selectedFilter;
    const QString filePath = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("保存编码图"),
        QStringLiteral("barcode.png"),
        QStringLiteral("PNG 图片 (*.png);;JPG 图片 (*.jpg);;BMP 图片 (*.bmp);;SVG 矢量图 (*.svg)"),
        &selectedFilter
    );

    if (filePath.isEmpty()) {
        return;
    }

    const QString outputPath = appendSuffixIfMissing(filePath, selectedFilter);
    if (isSvgPath(outputPath)) {
        QString errorMessage;
        const QString svg = encoderService_.renderSvg(generatedRequest_, &errorMessage);
        if (svg.isEmpty()) {
            showMessage(errorMessage.isEmpty() ? QStringLiteral("SVG 生成失败。") : errorMessage, false);
            return;
        }

        QFile file(outputPath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            showMessage(QStringLiteral("SVG 文件写入失败。"), false);
            return;
        }
        file.write(svg.toUtf8());
        showMessage(QStringLiteral("已保存：%1").arg(outputPath), true);
        return;
    }

    const QByteArray format = imageFormatForPath(outputPath);
    if (!generatedImage_.save(outputPath, format.constData())) {
        showMessage(QStringLiteral("图片保存失败，请检查路径或文件权限。"), false);
        return;
    }

    showMessage(QStringLiteral("已保存：%1").arg(outputPath), true);
}

void MainWindow::copySelectedDecodedPayload()
{
    const DecodedSymbol* symbol = selectedDecodedSymbol();
    if (!symbol) {
        return;
    }

    QApplication::clipboard()->setText(symbol->payload);
    showMessage(QStringLiteral("已复制识别结果。"), true);
}

void MainWindow::openSelectedDecodedUrl()
{
    const DecodedSymbol* symbol = selectedDecodedSymbol();
    if (!symbol || !isOpenableUrl(symbol->payload)) {
        return;
    }

    QDesktopServices::openUrl(QUrl::fromUserInput(symbol->payload.trimmed()));
}

void MainWindow::markGeneratedPreviewStale()
{
    if (generatedImage_.isNull()) {
        return;
    }

    // 生成参数变化后不再自动重绘，避免用户输入半截 URL 时提前显示可保存结果。
    generatedImage_ = {};
    saveButton_->setEnabled(false);
    if (generatedPreviewVisible_) {
        currentImage_ = {};
        generatedPreviewVisible_ = false;
        updatePreview(currentImage_);
    }
    showMessage(QStringLiteral("参数已变化，请点击“立即生成”重新生成。"), false);
}

void MainWindow::refreshGeneratedPreview()
{
    const EncodeRequest request = buildEncodeRequest();
    if (request.payload.trimmed().isEmpty()) {
        generatedImage_ = {};
        currentImage_ = {};
        generatedPreviewVisible_ = false;
        saveButton_->setEnabled(false);
        updatePreview(currentImage_);
        resultLabel_->setStyleSheet({});
        resultLabel_->setText(QStringLiteral("等待输入"));
        return;
    }

    const EncodeResult result = encoderService_.encode(request);
    if (!result.success) {
        generatedImage_ = {};
        currentImage_ = {};
        generatedPreviewVisible_ = false;
        saveButton_->setEnabled(false);
        updatePreview(currentImage_);
        showMessage(result.message, false);
        return;
    }

    generatedRequest_ = request;
    generatedImage_ = result.image;
    currentImage_ = result.image;
    generatedPreviewVisible_ = true;
    saveButton_->setEnabled(true);
    clearDecodedResults({});
    updatePreview(currentImage_);
    showMessage(result.message, true);
}

void MainWindow::updateGeneratorOptionState()
{
    const bool qrCodeSelected = symbologyFromIndex(symbologyCombo_->currentIndex()) == Symbology::QrCode;
    errorCorrectionCombo_->setEnabled(qrCodeSelected);
    chooseLogoButton_->setEnabled(qrCodeSelected);
    clearLogoButton_->setEnabled(qrCodeSelected && !logoImage_.isNull());
    readableTextCheck_->setEnabled(!qrCodeSelected);

    if (!qrCodeSelected) {
        logoStatusLabel_->setText(QStringLiteral("仅二维码可用"));
    } else if (logoImage_.isNull()) {
        logoStatusLabel_->setText(QStringLiteral("未选择"));
    } else if (logoStatusLabel_->text() == QStringLiteral("仅二维码可用")) {
        logoStatusLabel_->setText(QStringLiteral("已选择 Logo"));
    }
}

void MainWindow::updateDecodedActionState()
{
    const DecodedSymbol* symbol = selectedDecodedSymbol();
    copyButton_->setEnabled(symbol != nullptr && !symbol->payload.isEmpty());
    openUrlButton_->setEnabled(symbol != nullptr && isOpenableUrl(symbol->payload));
}

#if defined(PTAPRO_HAS_QT_MULTIMEDIA)
void MainWindow::processCameraFrame(const QVideoFrame& frame)
{
    if (!cameraActive_ || !frame.isValid()) {
        return;
    }

    const QImage frameImage = frame.toImage();
    if (frameImage.isNull()) {
        return;
    }

    currentImage_ = frameImage;
    generatedPreviewVisible_ = false;
    if (cameraDecodeClock_.elapsed() >= kCameraDecodeIntervalMs) {
        cameraDecodeClock_.restart();
        // 摄像头场景要控制单帧耗时，只追加一个轻量预处理变体，避免 UI 卡顿。
        const DecodeResult result = decoderService_.decodeImage(frameImage, DecodeOptions{true, 1});
        cameraSymbols_ = result.symbols;

        if (result.success) {
            updateDecodedResults(result);
            showMessage(result.message, true);
        } else {
            clearDecodedResults(QStringLiteral("摄像头识别中，未检测到码。"));
        }
    }

    updatePreview(renderDecodeOverlay(frameImage, cameraSymbols_));
}
#endif

EncodeRequest MainWindow::buildEncodeRequest() const
{
    const Symbology symbology = symbologyFromIndex(symbologyCombo_->currentIndex());
    const int baseSize = sizeSpin_->value();
    const QSize imageSize = symbology == Symbology::QrCode
        ? QSize(baseSize, baseSize)
        : QSize(baseSize, qMax(200, baseSize / 2));

    return {
        payloadEdit_->text(),
        symbology,
        imageSize,
        marginSpin_->value(),
        errorCorrectionFromIndex(errorCorrectionCombo_->currentIndex()),
        readableTextCheck_->isChecked(),
        symbology == Symbology::QrCode ? logoImage_ : QImage{},
    };
}

void MainWindow::updatePreview(const QImage& image)
{
    if (image.isNull()) {
        previewLabel_->setText(QStringLiteral("输入内容生成预览，或打开图片进行识别"));
        previewLabel_->setPixmap({});
        return;
    }

    // 预览按控件尺寸等比缩放，避免大图撑开布局或小图被拉伸变形。
    const QPixmap pixmap = QPixmap::fromImage(image).scaled(
        previewLabel_->size(),
        Qt::KeepAspectRatio,
        Qt::SmoothTransformation
    );
    previewLabel_->setText({});
    previewLabel_->setPixmap(pixmap);
}

void MainWindow::decodeAndDisplayImage(const QImage& image)
{
    currentImage_ = image;
    generatedPreviewVisible_ = false;
    const DecodeResult result = decoderService_.decodeImage(currentImage_);
    updateDecodedResults(result);

    if (result.success) {
        updatePreview(renderDecodeOverlay(currentImage_, result.symbols));
        showMessage(result.message, true);
    } else {
        updatePreview(currentImage_);
        showMessage(result.message, false);
    }
}

void MainWindow::updateDecodedResults(const DecodeResult& result)
{
    decodedSymbols_ = result.symbols;
    resultList_->clear();

    for (int index = 0; index < decodedSymbols_.size(); ++index) {
        auto* item = new QListWidgetItem(resultItemText(index, decodedSymbols_.at(index)), resultList_);
        item->setData(Qt::UserRole, index);
    }

    if (!decodedSymbols_.isEmpty()) {
        resultList_->setCurrentRow(0);
    }
    updateDecodedActionState();
}

void MainWindow::clearDecodedResults(const QString& message)
{
    decodedSymbols_.clear();
    resultList_->clear();
    updateDecodedActionState();

    if (!message.isEmpty()) {
        resultLabel_->setStyleSheet({});
        resultLabel_->setText(message);
    }
}

QImage MainWindow::renderDecodeOverlay(const QImage& image, const QVector<DecodedSymbol>& symbols) const
{
    if (image.isNull() || symbols.isEmpty()) {
        return image;
    }

    QImage overlay = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    QPainter painter(&overlay);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const qreal penWidth = qMax<qreal>(3.0, qMin(overlay.width(), overlay.height()) / 160.0);
    QPen borderPen(QColor(250, 204, 21), penWidth);
    borderPen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(borderPen);
    painter.setBrush(Qt::NoBrush);

    for (int index = 0; index < symbols.size(); ++index) {
        const DecodedSymbol& symbol = symbols.at(index);
        const QRectF rect = symbolBoundingRect(symbol);
        if (rect.isNull()) {
            continue;
        }

        if (symbol.corners.size() == 4 && rect.width() >= 8 && rect.height() >= 8) {
            painter.drawPolygon(QPolygonF(symbol.corners));
        } else {
            painter.drawRect(rect);
        }

        const QString label = QStringLiteral("%1 %2").arg(index + 1).arg(symbol.formatName);
        const QRectF labelRect(rect.topLeft() + QPointF(0, -28), QSizeF(92, 24));
        painter.fillRect(labelRect, QColor(22, 101, 52, 220));
        painter.setPen(Qt::white);
        painter.drawText(labelRect.adjusted(6, 0, -6, 0), Qt::AlignVCenter | Qt::AlignLeft, label);
        painter.setPen(borderPen);
    }

    return overlay;
}

const DecodedSymbol* MainWindow::selectedDecodedSymbol() const
{
    const QListWidgetItem* item = resultList_->currentItem();
    if (!item) {
        return nullptr;
    }

    const int index = item->data(Qt::UserRole).toInt();
    if (index < 0 || index >= decodedSymbols_.size()) {
        return nullptr;
    }
    return &decodedSymbols_.at(index);
}

void MainWindow::stopCameraRecognition()
{
    if (!cameraActive_) {
        return;
    }

    cameraActive_ = false;
    cameraSymbols_.clear();
    currentImage_ = {};
    generatedPreviewVisible_ = false;
    clearDecodedResults({});
    updatePreview(currentImage_);
    if (cameraButton_) {
        cameraButton_->setText(QStringLiteral("启动摄像头识别"));
    }

#if defined(PTAPRO_HAS_QT_MULTIMEDIA)
    if (camera_) {
        camera_->stop();
    }
    if (cameraSession_) {
        cameraSession_->setCamera(nullptr);
        cameraSession_->setVideoSink(nullptr);
    }
    if (camera_) {
        camera_->deleteLater();
        camera_ = nullptr;
    }
    if (cameraSink_) {
        cameraSink_->deleteLater();
        cameraSink_ = nullptr;
    }
    if (cameraSession_) {
        cameraSession_->deleteLater();
        cameraSession_ = nullptr;
    }
#endif
}

void MainWindow::showMessage(const QString& message, bool success)
{
    resultLabel_->setText(message);
    resultLabel_->setStyleSheet(success ? QStringLiteral("color: #166534;") : QStringLiteral("color: #991b1b;"));
}

} // namespace ptapro
