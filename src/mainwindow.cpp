#include "mainwindow.h"
#include "llamaworker.h"
#include "llmcontroller.h"
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QGridLayout>
#include <QMessageBox>
#include <QTextStream>
#include <QMenuBar>
#include <QAction>
#include <QSplitter>
#include <QPixmap>
#include <QIcon>
#include <QThread>

/**
 * @brief Constructor - Initialize main window and UI
 */
MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), m_workerThread(nullptr), m_worker(nullptr) {
  m_controller = new LLMControllerDialog(this);

  setupUI();
  updateAnnotationCount();
  applyTheme();

  // Initialize Worker Thread
  m_workerThread = new QThread(this);
  m_worker = new LlamaWorker();
  m_worker->moveToThread(m_workerThread);

  connect(m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);
  connect(this, &MainWindow::requestGeneration, m_worker, &LlamaWorker::generate);
  connect(this, &MainWindow::requestLoadModel, m_worker, &LlamaWorker::loadModel);
  
  connect(m_worker, &LlamaWorker::finished, this, &MainWindow::onGenerationComplete);
  connect(m_worker, &LlamaWorker::error, this, &MainWindow::onGenerationError);
  connect(m_worker, &LlamaWorker::statsReady, this, [this](float seconds){
      m_elapsedTimeLabel->setText(QString("Time: %1 s").arg(seconds, 0, 'f', 2));
      m_controller->setLastElapsedTime(seconds);
  });
  connect(m_worker, &LlamaWorker::statusUpdate, this, [this](const QString &status){
      statusLabel->setText(status);
  });

  m_workerThread->start();
}

/**
 * @brief Destructor - Clean up resources
 */
MainWindow::~MainWindow() {
  if (m_workerThread) {
      m_workerThread->quit();
      m_workerThread->wait();
  }
}

/**
 * @brief Set up the user interface
 */
