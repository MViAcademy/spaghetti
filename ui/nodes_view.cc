#include "ui/nodes_view.h"

#include <QDebug>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QGraphicsItemGroup>
#include <QGraphicsLineItem>
#include <QMimeData>
#include <QTimeLine>

#include "core/registry.h"
#include "elements/package.h"
#include "nodes/node.h"
#include "ui/elements_list.h"
#include "ui/link_item.h"
#include "ui/package_view.h"

NodesView::NodesView(QGraphicsScene *const a_scene, PackageView *a_parent)
  : QGraphicsView{ a_scene, a_parent }
  , m_packageView{ a_parent }
  , m_inputs{ new nodes::Node }
  , m_outputs{ new nodes::Node }
  , m_gridLarge{ new QGraphicsItemGroup }
  , m_gridSmall{ new QGraphicsItemGroup }
{
  setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::HighQualityAntialiasing |
                 QPainter::SmoothPixmapTransform);
  setDragMode(QGraphicsView::ScrollHandDrag);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
  setResizeAnchor(QGraphicsView::NoAnchor);
  setTransformationAnchor(QGraphicsView::AnchorViewCenter);
  setObjectName("GraphicsView");

  setViewportUpdateMode(QGraphicsView::FullViewportUpdate);

  createGrid();

  setAcceptDrops(true);

  using NodeType = nodes::Node::Type;
  m_inputs->setPos(-600.0, 0.0);
  m_inputs->setType(NodeType::eInputs);
  m_inputs->setElement(m_packageView->package());
  m_inputs->setIcon(":/elements/logic/inputs.png");
  m_inputs->setNodesView(this);
  m_inputs->iconify();
  m_outputs->setPos(600.0, 0.0);
  m_outputs->setType(NodeType::eOutputs);
  m_outputs->setElement(m_packageView->package());
  m_outputs->setIcon(":/elements/logic/outputs.png");
  m_outputs->setNodesView(this);
  m_outputs->iconify();

  a_scene->addItem(m_inputs);
  a_scene->addItem(m_outputs);
}

NodesView::~NodesView() {}

void NodesView::dragEnterEvent(QDragEnterEvent *a_event)
{
  if (a_event->mimeData()->hasText()) {
    auto const pathString = a_event->mimeData()->text();
    auto const name = a_event->mimeData()->data("metadata/name");
    auto const icon = a_event->mimeData()->data("metadata/icon");
    auto const stringData = pathString.toLatin1();
    char const *const path{ stringData.data() };

    auto const dropPosition = mapToScene(a_event->pos());

    core::Registry &registry{ core::Registry::get() };

    assert(m_dragNode == nullptr);
    m_dragNode = registry.createNode(path);
    m_dragNode->setNodesView(this);
    m_dragNode->setName(name);
    m_dragNode->setPath(pathString);
    m_dragNode->setIcon(icon);
    m_dragNode->setPos(dropPosition);
    scene()->addItem(m_dragNode);

    a_event->accept();
  } else
    QGraphicsView::dragEnterEvent(a_event);
}

void NodesView::dragLeaveEvent([[maybe_unused]] QDragLeaveEvent *a_event)
{
  scene()->removeItem(m_dragNode);
  delete m_dragNode;
  m_dragNode = nullptr;
}

void NodesView::dragMoveEvent(QDragMoveEvent *a_event)
{
  auto const dropPosition = mapToScene(a_event->pos());
  if (m_dragNode)
    m_dragNode->setPos(dropPosition);
  else if (m_dragLink) {
    QGraphicsView::dragMoveEvent(a_event);
    m_dragLink->setTo(mapToScene(a_event->pos()));
  }
}

void NodesView::dropEvent(QDropEvent *a_event)
{
  if (a_event->mimeData()->hasText()) {
    auto const pathString = a_event->mimeData()->text();
    auto const stringData = pathString.toLatin1();
    char const *const path{ stringData.data() };

    auto package = m_packageView->package();
    auto element = package->add(path);
    m_dragNode->setElement(element);
    m_dragNode->iconify();
    m_dragNode = nullptr;
  }

  QGraphicsView::dropEvent(a_event);
}

void NodesView::keyPressEvent(QKeyEvent* a_event)
{
  m_snapToGrid = a_event->modifiers() & Qt::ShiftModifier;
}

