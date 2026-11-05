#include <QApplication>
#include <QDockWidget>
#include <QLabel>
#include <QMainWindow>
#include <QTreeWidget>
#include <QTreeWidgetItem>

#include "QtManager.h"
#include "LocalizationManager.h"

using namespace Editor;


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
	const auto text = [](const char* key)
	{
		if (!Engine::LocalizationManager::IsCreated()) return QString::fromUtf8(key);
		return QString::fromStdString(Engine::LocalizationManager::Get().GetText(key));
	};

	m_MainWindow = std::make_unique<QMainWindow>();

	auto* window = m_MainWindow.get();
	window->setWindowTitle(QString::fromUtf8(desc.Title) + QStringLiteral(" - Qt"));
	window->resize(desc.Width, desc.Height);
	window->setDockNestingEnabled(true);
	window->setAttribute(Qt::WA_DeleteOnClose, false);

	auto* viewport = new QLabel(text("QtEditor.ViewportPending"), window);
	viewport->setAlignment(Qt::AlignCenter);
	window->setCentralWidget(viewport);

	auto* hierarchyDock = new QDockWidget(text("Hierarchy.Title"), window);
	hierarchyDock->setObjectName("HierarchyDock");

	auto* hierarchyTree = new QTreeWidget(hierarchyDock);
	hierarchyTree->setHeaderLabel(text("Viewport.Scene"));

	auto* sceneItem = new QTreeWidgetItem(hierarchyTree);
	sceneItem->setText(0, text("QtEditor.ScenePending"));

	hierarchyDock->setWidget(hierarchyTree);
	window->addDockWidget(Qt::LeftDockWidgetArea, hierarchyDock);

	auto* inspectorDock = new QDockWidget(text("Inspector.Title"), window);
	inspectorDock->setObjectName("InspectorDock");
	inspectorDock->setWidget(new QLabel(text("QtEditor.SelectionPending"), inspectorDock));
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
