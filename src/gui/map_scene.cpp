/*
 * Copyright (C) 2014-2015 Christopho, Solarus - http://www.solarus-games.org
 *
 * Solarus Quest Editor is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Solarus Quest Editor is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include "gui/entity_item.h"
#include "gui/map_scene.h"
#include "map_model.h"
#include "tileset_model.h"
#include "view_settings.h"
#include <QPainter>

/**
 * @brief Creates a map scene.
 * @param map The map data to represent in the scene.
 * @param parent The parent object or nullptr.
 */
MapScene::MapScene(MapModel& map, QObject* parent) :
  QGraphicsScene(parent),
  map(map),
  entity_items(),
  layer_parent_items() {

  build();

  connect(&map, SIGNAL(size_changed(QSize)),
          this, SLOT(size_changed(QSize)));
  connect(&map, SIGNAL(entities_added(EntityIndexes)),
          this, SLOT(entities_added(EntityIndexes)));
  connect(&map, SIGNAL(entities_about_to_be_removed(EntityIndexes)),
          this, SLOT(entities_about_to_be_removed(EntityIndexes)));
  connect(&map, SIGNAL(entity_layer_changed(EntityIndex, EntityIndex)),
          this, SLOT(entity_layer_changed(EntityIndex, EntityIndex)));
  connect(&map, SIGNAL(entity_order_changed(EntityIndex, int)),
          this, SLOT(entity_order_changed(EntityIndex, int)));
  connect(&map, SIGNAL(entity_xy_changed(EntityIndex, QPoint)),
          this, SLOT(entity_xy_changed(EntityIndex, QPoint)));
  connect(&map, SIGNAL(entity_size_changed(EntityIndex, QSize)),
          this, SLOT(entity_size_changed(EntityIndex, QSize)));
}

/**
 * @brief Returns the map represented in the scene.
 * @return The map.
 */
const MapModel& MapScene::get_model() const {
  return map;
}

/**
 * @brief Returns the quest the map belongs to.
 * @return The quest.
 */
const Quest& MapScene::get_quest() const {
  return map.get_quest();
}

/**
 * @brief Returns the coordinates of the point (0,0) of the map on the scene.
 *
 * It is different due to the margin.
 *
 * @return The top-left map corner coordinates on the scene.
 */
QPoint MapScene::get_margin_top_left() {

  const QSize margin = get_margin_size();
  return QPoint(margin.width(), margin.height());
}

/**
 * @brief Returns the size of the margin around the map.
 * @return The margin in scene coordinates.
 */
QSize MapScene::get_margin_size() {

  const QSize margin(64, 64);
  return margin;
}

/**
 * @brief Create all entity items in the scene.
 */
void MapScene::build() {

  update_scene_size();
  setBackgroundBrush(Qt::gray);

  for (int i = 0; i < Layer::LAYER_NB; ++i) {
    Layer layer = static_cast<Layer>(i);

    // Create the parent item of everything that will be on this layer.
    if (layer_parent_items[i] != nullptr) {
      removeItem(layer_parent_items[i]);
      delete layer_parent_items[i];
    }
    layer_parent_items[i] = new QGraphicsPixmapItem(nullptr);
    layer_parent_items[i]->setZValue(i);
    addItem(layer_parent_items[i]);

    // Create the items on this layer.
    entity_items[i].clear();
    for (int j = 0; j < map.get_num_entities(layer); ++j) {
      EntityModel& entity = map.get_entity(EntityIndex(layer, j));
      create_entity_item(entity);
    }
  }
}

/**
 * @brief Updates the size of the scene to reflect the size of the map.
 */
void MapScene::update_scene_size() {
  setSceneRect(QRectF(QPoint(0, 0), (get_margin_size() * 2) + map.get_size()));
  update();
}

/**
 * @brief Creates a graphic item for the specified entity on the map.
 * @param entity A map entity.
 */
void MapScene::create_entity_item(EntityModel& entity) {

  Q_ASSERT(entity.is_on_map());

  const EntityIndex& index = entity.get_index();
  Layer layer = index.layer;
  int i = index.order;

  QGraphicsItem* parent_item = layer_parent_items[static_cast<int>(layer)];
  Q_ASSERT(parent_item != nullptr);
  EntityItem* item = new EntityItem(entity, parent_item);

  if (i < entity_items[layer].size()) {
    item->stackBefore(get_entity_item(index));
  }

  Q_ASSERT(layer == entity.get_layer());

  entity_items[layer].insert(i, item);
}

/**
 * @brief Returns the graphic item of the specified entity.
 * @param index Index of a map entity.
 * @return The corresponding item or nullptr if there is no such entity.
 */
EntityItem* MapScene::get_entity_item(const EntityIndex& index) {

  if (!index.is_valid()) {
    return nullptr;
  }

  const EntityItems& items = get_entity_items(index.layer);
  if (index.order < 0 || index.order >= items.size()) {
    // Index out of range.
    return nullptr;
  }

  return items.at(index.order);
}

