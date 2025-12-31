#ifndef LLMCONTROLLER_H
#define LLMCONTROLLER_H

#include <QDialog>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSettings>
#include <QTableWidget>
#include <QTextEdit>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QLabel>
#include <QProgressBar>
#include "llamaworker.h"

struct ModelInfo {
    QString name;
    QString size;
    QString params;
    QString description;
    QString url;
    QString filename;
};

class LLMControllerDialog : public QDialog {
    Q_OBJECT

public:
    explicit LLMControllerDialog(QWidget *parent = nullptr);
    ~LLMControllerDialog();

    LlamaParams getParams() const;
    void setLastElapsedTime(float seconds);

private slots:
    void savePrompt();
    void downloadModel(int row);
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onDownloadFinished();
    void saveSettings();

private:
    void setupUi();
    void loadSettings();
    void loadPrompt();
    void setupModelList();

    // UI Elements
    QTextEdit *m_promptEdit;
    QDoubleSpinBox *m_tempSpin;
    QDoubleSpinBox *m_topPSpin;
    QSpinBox *m_threadsSpin;
    QSpinBox *m_ctxSpin;
    QTableWidget *m_modelTable;
    QLabel *m_statusLabel;
    QProgressBar *m_progressBar;
    QLabel *m_currentModelLabel;

    // Data
    QList<ModelInfo> m_models;
    QNetworkAccessManager *m_networkManager;
    QNetworkReply *m_currentReply;
    QSettings m_settings;
    QString m_currentModelPath;
};

#endif // LLMCONTROLLER_H
