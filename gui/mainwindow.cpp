#include "mainwindow.h"

#include <QGridLayout>
#include <QPlainTextEdit>
#include <QMenu>
#include <QTabWidget>
#include <QMenuBar>
#include <QtWidgets>
#include <QContextMenuEvent>
#include <QMouseEvent>
#include <QListView>

#include "vulkan_window.h"

#include "items.h"
#include "time_point.h"
#include "item_list.h"
#include "qt_util.h"

QLabel *itemImage(uint16_t serverId)
{
    QLabel *container = new QLabel;
    container->setPixmap(QtUtil::itemPixmap(serverId));

    return container;
}

void MainWindow::experimentLayout()
{
    // rootLayout = new QVBoxLayout;
    // setLayout(rootLayout);

    // std::vector<QString> data;
    // for (int i = 0; i < 50; ++i)
    // {
    //     data.push_back("" + QString::number(i));
    // }

    // QListView *listView = new QListView();
    // QtItemTypeModel *model = new QtItemTypeModel(listView);
    // model->populate(std::move(data));

    // listView->setModel(model);
    // listView->setRootIndex(model->index(0, 0));
    // listView->setCurrentIndex(listView->rootIndex());

    // QPushButton *button = new QPushButton("&Test", this);
    // button->connect(button, &QPushButton::clicked, [=](bool checked) {
    //     VME_LOG_D("Current data: " << listView->currentIndex().data(Qt::DisplayRole).toString().toStdString());
    //     auto model = listView->currentIndex();
    //     int x = 2;
    //     // VME_LOG_D(model->_data.at(0).toStdString());
    // });

    // rootLayout->addWidget(listView);
    // rootLayout->addWidget(button);
}

void MainWindow::experiment2()
{
    rootLayout = new QVBoxLayout();
    setLayout(rootLayout);

    QListView *listView = new QListView;
    listView->setItemDelegate(new Delegate(this));

    std::vector<ItemTypeModelItem> data;
    data.push_back(ItemTypeModelItem::fromServerId(2554));
    data.push_back(ItemTypeModelItem::fromServerId(2148));
    data.push_back(ItemTypeModelItem::fromServerId(2555));

    QtItemTypeModel *model = new QtItemTypeModel(listView);
    model->populate(std::move(data));

    listView->setModel(model);
    rootLayout->addWidget(listView);
}

MainWindow::MainWindow(VulkanWindow *vulkanWindow)
{
    experiment2();
    return;

    QWidget *wrapper = vulkanWindow->wrapInWidget();

    wrapper->setFocusPolicy(Qt::StrongFocus);
    wrapper->setFocus();

    setWindowTitle("Vulkan editor");

    rootLayout = new QVBoxLayout;
    createMenuBar();

    QVBoxLayout *testLayout = new QVBoxLayout;

    // std::vector<QPixmap> pixmaps;
    // pixmaps.push_back(itemPixmap(2554));
    // pixmaps.push_back(itemPixmap(2555));
    // pixmaps.push_back(itemPixmap(2556));

    textEdit = new QPlainTextEdit;
    textEdit->setReadOnly(false);
    textEdit->setPlainText("100");
    textEdit->setMaximumHeight(80);
    // testLayout->addWidget(textEdit);

    // for (uint16_t id = 100; id < Items::items.size(); ++id)
    // {
    //     ItemType *t = Items::items.getItemType(id);

    //     if (!Items::items.validItemType(id))
    //         continue;
    //     itemImage(id);
    // }

    // testLayout->addWidget(itemImage(100));
    // testLayout->addWidget(itemImage(2554));
    // testLayout->addWidget(itemImage(2148));

    QGridLayout *gridLayout = new QGridLayout;

    gridLayout->addLayout(testLayout, 0, 0);

    mapTabs = new QTabWidget(this);
    mapTabs->setTabsClosable(true);
    QObject::connect(mapTabs, SIGNAL(tabCloseRequested(int)), this, SLOT(closeMapTab(int)));

    mapTabs->addTab(wrapper, "untitled.otbm");

    // gridLayout->addWidget(mapTabs, 1, 0, 7, 2);

    // rootLayout->addItem(gridLayout);

    setLayout(rootLayout);
}

void MainWindow::mousePressEvent(QMouseEvent *)
{
    VME_LOG_D("MainWindow::mousePressEvent");
}

void MainWindow::createMenuBar()
{
    QMenuBar *menuBar = new QMenuBar;
    QMenu *fileMenu = menuBar->addMenu(tr("&File"));

    QAction *_new = new QAction(tr("&New"), this);
    fileMenu->addAction(_new);

    QMenu *editMenu = menuBar->addMenu(tr("&Edit"));

    QAction *undo = new QAction(tr("&Undo"), this);
    editMenu->addAction(undo);

    QAction *redo = new QAction(tr("&Redo"), this);
    editMenu->addAction(redo);

    this->rootLayout->setMenuBar(menuBar);
}

void MainWindow::closeMapTab(int index)
{
    std::cout << "MainWindow::closeMapTab" << std::endl;
    this->mapTabs->widget(index)->deleteLater();
    this->mapTabs->removeTab(index);
}