/**
 * @brief Returns the entity items on the specified layer.
 * @param layer A layer.
 * @return Items of entities on that layer.
 */
const MapScene::EntityItems& MapScene::get_entity_items(Layer layer) {
  return entity_items[layer];
}

/**
 * @brief Shows or hides entities on a layer.
 * @param layer The layer to update.
 * @param view_settings The new view settings to apply.
 */
void MapScene::update_layer_visibility(Layer layer, const ViewSettings& view_settings) {

  for (EntityItem* item : get_entity_items(layer)) {
    item->update_visibility(view_settings);
  }
}

/**
 * @brief Shows or hides entities of the specified type.
 * @param type The entity type to change.
 * @param view_settings The new view settings to apply.
 */
void MapScene::update_entity_type_visibility(EntityType type, const ViewSettings& view_settings) {

  for (int i = 0; i < Layer::LAYER_NB; ++i) {
    Layer layer = static_cast<Layer>(i);
    for (EntityItem* item : get_entity_items(layer)) {
      if (item->get_entity_type() == type) {
        item->update_visibility(view_settings);
      }
    }
  }
}

/**
 * @brief Draws the tileset's background color as background of the map.
 * @param painter The painter.
 * @param rect The exposed rectangle in scene coordinates.
 * It may be larger than the scene.
 */
void MapScene::drawBackground(QPainter* painter, const QRectF& rect) {

  // Call the parent class to have the correct color in margins.
  QGraphicsScene::drawBackground(painter, rect);

  // Draw the background color from the tileset.
  TilesetModel* tileset = map.get_tileset_model();
  if (tileset == nullptr) {
    return;
  }

  QRect rect_no_margins = rect.toRect().intersected(QRect(get_margin_top_left(), map.get_size()));
  painter->fillRect(rect_no_margins, tileset->get_background_color());
}

/**
 * @brief Returns the entity represented by the specified item.
 * @param item An item of the scene.
 * @return The entity, or nullptr if the item does not
 * represent a map entity.
 */
const EntityModel* MapScene::get_entity_from_item(const QGraphicsItem& item) const {

  const EntityItem* entity_item = qgraphicsitem_cast<const EntityItem*>(&item);
  if (entity_item == nullptr) {
    // Not a map entity.
    return nullptr;
  }

  return &entity_item->get_entity();
}

/**
 * @overload
 *
 * Non-const version.
 */
EntityModel* MapScene::get_entity_from_item(const QGraphicsItem& item) {

  const EntityItem* entity_item = qgraphicsitem_cast<const EntityItem*>(&item);
  if (entity_item == nullptr) {
    // Not a map entity.
    return nullptr;
  }

  return &entity_item->get_entity();
}

/**
 * @brief Slot called when the size of the map has changed.
 * @param The size of the scene is updated accordingly.
 */
void MapScene::size_changed(const QSize& size) {

  Q_UNUSED(size);
  update_scene_size();
}

/**
 * @brief Slot called when entity have just been added to the map.
 *
 * Items on the scene is created accordingly.
 *
 * @param indexes Indexes of the new entities in ascending order of indexes.
 */
void MapScene::entities_added(const EntityIndexes& indexes) {

  for (const EntityIndex& index : indexes) {

    Q_ASSERT(map.entity_exists(index));
    EntityModel& entity = map.get_entity(index);
    Q_ASSERT(entity.get_index() == index);
    create_entity_item(entity);
  }
}

/**
 * @brief Slot called when entities are about to be removed from the map.
 *
 * Their items on the scene are deleted accordingly.
 *
 * @param indexes Index of the entities in ascending order of indexes.
 */
void MapScene::entities_about_to_be_removed(const EntityIndexes& indexes) {

  // Traverse the list from the end to keep correct indexes.
  for (auto it = indexes.end(); it != indexes.begin();) {
    --it;

    const EntityIndex& index = *it;
    EntityItem* item = get_entity_item(index);
    Q_ASSERT(item != nullptr);

    EntityModel& entity = map.get_entity(index);
    Q_UNUSED(entity);
    Q_ASSERT(entity.get_index() == index);
    Q_ASSERT(&item->get_entity() == &entity);
    Q_ASSERT(entity_items[index.layer][index.order] == item);
    removeItem(item);
    entity_items[index.layer].removeAt(index.order);
    delete item;
  }
}

/**
 * @brief Slot called when the layer of an entity has changed.
 *
 * Its item on the scene is updated accordingly.
 *
 * @param index_before Index of the entity before the change.
 * @param index_after Index of the entity after the change.
 */