void MainWindow::setupUI() {
  // Create Menu Bar
  QMenuBar *menuBar = new QMenuBar(this);
  setMenuBar(menuBar);
  
  QMenu *fileMenu = menuBar->addMenu("File");
  QAction *settingsAction = fileMenu->addAction("LLM Settings...");
  connect(settingsAction, &QAction::triggered, this, &MainWindow::openSettings);
  
  QAction *exitAction = fileMenu->addAction("Exit");
  connect(exitAction, &QAction::triggered, this, &QWidget::close);

  // Create central widget
  centralWidget = new QWidget(this);
  setCentralWidget(centralWidget);

  mainLayout = new QVBoxLayout(centralWidget);
  mainLayout->setSpacing(10);
  mainLayout->setContentsMargins(10, 10, 10, 10);

  // Create Splitter
  QSplitter *splitter = new QSplitter(Qt::Vertical, centralWidget);

  // ====== Annotation Section ======
  annotationGroup = new QGroupBox("Damage Annotations", splitter);
  QVBoxLayout *annotationLayout = new QVBoxLayout(annotationGroup);

  // Annotation list widget
  annotationList = new QListWidget(annotationGroup);
  annotationList->setAlternatingRowColors(true);
  annotationList->setMinimumHeight(200); 
  annotationLayout->addWidget(annotationList);

  // Input Grid
  QGridLayout *inputGrid = new QGridLayout();

  // Classification
  inputGrid->addWidget(new QLabel("Classification:"), 0, 0);
  classificationInput = new QComboBox(annotationGroup);
  classificationInput->addItems(getValidClassifications());
  inputGrid->addWidget(classificationInput, 0, 1);

  // Severity
  inputGrid->addWidget(new QLabel("Severity:"), 0, 2);
  severityInput = new QComboBox(annotationGroup);
  QStringList severities = {"Low", "Medium", "High", "Critical"};
  for (const QString &sev : severities) {
      severityInput->addItem(sev);
      int index = severityInput->count() - 1;
      QPixmap pixmap(16, 16);
      pixmap.fill(getSeverityColor(sev));
      severityInput->setItemIcon(index, QIcon(pixmap));
  }
  inputGrid->addWidget(severityInput, 0, 3);

  // Radius
  inputGrid->addWidget(new QLabel("Blade Radius (m):"), 1, 0);
  radiusInput = new QLineEdit(annotationGroup);
  radiusInput->setPlaceholderText("e.g. 45.5");
  inputGrid->addWidget(radiusInput, 1, 1);

  // Side
  inputGrid->addWidget(new QLabel("Blade Side:"), 1, 2);
  sideInput = new QComboBox(annotationGroup);
  sideInput->addItems(
      {"Pressure Side", "Suction Side", "Leading Edge", "Trailing Edge"});
  inputGrid->addWidget(sideInput, 1, 3);

  // Description
  inputGrid->addWidget(new QLabel("Description:"), 2, 0);
  descriptionInput = new QLineEdit(annotationGroup);
  descriptionInput->setPlaceholderText("Enter detailed description...");
  inputGrid->addWidget(descriptionInput, 2, 1, 1, 3);

  annotationLayout->addLayout(inputGrid);

  // Buttons Layout
  QHBoxLayout *buttonLayout = new QHBoxLayout();
  buttonLayout->addStretch();

  // Add button
  addButton = new QPushButton("[+] Add Annotation", annotationGroup);
  addButton->setObjectName("addButton");
  buttonLayout->addWidget(addButton);

  // Update button
  updateButton = new QPushButton("[✎] Update Selected", annotationGroup);
  updateButton->setObjectName("updateButton");
  updateButton->setEnabled(false);
  buttonLayout->addWidget(updateButton);

  // Add Random button
  randomButton = new QPushButton("[?] Add Random", annotationGroup);
  randomButton->setObjectName("randomButton");
  buttonLayout->addWidget(randomButton);

  // Remove button
  removeButton = new QPushButton("[-] Remove Selected", annotationGroup);
  removeButton->setObjectName("removeButton");
  removeButton->setEnabled(false); // Disabled until an item is selected
  buttonLayout->addWidget(removeButton);

  // Settings button
  QPushButton *settingsButton = new QPushButton("⚙ Settings", annotationGroup);
  connect(settingsButton, &QPushButton::clicked, this, &MainWindow::openSettings);
  buttonLayout->addWidget(settingsButton);

  annotationLayout->addLayout(buttonLayout);

  // Annotation count label
  countLabel = new QLabel("Total annotations: 0", annotationGroup);
  countLabel->setStyleSheet("font-weight: bold; color: #2c3e50;");
  annotationLayout->addWidget(countLabel);

  // ====== Report Generation Section ======
  reportGroup = new QGroupBox("Expert Technical Conclusion", splitter);
  QVBoxLayout *reportLayout = new QVBoxLayout(reportGroup);

  // Generate button
  generateButton =
      new QPushButton("[>] Generate Expert Conclusion", reportGroup);
  generateButton->setObjectName("generateButton");
  reportLayout->addWidget(generateButton);

  // Status label
  statusLabel = new QLabel("Ready", reportGroup);
  statusLabel->setStyleSheet("color: #27ae60; font-style: italic;");
  reportLayout->addWidget(statusLabel);

  // Report output text area
  reportOutput = new QTextEdit(reportGroup);
  reportOutput->setReadOnly(true);
  reportOutput->setPlaceholderText(
      "The expert conclusion will appear here after generation...");
  reportOutput->setMinimumHeight(100);
  reportLayout->addWidget(reportOutput);

  // Status Bar Area
  QHBoxLayout *statusBarLayout = new QHBoxLayout();
  m_elapsedTimeLabel = new QLabel("Time: -- s");
  m_modelInfoLabel = new QLabel("Model: None");
  
  statusBarLayout->addWidget(statusLabel);
  statusBarLayout->addStretch();
  statusBarLayout->addWidget(m_modelInfoLabel);
  statusBarLayout->addWidget(m_elapsedTimeLabel);
  
  reportLayout->addLayout(statusBarLayout);

  // Add groups to splitter
  splitter->addWidget(annotationGroup);
  splitter->addWidget(reportGroup);

  // Set layout stretch factors for splitter
  splitter->setStretchFactor(0, 7); // Give more space to annotations
  splitter->setStretchFactor(1, 4); // Give less space to report section

  mainLayout->addWidget(splitter);

  // ====== Connect Signals and Slots ======
  connect(addButton, &QPushButton::clicked, this, &MainWindow::onAddAnnotation);
  connect(updateButton, &QPushButton::clicked, this, &MainWindow::onUpdateAnnotation);
  connect(randomButton, &QPushButton::clicked, this,
          &MainWindow::onAddRandomAnnotation);
  connect(removeButton, &QPushButton::clicked, this,
          &MainWindow::onRemoveAnnotation);
  connect(annotationList, &QListWidget::itemSelectionChanged, this, &MainWindow::onAnnotationSelected);
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
 * @brief Get static list of valid classifications
 */
const QStringList& MainWindow::getValidClassificationsList() {
  static const QStringList validClassifications = 
      {"Crack", "Erosion", "Lightning Strike", "Delamination", "Other"};
  return validClassifications;
}

/**
 * @brief Get list of valid classifications
 */
QStringList MainWindow::getValidClassifications() const {
  return getValidClassificationsList();
}

/**
 * @brief Sanitize and validate damage classification input
 */
QString MainWindow::sanitizeClassification(const QString &classification) {
  // Trim whitespace and normalize
  QString sanitized = classification.trimmed();
  
  // Check if it's valid
  if (!isValidClassification(sanitized)) {
    return QString(); // Return empty string for invalid classifications
  }
  
  return sanitized;
}

/**
 * @brief Check if a classification is valid
 */
bool MainWindow::isValidClassification(const QString &classification) {
  const QStringList& validClassifications = getValidClassificationsList();
  return validClassifications.contains(classification, Qt::CaseSensitive);
}

/**
 * @brief Create LLM prompt based on annotations
 */
QString MainWindow::createPrompt() {
  // Build a simple list of damage classifications only
  QStringList classificationsList;
  
  for (int i = 0; i < annotations.size(); ++i) {
    const auto &ann = annotations[i];
    
    // Sanitize the classification before adding it to the prompt
    QString sanitized = sanitizeClassification(ann.classification);
    
    // Only include valid classifications
    if (!sanitized.isEmpty()) {
      classificationsList.append(sanitized);
    }
  }
  
  // Join classifications with comma separator
  QString classificationsOnly = classificationsList.join(", ");

  // Wrap in Chat Template based on model type
  QString templateType = m_controller->getTemplateType();
  QString finalPrompt;

  if (templateType == "llama3") {
      finalPrompt = QString("<|begin_of_text|><|start_header_id|>system<|end_header_id|>\n\n"
              "You are a Wind Turbine Blade Expert. Follow the Blade Handbook "
              "2022 standards strictly.<|eot_id|>"
              "<|start_header_id|>user<|end_header_id|>\n\n"
              "%1<|eot_id|>"
              "<|start_header_id|>assistant<|end_header_id|>\n\n")
          .arg(classificationsOnly);
  } else if (templateType == "chatml") {
      finalPrompt = QString("<|im_start|>system\n"
              "You are a Wind Turbine Blade Expert. Follow the Blade Handbook "
              "2022 standards strictly.<|im_end|>\n"
              "<|im_start|>user\n"
              "%1<|im_end|>\n"
              "<|im_start|>assistant\n")
          .arg(classificationsOnly);
  } else if (templateType == "mistral") {
      finalPrompt = QString("<s>[INST] You are a Wind Turbine Blade Expert. Follow the Blade Handbook "
              "2022 standards strictly.\n\n"
              "%1 [/INST]")
          .arg(classificationsOnly);
  } else if (templateType == "phi3") {
      finalPrompt = QString("<|user|>\n"
              "You are a Wind Turbine Blade Expert. Follow the Blade Handbook "
              "2022 standards strictly.\n\n"
              "%1<|end|>\n"
              "<|assistant|>\n")
          .arg(classificationsOnly);
  } else if (templateType == "gemma") {
      finalPrompt = QString("<start_of_turn>user\n"
              "You are a Wind Turbine Blade Expert. Follow the Blade Handbook "
              "2022 standards strictly.\n\n"
              "%1<end_of_turn>\n"
              "<start_of_turn>model\n")
          .arg(classificationsOnly);
  } else {
      // Fallback to raw prompt if unknown
      finalPrompt = classificationsOnly;
  }

  return finalPrompt;
}

/**
 * @brief Handle "Add Annotation" button click
 */
void MainWindow::onAddAnnotation() {
  Annotation ann;
  ann.classification = sanitizeClassification(classificationInput->currentText());
  ann.severity = severityInput->currentText();
  ann.radius = radiusInput->text().trimmed();
  ann.description = descriptionInput->text().trimmed();
  ann.side = sideInput->currentText();

  // Validate classification (sanitizeClassification returns empty string if invalid)
  if (ann.classification.isEmpty()) {
    QMessageBox::warning(this, "Invalid Classification",
                         "Please select a valid damage classification.");
    return;
  }

  if (ann.radius.isEmpty() || ann.description.isEmpty()) {
    QMessageBox::warning(this, "Incomplete Data",
                         "Please enter a radius and description.");
    return;
  }

  // Add to storage
  annotations.append(ann);

  // Add to list widget
  QListWidgetItem *item = new QListWidgetItem(ann.toString());
  QPixmap pixmap(16, 16);
  pixmap.fill(getSeverityColor(ann.severity));
  item->setIcon(QIcon(pixmap));
  annotationList->addItem(item);

  // Clear numeric/text fields
  radiusInput->clear();
  descriptionInput->clear();
  classificationInput->setFocus();

  // Update count
  updateAnnotationCount();
}

/**
 * @brief Handle "Update Annotation" button click
 */
void MainWindow::onUpdateAnnotation() {
  int row = annotationList->currentRow();
  if (row < 0 || row >= annotations.size()) return;

  Annotation &ann = annotations[row];
  ann.classification = sanitizeClassification(classificationInput->currentText());
  ann.severity = severityInput->currentText();
  ann.radius = radiusInput->text().trimmed();
  ann.description = descriptionInput->text().trimmed();
  ann.side = sideInput->currentText();

  // Validate classification (sanitizeClassification returns empty string if invalid)
  if (ann.classification.isEmpty()) {
    QMessageBox::warning(this, "Invalid Classification",
                         "Please select a valid damage classification.");
    return;
  }

  if (ann.radius.isEmpty() || ann.description.isEmpty()) {
    QMessageBox::warning(this, "Incomplete Data",
                         "Please enter a radius and description.");
    return;
  }

  // Update list widget item
  QListWidgetItem *item = annotationList->item(row);
  item->setText(ann.toString());
  QPixmap pixmap(16, 16);
  pixmap.fill(getSeverityColor(ann.severity));
  item->setIcon(QIcon(pixmap));

  // Clear numeric/text fields
  radiusInput->clear();
  descriptionInput->clear();
  classificationInput->setFocus();
  
  // Deselect
  annotationList->clearSelection();
}

/**
 * @brief Handle annotation selection change
 */
void MainWindow::onAnnotationSelected() {
  int row = annotationList->currentRow();
  bool hasSelection = (row >= 0 && row < annotations.size());
  
  removeButton->setEnabled(hasSelection);
  updateButton->setEnabled(hasSelection);
  
  if (hasSelection) {
    const Annotation &ann = annotations[row];
    classificationInput->setCurrentText(ann.classification);
    severityInput->setCurrentText(ann.severity);
    radiusInput->setText(ann.radius);
    descriptionInput->setText(ann.description);
    sideInput->setCurrentText(ann.side);
  } else {
    radiusInput->clear();
    descriptionInput->clear();
    classificationInput->setCurrentIndex(0);
    severityInput->setCurrentIndex(0);
    sideInput->setCurrentIndex(0);
  }
}

/**
 * @brief Handle "Add Random" button click
 */
void MainWindow::onAddRandomAnnotation() {
  QStringList classifications = getValidClassifications();
  QStringList severities = {"Low", "Medium", "High", "Critical"};
  QStringList sides = {"Pressure Side", "Suction Side", "Leading Edge",
                       "Trailing Edge"};
  QStringList sampleDescriptions = {
      "Minor surface wear detected",       "Hairline fracture along the edge",
      "Evidence of recent strike impact",  "Surface coating peeling off",
      "Noticeable structural deformation", "Moisture ingress at the tip"};

  Annotation ann;
  ann.classification = classifications.at(rand() % classifications.size());
  ann.severity = severities.at(rand() % severities.size());
  ann.side = sides.at(rand() % sides.size());
  ann.radius = QString::number((rand() % 900) / 10.0, 'f', 1);
  ann.description = sampleDescriptions.at(rand() % sampleDescriptions.size());

  // Add to storage
  annotations.append(ann);

  // Add to list widget
  QListWidgetItem *item = new QListWidgetItem(ann.toString());
  QPixmap pixmap(16, 16);
  pixmap.fill(getSeverityColor(ann.severity));
  item->setIcon(QIcon(pixmap));
  annotationList->addItem(item);

  // Update count
  updateAnnotationCount();
}

/**
 * @brief Handle "Remove" button click
 */
void MainWindow::onRemoveAnnotation() {
  int currentRow = annotationList->currentRow();

  if (currentRow < 0) {
    return; // No selection
  }

  // Remove from data storage
  annotations.removeAt(currentRow);

  // Remove from list widget
  delete annotationList->takeItem(currentRow);

  // Update count
  updateAnnotationCount();

  // Disable remove button if no items remain
  if (annotationList->count() == 0) {
    removeButton->setEnabled(false);
  }
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

  LlamaParams params = m_controller->getParams();

  // Check if model file exists
  if (!QFile::exists(params.modelPath)) {
    QMessageBox::warning(this, "Model Not Found", 
        "Please select a valid model in the Settings menu.");
    openSettings();
    return;
  }

  // Create prompt
  QString prompt = createPrompt();

  // Update UI state
  generateButton->setEnabled(false);
  addButton->setEnabled(false);
  statusLabel->setText("Generating conclusion... (this may take a moment)");
  statusLabel->setStyleSheet("color: #f39c12; font-style: italic;");
  reportOutput->clear();
  reportOutput->setPlaceholderText("Please wait, LLM is generating...");

  m_modelInfoLabel->setText("Model: " + QFileInfo(params.modelPath).fileName());

  // Request generation via signal
  emit requestGeneration(prompt, params);
}

void MainWindow::openSettings() {
    m_controller->show();
    m_controller->raise();
    m_controller->activateWindow();
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
}

/**
 * @brief Apply modern theme to the application
 */
void MainWindow::applyTheme() {
  // Set window properties
  setWindowTitle("LocalLLM - Wind Turbine Blade Inspector");
  setMinimumSize(900, 700);

  // Modern flat theme stylesheet
  QString theme = R"(
    /* Main window background */
    QMainWindow {
      background-color: #f5f6fa;
    }
    
    /* Group boxes */
    QGroupBox {
      font-weight: bold;
      font-size: 14px;
      color: #2c3e50;
      border: 2px solid #dfe6e9;
      border-radius: 8px;
      margin-top: 12px;
      padding-top: 15px;
      background-color: white;
    }
    
    QGroupBox::title {
      subcontrol-origin: margin;
      subcontrol-position: top left;
      padding: 5px 10px;
      color: #2c3e50;
    }
    
    /* Input fields */
    QLineEdit, QComboBox {
      border: 1px solid #dfe6e9;
      border-radius: 4px;
      padding: 6px 10px;
      background-color: white;
      selection-background-color: #3498db;
      font-size: 13px;
    }
    
    QLineEdit:focus, QComboBox:focus {
      border: 2px solid #3498db;
    }
    
    QComboBox::drop-down {
      border: none;
      width: 20px;
    }
    
    /* List widget */
    QListWidget {
      border: 1px solid #dfe6e9;
      border-radius: 4px;
      background-color: white;
      font-size: 13px;
      padding: 4px;
    }
    
    QListWidget::item {
      padding: 8px;
      border-radius: 4px;
    }
    
    QListWidget::item:selected {
      background-color: #3498db;
      color: white;
    }
    
    QListWidget::item:hover {
      background-color: #ecf0f1;
      color: #2c3e50;
    }
    
    /* Text edit */
    QTextEdit {
      border: 1px solid #dfe6e9;
      border-radius: 4px;
      background-color: white;
      font-size: 13px;
      padding: 8px;
    }
    
    /* Labels */
    QLabel {
      color: #2c3e50;
      font-size: 13px;
    }
    
    /* Buttons - General */
    QPushButton {
      border: none;
      border-radius: 6px;
      padding: 10px 20px;
      font-weight: bold;
      font-size: 13px;
      min-height: 20px;
    }
    
    QPushButton:hover {
      opacity: 0.9;
    }
    
    QPushButton:pressed {
      padding-top: 12px;
      padding-bottom: 8px;
    }
    
    /* Add button - Green */
    QPushButton#addButton {
      background-color: #27ae60;
      color: white;
    }
    
    QPushButton#addButton:hover {
      background-color: #229954;
    }
    
    QPushButton#addButton:disabled {
      background-color: #95a5a6;
    }

    /* Update button - Teal */
    QPushButton#updateButton {
      background-color: #1abc9c;
      color: white;
    }

    QPushButton#updateButton:hover {
      background-color: #16a085;
    }

    QPushButton#updateButton:disabled {
      background-color: #95a5a6;
    }
    
    /* Random button - Orange */
    QPushButton#randomButton {
      background-color: #e67e22;
      color: white;
    }
    
    QPushButton#randomButton:hover {
      background-color: #d35400;
    }
    
    /* Remove button - Red */
    QPushButton#removeButton {
      background-color: #e74c3c;
      color: white;
    }
    
    QPushButton#removeButton:hover {
      background-color: #c0392b;
    }
    
    QPushButton#removeButton:disabled {
      background-color: #95a5a6;
    }
    
    /* Generate button - Blue */
    QPushButton#generateButton {
      background-color: #3498db;
      color: white;
      padding: 12px 24px;
      font-size: 14px;
    }
    
    QPushButton#generateButton:hover {
      background-color: #2980b9;
    }
    
    QPushButton#generateButton:disabled {
      background-color: #95a5a6;
    }
  )";

  setStyleSheet(theme);
}

QColor MainWindow::getSeverityColor(const QString &severity) {
  if (severity == "Low") return QColor("#2ecc71");      // Green
  if (severity == "Medium") return QColor("#f1c40f");   // Yellow
  if (severity == "High") return QColor("#e67e22");     // Orange
  if (severity == "Critical") return QColor("#e74c3c"); // Red
  return Qt::black;
}
