#include "mainwindow.h"
#include "llamaworker.h"
#include <QDir>
#include <QFileDialog>
#include <QMessageBox>

/**
 * @brief Constructor - Initialize main window and UI
 */
MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), worker(nullptr) {
  // Set default model path (relative to executable or absolute)
  modelPath = QDir::currentPath() + "/models/model.gguf";

  setupUI();
  updateAnnotationCount();
}

/**
 * @brief Destructor - Clean up resources
 */
MainWindow::~MainWindow() {
  // Clean up worker if it exists
  if (worker) {
    worker->quit();
    worker->wait();
    delete worker;
  }
}

/**
 * @brief Set up the user interface
 */
void MainWindow::setupUI() {
  // Create central widget
  centralWidget = new QWidget(this);
  setCentralWidget(centralWidget);

  mainLayout = new QVBoxLayout(centralWidget);
  mainLayout->setSpacing(10);
  mainLayout->setContentsMargins(10, 10, 10, 10);

  // ====== Annotation Section ======
  annotationGroup = new QGroupBox("Annotations", centralWidget);
  QVBoxLayout *annotationLayout = new QVBoxLayout(annotationGroup);

  // Annotation list widget
  annotationList = new QListWidget(annotationGroup);
  annotationList->setAlternatingRowColors(true);
  annotationLayout->addWidget(annotationList);

  // Input row (text input + add button)
  QHBoxLayout *inputLayout = new QHBoxLayout();
  annotationInput = new QLineEdit(annotationGroup);
  annotationInput->setPlaceholderText("Enter annotation text...");
  addButton = new QPushButton("Add Annotation", annotationGroup);
  addButton->setMaximumWidth(150);

  inputLayout->addWidget(annotationInput);
  inputLayout->addWidget(addButton);
  annotationLayout->addLayout(inputLayout);

  // Annotation count label
  countLabel = new QLabel("Total annotations: 0", annotationGroup);
  countLabel->setStyleSheet("font-weight: bold; color: #2c3e50;");
  annotationLayout->addWidget(countLabel);

  mainLayout->addWidget(annotationGroup);

  // ====== Report Generation Section ======
  reportGroup = new QGroupBox("Report Conclusion", centralWidget);
  QVBoxLayout *reportLayout = new QVBoxLayout(reportGroup);

  // Generate button
  generateButton = new QPushButton("Generate Report Conclusion", reportGroup);
  generateButton->setStyleSheet(
      "QPushButton { background-color: #3498db; color: white; padding: 8px; "
      "font-weight: bold; }"
      "QPushButton:hover { background-color: #2980b9; }"
      "QPushButton:disabled { background-color: #95a5a6; }");
  reportLayout->addWidget(generateButton);

  // Status label
  statusLabel = new QLabel("Ready", reportGroup);
  statusLabel->setStyleSheet("color: #27ae60; font-style: italic;");
  reportLayout->addWidget(statusLabel);

  // Report output text area
  reportOutput = new QTextEdit(reportGroup);
  reportOutput->setReadOnly(true);
  reportOutput->setPlaceholderText(
      "Generated report conclusion will appear here...");
  reportOutput->setMinimumHeight(150);
  reportLayout->addWidget(reportOutput);

  mainLayout->addWidget(reportGroup);

  // Set layout stretch factors (60% annotations, 40% report)
  mainLayout->setStretch(0, 6);
  mainLayout->setStretch(1, 4);

  // ====== Connect Signals and Slots ======
  connect(addButton, &QPushButton::clicked, this, &MainWindow::onAddAnnotation);
  connect(annotationInput, &QLineEdit::returnPressed, this,
          &MainWindow::onAddAnnotation);
  connect(generateButton, &QPushButton::clicked, this,
          &MainWindow::onGenerateReport);
}

/**
 * @brief Update the annotation count display
 */
void MainWindow::updateAnnotationCount() {
  countLabel->setText(QString("Total annotations: %1").arg(annotations.size()));
}

/**
 * @brief Create LLM prompt based on annotation count
 */
