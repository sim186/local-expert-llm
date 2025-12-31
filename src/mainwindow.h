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
   * @brief Sanitize and validate damage classification input
   * @param classification The classification string to sanitize
   * @return Sanitized classification string, or empty string if invalid
   */
  QString sanitizeClassification(const QString &classification);

  /**
   * @brief Check if a classification is valid
   * @param classification The classification string to validate
   * @return true if valid, false otherwise
   */
  bool isValidClassification(const QString &classification);

  /**
   * @brief Get list of valid classifications (for UI usage)
   * 
   * This method returns a copy of the valid classifications list for use
   * in UI components like QComboBox::addItems() which expect a QStringList.
   * For validation purposes, use getValidClassificationsList() instead to
   * avoid copying.
   * 
   * @return QStringList of valid classification values
   */
  QStringList getValidClassifications() const;

  /**
   * @brief Get static list of valid classifications (for validation)
   * 
   * This static method returns a const reference to the cached list of
   * valid classifications. Use this for validation to avoid copying.
   * 
   * @return Static const reference to valid classification list
   */
  static const QStringList& getValidClassificationsList();

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
