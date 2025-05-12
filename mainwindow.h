#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMainWindow>
#include <QProgressDialog>
#include <QThread>

QT_BEGIN_NAMESPACE

namespace Ui {
class MainWindow;
}

QT_END_NAMESPACE

const qint64 BUFFER_SIZE = 1024 * 1024;  // 1 мегабайт

class FileProcessor : public QObject {
    Q_OBJECT
public:
    explicit FileProcessor(QObject *parent = nullptr) : QObject(parent) {
    }

public slots:

    void processFile(
        const QString &input_path,
        const QString &output_path,
        quint64 xorValue,
        bool deleteInput
    ) {
        QFile inputFile(input_path);
        QFile outputFile(output_path);

        if (!inputFile.open(QIODevice::ReadOnly)) {
            emit errorOccurred(
                QString("Ошибка при открытии файла: %1").arg(input_path)
            );
            return;
        }

        if (!outputFile.open(QIODevice::WriteOnly)) {
            inputFile.close();
            emit errorOccurred(
                QString("Ошибка при открытии файла: %1").arg(output_path)
            );
            return;
        }

        qint64 file_size = inputFile.size();
        char *buffer = new char[BUFFER_SIZE];
        qint64 written_bytes = 0;

        while (!inputFile.atEnd()) {
            qint64 bytes_read = inputFile.read(buffer, BUFFER_SIZE);
            if (bytes_read == -1) {
                emit errorOccurred(
                    QString("Ошибка во время чтения: %1").arg(input_path)
                );
                break;
            }

            quint64 *data = reinterpret_cast<quint64 *>(buffer);
            size_t count = bytes_read / sizeof(quint64);
            for (size_t i = 0; i < count; i++) {
                data[i] ^= xorValue;
            }

            if (outputFile.write(buffer, bytes_read) == -1) {
                emit errorOccurred(
                    QString("Ошибка во время записи: %1").arg(output_path)
                );
                break;
            }

            written_bytes += bytes_read;
            emit progressUpdated(written_bytes * 100 / file_size);
        }

        delete[] buffer;
        inputFile.close();
        outputFile.close();

        if (deleteInput && !inputFile.remove()) {
            emit errorOccurred(
                QString("Не вышло удалить файл: %1").arg(input_path)
            );
        }

        emit fileProcessed(input_path, output_path);
    }

signals:
    void progressUpdated(int percent);
    void fileProcessed(const QString &input_path, const QString &output_path);
    void errorOccurred(const QString &message);
};

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void stopButton_clicked();
    void browseOutputButton_clicked();
    void browseInputButton_clicked();
    void processFiles();
    void
    handleFileProcessed(const QString &input_path, const QString &output_path);
    void handleError(const QString &message);
    void updateProgress(int percent);

private:
    Ui::MainWindow *ui;
    QThread *thread_;
    FileProcessor *fileProcessor;
    QProgressDialog *progressDialog;
    QString currentProcessingFile;

    QString make_output_path(const QString &originalPath);
};

#endif  // MAINWINDOW_H
