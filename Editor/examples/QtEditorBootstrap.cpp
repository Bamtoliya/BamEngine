#include <QApplication>
#include <QDockWidget>
#include <QLabel>
#include <QMainWindow>
#include <QTreeWidget>
#include <QTreeWidgetItem>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    QMainWindow window;
    window.setWindowTitle("BamEngine Qt Editor");
    window.resize(1280, 720);
    window.setDockNestingEnabled(true);

    auto* viewport = new QLabel("Viewport", &window);
    viewport->setAlignment(Qt::AlignCenter);
    window.setCentralWidget(viewport);

    auto* hierarchyDock = new QDockWidget("Hierarchy", &window);
    hierarchyDock->setObjectName("HierarchyDock");

    auto* hierarchyTree = new QTreeWidget(hierarchyDock);
    hierarchyTree->setHeaderLabel("Scene");

    auto* sceneItem = new QTreeWidgetItem(hierarchyTree);
    sceneItem->setText(0, "Untitled Scene");

    hierarchyDock->setWidget(hierarchyTree);
    window.addDockWidget(Qt::LeftDockWidgetArea, hierarchyDock);

    auto* inspectorDock = new QDockWidget("Inspector", &window);
    inspectorDock->setObjectName("InspectorDock");
    inspectorDock->setWidget(new QLabel("No selection", inspectorDock));
    window.addDockWidget(Qt::RightDockWidgetArea, inspectorDock);

    window.show();
    return app.exec();
}