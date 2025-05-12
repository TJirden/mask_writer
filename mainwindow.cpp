#include "mainwindow.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow),
      thread_(new QThread(this)),
      fileProcessor(new FileProcessor),
      progressDialog(new QProgressDialog(this)) {
    ui->setupUi(this);

    fileProcessor->moveToThread(thread_);
    thread_->start();

    progressDialog->setWindowTitle("Работа над файлом");
    progressDialog->setCancelButton(nullptr);
    progressDialog->setMinimumDuration(0);
    progressDialog->reset();

    connect(
        ui->startButton, &QPushButton::clicked, this, &MainWindow::processFiles
    );
    connect(
        fileProcessor, &FileProcessor::fileProcessed, this,
        &MainWindow::handleFileProcessed
    );
    connect(
        fileProcessor, &FileProcessor::errorOccurred, this,
        &MainWindow::handleError
    );
    connect(
        fileProcessor, &FileProcessor::progressUpdated, this,
        &MainWindow::updateProgress
    );
    connect(
        ui->browseInputButton, &QPushButton::clicked, this,
        &MainWindow::browseInputButton_clicked
    );

    QSettings settings;
    ui->file_name_field->setText(settings.value("fileName", "").toString());
    ui->deleteInputCheck->setChecked(
        settings.value("deleteInput", false).toBool()
    );
    ui->outputPathEdit->setText(
        settings.value("outputPath", QDir::homePath()).toString()
    );
    ui->overwrite_mode->setCurrentIndex(
        settings.value("overwriteMode", 0).toInt()
    );
    ui->xorValueEdit->setText(
        settings.value("xorValue", "0xFFFFFFFFFFFFFFFF").toString()
    );
}

MainWindow::~MainWindow() {
    QSettings settings;
    settings.setValue("fileName", ui->file_name_field->text());
    settings.setValue("deleteInput", ui->deleteInputCheck->isChecked());
    settings.setValue("outputPath", ui->outputPathEdit->text());
    settings.setValue("overwriteMode", ui->overwrite_mode->currentIndex());
    settings.setValue("xorValue", ui->xorValueEdit->text());

    thread_->quit();
    thread_->wait();
    delete fileProcessor;
    delete ui;
}

void MainWindow::stopButton_clicked() {
    ui->startButton->setEnabled(true);
    ui->stopButton->setEnabled(false);
}

void MainWindow::browseOutputButton_clicked() {
    QString dir = QFileDialog::getExistingDirectory(
        this, "Выберите выходную директорию", ui->outputPathEdit->text()
    );
    if (!dir.isEmpty()) {
        ui->outputPathEdit->setText(dir);
    }
}

void MainWindow::browseInputButton_clicked() {
    QString fileName = QFileDialog::getOpenFileName(
        this, "Выберите входной файл", "", "All Files (*.*)"
    );
    if (!fileName.isEmpty()) {
        ui->file_name_field->setText(fileName);
    }
}

void MainWindow::processFiles() {
    QString inputFileName = ui->file_name_field->text();
    if (inputFileName.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Входной файл не дали");
        return;
    }

    QString outputDir = ui->outputPathEdit->text();
    bool deleteInput = ui->deleteInputCheck->isChecked();
    bool overwriteMode = ui->overwrite_mode->currentIndex() == 0;
    bool ok;
    quint64 xorValue = ui->xorValueEdit->text().toULongLong(&ok, 0);
    if (!ok) {
        QMessageBox::warning(
            this, "Неправильное число", "Введите корректную 8 байтную маску"
        );
        return;
    }

    QFileInfo fileInfo(inputFileName);
    if (!fileInfo.exists()) {
        QMessageBox::warning(this, "Ошибка", "Входной файл не существует");
        return;
    }

    QString outputPath = outputDir + QDir::separator() + fileInfo.fileName();
    if (!overwriteMode) {
        outputPath = make_output_path(outputPath);
    }

    currentProcessingFile = fileInfo.fileName();
    progressDialog->setLabelText(
        QString("Обработка файла: %1").arg(fileInfo.fileName())
    );
    progressDialog->show();

    QMetaObject::invokeMethod(
        fileProcessor, "processFile", Qt::QueuedConnection,
        Q_ARG(QString, fileInfo.absoluteFilePath()), Q_ARG(QString, outputPath),
        Q_ARG(quint64, xorValue), Q_ARG(bool, deleteInput)
    );
}

QString MainWindow::make_output_path(const QString &originalPath) {
    QFileInfo fileInfo(originalPath);
    QString baseName = fileInfo.completeBaseName();
    QString extension = fileInfo.suffix();
    QString dir = fileInfo.path();

    QString newPath = originalPath;
    int counter = 1;

    while (QFile::exists(newPath)) {
        newPath = dir + QDir::separator() + baseName + " (" +
                  QString::number(counter) + ")";
        if (!extension.isEmpty()) {
            newPath += "." + extension;
        }
        counter++;
    }

    return newPath;
}

void MainWindow::handleFileProcessed(
    const QString &inputPath,
    const QString &outputPath
) {
    progressDialog->reset();
    ui->logTextEdit->append(QString("Обработано: %1 -> %2")
                                .arg(
                                    QFileInfo(inputPath).fileName(),
                                    QFileInfo(outputPath).fileName()
                                ));

    ui->startButton->setEnabled(true);
    ui->stopButton->setEnabled(false);
}

void MainWindow::handleError(const QString &message) {
    progressDialog->reset();
    ui->logTextEdit->append(message);
}

void MainWindow::updateProgress(int percent) {
    progressDialog->setValue(percent);
}
