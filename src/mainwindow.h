#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QPushButton>
#include <QString>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QVector>
#include "llmcontroller.h"

// Forward declaration
class LlamaWorker;
class LLMControllerDialog;

/**
 * @brief Structured annotation data
 */
struct Annotation {
  QString classification;
  QString severity;
  QString radius;
  QString description;
  QString side;

  /**
   * @brief Convert annotation to a readable string for the list
   */
  QString toString() const {
    return QString("[%1] %2 - %3 (%4m, %5)")
        .arg(severity)
        .arg(classification)
        .arg(description)
        .arg(radius)
        .arg(side);
  }

  /**
   * @brief Convert annotation to a prompt-ready string (classification, severity, location)
   * Includes the operator description only if non-empty.
   */
  QString toPromptString() const {
    // Provide an explicit severity→category mapping to keep the model consistent
    QString categoryHint;
    if (severity == "Low") categoryHint = " (Category 1: Cosmetic)";
    else if (severity == "Medium") categoryHint = " (Category 3: Significant)";
    else if (severity == "High") categoryHint = " (Category 4: Serious)";
    else if (severity == "Critical") categoryHint = " (Category 5: Critical)";

    QString out;
    out += QString("Type: %1\n").arg(classification);
    out += QString("Severity: %1%2\n").arg(severity, categoryHint);
    out += QString("Location: %1m on %2\n").arg(radius, side);
    if (!description.trimmed().isEmpty()) {
      out += QString("Description: %1\n").arg(description.trimmed());
    }
    out += "\n";
    return out;
  }
};

/**
 * @brief Main window class for the LocalLLM application
 *
 * Provides UI for managing annotations and generating LLM-based report
 * conclusions.
 */
class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  /**
   * @brief Constructor
   * @param parent Parent widget
   */
  explicit MainWindow(QWidget *parent = nullptr);

  /**
   * @brief Destructor
   */
  ~MainWindow();

private slots:
  /**
   * @brief Slot called when "Add Annotation" button is clicked
   */
  void onAddAnnotation();

  /**
   * @brief Slot called when "Generate Report" button is clicked
   */
  void onGenerateReport();

  /**
   * @brief Slot called when LLM worker completes text generation
   * @param result Generated text from LLM
   */
  void onGenerationComplete(const QString &result);

  /**
   * @brief Slot called when LLM worker encounters an error
   * @param error Error message
   */
  void onGenerationError(const QString &error);

  /**
   * @brief Slot called when "Add Random" button is clicked
   */
  void onAddRandomAnnotation();

  void onUpdateAnnotation();
  void onAnnotationSelected();

  /**
   * @brief Slot called when "Remove" button is clicked
   */
  void onRemoveAnnotation();

  void openSettings();

private:
  /**
   * @brief Initialize the user interface
   */
  void setupUI();

  /**
   * @brief Update the annotation count label
   */
  void updateAnnotationCount();

  /**
   * @brief Create the prompt for LLM based on annotations
   * @return Formatted prompt string
   */
  QString createPrompt();

  /**
   * @brief Apply modern theme to the application
   */
  void applyTheme();
  QColor getSeverityColor(const QString &severity);

  // UI Components
  QWidget *centralWidget;
  QVBoxLayout *mainLayout;

  QGroupBox *annotationGroup;
  QListWidget *annotationList;

  // Input Fields
  QComboBox *classificationInput;
  QComboBox *severityInput;
  QLineEdit *radiusInput;
  QLineEdit *descriptionInput;
  QComboBox *sideInput;

  QPushButton *addButton;
  QPushButton *updateButton;
  QPushButton *randomButton;
  QPushButton *removeButton;
  QLabel *countLabel;

  QGroupBox *reportGroup;
  QPushButton *generateButton;
  QTextEdit *reportOutput;
  QLabel *statusLabel;
  QLabel *m_elapsedTimeLabel;
  QLabel *m_modelInfoLabel;

  // Data storage
  QVector<Annotation> annotations;

  // LLM worker (runs in separate thread)
  QThread *m_workerThread;
  LlamaWorker *m_worker;

  LLMControllerDialog *m_controller;

signals:
    void requestGeneration(const QString &prompt, const LlamaParams &params);
    void requestLoadModel(const LlamaParams &params);
};

#endif // MAINWINDOW_H