QString MainWindow::createPrompt(int count) {
  QString prompt = QString("You are a helpful assistant. Based on %1 "
                           "annotations made by the user, "
                           "write a brief conclusion for their report. ")
                       .arg(count);

  if (count < 5) {
    prompt += "Since there are few annotations (less than 5), suggest that "
              "more analysis may be needed. ";
  } else if (count >= 10) {
    prompt += "Since there are many annotations (10 or more), highlight that "
              "there is substantial data for insights. ";
  }

  prompt += "\n\nWrite a 2-3 sentence conclusion:";

  return prompt;
}

/**
 * @brief Handle "Add Annotation" button click
 */
void MainWindow::onAddAnnotation() {
  QString text = annotationInput->text().trimmed();

  if (text.isEmpty()) {
    QMessageBox::warning(this, "Empty Annotation",
                         "Please enter annotation text before adding.");
    return;
  }

  // Add to storage
  annotations.append(text);

  // Add to list widget
  annotationList->addItem(text);

  // Clear input field
  annotationInput->clear();
  annotationInput->setFocus();

  // Update count
  updateAnnotationCount();
}

/**
 * @brief Handle "Generate Report" button click
 */
void MainWindow::onGenerateReport() {
  // Check if we have annotations
  if (annotations.isEmpty()) {
    QMessageBox::information(
        this, "No Annotations",
        "Please add some annotations before generating a report.");
    return;
  }

  // Check if model file exists
  if (!QFile::exists(modelPath)) {
    QMessageBox::StandardButton reply =
        QMessageBox::question(this, "Model Not Found",
                              QString("Model file not found at:\n%1\n\nWould "
                                      "you like to select a model file?")
                                  .arg(modelPath),
                              QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
      QString selectedPath = QFileDialog::getOpenFileName(
          this, "Select GGUF Model File", QDir::currentPath() + "/models",
          "GGUF Model Files (*.gguf);;All Files (*)");

      if (!selectedPath.isEmpty()) {
        modelPath = selectedPath;
      } else {
        return; // User cancelled
      }
    } else {
      return; // User declined
    }
  }

  // Create prompt
  QString prompt = createPrompt(annotations.size());

  // Update UI state
  generateButton->setEnabled(false);
  addButton->setEnabled(false);
  statusLabel->setText("Generating conclusion... (this may take a moment)");
  statusLabel->setStyleSheet("color: #f39c12; font-style: italic;");
  reportOutput->clear();
  reportOutput->setPlaceholderText("Please wait, LLM is generating...");

  // Create and start worker thread
  worker = new LlamaWorker(modelPath, prompt);

  connect(worker, &LlamaWorker::finished, this,
          &MainWindow::onGenerationComplete);
  connect(worker, &LlamaWorker::error, this, &MainWindow::onGenerationError);
  connect(worker, &QThread::finished, worker, &QObject::deleteLater);

  worker->start();
}

/**
 * @brief Handle successful LLM generation
 */
void MainWindow::onGenerationComplete(const QString &result) {
  // Update UI with result
  reportOutput->setPlaceholderText(
      "Generated report conclusion will appear here...");
  reportOutput->setPlainText(result);

  // Reset UI state
  generateButton->setEnabled(true);
  addButton->setEnabled(true);
  statusLabel->setText("Generation complete!");
  statusLabel->setStyleSheet("color: #27ae60; font-style: italic;");

  // Clean up worker
  worker = nullptr;
}

/**
 * @brief Handle LLM generation error
 */
void MainWindow::onGenerationError(const QString &error) {
  // Show error message
  reportOutput->setPlaceholderText(
      "Generated report conclusion will appear here...");
  QMessageBox::critical(
      this, "Generation Error",
      QString("Failed to generate report conclusion:\n\n%1").arg(error));

  // Reset UI state
  generateButton->setEnabled(true);
  addButton->setEnabled(true);
  statusLabel->setText("Error occurred");
  statusLabel->setStyleSheet("color: #e74c3c; font-style: italic;");

  // Clean up worker
  worker = nullptr;
}
