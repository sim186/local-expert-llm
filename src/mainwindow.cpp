#include "mainwindow.h"
#include "llamaworker.h"
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QGridLayout>
#include <QMessageBox>
#include <QTextStream>

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
  annotationGroup = new QGroupBox("Damage Annotations", centralWidget);
  QVBoxLayout *annotationLayout = new QVBoxLayout(annotationGroup);

  // Annotation list widget
  annotationList = new QListWidget(annotationGroup);
  annotationList->setAlternatingRowColors(true);
  annotationLayout->addWidget(annotationList);

  // Input Grid
  QGridLayout *inputGrid = new QGridLayout();

  // Classification
  inputGrid->addWidget(new QLabel("Classification:"), 0, 0);
  classificationInput = new QComboBox(annotationGroup);
  classificationInput->addItems(
      {"Crack", "Erosion", "Lightning Strike", "Delamination", "Other"});
  inputGrid->addWidget(classificationInput, 0, 1);

  // Severity
  inputGrid->addWidget(new QLabel("Severity:"), 0, 2);
  severityInput = new QComboBox(annotationGroup);
  severityInput->addItems({"Low", "Medium", "High", "Critical"});
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
  inputGrid->addWidget(descriptionInput, 2, 1);

  // Add Random button
  randomButton = new QPushButton("Add Random", annotationGroup);
  randomButton->setStyleSheet(
      "background-color: #e67e22; color: white; font-weight: bold;");
  inputGrid->addWidget(randomButton, 2, 2);

  // Add button
  addButton = new QPushButton("Add Annotation", annotationGroup);
  addButton->setStyleSheet(
      "background-color: #2ecc71; color: white; font-weight: bold;");
  inputGrid->addWidget(addButton, 2, 3);

  annotationLayout->addLayout(inputGrid);

  // Annotation count label
  countLabel = new QLabel("Total annotations: 0", annotationGroup);
  countLabel->setStyleSheet("font-weight: bold; color: #2c3e50;");
  annotationLayout->addWidget(countLabel);

  mainLayout->addWidget(annotationGroup);

  // ====== Report Generation Section ======
  reportGroup = new QGroupBox("Expert Technical Conclusion", centralWidget);
  QVBoxLayout *reportLayout = new QVBoxLayout(reportGroup);

  // Generate button
  generateButton = new QPushButton("Generate Expert Conclusion", reportGroup);
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
      "The expert conclusion will appear here after generation...");
  reportOutput->setMinimumHeight(200);
  reportLayout->addWidget(reportOutput);

  mainLayout->addWidget(reportGroup);

  // Set layout stretch factors
  mainLayout->setStretch(0, 5);
  mainLayout->setStretch(1, 5);

  // ====== Connect Signals and Slots ======
  connect(addButton, &QPushButton::clicked, this, &MainWindow::onAddAnnotation);
  connect(randomButton, &QPushButton::clicked, this,
          &MainWindow::onAddRandomAnnotation);
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
  // Try to load the context template file
  QString contextPath = QDir::currentPath() + "/context/input_context.txt";
  QFile contextFile(contextPath);

  QString prompt;

  if (contextFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
    // Read the template
    QTextStream in(&contextFile);
    QString templateContent = in.readAll();
    contextFile.close();

    // Build formatted annotations section
    QString annotationsSection;
    for (int i = 0; i < annotations.size(); ++i) {
      const auto &ann = annotations[i];

      if (i > 0) {
        annotationsSection += "\n"; // Separate multiple annotations
      }

      annotationsSection += QString("Annotation #%1:\n").arg(i + 1);
      annotationsSection +=
          QString("Classification: %1\n").arg(ann.classification);
      annotationsSection += QString("Severity (1-5): %1\n").arg(ann.severity);
      annotationsSection +=
          QString("Radius (meters from root): %1\n").arg(ann.radius);
      annotationsSection += QString("Side (SS/PS): %1\n").arg(ann.side);
      annotationsSection += QString("Description: %1\n").arg(ann.description);
    }

    // Replace the placeholder section in the template
    // Find and replace the INPUT ANNOTATIONS section (lines 10-15 in the
    // template)
    QStringList lines = templateContent.split('\n');
    QString result;
    bool inAnnotationSection = false;

    for (const QString &line : lines) {
      if (line.contains("### INPUT ANNOTATIONS:")) {
        result += line + "\n";
        result += annotationsSection;
        inAnnotationSection = true;
      } else if (inAnnotationSection && line.contains("### TASK:")) {
        result += "\n" + line + "\n";
        inAnnotationSection = false;
      } else if (!inAnnotationSection) {
        result += line + "\n";
      }
    }

    prompt = result;

  } else {
    // Fallback to the original prompt if context file is not found
    prompt = "You are a Wind Turbine Blade Expert. Your task is to "
             "analyze damage annotations and provide a professional "
             "technical conclusion based on blade standards reports.\n\n";
    prompt += "Input Annotations:\n";

    for (const auto &ann : annotations) {
      prompt +=
          QString(
              "- Damage: %1, Severity: %2, Location: %3m on %4. Detail: %5\n")
              .arg(ann.classification)
              .arg(ann.severity)
              .arg(ann.radius)
              .arg(ann.side)
              .arg(ann.description);
    }

    prompt += "\nBased on these findings, please generate a report with the "
              "following sections:\n";
    prompt += "1. Summary of Findings: A technical summary of the identified "
              "damages.\n";
    prompt += "2. Recommendations/Guidelines: Specific actions based on blade "
              "inspection standards.\n";
    prompt += "3. Next Inspection Suggestions: Timeline and focus areas.\n";
    prompt += "4. Conclusion: Final assessment of the blade's condition.\n\n";
    prompt += "Expert Conclusion:";
  }

  return prompt;
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

  if (ann.radius.isEmpty() || ann.description.isEmpty()) {
    QMessageBox::warning(this, "Incomplete Data",
                         "Please enter a radius and description.");
    return;
  }

  // Add to storage
  annotations.append(ann);

  // Add to list widget
  annotationList->addItem(ann.toString());

  // Clear numeric/text fields
  radiusInput->clear();
  descriptionInput->clear();
  classificationInput->setFocus();

  // Update count
  updateAnnotationCount();
}

/**
 * @brief Handle "Add Random" button click
 */
void MainWindow::onAddRandomAnnotation() {
  QStringList classifications = {"Crack", "Erosion", "Lightning Strike",
                                 "Delamination", "Other"};
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
  annotationList->addItem(ann.toString());

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
  QString prompt = createPrompt();

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
