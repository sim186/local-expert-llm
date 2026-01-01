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
#include <QDockWidget>
#include <QVBoxLayout>
#include <QResizeEvent>
#include <QCloseEvent>
#include <QEvent>

/**
 * @brief Constructor - Initialize main window and UI
 */
MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), m_workerThread(nullptr), m_worker(nullptr) {
  m_controller = new LLMControllerDialog(this);

  setupUI();
  updateAnnotationCount();
  applyTheme();

  // Start with a wide window to accommodate three columns
  resize(1200, 900);

  setCentralWidget(nullptr); // Central area is empty, docks are used

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
  
  QAction *exitAction = fileMenu->addAction("Exit");
  connect(exitAction, &QAction::triggered, this, &QWidget::close);

  // Create View Menu
  QMenu *viewMenu = menuBar->addMenu("View");

  // Create central widget
  centralWidget = new QWidget(this);
  setCentralWidget(centralWidget);

  mainLayout = new QVBoxLayout(centralWidget);
  mainLayout->setSpacing(10);
  mainLayout->setContentsMargins(10, 10, 10, 10);

  // We'll use dock widgets for a three-column layout (annotation | output | settings)

  // ====== Annotation Section ======
  annotationGroup = new QGroupBox("Damage Annotations", centralWidget);
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
    classificationInput->addItems({
      "Crack",
      "Erosion",
      "Lightning Strike",
      "Delamination",
      "Corrosion",
      "Impact",
      "Abrasion",
      "Fretting",
      "Buckling",
      "Void",
      "Fiber Break",
      "Blistering",
      "Surface Contamination",
      "Bonding Failure",
      "Resin Starvation",
      "Other"
    });
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
  addButton = new QPushButton("Add", annotationGroup);
  addButton->setObjectName("addButton");
  buttonLayout->addWidget(addButton);

  // Update button
  updateButton = new QPushButton("Update", annotationGroup);
  updateButton->setObjectName("updateButton");
  updateButton->setEnabled(false);
  buttonLayout->addWidget(updateButton);

  // Add Random button
  randomButton = new QPushButton("Random", annotationGroup);
  randomButton->setObjectName("randomButton");
  buttonLayout->addWidget(randomButton);

  // Remove button
  removeButton = new QPushButton("Remove", annotationGroup);
  removeButton->setObjectName("removeButton");
  removeButton->setEnabled(false); // Disabled until an item is selected
  buttonLayout->addWidget(removeButton);

  annotationLayout->addLayout(buttonLayout);

  // Annotation count label
  countLabel = new QLabel("Total annotations: 0", annotationGroup);
  countLabel->setStyleSheet("font-weight: bold; color: #2c3e50;");
  annotationLayout->addWidget(countLabel);

  // ====== Report Generation Section ======
  reportGroup = new QGroupBox("Expert Technical Conclusion", centralWidget);
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

  // Create a dock to host the settings dialog and keep it visible
  m_controllerDock = new QDockWidget("LLM Settings", this);
  m_controllerDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
  // Make settings dock movable, floatable, and closable
  m_controllerDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);
  m_controllerDock->installEventFilter(this);
  viewMenu->addAction(m_controllerDock->toggleViewAction());

  m_controllerDockContainer = new QWidget(m_controllerDock);
  QVBoxLayout *dockLayout = new QVBoxLayout(m_controllerDockContainer);
  dockLayout->setContentsMargins(0, 0, 0, 0);

  m_controller->setParent(m_controllerDockContainer);
  m_controller->setWindowFlags(Qt::Widget);
  dockLayout->addWidget(m_controller);

  m_controllerDock->setWidget(m_controllerDockContainer);
  m_controllerDock->setMinimumWidth(300);

  // Create a dock for the LLM output (report) so it's dockable and visible
  m_outputDock = new QDockWidget("Expert Conclusion", this);
  m_outputDock->setAllowedAreas(Qt::RightDockWidgetArea | Qt::LeftDockWidgetArea);
  m_outputDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);
  m_outputDock->installEventFilter(this);
  viewMenu->addAction(m_outputDock->toggleViewAction());

  m_outputDockContainer = new QWidget(m_outputDock);
  QVBoxLayout *outDockLayout = new QVBoxLayout(m_outputDockContainer);
  outDockLayout->setContentsMargins(6, 6, 6, 6);

  // Reparent the report group into the output dock
  reportGroup->setParent(m_outputDockContainer);
  outDockLayout->addWidget(reportGroup);

  m_outputDock->setWidget(m_outputDockContainer);
  m_outputDock->setMinimumWidth(400);

  // Create a dock for the annotation panel and reparent the annotationGroup into it
  m_annotationDock = new QDockWidget("Damage Annotations", this);
  m_annotationDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
  m_annotationDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);
  m_annotationDock->installEventFilter(this);
  viewMenu->addAction(m_annotationDock->toggleViewAction());

  m_annotationDockContainer = new QWidget(m_annotationDock);
  QVBoxLayout *annDockLayout = new QVBoxLayout(m_annotationDockContainer);
  annDockLayout->setContentsMargins(6,6,6,6);

  // Reparent the annotation group into the dock
  annotationGroup->setParent(m_annotationDockContainer);
  annDockLayout->addWidget(annotationGroup);

  m_annotationDock->setWidget(m_annotationDockContainer);
  addDockWidget(Qt::LeftDockWidgetArea, m_annotationDock);
  m_annotationDock->setMinimumWidth(250);
  m_annotationDock->show();

  // Add output and controller docks after the annotation dock to ensure
  // they are split horizontally in the desired order: annotation | output | settings
  addDockWidget(Qt::RightDockWidgetArea, m_outputDock);
  m_outputDock->show();

  addDockWidget(Qt::RightDockWidgetArea, m_controllerDock);
  m_controllerDock->show();

  // Arrange docks horizontally: annotation | output | settings
  splitDockWidget(m_annotationDock, m_outputDock, Qt::Horizontal);
  splitDockWidget(m_outputDock, m_controllerDock, Qt::Horizontal);

  // Set initial dock sizes to prevent collapsing
  resizeDocks({m_annotationDock, m_outputDock, m_controllerDock}, {250, 600, 300}, Qt::Horizontal);

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
    annotationsSection += QString("--- DAMAGE #%1 ---\n").arg(i + 1);
    annotationsSection += ann.toPromptString();
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
  ann.severity = severityInput->currentText();
  ann.radius = radiusInput->text().trimmed();
  ann.description = descriptionInput->text().trimmed();
  ann.side = sideInput->currentText();

  if (ann.radius.isEmpty()) {
    QMessageBox::warning(this, "Incomplete Data",
                         "Please enter a radius.");
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
  ann.classification = classificationInput->currentText();
  ann.severity = severityInput->currentText();
  ann.radius = radiusInput->text().trimmed();
  ann.description = descriptionInput->text().trimmed();
  ann.side = sideInput->currentText();

  if (ann.radius.isEmpty()) {
    QMessageBox::warning(this, "Incomplete Data",
                         "Please enter a radius.");
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
  QStringList classifications = {"Crack", "Erosion", "Lightning Strike", "Delamination", "Corrosion", "Impact", "Abrasion", "Fretting", "Buckling", "Void", "Fiber Break", "Blistering", "Surface Contamination", "Bonding Failure", "Resin Starvation", "Other"};
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
  // Always show the settings dock and bring it to front
  if (m_controllerDock) {
    m_controllerDock->show();
    m_controllerDock->raise();
  }
  if (m_controller) {
    m_controller->show();
    m_controller->raise();
    m_controller->activateWindow();
  }
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
  setMinimumSize(1200, 800);

  // Modern flat theme stylesheet
  QString theme = R"(
    /* Main window background */
    QMainWindow, QDialog {
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
  // Apply same theme to the settings dialog so it matches other panels
  if (m_controller) {
    m_controller->setStyleSheet(theme);
  }
}

QColor MainWindow::getSeverityColor(const QString &severity) {
  if (severity == "Low") return QColor("#2ecc71");      // Green
  if (severity == "Medium") return QColor("#f1c40f");   // Yellow
  if (severity == "High") return QColor("#e67e22");     // Orange
  if (severity == "Critical") return QColor("#e74c3c"); // Red
  return Qt::black;
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
  if (event->type() == QEvent::Close) {
    QDockWidget *dock = qobject_cast<QDockWidget *>(watched);
    if (dock && dock->isFloating()) {
      dock->setFloating(false);
      event->ignore(); // Prevent closing/hiding
      return true;
    }
  }
  return QMainWindow::eventFilter(watched, event);
}
