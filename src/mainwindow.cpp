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
  classificationInput->addItems(
      {"Crack", "Erosion", "Lightning Strike", "Delamination", "Other"});
  inputGrid->addWidget(classificationInput, 0, 1);

  // Damage Classification (renamed from Severity for clarity)
  inputGrid->addWidget(new QLabel("Damage Classification:"), 0, 2);
  damageClassificationInput = new QComboBox(annotationGroup);
  QStringList damageClassifications = {"Minor", "Moderate", "Severe", "Critical"};
  for (const QString &dmgClass : damageClassifications) {
      damageClassificationInput->addItem(dmgClass);
      int index = damageClassificationInput->count() - 1;
      QPixmap pixmap(16, 16);
      pixmap.fill(getDamageClassificationColor(dmgClass));
      damageClassificationInput->setItemIcon(index, QIcon(pixmap));
  }
  inputGrid->addWidget(damageClassificationInput, 0, 3);

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
 * @brief Create LLM prompt based on annotations
 */
QString MainWindow::createPrompt() {
  QString contextPath = QDir::currentPath() + "/context/custom_prompt.txt";
  if (!QFile::exists(contextPath)) {
      contextPath = QDir::currentPath() + "/context/input_context.txt";
  }
  
  QFile contextFile(contextPath);

  if (!contextFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return "Critical Error: input_context.txt not found.";
  }

  QTextStream in(&contextFile);
  QString templateContent = in.readAll();
  contextFile.close();

  // 1. Format the dynamic annotation data
  QString annotationsSection;
  for (int i = 0; i < annotations.size(); ++i) {
    const auto &ann = annotations[i];
    
    // Map damage classification to category hint to guide the LLM
    QString categoryHint;
    if (ann.damageClassification == "Minor") categoryHint = " (Category 1-2)";
    else if (ann.damageClassification == "Moderate") categoryHint = " (Category 3)";
    else if (ann.damageClassification == "Severe") categoryHint = " (Category 4)";
    else if (ann.damageClassification == "Critical") categoryHint = " (Category 5)";

    annotationsSection += QString("--- DAMAGE #%1 ---\n").arg(i + 1);
    annotationsSection += QString("Type: %1\n").arg(ann.classification);
    annotationsSection += QString("Damage Classification: %1%2\n").arg(ann.damageClassification, categoryHint);
    annotationsSection += QString("Description: %1\n").arg(ann.description);
    annotationsSection +=
        QString("Location: %1m on %2\n\n").arg(ann.radius, ann.side);
  }

  // 2. Inject data into the template
  QString fullInstructions = templateContent;
  fullInstructions.replace("{{ANNOTATIONS}}", annotationsSection);

  // 3. Wrap in Chat Template based on model type
  QString templateType = m_controller->getTemplateType();
  QString finalPrompt;

  if (templateType == "llama3") {
      finalPrompt = QString("<|begin_of_text|><|start_header_id|>system<|end_header_id|>\n\n"
              "You are a Wind Turbine Blade Expert. Follow the Blade Handbook "
              "2022 standards strictly.<|eot_id|>"
              "<|start_header_id|>user<|end_header_id|>\n\n"
              "%1<|eot_id|>"
              "<|start_header_id|>assistant<|end_header_id|>\n\n")
          .arg(fullInstructions);
  } else if (templateType == "chatml") {
      finalPrompt = QString("<|im_start|>system\n"
              "You are a Wind Turbine Blade Expert. Follow the Blade Handbook "
              "2022 standards strictly.<|im_end|>\n"
              "<|im_start|>user\n"
              "%1<|im_end|>\n"
              "<|im_start|>assistant\n")
          .arg(fullInstructions);
  } else if (templateType == "mistral") {
      finalPrompt = QString("<s>[INST] You are a Wind Turbine Blade Expert. Follow the Blade Handbook "
              "2022 standards strictly.\n\n"
              "%1 [/INST]")
          .arg(fullInstructions);
  } else if (templateType == "phi3") {
      finalPrompt = QString("<|user|>\n"
              "You are a Wind Turbine Blade Expert. Follow the Blade Handbook "
              "2022 standards strictly.\n\n"
              "%1<|end|>\n"
              "<|assistant|>\n")
          .arg(fullInstructions);
  } else if (templateType == "gemma") {
      finalPrompt = QString("<start_of_turn>user\n"
              "You are a Wind Turbine Blade Expert. Follow the Blade Handbook "
              "2022 standards strictly.\n\n"
              "%1<end_of_turn>\n"
              "<start_of_turn>model\n")
          .arg(fullInstructions);
  } else {
      // Fallback to raw prompt if unknown
      finalPrompt = fullInstructions;
  }

  return finalPrompt;
}