void MapScene::entity_layer_changed(const EntityIndex& index_before,
                                    const EntityIndex& index_after) {

  EntityItem* item = get_entity_item(index_before);
  Q_ASSERT(item != nullptr);

  EntityModel& entity = map.get_entity(index_after);
  Q_UNUSED(entity);
  Q_ASSERT(entity.get_index() == index_after);
  Q_ASSERT(&item->get_entity() == &entity);
  Q_ASSERT(get_entity_item(index_before) == item);

  entity_items[index_before.layer].removeAt(index_before.order);

  removeItem(item);
  addItem(item);
  int layer_after = static_cast<int>(index_after.layer);
  int order_after = index_after.order;
  item->setParentItem(layer_parent_items[layer_after]);
  if (order_after < entity_items[layer_after].size()) {
    item->stackBefore(get_entity_item(index_after));
  }

  entity_items[layer_after].insert(order_after, item);
}

/**
 * @brief Slot called when the order of an entity has changed.
 *
 * Its item on the scene is updated accordingly.
 *
 * @param index_before Index of the entity before the change.
 * @param order_after The new order of the entity in its layer.
 */
void MapScene::entity_order_changed(const EntityIndex& index_before,
                                    int order_after) {

  EntityItem* item = get_entity_item(index_before);
  Q_ASSERT(item != nullptr);

  Layer layer = index_before.layer;
  int order_before = index_before.order;
  EntityIndex index_after(layer, order_after);
  EntityModel& entity = map.get_entity(index_after);
  Q_UNUSED(entity);
  Q_ASSERT(entity.get_index() == index_after);
  Q_ASSERT(&item->get_entity() == &entity);
  Q_ASSERT(entity_items[layer][order_before] == item);

  // Delete and recreate the item again.
  // Just removing and adding it does not seem to work when bringing entities
  // to the front.
  entity_items[layer].removeAt(order_before);
  delete item;
  create_entity_item(entity);
}

/**
 * @brief Slot called when the position of an entity has changed.
 *
 * Its item on the scene is updated accordingly.
 *
 * @param index Index of an entity.
 * @param xy Its new position.
 */
void MapScene::entity_xy_changed(const EntityIndex& index, const QPoint& xy) {

  Q_UNUSED(xy);

  EntityItem* item = get_entity_item(index);
  Q_ASSERT(item != nullptr);

  item->update_xy();
}

/**
 * @brief Slot called when the size of an entity has changed.
 *
 * Its item on the scene is updated accordingly.
 *
 * @param index Index of an entity.
 * @param size Its new size.
 */
void MapScene::entity_size_changed(const EntityIndex& index, const QSize& size) {

  Q_UNUSED(size);

  EntityItem* item = get_entity_item(index);
  Q_ASSERT(item != nullptr);

  item->update_size();
}

/**
 * @brief Returns the indexes of selected entities.
 * @return The selected entities, sorted in the order of the map.
 */
EntityIndexes MapScene::get_selected_entities() {

  EntityIndexes result;
  for (QGraphicsItem* item : selectedItems()) {
    EntityModel* entity = get_entity_from_item(*item);
    if (entity == nullptr) {
      continue;
    }
    EntityIndex index = entity->get_index();
    if (!index.is_valid()) {
      continue;
    }
    result.append(index);
  }

  qSort(result);
  return result;
}


/**
 * @brief Selects the specified entities and unselect the rest.
 * @param indexes Indexes of the entities to make selecteded.
 */
void MapScene::set_selected_entities(const EntityIndexes& indexes) {

  clearSelection();
  for (const EntityIndex& index : indexes) {
    EntityItem* item = get_entity_item(index);
    Q_ASSERT(item != nullptr);
    if (item == nullptr) {
      continue;
    }
    item->setSelected(true);
  }
}


/**
 * @brief Returns the highest layer where a specified rectangle overlaps an
 * existing visible entity.
 * @param rectangle A rectangle in map coordinates.
 * If nullptr, all entities are considered visible.
 * @return The highest layer where an entity exists in this rectangle,
 * or Layer::LAYER_LOW if there is nothing here.
 */
Layer MapScene::get_layer_in_rectangle(const QRect& rectangle) const {

  QRect scene_rectangle = rectangle;
  scene_rectangle.translate(get_margin_top_left());
  const QList<QGraphicsItem*>& items_in_rectangle = items(
        scene_rectangle, Qt::IntersectsItemBoundingRect
  );

  int max_layer = Layer::LAYER_LOW;
  for (QGraphicsItem* item : items_in_rectangle) {

    if (item->zValue() >= static_cast<int>(Layer::LAYER_NB)) {
      // This is the case of the selection rectangle and
      // of entities being added.
      continue;
    }
    const EntityModel* entity = get_entity_from_item(*item);
    if (entity == nullptr) {
      // Not a map entity.
      continue;
    }

    if (!item->isVisible()) {
      // The item is hidden by view settings.
      continue;
    }

    max_layer = qMax(max_layer, static_cast<int>(entity->get_layer()));
  }
  return static_cast<Layer>(max_layer);
}

