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

// Forward declaration
class LlamaWorker;

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

  /**
   * @brief Slot called when "Remove" button is clicked
   */
  void onRemoveAnnotation();

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
  QPushButton *randomButton;
  QPushButton *removeButton;
  QLabel *countLabel;

  QGroupBox *reportGroup;
  QPushButton *generateButton;
  QTextEdit *reportOutput;
  QLabel *statusLabel;

  // Data storage
  QVector<Annotation> annotations;

  // LLM worker (runs in separate thread)
  LlamaWorker *worker;

  // Model path (could be made configurable via settings)
  QString modelPath;
};

#endif // MAINWINDOW_H
