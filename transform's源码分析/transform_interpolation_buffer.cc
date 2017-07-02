
#include "cartographer/transform/transform_interpolation_buffer.h"

#include <algorithm>

#include "Eigen/Core"
#include "Eigen/Geometry"
#include "cartographer/common/make_unique.h"
#include "cartographer/transform/transform.h"
#include "glog/logging.h"

namespace cartographer {
namespace transform {

void TransformInterpolationBuffer::Push(const common::Time time,
                                        const transform::Rigid3d& transform) {
  if (deque_.size() > 0) {
    CHECK_GE(time, latest_time()) << "New transform is older than latest.";
  }
  deque_.push_back(TimestampedTransform{time, transform});//构造临时对象
}

bool TransformInterpolationBuffer::Has(const common::Time time) const {
  if (deque_.empty()) {
    return false;
  }
  return earliest_time() <= time && time <= latest_time();//检查时间合理性
}

transform::Rigid3d TransformInterpolationBuffer::Lookup( //按照time查找
    const common::Time time) const {
  CHECK(Has(time)) << "Missing transform for: " << time;
  /*
lower_bound(ForwardIter first, ForwardIter last,const _Tp& val)
返回一个非递减序列[first, last)中的第一个大于等于值val的位置。
upper_bound返回的是最后一个大于val的位置，也是有一个新元素val进来时的插入位置。
  */

  //lower_bound,二分查找的函数,查找序列中的第一个出现的值小于time的位置
  auto start =
      std::lower_bound(deque_.begin(), deque_.end(), time,
                       [](const TimestampedTransform& timestamped_transform,//自定义lambda表达式,按照时间戳比较
                          const common::Time time) {
                         return timestamped_transform.time < time;
                       });
  auto end = start;//迭代器
  if (end->time == time) {
    return end->transform;
  }
  --start;
  if (start->time == time) {
    return start->transform;
  }

  const double duration = common::ToSeconds(end->time - start->time);
  const double factor = common::ToSeconds(time - start->time) / duration;
  const Eigen::Vector3d origin =
      start->transform.translation() +
      (end->transform.translation() - start->transform.translation()) * factor;
  const Eigen::Quaterniond rotation =
      Eigen::Quaterniond(start->transform.rotation())
          .slerp(factor, Eigen::Quaterniond(end->transform.rotation()));
  return transform::Rigid3d(origin, rotation);
}

common::Time TransformInterpolationBuffer::earliest_time() const {
  CHECK(!empty()) << "Empty buffer.";
  return deque_.front().time;
}

common::Time TransformInterpolationBuffer::latest_time() const {
  CHECK(!empty()) << "Empty buffer.";
  return deque_.back().time;
}

bool TransformInterpolationBuffer::empty() const { return deque_.empty(); }

std::unique_ptr<TransformInterpolationBuffer>
TransformInterpolationBuffer::FromTrajectory(
    const mapping::proto::Trajectory& trajectory) {
  auto interpolation_buffer =
      common::make_unique<TransformInterpolationBuffer>();//返回智能指针
  for (const mapping::proto::Trajectory::Node& node : trajectory.node()) {
    interpolation_buffer->Push(common::FromUniversal(node.timestamp()),
                               transform::ToRigid3(node.pose()));
  }
  return interpolation_buffer;
}

}  // namespace transform
}  // namespace cartographer
