#include "mainwindow.h"
#include <QApplication>

/**
 * @brief Main entry point for the LocalLLM application
 * 
 * Initializes the Qt application and displays the main window.
 * 
 * @param argc Argument count
 * @param argv Argument values
 * @return Application exit code
 */
int main(int argc, char *argv[])
{
    // Create Qt application
    QApplication app(argc, argv);
    
    // Set application metadata
    QApplication::setApplicationName("LocalLLM");
    QApplication::setApplicationVersion("1.0.0");
    QApplication::setOrganizationName("LocalLLM Project");
    
    // Create and show main window
    MainWindow window;
    window.setWindowTitle("LocalLLM - Annotation Report Generator");
    window.resize(800, 600);
    window.show();
    
    // Run application event loop
    return app.exec();
}