void NodesView::keyReleaseEvent(QKeyEvent *a_event)
{
  qDebug() << a_event;
  m_snapToGrid = a_event->modifiers() & Qt::ShiftModifier;

  switch (a_event->key()) {
    case Qt::Key_R:
      centerOn(0.0, 0.0);
      resetMatrix();
      updateGrid(matrix().m11());
      break;
    default:
      break;
  }

  auto const selected = scene()->selectedItems();
  for (auto &&item : selected) {
    if (item->type() == nodes::NODE_TYPE) {
      qDebug() << "Node:" << item;
    }
  }
}

void NodesView::wheelEvent(QWheelEvent *a_event)
{
  int32_t const numDegrees{ a_event->delta() / 8 };
  int32_t const numSteps{ numDegrees / 15 };

  m_scheduledScalings += numSteps;

  if (m_scheduledScalings * numSteps < 0) m_scheduledScalings = numSteps;

  QTimeLine *const animation{ new QTimeLine{ 350, this } };
  animation->setUpdateInterval(20);

  connect(animation, &QTimeLine::valueChanged, [&](qreal) {
    qreal const factor{ 1.0 + static_cast<qreal>(m_scheduledScalings) / 300.0 };
    QMatrix temp{ matrix() };
    temp.scale(factor, factor);
    if (temp.m11() >= 0.2 && temp.m11() <= 4.0) scale(factor, factor);
  });
  connect(animation, &QTimeLine::finished, [&]() {
    if (m_scheduledScalings > 0)
      m_scheduledScalings--;
    else
      m_scheduledScalings++;
    if (sender()) sender()->deleteLater();
    qreal const realScale = matrix().m11();
    updateGrid(realScale);
  });

  animation->start();
}

void NodesView::mouseMoveEvent(QMouseEvent *a_event)
{
  QGraphicsView::mouseMoveEvent(a_event);
}

void NodesView::setDragLink(LinkItem *a_link)
{
  m_dragLink = a_link;
}

void NodesView::acceptDragLink()
{
  m_dragLink = nullptr;
}

void NodesView::cancelDragLink()
{
  delete m_dragLink;
  m_dragLink = nullptr;
}

void NodesView::createGrid()
{
  constexpr int32_t SIZE = 10000;
  constexpr int32_t SPACING = 10;
  QPen penNormal{ QColor(156, 156, 156, 32) };
  QPen penAxis{ QColor(156, 156, 156, 128) };
  QGraphicsLineItem *horizontal{};
  QGraphicsLineItem *vertical{};

  for (int32_t i = -SIZE; i <= SIZE; i += SPACING) {
    penNormal.setWidth(1);
    penAxis.setWidth(1);

    horizontal = new QGraphicsLineItem{ static_cast<qreal>(i), -SIZE, static_cast<qreal>(i), SIZE };
    horizontal->setPen(i == 0 ? penAxis : penNormal);
    horizontal->setZValue(-100.0);
    m_gridLarge->addToGroup(horizontal);

    vertical = new QGraphicsLineItem{ -SIZE, static_cast<qreal>(i), SIZE, static_cast<qreal>(i) };
    vertical->setPen(i == 0 ? penAxis : penNormal);
    vertical->setZValue(-100.0);
    m_gridLarge->addToGroup(vertical);

    if (i % 100 == 0 || i == 0) {
      penNormal.setWidth(3);
      penAxis.setWidth(3);

      horizontal = new QGraphicsLineItem{ static_cast<qreal>(i), -SIZE, static_cast<qreal>(i), SIZE };
      horizontal->setPen(i == 0 ? penAxis : penNormal);
      horizontal->setZValue(-100.0);
      m_gridSmall->addToGroup(horizontal);

      vertical = new QGraphicsLineItem{ -SIZE, static_cast<qreal>(i), SIZE, static_cast<qreal>(i) };
      vertical->setPen(i == 0 ? penAxis : penNormal);
      vertical->setZValue(-100.0);
      m_gridSmall->addToGroup(vertical);
    }
  }

  m_gridDensity = GridDensity::eLarge;
  scene()->addItem(m_gridLarge);
  scene()->addItem(m_gridSmall);
  m_gridSmall->hide();

  updateGrid(matrix().m11());
}

void NodesView::updateGrid(qreal const a_scale)
{
  GridDensity const newDensity{ (a_scale >= 0.85 ? GridDensity::eLarge : GridDensity::eSmall) };

  if (newDensity == m_gridDensity) return;

  switch (m_gridDensity = newDensity; m_gridDensity) {
    case GridDensity::eLarge:
      m_gridLarge->show();
      m_gridSmall->hide();
      break;
    case GridDensity::eSmall:
      m_gridLarge->hide();
      m_gridSmall->show();
      break;
  }
}
