#include "llmcontroller.h"
#include "consolelogger.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QPushButton>
#include <QHeaderView>
#include <QFile>
#include <QDir>
#include <QMessageBox>
#include <QStandardPaths>
#include <QDebug>
#include <QGroupBox>
#include <QFormLayout>
#include <QDialogButtonBox>

LLMControllerDialog::LLMControllerDialog(QWidget *parent)
    : QDialog(parent), m_networkManager(new QNetworkAccessManager(this)), m_currentReply(nullptr),
      m_settings("settings.ini", QSettings::IniFormat)
{
    setWindowTitle("LLM Controller");
    resize(800, 600);
    setupUi();
    setupModelList();
    loadSettings();
    loadPrompt();
}

LLMControllerDialog::~LLMControllerDialog()
{
    saveSettings();
}

void LLMControllerDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    QTabWidget *tabWidget = new QTabWidget(this);

    // --- Settings Tab ---
    QWidget *settingsTab = new QWidget();
    QVBoxLayout *settingsLayout = new QVBoxLayout(settingsTab);

    // Prompt Section
    settingsLayout->addWidget(new QLabel("System Prompt:"));
    m_promptEdit = new QTextEdit();
    settingsLayout->addWidget(m_promptEdit);

    QPushButton *savePromptBtn = new QPushButton("Save Prompt");
    connect(savePromptBtn, &QPushButton::clicked, this, &LLMControllerDialog::savePrompt);
    settingsLayout->addWidget(savePromptBtn);

    // Parameters Section
    QGroupBox *paramsGroup = new QGroupBox("Parameters");
    QFormLayout *paramsLayout = new QFormLayout(paramsGroup);

    m_tempSpin = new QDoubleSpinBox();
    m_tempSpin->setRange(0.0, 2.0);
    m_tempSpin->setSingleStep(0.1);
    m_tempSpin->setValue(0.7);
    paramsLayout->addRow("Temperature:", m_tempSpin);

    m_topPSpin = new QDoubleSpinBox();
    m_topPSpin->setRange(0.0, 1.0);
    m_topPSpin->setSingleStep(0.05);
    m_topPSpin->setValue(0.9);
    paramsLayout->addRow("Top-P:", m_topPSpin);

    settingsLayout->addWidget(paramsGroup);
    tabWidget->addTab(settingsTab, "Settings");

    // --- Models Tab ---
    QWidget *modelsTab = new QWidget();
    QVBoxLayout *modelsLayout = new QVBoxLayout(modelsTab);

    m_currentModelLabel = new QLabel("Selected Model: None");
    modelsLayout->addWidget(m_currentModelLabel);

    m_modelTable = new QTableWidget();
    m_modelTable->setColumnCount(5);
    m_modelTable->setHorizontalHeaderLabels({"Name", "Size", "Params", "Description", "Action"});
    m_modelTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    modelsLayout->addWidget(m_modelTable);

    m_progressBar = new QProgressBar();
    m_progressBar->setVisible(false);
    modelsLayout->addWidget(m_progressBar);

    tabWidget->addTab(modelsTab, "Models");

    // --- Console Tab ---
    QWidget *consoleTab = new QWidget();
    QVBoxLayout *consoleLayout = new QVBoxLayout(consoleTab);
    
    QTextEdit *consoleEdit = new QTextEdit();
    consoleEdit->setReadOnly(true);
    consoleEdit->setStyleSheet("background-color: #1e1e1e; color: #d4d4d4; font-family: Monospace; font-size: 11px;");
    consoleLayout->addWidget(consoleEdit);
    
    tabWidget->addTab(consoleTab, "Console");

    // Connect Console Logger
    connect(&ConsoleLogger::instance(), &ConsoleLogger::logMessage, 
            this, [consoleEdit](const QString &msg) {
        consoleEdit->moveCursor(QTextCursor::End);
        consoleEdit->insertPlainText(msg);
        if (!msg.endsWith('\n')) consoleEdit->insertPlainText("\n");
        consoleEdit->moveCursor(QTextCursor::End);
    });

    mainLayout->addWidget(tabWidget);

    // Status Bar
    m_statusLabel = new QLabel("Ready");
    mainLayout->addWidget(m_statusLabel);

    // Dialog Buttons
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);
}

