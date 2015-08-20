/*
 * GridMap.cpp
 *
 *  Created on: Jul 14, 2014
 *      Author: Péter Fankhauser
 *	 Institute: ETH Zurich, Autonomous Systems Lab
 */

#include <grid_map_core/SubmapGeometry.hpp>
#include "grid_map_core/GridMap.hpp"
#include "grid_map_core/GridMapMath.hpp"
#include <iostream>
#include <cassert>
#include <math.h>
#include <algorithm>
#include <stdexcept>

using namespace std;
using namespace grid_map;

namespace grid_map {

GridMap::GridMap(const std::vector<std::string>& layers)
{
  position_.setZero();
  length_.setZero();
  resolution_ = 0.0;
  size_.setZero();
  startIndex_.setZero();
  timestamp_ = 0;
  layers_ = layers;

  for (auto& layer : layers_) {
    data_.insert(std::pair<std::string, Matrix>(layer, Matrix()));
  }
}

GridMap::GridMap() :
    GridMap(std::vector<std::string>())
{
}

GridMap::~GridMap()
{
}

void GridMap::setGeometry(const grid_map::Length& length, const double resolution,
                          const grid_map::Position& position)
{
  assert(length(0) > 0.0);
  assert(length(1) > 0.0);
  assert(resolution > 0.0);

  Size size;
  size(0) = static_cast<int>(round(length(0) / resolution)); // There is no round() function in Eigen.
  size(1) = static_cast<int>(round(length(1) / resolution));
  resize(size);
  clearAll();

  resolution_ = resolution;
  length_ = (size_.cast<double>() * resolution_).matrix();
  position_ = position;
  startIndex_.setZero();

  return;
}

void GridMap::setGeometry(const SubmapGeometry& geometry)
{
  setGeometry(geometry.getLength(), geometry.getResolution(), geometry.getPosition());
}

void GridMap::setBasicLayers(const std::vector<std::string>& basicLayers)
{
  basicLayers_ = basicLayers;
}

const std::vector<std::string>& GridMap::getBasicLayers() const
{
  return basicLayers_;
}

void GridMap::add(const std::string& layer, const double value)
{
  add(layer, Matrix::Constant(size_(0), size_(1), value));
}

void GridMap::add(const std::string& layer, const Matrix& data)
{
  assert(size_(0) == data.rows());
  assert(size_(1) == data.cols());

  if (exists(layer)) {
    // Type exists already, overwrite its data.
    data_.at(layer) = data;
  } else {
    // Type does not exist yet, add type and data.
    data_.insert(std::pair<std::string, Matrix>(layer, data));
    layers_.push_back(layer);
  }
}

bool GridMap::exists(const std::string& layer) const
{
  return !(data_.find(layer) == data_.end());
}

const grid_map::Matrix& GridMap::get(const std::string& layer) const
{
  try {
    return data_.at(layer);
  } catch (const std::out_of_range& exception) {
    throw std::out_of_range("GridMap::get(...) : No map layer '" + layer + "' available.");
  }
}

grid_map::Matrix& GridMap::get(const std::string& layer)
{
  try {
    return data_.at(layer);
  } catch (const std::out_of_range& exception) {
    throw std::out_of_range("GridMap::get(...) : No map layer of type '" + layer + "' available.");
  }
}

const grid_map::Matrix& GridMap::operator [](const std::string& layer) const
{
  return get(layer);
}

grid_map::Matrix& GridMap::operator [](const std::string& layer)
{
  return get(layer);
}

bool GridMap::erase(const std::string& layer)
{
  const auto dataIterator = data_.find(layer);
  if (dataIterator == data_.end()) return false;
  data_.erase(dataIterator);

  const auto layerIterator = std::find(layers_.begin(), layers_.end(), layer);
  if (layerIterator == layers_.end()) return false;
  layers_.erase(layerIterator);

  const auto basicLayerIterator = std::find(basicLayers_.begin(), basicLayers_.end(), layer);
  if (basicLayerIterator != basicLayers_.end()) basicLayers_.erase(basicLayerIterator);

  return true;
}

const std::vector<std::string>& GridMap::getLayers() const
{
  return layers_;
}

float& GridMap::atPosition(const std::string& layer, const grid_map::Position& position)
{
  Eigen::Array2i index;
  if (getIndex(position, index)) {
    return at(layer, index);
  }
  throw std::out_of_range("GridMap::atPosition(...) : Position is out of range.");
}

float GridMap::atPosition(const std::string& layer, const grid_map::Position& position) const
{
  Eigen::Array2i index;
  if (getIndex(position, index)) {
    return at(layer, index);
  }
  throw std::out_of_range("GridMap::atPosition(...) : Position is out of range.");
}

float& GridMap::at(const std::string& layer, const grid_map::Index& index)
{
  try {
    return data_.at(layer)(index(0), index(1));
  } catch (const std::out_of_range& exception) {
    throw std::out_of_range("GridMap::at(...) : No map layer '" + layer + "' available.");
  }
}

float GridMap::at(const std::string& layer, const Eigen::Array2i& index) const
{
  try {
    return data_.at(layer)(index(0), index(1));
  } catch (const std::out_of_range& exception) {
    throw std::out_of_range("GridMap::at(...) : No map layer '" + layer + "' available.");
  }
}

bool GridMap::getIndex(const grid_map::Position& position, grid_map::Index& index) const
{
  return getIndexFromPosition(index, position, length_, position_, resolution_, size_, startIndex_);
}

bool GridMap::getPosition(const grid_map::Index& index, grid_map::Position& position) const
{
  return getPositionFromIndex(position, index, length_, position_, resolution_, size_, startIndex_);
}

bool GridMap::isInside(const grid_map::Position& position)
{
  return checkIfPositionWithinMap(position, length_, position_);
}

bool GridMap::isValid(const grid_map::Index& index) const
{
  return isValid(index, basicLayers_);
}

bool GridMap::isValid(const grid_map::Index& index, const std::string& layer) const
{
  if (!isfinite(at(layer, index))) return false;
  return true;
}

bool GridMap::isValid(const grid_map::Index& index, const std::vector<std::string>& layers) const
{
  if (layers.empty()) return false;
  for (auto& layer : layers) {
    if (!isfinite(at(layer, index))) return false;
  }
  return true;
}

bool GridMap::getPosition3(const std::string& layer, const grid_map::Index& index,
                           grid_map::Position3& position) const
{
  if (!isValid(index, layer)) return false;
  Position position2d;
  getPosition(index, position2d);
  position.head(2) = position2d;
  position.z() = at(layer, index);
  return true;
}

bool GridMap::getVector(const std::string& layerPrefix, const grid_map::Index& index,
                        Eigen::Vector3d& vector) const
{
  std::vector<std::string> layers;
  layers.push_back(layerPrefix + "x");
  layers.push_back(layerPrefix + "y");
  layers.push_back(layerPrefix + "z");
  if (!isValid(index, layers)) return false;
  for (size_t i = 0; i < 3; ++i) {
    vector(i) = at(layers[i], index);
  }
  return true;
}

GridMap GridMap::getSubmap(const grid_map::Position& position, const grid_map::Length& length,
                           bool& isSuccess)
{
  Index index;
  return getSubmap(position, length, index, isSuccess);
}

GridMap GridMap::getSubmap(const grid_map::Position& position, const grid_map::Length& length,
                           grid_map::Index& indexInSubmap, bool& isSuccess)
{
  // Submap the generate.
  GridMap submap(layers_);
  submap.setBasicLayers(basicLayers_);
  submap.setTimestamp(timestamp_);
  submap.setFrameId(frameId_);

  // Get submap geometric information.
  SubmapGeometry submapInformation(*this, position, length, isSuccess);
  if (isSuccess == false) return GridMap(layers_);
  submap.setGeometry(submapInformation);
  submap.startIndex_.setZero(); // Because of the way we copy the data below.

  // Copy data.
  std::vector<BufferRegion> bufferRegions;

  if (!getBufferRegionsForSubmap(bufferRegions, submapInformation.getTopLeftIndex(),
                                 submap.getSize(), size_, startIndex_)) {
    cout << "Cannot access submap of this size." << endl;
    isSuccess = false;
    return GridMap(layers_);
  }

  for (auto& data : data_) {
    for (const auto& bufferRegion : bufferRegions) {
      Index index = bufferRegion.getIndex();
      Size size = bufferRegion.getSize();

      if (bufferRegion.getQuadrant() == BufferRegion::Quadrant::TopLeft) {
        submap.data_[data.first].topLeftCorner(size(0), size(1)) = data.second.block(index(0), index(1), size(0), size(1));
      } else if (bufferRegion.getQuadrant() == BufferRegion::Quadrant::TopRight) {
        submap.data_[data.first].topRightCorner(size(0), size(1)) = data.second.block(index(0), index(1), size(0), size(1));
      } else if (bufferRegion.getQuadrant() == BufferRegion::Quadrant::BottomLeft) {
        submap.data_[data.first].bottomLeftCorner(size(0), size(1)) = data.second.block(index(0), index(1), size(0), size(1));
      } else if (bufferRegion.getQuadrant() == BufferRegion::Quadrant::BottomRight) {
        submap.data_[data.first].bottomRightCorner(size(0), size(1)) = data.second.block(index(0), index(1), size(0), size(1));
      }

    }
  }

  isSuccess = true;
  return submap;
}

bool GridMap::move(const grid_map::Position& position, std::vector<Index>& newRegionIndeces,
                   std::vector<Size>& newRegionSizes)
{
  struct Lines
  {
    Lines(int index, int size)
    {
      index_ = index;
      size_ = size;
    }

    int index_;
    int size_;
  };

  Index indexShift;
  Position positionShift = position - position_;
  getIndexShiftFromPositionShift(indexShift, positionShift, resolution_);
  Position alignedPositionShift;
  getPositionShiftFromIndexShift(alignedPositionShift, indexShift, resolution_);
  vector<Lines> colLines, rowLines;

  // Delete fields that fall out of map (and become empty cells).
  for (int i = 0; i < indexShift.size(); i++) {
    if (indexShift(i) != 0) {
      if (abs(indexShift(i)) >= getSize()(i)) {
        // Entire map is dropped.
        clearAll();
        // TODO
      } else {
        // Drop cells out of map.
        int sign = (indexShift(i) > 0 ? 1 : -1);
        int startIndex = startIndex_(i) - (sign < 0 ? 1 : 0);
        int endIndex = startIndex - sign + indexShift(i);
        int nCells = abs(indexShift(i));

        int index = (sign > 0 ? startIndex : endIndex);
        mapIndexWithinRange(index, getSize()(i));

        if (index + nCells <= getSize()(i)) {
          // One region to drop.
          Lines lines(index, nCells);
          if (i == 0) { clearCols(index, nCells); colLines.push_back(lines); }
          if (i == 1) { clearRows(index, nCells); rowLines.push_back(lines); }
        } else {
          // Two regions to drop.
          int firstIndex = index;
          int firstNCells = getSize()(i) - firstIndex;
          Lines firstLines(firstIndex, firstNCells);
          if (i == 0) { clearCols(firstIndex, firstNCells); colLines.push_back(firstLines); }
          if (i == 1) { clearRows(firstIndex, firstNCells); rowLines.push_back(firstLines); }

          int secondIndex = 0;
          int secondNCells = nCells - firstNCells;
          Lines secondLines(secondIndex, secondNCells);
          if (i == 0) { clearCols(secondIndex, secondNCells); colLines.push_back(secondLines); }
          if (i == 1) { clearRows(secondIndex, secondNCells); rowLines.push_back(secondLines); }
        }
      }
    }
  }

  // Update information.
  startIndex_ += indexShift;
  mapIndexWithinRange(startIndex_, getSize());
  position_ += alignedPositionShift;

  // Retrieve cell indices that cover new area.
  for (const auto& lines : colLines) {
    newRegionIndeces.push_back(Index(0, lines.index_));
    newRegionSizes.push_back(Size(getSize()(0), lines.size_));
  }
  for (const auto& lines : rowLines) {
    newRegionIndeces.push_back(Index(lines.index_, 0));
    newRegionSizes.push_back(Size(lines.size_, getSize()(1)));
  }

  // Check if map has been moved at all.
  return (indexShift.any() != 0);
}

bool GridMap::move(const grid_map::Position& position)
{
  std::vector<Index> newRegionIndices;
  std::vector<Size> newRegionSizes;
  return move(position, newRegionIndices, newRegionSizes);
}

void GridMap::setTimestamp(const Time timestamp)
{
  timestamp_ = timestamp;
}

Time GridMap::getTimestamp() const
{
  return timestamp_;
}

void GridMap::resetTimestamp()
{
  timestamp_ = 0.0;
}

void GridMap::setFrameId(const std::string& frameId)
{
  frameId_ = frameId;
}

const std::string& GridMap::getFrameId() const
{
  return frameId_;
}

const Eigen::Array2d& GridMap::getLength() const
{
  return length_;
}

const Eigen::Vector2d& GridMap::getPosition() const
{
  return position_;
}

double GridMap::getResolution() const
{
  return resolution_;
}

const grid_map::Size& GridMap::getSize() const
{
  return size_;
}

void GridMap::setStartIndex(const grid_map::Index& startIndex) {
  startIndex_ = startIndex;
}

const Eigen::Array2i& GridMap::getStartIndex() const
{
  return startIndex_;
}

void GridMap::clear(const std::string& layer)
{
  try {
    data_.at(layer).setConstant(NAN);
  } catch (const std::out_of_range& exception) {
    throw std::out_of_range("GridMap::clear(...) : No map layer '" + layer + "' available.");
  }
}

void GridMap::clearBasic()
{
  for (auto& layer : basicLayers_) {
    clear(layer);
  }
}

void GridMap::clearAll()
{
  for (auto& data : data_) {
    data.second.setConstant(NAN);
  }
}

void GridMap::clearCols(unsigned int index, unsigned int nCols)
{
  for (auto& layer : basicLayers_) {
    data_.at(layer).block(index, 0, nCols, getSize()(1)).setConstant(NAN);
  }
}

void GridMap::clearRows(unsigned int index, unsigned int nRows)
{
  for (auto& layer : basicLayers_) {
    data_.at(layer).block(0, index, getSize()(0), nRows).setConstant(NAN);
  }
}

void GridMap::resize(const Eigen::Array2i& size)
{
  size_ = size;
  for (auto& data : data_) {
    data.second.resize(size_(0), size_(1));
  }
}

} /* namespace */
