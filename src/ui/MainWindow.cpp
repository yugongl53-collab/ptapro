#include "ptapro/ui/MainWindow.h"

#include "ptapro/core/CodecTypes.h"

#include <QComboBox>
#include <QFileDialog>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

namespace ptapro {

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

    auto* inputGroup = new QGroupBox(QStringLiteral("生成"), centralWidget);
    auto* inputLayout = new QVBoxLayout(inputGroup);

    payloadEdit_ = new QTextEdit(inputGroup);
    payloadEdit_->setPlaceholderText(QStringLiteral("输入需要编码的文本或 URL"));
    payloadEdit_->setMinimumHeight(140);

    symbologyCombo_ = new QComboBox(inputGroup);
    symbologyCombo_->addItems(supportedSymbologyNames());

    generateButton_ = new QPushButton(QStringLiteral("生成编码图"), inputGroup);
    connect(generateButton_, &QPushButton::clicked, this, &MainWindow::generateCode);

    inputLayout->addWidget(new QLabel(QStringLiteral("内容"), inputGroup));
    inputLayout->addWidget(payloadEdit_);
    inputLayout->addWidget(new QLabel(QStringLiteral("码制"), inputGroup));
    inputLayout->addWidget(symbologyCombo_);
    inputLayout->addWidget(generateButton_);
    inputLayout->addStretch();

    auto* previewGroup = new QGroupBox(QStringLiteral("预览"), centralWidget);
    auto* previewLayout = new QVBoxLayout(previewGroup);

    previewLabel_ = new QLabel(QStringLiteral("生成或打开图片后显示预览"), previewGroup);
    previewLabel_->setAlignment(Qt::AlignCenter);
    previewLabel_->setMinimumSize(360, 360);
    previewLabel_->setFrameShape(QFrame::StyledPanel);
    previewLabel_->setScaledContents(false);

    decodeButton_ = new QPushButton(QStringLiteral("打开图片识别"), previewGroup);
    connect(decodeButton_, &QPushButton::clicked, this, &MainWindow::openImageAndDecode);

    resultLabel_ = new QLabel(QStringLiteral("等待操作"), previewGroup);
    resultLabel_->setWordWrap(true);

    previewLayout->addWidget(previewLabel_, 1);
    previewLayout->addWidget(decodeButton_);
    previewLayout->addWidget(resultLabel_);

    rootLayout->addWidget(inputGroup, 0, 0);
    rootLayout->addWidget(previewGroup, 0, 1);
    rootLayout->setColumnStretch(0, 1);
    rootLayout->setColumnStretch(1, 2);

    setCentralWidget(centralWidget);
}

void MainWindow::generateCode()
{
    const EncodeRequest request{
        payloadEdit_->toPlainText(),
        symbologyFromIndex(symbologyCombo_->currentIndex()),
        QSize(420, 420),
    };

    const EncodeResult result = encoderService_.encode(request);
    if (!result.success) {
        showMessage(result.message, false);
        return;
    }

    currentImage_ = result.image;
    updatePreview(currentImage_);
    showMessage(result.message, true);
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

void MainWindow::updatePreview(const QImage& image)
{
    if (image.isNull()) {
        previewLabel_->setText(QStringLiteral("暂无预览"));
        previewLabel_->setPixmap({});
        return;
    }

    // 预览按控件尺寸等比缩放，避免大图撑开布局或小图被拉伸变形。
    const QPixmap pixmap = QPixmap::fromImage(image).scaled(
        previewLabel_->size(),
        Qt::KeepAspectRatio,
        Qt::SmoothTransformation
    );
    previewLabel_->setPixmap(pixmap);
}

void MainWindow::showMessage(const QString& message, bool success)
{
    resultLabel_->setText(message);
    resultLabel_->setStyleSheet(success ? QStringLiteral("color: #166534;") : QStringLiteral("color: #991b1b;"));
}

} // namespace ptapro