void LLMControllerDialog::setupModelList()
{
    m_models = {
        {"Llama 3.2 1B", "1.2 GB", "1B", "Fastest, good for mobile/edge", 
         "https://huggingface.co/bartowski/Llama-3.2-1B-Instruct-GGUF/resolve/main/Llama-3.2-1B-Instruct-Q4_K_M.gguf", "llama-3.2-1b-instruct-q4_k_m.gguf"},
        {"Llama 3.2 3B", "2.4 GB", "3B", "Balanced performance/quality", 
         "https://huggingface.co/bartowski/Llama-3.2-3B-Instruct-GGUF/resolve/main/Llama-3.2-3B-Instruct-Q4_K_M.gguf", "llama-3.2-3b-instruct-q4_k_m.gguf"},
        {"Phi-3.5 Mini", "2.2 GB", "3.8B", "Strong reasoning capabilities", 
         "https://huggingface.co/bartowski/Phi-3.5-mini-instruct-GGUF/resolve/main/Phi-3.5-mini-instruct-Q4_K_M.gguf", "phi-3.5-mini-instruct-q4_k_m.gguf"},
        {"Qwen 2.5 1.5B", "1.1 GB", "1.5B", "Good for coding & general tasks", 
         "https://huggingface.co/bartowski/Qwen2.5-1.5B-Instruct-GGUF/resolve/main/Qwen2.5-1.5B-Instruct-Q4_K_M.gguf", "qwen2.5-1.5b-instruct-q4_k_m.gguf"}
    };

    m_modelTable->setRowCount(m_models.size());
    for (int i = 0; i < m_models.size(); ++i) {
        const auto &model = m_models[i];
        m_modelTable->setItem(i, 0, new QTableWidgetItem(model.name));
        m_modelTable->setItem(i, 1, new QTableWidgetItem(model.size));
        m_modelTable->setItem(i, 2, new QTableWidgetItem(model.params));
        m_modelTable->setItem(i, 3, new QTableWidgetItem(model.description));

        QPushButton *btn = new QPushButton();
        QString modelPath = "models/" + model.filename;
        if (QFile::exists(modelPath)) {
            btn->setText("Select");
            connect(btn, &QPushButton::clicked, [this, modelPath, i]() {
                m_currentModelPath = modelPath;
                m_currentModelLabel->setText("Selected Model: " + m_models[i].name);
                saveSettings();
            });
        } else {
            btn->setText("Download");
            connect(btn, &QPushButton::clicked, [this, i]() { downloadModel(i); });
        }
        m_modelTable->setCellWidget(i, 4, btn);
    }
}

void LLMControllerDialog::downloadModel(int row)
{
    if (m_currentReply) return;

    const auto &model = m_models[row];
    QNetworkRequest request(QUrl(model.url));
    m_currentReply = m_networkManager->get(request);

    m_progressBar->setVisible(true);
    m_progressBar->setValue(0);
    m_statusLabel->setText("Downloading " + model.name + "...");

    connect(m_currentReply, &QNetworkReply::downloadProgress, this, &LLMControllerDialog::onDownloadProgress);
    connect(m_currentReply, &QNetworkReply::finished, this, &LLMControllerDialog::onDownloadFinished);
    
    // Store the target filename in the reply object property for retrieval later
    m_currentReply->setProperty("filename", model.filename);
    m_currentReply->setProperty("row", row);
}

void LLMControllerDialog::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    if (bytesTotal > 0) {
        m_progressBar->setValue((bytesReceived * 100) / bytesTotal);
    }
}

void LLMControllerDialog::onDownloadFinished()
{
    if (m_currentReply->error() == QNetworkReply::NoError) {
        QByteArray data = m_currentReply->readAll();
        QString filename = m_currentReply->property("filename").toString();
        int row = m_currentReply->property("row").toInt();

        QDir dir("models");
        if (!dir.exists()) dir.mkpath(".");

        QFile file("models/" + filename);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(data);
            file.close();
            m_statusLabel->setText("Download complete.");
            
            // Update button to "Select"
            QPushButton *btn = qobject_cast<QPushButton*>(m_modelTable->cellWidget(row, 4));
            if (btn) {
                btn->setText("Select");
                btn->disconnect();
                QString modelPath = "models/" + filename;
                connect(btn, &QPushButton::clicked, [this, modelPath, row]() {
                    m_currentModelPath = modelPath;
                    m_currentModelLabel->setText("Selected Model: " + m_models[row].name);
                    saveSettings();
                });
            }
        } else {
            m_statusLabel->setText("Error saving file.");
        }
    } else {
        m_statusLabel->setText("Download error: " + m_currentReply->errorString());
    }

    m_currentReply->deleteLater();
    m_currentReply = nullptr;
    m_progressBar->setVisible(false);
}

void LLMControllerDialog::savePrompt()
{
    QDir dir("context");
    if (!dir.exists()) dir.mkpath(".");

    QFile file("context/custom_prompt.txt");
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << m_promptEdit->toPlainText();
        file.close();
        m_statusLabel->setText("Prompt saved.");
    } else {
        m_statusLabel->setText("Error saving prompt.");
    }
}

void LLMControllerDialog::loadPrompt()
{
    QString promptPath = "context/custom_prompt.txt";
    if (!QFile::exists(promptPath)) {
        promptPath = "context/input_context.txt";
    }

    QFile file(promptPath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        m_promptEdit->setText(in.readAll());
        file.close();
    }
}

void LLMControllerDialog::saveSettings()
{
    m_settings.setValue("temperature", m_tempSpin->value());
    m_settings.setValue("topP", m_topPSpin->value());
    m_settings.setValue("modelPath", m_currentModelPath);
}

void LLMControllerDialog::loadSettings()
{
    m_tempSpin->setValue(m_settings.value("temperature", 0.7).toDouble());
    m_topPSpin->setValue(m_settings.value("topP", 0.9).toDouble());
    m_currentModelPath = m_settings.value("modelPath", "").toString();
    
    if (!m_currentModelPath.isEmpty()) {
        m_currentModelLabel->setText("Selected Model: " + QFileInfo(m_currentModelPath).fileName());
    }
}

LlamaParams LLMControllerDialog::getParams() const
{
    LlamaParams params;
    params.temperature = m_tempSpin->value();
    params.topP = m_topPSpin->value();
    params.modelPath = m_currentModelPath;
    // Default values for others
    params.contextSize = 4096;
    params.threads = 4;
    return params;
}

void LLMControllerDialog::setLastElapsedTime(float seconds)
{
    m_statusLabel->setText(QString("Last run: %1 seconds").arg(seconds, 0, 'f', 2));
}
