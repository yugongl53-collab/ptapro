#include "ptapro/ui/MainWindow.h"

#include "ptapro/core/CodecTypes.h"

#include <QCheckBox>
#include <QComboBox>
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
#include <QPixmap>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <QtGlobal>

namespace ptapro {
namespace {

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

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setupUi();
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

    auto* previewGroup = new QGroupBox(QStringLiteral("预览与识别"), centralWidget);
    auto* previewLayout = new QVBoxLayout(previewGroup);

    previewLabel_ = new QLabel(QStringLiteral("输入内容后自动生成预览"), previewGroup);
    previewLabel_->setAlignment(Qt::AlignCenter);
    previewLabel_->setMinimumSize(360, 360);
    previewLabel_->setFrameShape(QFrame::StyledPanel);
    previewLabel_->setScaledContents(false);

    decodeButton_ = new QPushButton(QStringLiteral("打开图片识别"), previewGroup);

    resultLabel_ = new QLabel(QStringLiteral("等待输入"), previewGroup);
    resultLabel_->setWordWrap(true);

    previewLayout->addWidget(previewLabel_, 1);
    previewLayout->addWidget(decodeButton_);
    previewLayout->addWidget(resultLabel_);

    rootLayout->addWidget(inputGroup, 0, 0);
    rootLayout->addWidget(previewGroup, 0, 1);
    rootLayout->setColumnStretch(0, 1);
    rootLayout->setColumnStretch(1, 2);

    previewTimer_ = new QTimer(this);
    previewTimer_->setSingleShot(true);
    previewTimer_->setInterval(180);

    connect(previewTimer_, &QTimer::timeout, this, &MainWindow::refreshGeneratedPreview);
    connect(payloadEdit_, &QLineEdit::textChanged, this, &MainWindow::schedulePreviewUpdate);
    connect(symbologyCombo_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() {
        updateGeneratorOptionState();
        schedulePreviewUpdate();
    });
    connect(errorCorrectionCombo_, qOverload<int>(&QComboBox::currentIndexChanged), this, &MainWindow::schedulePreviewUpdate);
    connect(marginSpin_, qOverload<int>(&QSpinBox::valueChanged), this, &MainWindow::schedulePreviewUpdate);
    connect(sizeSpin_, qOverload<int>(&QSpinBox::valueChanged), this, &MainWindow::schedulePreviewUpdate);
    connect(readableTextCheck_, &QCheckBox::toggled, this, &MainWindow::schedulePreviewUpdate);
    connect(generateButton_, &QPushButton::clicked, this, &MainWindow::generateCode);
    connect(saveButton_, &QPushButton::clicked, this, &MainWindow::saveGeneratedImage);
    connect(chooseLogoButton_, &QPushButton::clicked, this, &MainWindow::chooseLogoImage);
    connect(clearLogoButton_, &QPushButton::clicked, this, &MainWindow::clearLogoImage);
    connect(decodeButton_, &QPushButton::clicked, this, &MainWindow::openImageAndDecode);

    updateGeneratorOptionState();
    setCentralWidget(centralWidget);
}

void MainWindow::generateCode()
{
    if (previewTimer_->isActive()) {
        previewTimer_->stop();
    }
    refreshGeneratedPreview();
}

void MainWindow::openImageAndDecode()
{
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

    currentImage_ = image;
    updatePreview(currentImage_);

    const DecodeResult result = decoderService_.decodeImage(currentImage_);
    if (result.success) {
        showMessage(QStringLiteral("识别成功：%1").arg(result.payload), true);
    } else {
        showMessage(result.message, false);
    }
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
    schedulePreviewUpdate();
}

void MainWindow::clearLogoImage()
{
    logoImage_ = {};
    logoStatusLabel_->setText(QStringLiteral("未选择"));
    updateGeneratorOptionState();
    schedulePreviewUpdate();
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

void MainWindow::schedulePreviewUpdate()
{
    // 文本输入会连续触发信号，做轻量防抖可以避免每个按键都立即调用 ZXing 生成矩阵。
    previewTimer_->start();
}

void MainWindow::refreshGeneratedPreview()
{
    const EncodeRequest request = buildEncodeRequest();
    if (request.payload.trimmed().isEmpty()) {
        generatedImage_ = {};
        currentImage_ = {};
        saveButton_->setEnabled(false);
        updatePreview(currentImage_);
        resultLabel_->setStyleSheet({});
        resultLabel_->setText(QStringLiteral("等待输入"));
        return;
    }

    const EncodeResult result = encoderService_.encode(request);
    if (!result.success) {
        generatedImage_ = {};
        saveButton_->setEnabled(false);
        showMessage(result.message, false);
        return;
    }

    generatedRequest_ = request;
    generatedImage_ = result.image;
    currentImage_ = result.image;
    saveButton_->setEnabled(true);
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
        previewLabel_->setText(QStringLiteral("输入内容后自动生成预览"));
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

void MainWindow::showMessage(const QString& message, bool success)
{
    resultLabel_->setText(message);
    resultLabel_->setStyleSheet(success ? QStringLiteral("color: #166534;") : QStringLiteral("color: #991b1b;"));
}

} // namespace ptapro
