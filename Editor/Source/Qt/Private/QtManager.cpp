#include <QApplication>
#include <QDockWidget>
#include <QLabel>
#include <QMainWindow>
#include <QTreeWidget>
#include <QTreeWidgetItem>

#include "QtManager.h"


IMPLEMENT_SINGLETON(QtManager)

QtManager::QtManager() = default;
QtManager::~QtManager() = default;

Engine::EResult QtManager::Initialize(void* arg)
{
	if (!arg)
		return Engine::EResult::InvalidArgument;

	if (!qobject_cast<QApplication*>(QCoreApplication::instance()))
		return Engine::EResult::InvalidArgument;

	const auto& desc = *static_cast<const QtManagerDesc*>(arg);
	if (desc.Width <= 0 || desc.Height <= 0 || !desc.Title)
		return Engine::EResult::InvalidArgument;

	m_MainWindow = std::make_unique<QMainWindow>();

	auto* window = m_MainWindow.get();
	window->setWindowTitle(QString::fromUtf8(desc.Title) + QStringLiteral(" - Qt"));
	window->resize(desc.Width, desc.Height);
	window->setDockNestingEnabled(true);
	window->setAttribute(Qt::WA_DeleteOnClose, false);

	auto* viewport = new QLabel("Engine viewport integration pending", window);
	viewport->setAlignment(Qt::AlignCenter);
	window->setCentralWidget(viewport);

	auto* hierarchyDock = new QDockWidget("Hierarchy", window);
	hierarchyDock->setObjectName("HierarchyDock");

	auto* hierarchyTree = new QTreeWidget(hierarchyDock);
	hierarchyTree->setHeaderLabel("Scene");

	auto* sceneItem = new QTreeWidgetItem(hierarchyTree);
	sceneItem->setText(0, "Scene binding pending");

	hierarchyDock->setWidget(hierarchyTree);
	window->addDockWidget(Qt::LeftDockWidgetArea, hierarchyDock);

	auto* inspectorDock = new QDockWidget("Inspector", window);
	inspectorDock->setObjectName("InspectorDock");
	inspectorDock->setWidget(new QLabel("Selection binding pending", inspectorDock));
	window->addDockWidget(Qt::RightDockWidgetArea, inspectorDock);

	return Engine::EResult::Success;
}

void QtManager::Show()
{
	if (m_MainWindow)
		m_MainWindow->show();
}

void QtManager::Free()
{
	m_MainWindow.reset();
}