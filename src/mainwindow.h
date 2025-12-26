#ifndef MAINWINDOW_H
#define MAINWINDOW_H

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
   * @brief Create the prompt for LLM based on annotation count
   * @param count Number of annotations
   * @return Formatted prompt string
   */
  QString createPrompt(int count);

  // UI Components
  QWidget *centralWidget;
  QVBoxLayout *mainLayout;

  QGroupBox *annotationGroup;
  QListWidget *annotationList;
  QLineEdit *annotationInput;
  QPushButton *addButton;
  QLabel *countLabel;

  QGroupBox *reportGroup;
  QPushButton *generateButton;
  QTextEdit *reportOutput;
  QLabel *statusLabel;

  // Data storage
  QVector<QString> annotations;

  // LLM worker (runs in separate thread)
  LlamaWorker *worker;

  // Model path (could be made configurable via settings)
  QString modelPath;
};

#endif // MAINWINDOW_H