/**
 * @brief Handle "Add Annotation" button click
 */
void MainWindow::onAddAnnotation() {
  Annotation ann;
  ann.classification = classificationInput->currentText();
  ann.damageClassification = damageClassificationInput->currentText();
  ann.radius = radiusInput->text().trimmed();
  ann.description = descriptionInput->text().trimmed();
  ann.side = sideInput->currentText();

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
  pixmap.fill(getDamageClassificationColor(ann.damageClassification));
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
  ann.classification = classificationInput->currentText();
  ann.damageClassification = damageClassificationInput->currentText();
  ann.radius = radiusInput->text().trimmed();
  ann.description = descriptionInput->text().trimmed();
  ann.side = sideInput->currentText();

  if (ann.radius.isEmpty() || ann.description.isEmpty()) {
    QMessageBox::warning(this, "Incomplete Data",
                         "Please enter a radius and description.");
    return;
  }

  // Update list widget item
  QListWidgetItem *item = annotationList->item(row);
  item->setText(ann.toString());
  QPixmap pixmap(16, 16);
  pixmap.fill(getDamageClassificationColor(ann.damageClassification));
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
    damageClassificationInput->setCurrentText(ann.damageClassification);
    radiusInput->setText(ann.radius);
    descriptionInput->setText(ann.description);
    sideInput->setCurrentText(ann.side);
  } else {
    radiusInput->clear();
    descriptionInput->clear();
    classificationInput->setCurrentIndex(0);
    damageClassificationInput->setCurrentIndex(0);
    sideInput->setCurrentIndex(0);
  }
}

/**
 * @brief Handle "Add Random" button click
 */
void MainWindow::onAddRandomAnnotation() {
  QStringList classifications = {"Crack", "Erosion", "Lightning Strike",
                                 "Delamination", "Other"};
  QStringList damageClassifications = {"Minor", "Moderate", "Severe", "Critical"};
  QStringList sides = {"Pressure Side", "Suction Side", "Leading Edge",
                       "Trailing Edge"};
  QStringList sampleDescriptions = {
      "Minor surface wear detected",       "Hairline fracture along the edge",
      "Evidence of recent strike impact",  "Surface coating peeling off",
      "Noticeable structural deformation", "Moisture ingress at the tip"};

  Annotation ann;
  ann.classification = classifications.at(rand() % classifications.size());
  ann.damageClassification = damageClassifications.at(rand() % damageClassifications.size());
  ann.side = sides.at(rand() % sides.size());
  ann.radius = QString::number((rand() % 900) / 10.0, 'f', 1);
  ann.description = sampleDescriptions.at(rand() % sampleDescriptions.size());

  // Add to storage
  annotations.append(ann);

  // Add to list widget
  QListWidgetItem *item = new QListWidgetItem(ann.toString());
  QPixmap pixmap(16, 16);
  pixmap.fill(getDamageClassificationColor(ann.damageClassification));
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

QColor MainWindow::getDamageClassificationColor(const QString &damageClassification) {
  if (damageClassification == "Minor") return QColor("#2ecc71");      // Green
  if (damageClassification == "Moderate") return QColor("#f1c40f");   // Yellow
  if (damageClassification == "Severe") return QColor("#e67e22");     // Orange
  if (damageClassification == "Critical") return QColor("#e74c3c");   // Red
  return Qt::black;
}
